#!/usr/bin/env python3
"""
rp1_addr.py — Convert Raspberry Pi 5 device-tree addresses into the
CPU-physical addresses that QNX buildfiles/startup scripts expect.

Background — there are TWO different address families on the Pi 5:

1. RP1 south-bridge peripherals (I2C, SPI, USB, ethernet PHY, GPIO
   expander, etc.) — anything that lives inside the RP1 chip, reached
   over PCIe. RP1-internal addresses start at 0x40000000. The CPU sees
   RP1 aliased so that 0x1F00000000 (CPU-physical) == 0x40000000
   (RP1-internal):
       cpu_physical = 0x1F00000000 + (rp1_internal_addr - 0x40000000)
   Example: SPI0 at RP1-internal 0x40050000 -> CPU-physical 0x1f00050000

2. BCM2712-native ("legacy VideoCore bus") peripherals — things that
   live directly on the SoC itself, not behind RP1 (e.g. the debug
   UART0). These addresses are in the classic Broadcom 0x7dxxxxxx /
   0x7exxxxxx range used since the original Pi, and map to CPU-physical
   with a flat offset:
       cpu_physical = legacy_addr + 0x1000000000
   Example: UART0 at legacy 0x7d001000 -> CPU-physical 0x107d001000

The script auto-detects which family an address belongs to based on
its range. Use --family to force one if you already know which applies.

Usage:
    # Auto-detect (RP1 vs legacy) from a full device-tree address
    python3 rp1_addr.py 0x40050000        # -> RP1 -> 0x1f00050000
    python3 rp1_addr.py 0x7d004000        # -> legacy -> 0x107d004000

    # Force a family explicitly
    python3 rp1_addr.py --family rp1 0x40050000
    python3 rp1_addr.py --family legacy 0x7d004000

    # Convert just an offset from the RP1 peripheral base (0x40000000)
    python3 rp1_addr.py --offset 0x50000
    -> 0x1f00050000

    # Reverse: given a QNX/CPU-physical address, get the source address back
    python3 rp1_addr.py --reverse 0x1f00050000    # -> RP1 -> 0x40050000
    python3 rp1_addr.py --reverse 0x107d004000    # -> legacy -> 0x7d004000
"""

import argparse

# --- RP1 (PCIe south-bridge peripherals) ---
RP1_INTERNAL_BASE = 0x40000000
RP1_INTERNAL_TOP = 0x40400000    # per community-observed usable range
RP1_CPU_PHYS_BASE = 0x1F00000000

# --- BCM2712-native / legacy VideoCore-bus peripherals ---
LEGACY_BASE = 0x7C000000         # loose lower bound covering 0x7d/0x7e ranges
LEGACY_TOP = 0x80000000
LEGACY_CPU_OFFSET = 0x1000000000


def rp1_to_cpu(rp1_addr: int) -> int:
    return RP1_CPU_PHYS_BASE + (rp1_addr - RP1_INTERNAL_BASE)


def cpu_to_rp1(cpu_addr: int) -> int:
    return RP1_INTERNAL_BASE + (cpu_addr - RP1_CPU_PHYS_BASE)


def legacy_to_cpu(legacy_addr: int) -> int:
    return legacy_addr + LEGACY_CPU_OFFSET


def cpu_to_legacy(cpu_addr: int) -> int:
    return cpu_addr - LEGACY_CPU_OFFSET


def detect_family(addr: int, reverse: bool) -> str:
    """Guess which address family `addr` belongs to."""
    if reverse:
        if RP1_CPU_PHYS_BASE <= addr < RP1_CPU_PHYS_BASE + 0x1000000:
            return "rp1"
        if addr >= LEGACY_CPU_OFFSET:
            return "legacy"
    else:
        if RP1_INTERNAL_BASE <= addr < RP1_INTERNAL_TOP:
            return "rp1"
        if LEGACY_BASE <= addr < LEGACY_TOP:
            return "legacy"
    return "unknown"


def parse_hex(value: str) -> int:
    return int(value, 16)


def main():
    parser = argparse.ArgumentParser(
        description="Convert Pi 5 device-tree addresses (RP1 or BCM2712-native) to QNX/CPU-physical addresses, or back."
    )
    parser.add_argument("address", type=str, help="Address in hex, e.g. 0x40050000")
    parser.add_argument(
        "--family",
        choices=["rp1", "legacy"],
        help="Force address family instead of auto-detecting (rp1 = behind RP1/PCIe, legacy = native BCM2712 VideoCore-bus).",
    )
    parser.add_argument(
        "--offset",
        action="store_true",
        help="Treat the address as an offset from the RP1 peripheral base (0x40000000). Implies --family rp1.",
    )
    parser.add_argument(
        "--reverse",
        action="store_true",
        help="Convert a QNX/CPU-physical address back to its source (RP1-internal or legacy) address.",
    )
    args = parser.parse_args()

    value = parse_hex(args.address)

    if args.offset:
        if args.reverse:
            parser.error("--offset and --reverse cannot be used together")
        rp1_addr = RP1_INTERNAL_BASE + value
        result = rp1_to_cpu(rp1_addr)
        print(f"RP1-internal 0x{rp1_addr:x}  ->  QNX/CPU-physical 0x{result:x}")
        return

    family = args.family or detect_family(value, args.reverse)

    if family == "unknown":
        parser.error(
            f"Could not auto-detect address family for 0x{value:x}. "
            f"Use --family rp1 or --family legacy to specify explicitly."
        )

    if args.reverse:
        if family == "rp1":
            result = cpu_to_rp1(value)
        else:
            result = cpu_to_legacy(value)
        print(f"QNX/CPU-physical 0x{value:x}  ->  {family} 0x{result:x}")
    else:
        if family == "rp1":
            result = rp1_to_cpu(value)
        else:
            result = legacy_to_cpu(value)
        print(f"{family} 0x{value:x}  ->  QNX/CPU-physical 0x{result:x}")


if __name__ == "__main__":
    main()