# rpi-gpio

## Introduction

This repository provides a QNX resource manager for GPIO (General Purpose Input/Output) control on Raspberry Pi 3, 4, and 5 devices. The resource manager exposes GPIO pins as device nodes under `/dev/gpio/`, allowing userspace applications to configure, read, write, and handle events on GPIO pins through standard file operations and QNX message passing.

## Supported Hardware

| Platform | Variant | GPIO Base Address |
|----------|---------|-------------------|
| Raspberry Pi 3 | 3B, 3B+, 3A+, CM3 | `0x3F000000` |
| Raspberry Pi 4 | 4B, 400, CM4, CM4S | `0xFE000000` |
| Raspberry Pi 5 | 5, 500, CM5, CM5 Lite | `0x1F000D0000` |

The resource manager auto-detects the platform using `uname()`. It can also be overridden with the `-r` and `-a` command-line options.

## Architecture

The resource manager is built on the QNX resource manager framework (`libresmgr`) and consists of the following components:

```
rpi-gpio/
├── resmgr/
│   ├── main.c            # Resource manager entry point, dispatch loop, node setup
│   ├── event.c           # GPIO interrupt handling and event delivery
│   ├── pwm.c             # Hardware and software PWM implementation
│   ├── rpi2711_gpio.c    # BCM2711 (RPi 3/4) GPIO register-level driver
│   ├── rp1_gpio.c        # RP1 (RPi 5) GPIO register-level driver
│   ├── rpi_gpio.h        # Internal API and platform abstraction (gpio_config_t)
│   ├── rpi_gpio_priv.h   # Private structures (PWM state, client info, base addresses)
│   ├── public/
│   │   └── sys/
│   │       └── rpi_gpio.h  # Public API header (installed to QNX target)
│   ├── CMakeLists.txt
│   └── rpi_gpio.use      # Usage message embedded in binary
├── qnx/build/
│   └── aarch64-qnx.cmake # Cross-compilation toolchain file for QNX aarch64
├── common.cmake           # Shared CMake utilities (Python detection, usemsg)
├── CMakeLists.txt
└── README.md
```

## Device Interface

### Mount Point

The resource manager mounts at `/dev/gpio` by default (configurable via `-m`).

### Per-Pin Nodes (`/dev/gpio/<N>`)

Each GPIO pin is exposed as a numbered node (0–27). The node supports standard `open()`, `read()`, and `write()` operations.

**Write commands** (text-based, sent via `write()`):

| Command | Description |
|---------|-------------|
| `in` | Set pin as input |
| `out` | Set pin as output |
| `on` | Drive pin high (output mode only) |
| `off` | Drive pin low (output mode only) |
| `pwm` | Trigger PWM debug output |

**Read behavior**: Reading a pin node returns `'1'` (high) or `'0'` (low). Only works when the pin is configured as input.

### Message Node (`/dev/gpio/msg`)

The `msg` node accepts structured `_IO_MSG` messages for advanced control. The message header `subtype` field selects the operation:

| Subtype | Name | Description |
|---------|------|-------------|
| 0 | `RPI_GPIO_SET_SELECT` | Set pin function (in/out/alt). Returns previous value. |
| 1 | `RPI_GPIO_GET_SELECT` | Get current pin function. |
| 2 | `RPI_GPIO_WRITE` | Set pin high (1) or low (0). Requires output mode. |
| 3 | `RPI_GPIO_READ` | Read pin level. Requires input mode. |
| 4 | `RPI_GPIO_ADD_EVENT` | Register async event delivery on pin state change. |
| 5 | `RPI_GPIO_PWM_SETUP` | Configure PWM (frequency, range, mode). |
| 6 | `RPI_GPIO_PWM_DUTY` | Set PWM duty cycle. |
| 7 | `RPI_GPIO_PUD` | Set pull-up/pull-down resistor (off/down/up). |

### Pin Function Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `RPI_GPIO_FUNC_IN` | 0 | Input |
| `RPI_GPIO_FUNC_OUT` | 1 | Output |
| `RPI_GPIO_FUNC_ALT_0` | 4 | Alternate function 0 |
| `RPI_GPIO_FUNC_ALT_5` | 2 | Alternate function 5 |

### Pull-up/down Constants

| Constant | Value |
|----------|-------|
| `RPI_GPIO_PUD_OFF` | 0 |
| `RPI_GPIO_PUD_DOWN` | 1 |
| `RPI_GPIO_PUD_UP` | 2 |

