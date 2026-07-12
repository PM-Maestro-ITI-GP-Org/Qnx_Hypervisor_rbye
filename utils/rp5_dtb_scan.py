#!/usr/bin/env python3
"""
rp5_dtb_scan.py — Scan a Raspberry Pi 5 device-tree blob (.dtb) and list
every peripheral node with its QNX/CPU-physical address, computed the same
way we worked out by hand for SPI0 (see rp1_addr.py's docstring for the
background on why there are two address families on this board).

Requires `dtc` (device-tree-compiler) to decompile the .dtb to text .dts.
On Debian/Ubuntu:  sudo apt install device-tree-compiler

USAGE:
    python3 rp5_dtb_scan.py /path/to/bcm2712-rpi-5-b.dtb
    python3 rp5_dtb_scan.py --dts /path/to/already-decompiled.dts
    python3 rp5_dtb_scan.py my.dtb --only-enabled     # skip status=disabled nodes
    python3 rp5_dtb_scan.py my.dtb --filter spi        # only nodes whose name/compatible match "spi"

WHAT IT DOES:
    1. Decompiles the dtb with `dtc -I dtb -O dts`.
    2. Walks the tree, tracking #address-cells/#size-cells inheritance and
       node ancestry (so it knows which nodes live inside the RP1 subtree
       vs. directly on the main BCM2712 SoC).
    3. For each node with a `reg` property:
         - If it's nested under an RP1 node -> RP1 formula:
               cpu_physical = 0x1F00000000 + (rp1_offset - 0x40000000)
         - Elif its raw address falls in the legacy VideoCore-bus range
           (0x7c000000-0x80000000) -> legacy formula:
               cpu_physical = raw_addr + 0x1000000000
         - Else -> address family unknown, flagged for manual check.
    4. Prints a table: node path, compatible string, status, and the
       computed QNX address (or a warning if it can't be classified).

CAVEATS (read before trusting the output blindly):
    - This is a heuristic scan, not a verified source of truth. Always
      cross-check against a *known-working* address before using a new
      one in a QNX buildfile/qvmconf.
    - Legacy-looking nodes (compatible = "brcm,bcm2835-*" etc.) are
      usually NOT what QNX's modern drivers (spi-dwc, etc.) actually
      talk to on Pi 5 -- see the RP1 nodes instead. This script flags
      family=legacy vs family=rp1 explicitly so you can tell them apart
      and pick the one matching your driver's `compatible` expectations.
    - Some `reg` values use more address cells than 2 (e.g. PCI-style
      reg with a phys.hi/phys.mid/phys.lo triple) -- this script uses a
      simple heuristic (last non-zero 32-bit cell) that works for the
      SPI/I2C/UART-style nodes shown in QNX BSP examples, but may need
      adjustment for more exotic nodes.
"""

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass, field

RP1_INTERNAL_BASE = 0x40000000
RP1_INTERNAL_TOP = 0x40400000
RP1_CPU_PHYS_BASE = 0x1F00000000

LEGACY_BASE = 0x7C000000
LEGACY_TOP = 0x80000000
LEGACY_CPU_OFFSET = 0x1000000000


def dtb_to_dts(dtb_path: str) -> str:
    try:
        result = subprocess.run(
            ["dtc", "-I", "dtb", "-O", "dts", dtb_path],
            capture_output=True, text=True, check=True,
        )
    except FileNotFoundError:
        sys.exit("Error: `dtc` not found. Install it with: sudo apt install device-tree-compiler")
    except subprocess.CalledProcessError as e:
        sys.exit(f"Error: dtc failed to decompile {dtb_path}:\n{e.stderr}")
    return result.stdout


@dataclass
class Node:
    name: str
    path: str
    parent: "Node" = None
    address_cells: int = 2
    size_cells: int = 1
    compatible: str = ""
    status: str = ""
    reg: list = field(default_factory=list)   # list of ints (raw cell values)
    children: list = field(default_factory=list)

    def is_under_rp1(self) -> bool:
        n = self
        while n is not None:
            if "rp1" in n.name.lower() or "rp1" in n.compatible.lower():
                return True
            n = n.parent
        return False


