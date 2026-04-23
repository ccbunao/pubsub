#include <toolbus/dedup_cache.h>
#include <toolbus/id.h>
#include <toolbus/message.h>
#include <toolbus/topics.h>

#include <toolbus_tcp/publisher.h>
#include <toolbus_tcp/subscriber.h>

#include <tcp_pubsub/executor.h>
#include <tcp_pubsub/tcp_pubsub_logger.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace
{
  std::uint64_t now_ms()
  {
    return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
  }

  toolbus::ToolResult make_error(const toolbus::ToolCall& call, const std::string& code, const std::string& msg, const std::string& worker)
  {
    toolbus::ToolResult r;
    r.id = call.id;
    r.name = call.name;
    r.ok = false;
    r.error.code = code;
    r.error.message = msg;
    r.duration_ms = 0;
    r.caller = call.caller;
    r.worker = worker;
    r.trace_id = call.trace_id;
    r.ts_ms = now_ms();
    return r;
  }

  toolbus::ToolResult make_ok(const toolbus::ToolCall& call, nlohmann::json result, std::uint32_t dur_ms, const std::string& worker)
  {
    toolbus::ToolResult r;
    r.id = call.id;
    r.name = call.name;
    r.ok = true;
    r.result = std::move(result);
    r.duration_ms = dur_ms;
    r.caller = call.caller;
    r.worker = worker;
    r.trace_id = call.trace_id;
    r.ts_ms = now_ms();
    return r;
  }
}

// TCP worker model:
// - subscribes to orchestrator call bus host:port (single TCP publisher)
// - expects muxed frames with topic + ToolCall JSON payload
// - publishes results on a local TCP publisher (bind) for orchestrator to connect
int main(int argc, char** argv)
{
  const std::string service = (argc >= 2) ? argv[1] : "demo";
  const std::string worker_name = (argc >= 3) ? argv[2] : (std::string("worker-") + toolbus::make_call_id().substr(0, 8));
  const std::string call_host = (argc >= 4) ? argv[3] : "127.0.0.1";
  const std::uint16_t call_port = (argc >= 5) ? static_cast<std::uint16_t>(std::stoi(argv[4])) : 17000;
  const std::string result_bind = (argc >= 6) ? argv[5] : "0.0.0.0";
  const std::uint16_t result_port = (argc >= 7) ? static_cast<std::uint16_t>(std::stoi(argv[6])) : 17001;

  const auto exec = std::make_shared<tcp_pubsub::Executor>(4, tcp_pubsub::logger::logger_no_verbose_debug);

  toolbus_tcp::Publisher result_pub(exec, result_bind, result_port);
  if (!result_pub.isRunning())
  {
    std::cerr << "failed to start result publisher on " << result_bind << ":" << result_port << "\n";
    return 1;
  }

  std::unordered_set<std::string> allowlist{"demo.echo", "demo.add"};
  toolbus::DedupCache dedup(10U * 60U * 1000U);

  std::atomic<std::uint64_t> handled{0};
  std::atomic<std::uint64_t> rejected{0};

  const std::string call_topic = toolbus::call_topic(service);

  toolbus_tcp::Subscriber call_sub(exec, call_host, call_port);
  call_sub.on(
    call_topic,
    [&](const std::string& json_payload)
    {
      dedup.prune();

      toolbus::ToolCall call;
      try
      {
        call = toolbus::parse_tool_call(json_payload);
      }
      catch (const std::exception& e)
      {
        rejected.fetch_add(1);
        std::cerr << "[toolbus_tcp] invalid ToolCall: " << e.what() << "\n";
        return;
      }

      if (!call.target.empty() && call.target != worker_name)
        return;

      const std::string dedup_key = call.caller + ":" + call.id;
      std::string cached;
      if (dedup.get(dedup_key, cached))
      {
        result_pub.publish(toolbus::result_topic(call.caller), cached);
        handled.fetch_add(1);
        return;
      }

      const auto start = std::chrono::steady_clock::now();
      toolbus::ToolResult res;

      if (allowlist.find(call.name) == allowlist.end())
      {
        rejected.fetch_add(1);
        res = make_error(call, "forbidden", "tool not allowed", worker_name);
      }
      else if (call.name == "demo.echo")
      {
        if (!call.args.is_object() || !call.args.contains("text") || !call.args["text"].is_string())
        {
          rejected.fetch_add(1);
          res = make_error(call, "invalid_args", "expected args.text (string)", worker_name);
        }
        else
        {
          nlohmann::json out;
          out["text"] = call.args["text"];
          const auto dur = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
          res = make_ok(call, out, dur, worker_name);
        }
      }
      else if (call.name == "demo.add")
      {
        if (!call.args.is_object() || !call.args.contains("a") || !call.args.contains("b") ||
            !call.args["a"].is_number() || !call.args["b"].is_number())
        {
          rejected.fetch_add(1);
          res = make_error(call, "invalid_args", "expected args.a and args.b (numbers)", worker_name);
        }
        else
        {
          const double a = call.args["a"].get<double>();
          const double b = call.args["b"].get<double>();
          nlohmann::json out;
          out["sum"] = a + b;
          const auto dur = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
          res = make_ok(call, out, dur, worker_name);
        }
      }
      else
      {
        rejected.fetch_add(1);
        res = make_error(call, "not_implemented", "tool handler missing", worker_name);
      }

      const std::string res_json = toolbus::to_json_string(res);
      dedup.put(dedup_key, res_json);

      result_pub.publish(toolbus::result_topic(call.caller), res_json);
      handled.fetch_add(1);
    });

  std::cout << "[toolbus_tcp] worker started\n"
            << "  service: " << service << "\n"
            << "  worker:  " << worker_name << "\n"
            << "  call bus: " << call_host << ":" << call_port << " topic=" << call_topic << "\n"
            << "  result pub: " << result_bind << ":" << result_pub.port() << " (topic mux)\n";

  for (;;)
  {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "[toolbus_tcp] handled=" << handled.load() << " rejected=" << rejected.load() << "\n";
  }
}

