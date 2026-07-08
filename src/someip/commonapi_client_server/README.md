# CommonAPI Client/Server Apps

HelloWorld server and client applications using CommonAPI + SOME/IP over
vsomeip, cross-compiled for QNX.

## Directory layout

```
commonapi_client_server/
├── interface/
│   ├── HelloWorld.fidl            # Interface definition (sayHello method)
│   ├── HelloWorld.fdepl           # SOME/IP deployment (service/instance IDs)
│   └── commonapi4someip.ini       # CommonAPI binding config (binding=someip)
│
├── server/
│   ├── src/
│   │   ├── HelloWorldService.cpp       # Server main — registers service
│   │   ├── HelloWorldStubImpl.cpp      # Stub implementation (returns "Hello <name>!")
│   │   └── HelloWorldStubImpl.hpp
│   ├── CMakeLists.txt                  # Runs generators + builds server
│   └── vsomeip.json                    # vsomeip config (service side)
│
└── client/
    ├── src/
    │   └── HelloWorldClient.cpp        # Client main — calls sayHello in a loop
    ├── CMakeLists.txt                  # Runs generators + builds client
    └── vsomeip.json                    # vsomeip config (client side)
```

## Prerequisites

The libraries and toolchain must be built first in `commonapi-qnx/`:

```bash
cd ../commonapi-qnx
bash scripts/download.sh
bash scripts/build.sh
```

This produces `commonapi-qnx/build-rpi/` with:
- `lib/*.so` — all shared libraries
- `toolchain.cmake` — QNX cross-compile toolchain
- `generators/` — unzipped CommonAPI code generators

## Building

Source the QNX environment (sets `qcc`/`q++` on PATH + exports `OUTPUT_DIR`),
then build each app from its own directory:

```bash
# From commonapi-qnx/
source scripts/env.sh

# Build the server
cd ../commonapi_client_server/server
cmake -B build -S .
cmake --build build

# Build the client
cd ../client
cmake -B build -S .
cmake --build build
```

Each CMakeLists.txt automatically:
1. Finds the generators in `build-rpi/generators/`
2. Runs the core generator on `HelloWorld.fidl` → `build/generated/core/`
3. Runs the SOME/IP generator on `HelloWorld.fdepl` → `build/generated/someip/`
4. Compiles the generated code + app source against the installed libraries
5. Produces the executable

## Build output

| Path                          | Contents                            |
|-------------------------------|-------------------------------------|
| `server/build/HelloWorldService` | Server executable (QNX aarch64le) |
| `server/build/generated/`       | Generated C++ code (server side)  |
| `client/build/HelloWorldClient`  | Client executable (QNX aarch64le) |
| `client/build/generated/`        | Generated C++ code (client side)   |

## Files to copy to the QNX target

```
commonapi-qnx/build-rpi/lib/*.so              (13 libraries)
commonapi_client_server/server/build/HelloWorldService
commonapi_client_server/client/build/HelloWorldClient
commonapi_client_server/server/vsomeip.json   → vsomeip-service.json
commonapi_client_server/client/vsomeip.json   → vsomeip-client.json
commonapi_client_server/interface/commonapi4someip.ini
```

## Running on the QNX target

```sh
export LD_LIBRARY_PATH=/path/to/libs:$LD_LIBRARY_PATH

# Terminal 1 — start the service
./HelloWorldService

# Terminal 2 — start the client
./HelloWorldClient
```

Expected output:

**Server:**
```
Successfully Registered Service!
Waiting for calls... (Abort with CTRL+C)
sayHello('World'): 'Hello World!'
```

**Client:**
```
Checking availability!
Available...
Got message: 'Hello World!'
Got message: 'Hello World!'
```

## How it works

```
HelloWorld.fidl ──core-generator──► HelloWorldProxy.hpp / HelloWorldStubDefault.hpp
                        │                        │
HelloWorld.fdepl ──someip-generator──► HelloWorldSomeIPProxy.cpp / StubAdapter.cpp
                                                 │
                         ┌───────────────────────┘
                         ▼
                    server/  +  client/
                         │
                    links against
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
    libCommonAPI   libCommonAPI-SomeIP  libvsomeip3
```

The service registers on `local:commonapi.HelloWorld` (instance ID `0x5678`,
service ID `0x1234`).  The client builds a proxy to the same instance and
calls `sayHello("World")` every second.
