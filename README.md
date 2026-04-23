# pubsub: TCP/SHM Publish/Subscribe library

`pubsub` contains:

- `tcp_pubsub`: a minimal publish/subscribe library that transports data via TCP
- `shm_pubsub`: a minimal publish/subscribe library that transports data via local shared memory

The project is CMake based. Dependencies can be provided either via git submodules or via CMake `FetchContent` (fallback).

tcp_pubsub does not define a message format but only transports binary blobs. It does however define a protocol around that, which is kept as lightweight as possible.

## Dependencies

- [asio](https://github.com/chriskohlhoff/asio.git)
- [recycle](https://github.com/steinwurf/recycle.git)

## APIs

- TCP publisher/subscriber:
  - Publisher API: `#include <tcp_pubsub/publisher.h>`
  - Subscriber API: `#include <tcp_pubsub/subscriber.h>`
- SHM publisher/subscriber:
  - Publisher API: `#include <shm_pubsub/shm/publisher.h>`
  - Subscriber API: `#include <shm_pubsub/shm/subscriber.h>`

## Hello World Example

Similar examples are also provided in the repository under `samples/`.

### Publisher

```cpp
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <tcp_pubsub/executor.h>
#include <tcp_pubsub/publisher.h>

int main()
{
  // Create a "Hello World" buffer
  std::string data_to_send = "Hello World";
  
  // Create an Executor with a thread-pool size of 6. If you create multiple
  // publishers and subscribers, they all should share the same Executor.
  std::shared_ptr<tcp_pubsub::Executor> executor = std::make_shared<tcp_pubsub::Executor>(6);
  
  // Create a publisher that will offer the data on port 1588
  tcp_pubsub::Publisher hello_world_publisher(executor, 1588);

  for (;;)
  {
    // Send the "Hello World" string by passing the pointer to the first
    // character and the length.
    hello_world_publisher.send(data_to_send.data(), data_to_send.size());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  return 0;
}
```

### Subscriber

```cpp
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <tcp_pubsub/executor.h>
#include <tcp_pubsub/subscriber.h>

int main()
{
  // Create an Executor with a thread-pool size of 6. If you create multiple
  // publishers and subscribers, they all should share the same Executor.
  std::shared_ptr<tcp_pubsub::Executor> executor = std::make_shared<tcp_pubsub::Executor>(6);

  // Create a subscriber
  tcp_pubsub::Subscriber hello_world_subscriber(executor);
  
  // Add a session to the subscriber that connects to port 1588 on localhost. A 
  // subscriber will aggregate traffic from multiple source, if you add multiple
  // sessions.
  hello_world_subscriber.addSession("127.0.0.1", 1588);

  // Create a Callback that will be called each time a data packet is received.
  // This function will create an std::string from the packet and print it to
  // the console.
  std::function<void(const tcp_pubsub::CallbackData& callback_data)> callback_function
        = [](const tcp_pubsub::CallbackData& callback_data) -> void
          {
            std::cout << "Received payload: "
                      << std::string(callback_data.buffer_->data(), callback_data.buffer_->size())
                      << std::endl;
          };

  // Set the callback to the subscriber
  hello_world_subscriber.setCallback(callback_function);
    
  // Prevent the application from exiting immediately
  for (;;) std::this_thread::sleep_for(std::chrono::milliseconds(500));
  return 0;
}
```

## CMake Options

You can set the following CMake options to control how the project is built:

| Option                             | Type  | Default | Explanation                                                                                                                                         |
|------------------------------------|-------|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| `PUBSUB_BUILD_TCP_PUBSUB`          | `BOOL`| `ON`    | Build the `tcp_pubsub` library and TCP samples/tests.                                                                                                |
| `PUBSUB_BUILD_SHM_PUBSUB`          | `BOOL`| `ON`    | Build the `shm_pubsub` library.                                                                                                                     |
| `SHM_PUBSUB_BUILD_SAMPLES`         | `BOOL`| `ON`    | Build `shm_pubsub` samples (requires `PUBSUB_BUILD_SHM_PUBSUB=ON`).                                                                                 |
| `TCP_PUBSUB_BUILD_SAMPLES`         | `BOOL`| `ON`    | Build project samples.                                                                                                                              |
| `TCP_PUBSUB_USE_BUILTIN_ASIO`      | `BOOL`| `ON`    | Use the builtin asio submodule. If set to `OFF`, asio must be available from somewhere else (e.g. system libs).                                     |
| `TCP_PUBSUB_USE_BUILTIN_RECYCLE`   | `BOOL`| `ON`    | Use the builtin `steinwurf::recycle` submodule. If set to `OFF`, recycle must be available from somewhere else (e.g. system libs).                  |
| `TCP_PUBSUB_BUILD_TESTS`           | `BOOL`| `OFF`   | Build the tcp_pubsub tests. Requires Gtest::GTest to be findable by CMake. |
| `TCP_PUBSUB_USE_BUILTIN_GTEST`     | `BOOL`| `ON` (if building tests) | Use the builtin GoogleTest submodule. Only needed if `TCP_PUBSUB_BUILD_TESTS` is `ON`. If set to `OFF`, GoogleTest must be available from elsewhere. |
| `TCP_PUBSUB_LIBRARY_TYPE`          | `STRING` |             | Controls the library type of tcp_pubsub by injecting the string into the `add_library` call. Can be set to STATIC / SHARED / OBJECT. If set, this will override the regular `BUILD_SHARED_LIBS` CMake option. If not set, CMake will use the default setting, which is controlled by `BUILD_SHARED_LIBS`.                |

## shm_pubsub 原理与优化笔记

- `shm_pubsub` 实现原理、时序图与丢帧/背压设计：`docs/shm_pubsub.md`

## Function calling + pubsub（设计草案）

- 如何把结构化 “tool call / tool result” 跑在 pubsub 上（含需求/场景/协议/安全/演进）：`docs/function_calling.md`

## How to checkout and build

There are several examples provided that aim to show you the functionality.

1. Install cmake and git / git-for-windows

2. Checkout this repo and (optionally) initialize submodules

   ```console
   git clone https://github.com/ccbunao/pubsub.git
   cd pubsub
   git submodule init
   git submodule update
   ```

3. Configure + build

   ```console
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```

4. Run samples (examples)

   - TCP: `hello_world_publisher` + `hello_world_subscriber`
   - TCP performance: `performance_publisher` + `performance_subscriber`
   - SHM: `hello_world_shm_publisher` + `hello_world_shm_subscriber`
   - SHM performance: `performance_shm_publisher` + `performance_shm_subscriber`

## The Protocol (Version 0)

When using this library, you do not need to know how the protocol works. Both the subscriber and receiver are completely implemented and ready for you to use. This section is meant for advanced users that are interested in the underlying protocol.

<details>
<summary>Show</summary>

### Message flow

The Protocol is quite simple:

1. The **Subscriber** connects to the publisher and sends a ProtocolHandshakeRequest. This message contains the maximum protocol version the Subscriber supports.

2. The **Publisher** returns a ProtocolHandshakeResponse. This message contains the protocol version that will be used from now on. The version must not be higher than the version sent by the Subscriber.

3. The **Publisher** starts sending data to the Subscriber.

_The ProtocolHandshake is meant to provide future-proof expansions. At the moment the only available protocol version is 0._

```
Subscriber                     Publisher
   |                               |
   |  -> ProtocolHandshakeReq  ->  |
   |                               |
   |  <- ProtocolHandshakeResp <-  |
   |                               |
   |  <--------- DATA <----------  |
   |  <--------- DATA <----------  |
   |  <--------- DATA <----------  |
   |              ...              |
```

### Message layout

The protocol uses the following message layout. Values that are not sent by the sender are to be interpreted as 0.

- **General Message header**
	Each message will have a message header as follows. Values are to be interpreted little-endian.
	This header is defined in [tcp_pubsub/src/tcp_header.h](tcp_pubsub/src/tcp_header.h)

	- 16 bit: Header size
	- 8 bit: Type
		- 0 = Regular Payload
		- 1 = Handshake Message
	- 8 bit: Reserved
		- Must be 0
	- 64bit: Payload size

2. **ProtocolHandshakeReq & ProtocolHandshakeResp**
	The layout of ProtocolHandshakeReq / ProtocolHandshakeResp is the same.  Values are to be interpreted little-endian
	This message is defined in [tcp_pubsub/src/protocol_handshake_message.h](tcp_pubsub/src/protocol_handshake_message.h)
	
	- Message Header (size given in the first 16 bit)
	- 8 bit: Protocol Version

</details>
