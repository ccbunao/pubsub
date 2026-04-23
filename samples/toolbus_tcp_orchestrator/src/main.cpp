#include <toolbus/id.h>
#include <toolbus/message.h>
#include <toolbus/topics.h>

#include <toolbus_tcp/publisher.h>
#include <toolbus_tcp/subscriber.h>

#include <tcp_pubsub/executor.h>
#include <tcp_pubsub/tcp_pubsub_logger.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

namespace
{
  std::uint64_t now_ms()
  {
    return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
  }
}

int main(int argc, char** argv)
{
  const std::string service = (argc >= 2) ? argv[1] : "demo";
  const std::string caller  = (argc >= 3) ? argv[2] : "orchestrator";
  const std::string call_bind = (argc >= 4) ? argv[3] : "0.0.0.0";
  const std::uint16_t call_port = (argc >= 5) ? static_cast<std::uint16_t>(std::stoi(argv[4])) : 17000;
  const std::string worker_host = (argc >= 6) ? argv[5] : "127.0.0.1";
  const std::uint16_t worker_result_port = (argc >= 7) ? static_cast<std::uint16_t>(std::stoi(argv[6])) : 17001;

  const auto exec = std::make_shared<tcp_pubsub::Executor>(4, tcp_pubsub::logger::logger_no_verbose_debug);

  toolbus_tcp::Publisher call_pub(exec, call_bind, call_port);
  if (!call_pub.isRunning())
  {
    std::cerr << "failed to start call publisher on " << call_bind << ":" << call_port << "\n";
    return 1;
  }

  std::mutex m;
  std::condition_variable cv;
  bool got = false;
  toolbus::ToolResult last;

  const std::string res_topic = toolbus::result_topic(caller);
  toolbus_tcp::Subscriber res_sub(exec, worker_host, worker_result_port);
  res_sub.on(
    res_topic,
    [&](const std::string& json)
    {
      try
      {
        auto r = toolbus::parse_tool_result(json);
        if (r.caller != caller)
          return;
        {
          std::lock_guard<std::mutex> lock(m);
          last = std::move(r);
          got = true;
        }
        cv.notify_all();
      }
      catch (...)
      {
      }
    });

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  toolbus::ToolCall call;
  call.id = toolbus::make_call_id();
  call.name = "demo.echo";
  call.caller = caller;
  call.timeout_ms = 2000;
  call.args = nlohmann::json::object({{"text", "hello from toolbus over tcp"}}); // NOLINT
  call.ts_ms = now_ms();

  const std::string call_topic = toolbus::call_topic(service);
  const std::string call_json = toolbus::to_json_string(call);

  bool ok = false;
  for (int attempt = 1; attempt <= 3; ++attempt)
  {
    {
      std::lock_guard<std::mutex> lock(m);
      got = false;
    }

    call_pub.publish(call_topic, call_json);
    std::cout << "[toolbus_tcp] published ToolCall attempt=" << attempt
              << " id=" << call.id
              << " topic=" << call_topic
              << " call_port=" << call_pub.port()
              << " res_topic=" << res_topic
              << "\n";

    std::unique_lock<std::mutex> lock(m);
    ok = cv.wait_for(lock, std::chrono::milliseconds(call.timeout_ms), [&] { return got && last.id == call.id; });
    if (ok)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  if (!ok)
  {
    std::cerr << "[toolbus_tcp] timeout waiting for result\n";
    return 2;
  }

  std::cout << "[toolbus_tcp] result ok=" << (last.ok ? "true" : "false") << " duration_ms=" << last.duration_ms << "\n";
  if (last.ok)
    std::cout << last.result.dump(2) << "\n";
  else
    std::cout << "error " << last.error.code << ": " << last.error.message << "\n";

  return 0;
}