TOKEN_RE = re.compile(r'([A-Za-z0-9_,.\-@+]+)\s*\{|\};|([A-Za-z0-9_,.\-]+)\s*=\s*(.*?);', re.DOTALL)
NODE_OPEN_RE = re.compile(r'^([A-Za-z0-9_,.\-@+]*)\s*\{$')
PROP_RE = re.compile(r'^([A-Za-z0-9_,.\-\+#]+)\s*=\s*(.*);$')
CELLS_RE = re.compile(r'<([^>]*)>')


def parse_cell_list(raw: str):
    """Extract all 32-bit hex/dec cells from a `<...>` property value, across
    possibly multiple <...> groups concatenated together."""
    cells = []
    for group in CELLS_RE.findall(raw):
        for tok in group.split():
            tok = tok.strip()
            if not tok:
                continue
            try:
                cells.append(int(tok, 16) if tok.lower().startswith("0x") else int(tok, 10))
            except ValueError:
                pass
    return cells


def parse_dts(text: str) -> Node:
    # Strip comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*', '', text)

    lines = text.splitlines()
    root = Node(name="/", path="/")
    stack = [root]
    current_props_buffer = ""

    for raw_line in lines:
        line = raw_line.strip()
        if not line:
            continue
        # Skip top-level /dts-v1/; and memreserve etc.
        if line.startswith("/dts-v1/") or line.startswith("/memreserve/"):
            continue

        current_props_buffer += " " + line
        # Only process once we have a balanced statement ending in `;` or `{` or `};`
        if not (current_props_buffer.rstrip().endswith(("{", ";"))):
            continue
        stmt = current_props_buffer.strip()
        current_props_buffer = ""

        if stmt == "};":
            if len(stack) > 1:
                stack.pop()
            continue

        m = NODE_OPEN_RE.match(stmt)
        if m:
            raw_name = m.group(1)
            if raw_name in ("/", ""):
                # root node redeclared, or anonymous - treat as continuing root/current
                if raw_name == "/":
                    stack.append(root)
                    continue
                raw_name = raw_name or f"node{len(stack)}"
            parent = stack[-1]
            node = Node(
                name=raw_name,
                path=parent.path.rstrip("/") + "/" + raw_name,
                parent=parent,
                address_cells=parent.address_cells,
                size_cells=parent.size_cells,
            )
            parent.children.append(node)
            stack.append(node)
            continue

        pm = PROP_RE.match(stmt)
        if pm:
            prop_name, prop_val = pm.group(1), pm.group(2)
            node = stack[-1]
            if prop_name == "#address-cells":
                cells = parse_cell_list(prop_val)
                if cells:
                    node.address_cells = cells[0]
            elif prop_name == "#size-cells":
                cells = parse_cell_list(prop_val)
                if cells:
                    node.size_cells = cells[0]
            elif prop_name == "compatible":
                node.compatible = prop_val.replace('"', "").replace("\\0", " ").strip()
            elif prop_name == "status":
                node.status = prop_val.replace('"', "").strip()
            elif prop_name == "reg":
                node.reg = parse_cell_list(prop_val)
            continue
        # else: property with no '=' (rare/boolean prop, e.g. `spi-slave;`) -> ignore

    return root