## Event Handling

The event system supports asynchronous notification on GPIO state changes:

- **Edge detection**: Rising edge (`RPI_EVENT_EDGE_RISING`), falling edge (`RPI_EVENT_EDGE_FALLING`)
- **Level detection**: High level (`RPI_EVENT_LEVEL_HIGH`), low level (`RPI_EVENT_LEVEL_LOW`)
- **Event delivery**: Via QNX `MsgDeliverEvent()` using `SIGEV_SIGNAL`, `SIGEV_THREAD`, or `SIGEV_PULSE`
- **Match count**: Events can be batched — only delivered after N occurrences

Events are serviced by a dedicated high-priority interrupt service thread (IST) attached to the platform's GPIO interrupt. Each GPIO can only have one registered event at a time.

## PWM Support

### Hardware PWM

- Available on GPIO 18 (RPi 3/4) via the BCM2711 PWM controller
- On RPi 5, hardware PWM is available on multiple pins via the RP1 chip
- Supports PWM and Mark-Space modes
- Configurable frequency and range (duty cycle resolution)

### Software PWM

- Available on all GPIO pins (up to 8 concurrent channels)
- Uses the system timer (1 MHz, 1 µs resolution) via interrupt 1
- Timer ISR handles pin toggling for precise timing
- Configurable frequency and duty cycle

## Command-Line Options

| Option | Description |
|--------|-------------|
| `-a <addr>` | Override GPIO base physical address |
| `-i <intr>` | Override interrupt number |
| `-m <path>` | Set mount point (default: `/dev/gpio`) |
| `-o <mode>` | Set directory permissions (default: 0755) |
| `-p <priority>` | Set IST priority (default: 200) |
| `-r <version>` | Force platform version (3, 4, or 5) |
| `-s <path>` | Set shared memory path |
| `-u <user>` | Run as specified user:group |
| `-v` | Increase verbosity (can be repeated) |

## Prerequisites

- QNX SDP 8.0 or later
- CMake 3.22.1 or later
- The QNX toolchain must be available in your environment

## Build Instructions

### 1. Source the QNX environment

Before building, you must source the QNX environment script to set up the required toolchain variables (`QNX_HOST`, `QNX_TARGET`, etc.):

```bash
source /opt/qnx800/qnxsdp-env.sh
```

> **Note:** Adjust the path to match your QNX SDP installation directory.

### 2. Clone the repository

```bash
git clone https://gitlab.com/qnx/projects/rpi-gpio.git
cd rpi-gpio
```

### 3. Remove unused directories

This repo originally includes `python` and `gpioctrl` directories which are not needed for the resource manager build:

```bash
rm -rf python gpioctrl
```

### 4. Edit CMakeLists.txt

Remove the following lines from the root `CMakeLists.txt` since those subdirectories have been removed:

```cmake
add_subdirectory(gpioctrl)
add_subdirectory(python)
```

### 5. Build

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=qnx/build/aarch64-qnx.cmake
cmake --build build/
```

The build output will be placed in the `build/resmgr/` directory.

## Output

The compiled binary `rpi_gpio` is the GPIO resource manager executable for Raspberry Pi.

### Example Usage

```bash
# Start the resource manager
rpi_gpio -v

# Configure GPIO 17 as output and turn it on
echo out > /dev/gpio/17
echo on > /dev/gpio/17

# Configure GPIO 27 as input and read its state
echo in > /dev/gpio/27
cat /dev/gpio/27
```

## Source Files Reference

| File | Lines | Description |
|------|-------|-------------|
| `main.c` | 740 | Resource manager init, dispatch loop, open/read/write/msg handlers |
| `event.c` | 311 | GPIO interrupt handling, event registration and delivery |
| `pwm.c` | 472 | Hardware PWM (BCM2711/RP1) and software PWM via system timer |
| `rpi2711_gpio.c` | 642 | BCM2711 GPIO register driver (RPi 3/4) |
| `rp1_gpio.c` | 923 | RP1 GPIO register driver (RPi 5) |
| `rpi_gpio.h` | 413 | Internal API with platform-abstracted inline functions |
| `public/sys/rpi_gpio.h` | 148 | Public API header (message types, constants, structs) |

## License

Copyright 2021–2025 QNX Software Systems. See the repository for full license information.
