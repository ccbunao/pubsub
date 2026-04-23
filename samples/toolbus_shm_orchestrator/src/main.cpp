#include <shm_pubsub/shm/publisher.h>
#include <shm_pubsub/shm/subscriber.h>

#include <toolbus/id.h>
#include <toolbus/message.h>
#include <toolbus/topics.h>

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

// Minimal orchestrator:
// - publishes ToolCall to tool.call.<service>
// - waits for ToolResult on tool.result.<caller> and matches by id
int main(int argc, char** argv)
{
  const std::string service = (argc >= 2) ? argv[1] : "demo";
  const std::string caller  = (argc >= 3) ? argv[2] : "orchestrator";

  const std::string call_topic = toolbus::call_topic(service);
  const std::string result_topic = toolbus::result_topic(caller);

  shm_pubsub::shm::Publisher call_publisher(call_topic, 4U * 1024U * 1024U);
  if (!call_publisher.isRunning())
  {
    std::cerr << "failed to create call publisher shm topic '" << call_topic << "'\n";
    return 1;
  }

  shm_pubsub::shm::Subscriber result_subscriber(result_topic);

  std::mutex m;
  std::condition_variable cv;
  bool got = false;
  toolbus::ToolResult last;

  result_subscriber.setCallback(
    [&](const shm_pubsub::CallbackData& data)
    {
      if (!data.buffer_ || data.buffer_->empty())
        return;
      try
      {
        const std::string json(data.buffer_->data(), data.buffer_->size());
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
        // ignore
      }
    },
    true
  );
  result_subscriber.start(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Example call: demo.echo
  toolbus::ToolCall call;
  call.id = toolbus::make_call_id();
  call.name = "demo.echo";
  call.caller = caller;
  call.timeout_ms = 2000;
  call.args = nlohmann::json::object({{"text", "hello from toolbus"}}); // NOLINT
  call.ts_ms = now_ms();

  const std::string call_json = toolbus::to_json_string(call);
  std::cout << "[toolbus] call topic:   " << call_topic << "\n"
            << "[toolbus] result topic: " << result_topic << "\n";

  // Give subscribers a chance to attach to the call topic before publishing.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  bool ok = false;
  for (int attempt = 1; attempt <= 3; ++attempt)
  {
    {
      std::lock_guard<std::mutex> lock(m);
      got = false;
    }

    call_publisher.send(call_json.data(), call_json.size());
    std::cout << "[toolbus] published ToolCall attempt=" << attempt
              << " id=" << call.id
              << " name=" << call.name
              << " to " << call_topic << "\n";

    std::unique_lock<std::mutex> lock(m);
    ok = cv.wait_for(lock, std::chrono::milliseconds(call.timeout_ms), [&] { return got && last.id == call.id; });
    if (ok)
      break;

    if (attempt < 3)
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  if (!ok)
  {
    std::cerr << "[toolbus] timeout waiting for result\n";
    return 2;
  }

  std::cout << "[toolbus] result ok=" << (last.ok ? "true" : "false") << " duration_ms=" << last.duration_ms << "\n";
  if (last.ok)
    std::cout << last.result.dump(2) << "\n";
  else
    std::cout << "error " << last.error.code << ": " << last.error.message << "\n";
  return 0;
}