def classify_and_convert(node: Node):
    """Given a node's reg cells (using the PARENT's #address-cells to know
    how many cells form one address), return (family, source_addr, cpu_addr)
    or (None, None, None) if it can't figure it out."""
    if not node.reg or node.parent is None:
        return None, None, None

    addr_cells = node.parent.address_cells
    if addr_cells <= 0 or addr_cells > len(node.reg):
        addr_cells = min(2, len(node.reg))

    addr_group = node.reg[:addr_cells]
    # Combine cells big-endian into one integer, then take low 32 bits as
    # the "real" address for classification/translation purposes (matches
    # what we found by hand: high cell like 0xc0 is a bus tag, not part of
    # the flat physical offset used by RP1/legacy translation).
    low32 = addr_group[-1] if addr_group else None
    if low32 is None:
        return None, None, None

    if node.is_under_rp1() and RP1_INTERNAL_BASE <= low32 < RP1_INTERNAL_TOP:
        cpu_addr = RP1_CPU_PHYS_BASE + (low32 - RP1_INTERNAL_BASE)
        return "rp1", low32, cpu_addr
    if LEGACY_BASE <= low32 < LEGACY_TOP:
        cpu_addr = low32 + LEGACY_CPU_OFFSET
        return "legacy", low32, cpu_addr
    if node.is_under_rp1():
        # Under RP1 but address outside the known usable range - still show it,
        # flagged, since the range boundary (0x40400000) is community-observed,
        # not an official hard limit.
        cpu_addr = RP1_CPU_PHYS_BASE + (low32 - RP1_INTERNAL_BASE)
        return "rp1?", low32, cpu_addr
    return "unknown", low32, None


def walk(node: Node, results: list):
    if node.reg and node.compatible:
        results.append(node)
    for c in node.children:
        walk(c, results)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", help="Path to .dtb (or .dts with --dts)")
    parser.add_argument("--dts", action="store_true", help="Input is already a decompiled .dts text file")
    parser.add_argument("--only-enabled", action="store_true", help="Skip nodes with status = \"disabled\"")
    parser.add_argument("--filter", type=str, default=None, help="Only show nodes whose name or compatible contains this substring")
    args = parser.parse_args()

    if args.dts:
        with open(args.input) as f:
            text = f.read()
    else:
        text = dtb_to_dts(args.input)

    root = parse_dts(text)
    results = []
    walk(root, results)

    rows = []
    for node in results:
        if args.only_enabled and node.status == "disabled":
            continue
        if args.filter:
            hay = (node.name + " " + node.compatible).lower()
            if args.filter.lower() not in hay:
                continue
        family, src_addr, cpu_addr = classify_and_convert(node)
        rows.append((node.path, node.compatible, node.status or "okay", family, src_addr, cpu_addr))

    if not rows:
        print("No matching peripheral nodes found. Try without --filter/--only-enabled, "
              "or check the .dts output manually — dts parsing heuristics can miss unusual formatting.")
        return

    name_w = max(len(r[0]) for r in rows) + 2
    compat_w = max(len(r[1]) for r in rows) + 2
    print(f"{'Node':<{name_w}}{'Compatible':<{compat_w}}{'Status':<11}{'Family':<9}{'Source addr':<14}{'QNX/CPU addr'}")
    print("-" * (name_w + compat_w + 11 + 9 + 14 + 16))
    for path, compat, status, family, src, cpu in rows:
        src_s = f"0x{src:x}" if src is not None else "-"
        cpu_s = f"0x{cpu:x}" if cpu is not None else "?? (unclassified — verify manually)"
        fam_s = family or "unknown"
        print(f"{path:<{name_w}}{compat:<{compat_w}}{status:<11}{fam_s:<9}{src_s:<14}{cpu_s}")

    print()
    print("Legend: family=rp1     -> behind RP1/PCIe, formula 0x1F00000000 + (addr - 0x40000000)")
    print("        family=rp1?    -> under RP1 but outside the usual 0x40000000-0x40400000 range, double-check")
    print("        family=legacy  -> native BCM2712 block, formula addr + 0x1000000000")
    print("        family=unknown -> couldn't classify; verify by hand before using")
    print()
    print("Reminder: prefer nodes whose `compatible` matches the QNX driver you intend to use")
    print("(e.g. snps,dw-apb-ssi <-> spi-dwc). A legacy brcm,bcm2835-* node with the same peripheral")
    print("name is usually NOT what QNX's driver talks to on Pi 5.")


if __name__ == "__main__":
    main()