#include <shm_pubsub/shm/publisher.h>
#include <shm_pubsub/shm/subscriber.h>

#include <toolbus/dedup_cache.h>
#include <toolbus/id.h>
#include <toolbus/message.h>
#include <toolbus/topics.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
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

// A minimal shm tool worker:
// - subscribes to tool.call.<service>
// - allowlist by tool name
// - JSON args validation (best-effort)
// - at-least-once with dedup cache (by call_id)
// - publishes ToolResult to tool.result.<caller>
int main(int argc, char** argv)
{
  const std::string service = (argc >= 2) ? argv[1] : "demo";
  const std::string worker_name = (argc >= 3) ? argv[2] : (std::string("worker-") + toolbus::make_call_id().substr(0, 8));

  const std::string call_topic = toolbus::call_topic(service);

  std::unordered_set<std::string> allowlist{
    "demo.echo",
    "demo.add"
  };

  toolbus::DedupCache dedup(10U * 60U * 1000U);

  std::mutex publish_mutex;
  std::unordered_map<std::string, shm_pubsub::shm::Publisher> result_publishers;

  auto publish_result = [&](const std::string& caller, const std::string& res_json)
  {
    const std::string topic = toolbus::result_topic(caller);

    const std::lock_guard<std::mutex> lock(publish_mutex);
    auto it = result_publishers.find(topic);
    if (it == result_publishers.end())
    {
      auto ins = result_publishers.emplace(topic, shm_pubsub::shm::Publisher(topic, 4U * 1024U * 1024U));
      it = ins.first;
    }
    if (!it->second.isRunning())
    {
      std::cerr << "[toolbus] failed to publish result (topic not running): " << topic << std::endl;
      return;
    }
    it->second.send(res_json.data(), res_json.size());
  };

  shm_pubsub::shm::Subscriber call_subscriber(call_topic);
  if (!call_subscriber.isRunning())
  {
    // subscriber doesn't open shm until publisher exists; that's fine.
  }

  const int max_in_flight = 4;
  std::atomic<int> in_flight{0};

  std::atomic<std::uint64_t> handled{0};
  std::atomic<std::uint64_t> rejected{0};

  call_subscriber.setCallback(
    [&](const shm_pubsub::CallbackData& data)
    {
      if (!data.buffer_ || data.buffer_->empty())
        return;

      dedup.prune();

      const std::string payload(data.buffer_->data(), data.buffer_->size());

      toolbus::ToolCall call;
      try
      {
        call = toolbus::parse_tool_call(payload);
      }
      catch (const std::exception& e)
      {
        rejected.fetch_add(1);
        std::cerr << "[toolbus] invalid ToolCall JSON: " << e.what() << "\n";
        return;
      }

      std::cout << "[toolbus] call id=" << call.id << " name=" << call.name << " caller=" << call.caller
                << (call.target.empty() ? "" : (" target=" + call.target))
                << std::endl;

      // Optional target routing (empty = any)
      if (!call.target.empty() && call.target != worker_name)
        return;

      // Simple concurrency limiter (best-effort): reject when too busy.
      const int cur = in_flight.fetch_add(1) + 1;
      if (cur > max_in_flight)
      {
        in_flight.fetch_sub(1);
        rejected.fetch_add(1);
        const auto res = make_error(call, "busy", "worker concurrency limit reached", worker_name);
        const std::string res_json = toolbus::to_json_string(res);
        publish_result(call.caller, res_json);
        return;
      }

      const std::string dedup_key = call.caller + ":" + call.id;
      std::string cached_result;
      if (dedup.get(dedup_key, cached_result))
      {
        std::cout << "[toolbus] dedup hit id=" << call.id << std::endl;
        publish_result(call.caller, cached_result);
        handled.fetch_add(1);
        in_flight.fetch_sub(1);
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

      publish_result(call.caller, res_json);
      handled.fetch_add(1);
      in_flight.fetch_sub(1);
    },
    true /* synchronous */
  );

  call_subscriber.start(1);

  std::cout << "[toolbus] shm worker started" << std::endl
            << "  service: " << service << std::endl
            << "  worker:  " << worker_name << std::endl
            << "  call topic: " << call_topic << std::endl
            << "  result topic: tool.result.<caller>" << std::endl;

  for (;;)
  {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "[toolbus] handled=" << handled.load() << " rejected=" << rejected.load() << std::endl;
  }
}
