# motor_ai_server

CommonAPI / SOME/IP server that receives motor data batches from the QNX-side
client, logs them to CSV, runs AI inference (anomaly detection, fault
classification, predictive maintenance), and returns results to the client.

## Pipeline

```
motor_ai_client ──SOME/IP──→ motor_ai_server
                                   │
                                   ├── /tmp/motor_data_<timestamp>.csv
                                   ├── runAnomalyDetection()
                                   ├── runFaultClassification()
                                   └── runPredictiveMaintenance()
                                   │
                                   └── reply ──→ client → result SHM
```

## Dependencies

- CommonAPI, CommonAPI-SomeIP, vsomeip3 libraries compiled for the target
  (currently cross-compiled via `../someip/commonapi-qnx/build-rpi/`)
- CMake ≥ 3.13, C++14 compiler

## Build

```bash
make -C src/motor_ai_server
```

Or manually:

```bash
source ../someip/commonapi-qnx/scripts/env.sh
cd server
cmake -B build -S .
cmake --build build
```

Set `FORCE_REBUILD=1` to force a rebuild.

## Run (on target — Guest 2)

```bash
# Start server:
sh start_guest2.sh

# Or manually:
VSOMEIP_CONFIGURATION=/motor_ai_server/vsomeip_multicast.json \
    /motor_ai_server/MotorDataService
```

The binary:
- Registers the `MotorDataService` on `commonapi.MotorDataService` instance
- Listens for `sendBatch` calls from the client
- Logs every batch to `/tmp/motor_data_<timestamp>.csv`
- Runs AI model pipeline and replies with results
- Requires `vsomeip_multicast.json` (SD) in CWD

## AI Models (placeholders)

| Model | Function | Default return |
|---|---|---|
| Anomaly Detection | `runAnomalyDetection()` | `"normal"` |
| Fault Classification | `runFaultClassification()` | `"none"` |
| Predictive Maintenance | `runPredictiveMaintenance()` | `"RUL: N/A"` |

**Replace the bodies of these three functions** with actual model inference calls
(e.g. ONNX Runtime, TensorFlow Lite, custom C++ model).

When anomaly is detected (return != `"normal"`), the server also runs fault
classification and predictive maintenance before replying.

## Configuration

| Variable | Default | Description |
|---|---|---|
| `WINDOW_SIZE` | 200 | Expected rows per batch (must match client) |
| Connection name | `motor-ai-service` | SOME/IP application name |
| CSV path | `/tmp/motor_data_<timestamp>.csv` | Auto-generated on startup |

### vsomeip

- **Static routing**: `vsomeip.json` — no service discovery
- **SD enabled**: `vsomeip_multicast.json` or `vsomeip_server_mc.json`

Set `VSOMEIP_CONFIGURATION` env var to point to the config (default: `vsomeip_multicast.json` in CWD).

## Network

| Side | IP | App ID |
|---|---|---|
| Server (Guest 2) | `10.0.2.2` | `0x1280` |
| Client (Guest 1) | `10.0.2.1` | `0x1344` |

Service ID: `0x1240`, Instance ID: `0x5680`, Reliable port: `30501`.
