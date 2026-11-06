"""Verify the seven ARTIST ICE menu records and the targeted XDK PPC bodies."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
CHECKOUT = Path("C:/Users/adri1/.codex/worktrees/bp360/native_props")
BASE = "43044d5e77737e18572b7848676c6691720d5b95"
PRESERVED = "bc93e214d327c5e1670d7dce1f7f8c69d91c55db"
FILES = {
    "widget": "src/SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.cpp",
    "controller": "src/SDKs/Packages/ICE/ICEControllerMenus.cpp",
}
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import x360rd


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(CHECKOUT), *args], text=True).strip()


class Coff:
    def __init__(self, path):
        self.data = path.read_bytes()
        machine, _, _, table, count, optional, _ = struct.unpack_from("<HHIIIHH", self.data)
        assert machine == 0x1F2 and optional == 0, path
        strings = table + 18 * count
        self.symbols = {}
        index = 0
        while index < count:
            name, value, section, kind, _, aux = struct.unpack_from("<8sIhHBB", self.data, table + 18 * index)
            if name[:4] == bytes(4):
                start = strings + struct.unpack_from("<I", name, 4)[0]
                name = self.data[start:self.data.index(b"\0", start)]
            self.symbols[index] = name.rstrip(b"\0").decode("ascii"), value, section, kind
            index += 1 + aux

    def symbol(self, prefix):
        [row] = [row for row in self.symbols.values() if row[0].startswith(prefix) and row[2] > 0]
        return row

    def section(self, index):
        header = 20 + 40 * (index - 1)
        size, offset, relocations = struct.unpack_from("<III", self.data, header + 16)
        count = struct.unpack_from("<H", self.data, header + 32)[0]
        return self.data[header:header + 8].rstrip(b"\0"), size, offset, relocations, count

    def literal(self, prefix, size):
        name, value, index, kind = self.symbol(prefix)
        section, limit, offset, _, _ = self.section(index)
        assert kind == 0 and section == b".rdata" and value + size <= limit, name
        return self.data[offset + value:offset + value + size]

    def body(self, prefix):
        name, value, index, kind = self.symbol(prefix)
        section, size, offset, relocations, count = self.section(index)
        assert value == 0 and kind == 0x20 and section == b".text", name
        refs = {}
        for i in range(count):
            at, symbol, kind = struct.unpack_from("<IIH", self.data, relocations + 10 * i)
            if kind == 0x12:  # PPC split-address PAIR
                assert symbol == 0
                continue
            refs[at] = self.symbols[symbol][0], kind
        return self.data[offset:offset + size], refs


def words(body, offset, *expected):
    assert struct.unpack_from(f">{len(expected)}I", body, offset) == expected, hex(offset)


def relocation(refs, offset, prefix, kind):
    name, actual_kind = refs[offset]
    assert name.startswith(prefix) and actual_kind == kind, (hex(offset), refs[offset])


def trace(body, *offsets):
    return {hex(offset): f"{struct.unpack_from('>I', body, offset)[0]:08x}" for offset in offsets}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-head", required=True)
    args = parser.parse_args()
    assert git("rev-parse", "HEAD") == args.expected_head
    assert git("rev-parse", "HEAD^") == BASE and not git("status", "--porcelain")
    assert git("rev-parse", "codex/experimental-x360-r153-presentation-stream") == PRESERVED
    assert set(git("diff", "--name-only", BASE, "HEAD").splitlines()) == set(FILES.values())
    report_path = ROOT / "native" / "compile-results.json"
    report = json.loads(report_path.read_bytes())
    rows = {row["source"]: row for row in report["results"]}
    assert report["passed"] == 2 and report["failed"] == 0 and set(rows) == set(FILES.values())
    objects = {}
    for key, source in FILES.items():
        row = rows[source]
        assert row["passed"] and row["machine"] == 0x1F2
        assert f"/I{CHECKOUT / 'src'}" in row["command"]
        objects[key] = Coff(ROOT / "native" / Path(row["object"]).name)
    widget, controller = objects["widget"], objects["controller"]

    assert x360rd.u32(0x82815570) == 0x7C8A2378 and x360rd.u32(0x82F264A8) == 4
    style_data = widget.literal("kaMenuStyles", 7 * 0x20)
    assert style_data == x360rd.rd(0x8207ABB8, len(style_data))
    assert widget.literal("KF_ICE_MENU_MARGIN", 4) == x360rd.rd(0x82004A20, 4)
    styles = []
    for index, (rows, visible, columns, selectable, *widths) in enumerate(struct.iter_unpack(">iiiB3x4f", style_data)):
        styles.append({
            "index": index, "rows": rows, "visible": visible, "columns": columns,
            "initial_selection": 0 if selectable else -1, "column_widths": widths,
            "width_with_margins": sum(widths[:columns]) + 20, "row_heap_bytes": rows * 0xA4,
        })

    ctor, ctor_refs = widget.body("?Construct@ICEWidgetMenu@")
    relocation(ctor_refs, 0xC, "kaMenuStyles", 0x10)
    relocation(ctor_refs, 0x14, "kaMenuStyles", 0x11)
    words(ctor, 0x10, 0x54AA2834)  # r5 style index * 32
    words(ctor, 0x24, 0xD03F0034, 0x90BF0000, 0xD05F0038)  # f1/f2 origins
    words(ctor, 0x30, 0x814B0000, 0x915F0004, 0x814B0004, 0x915F000C, 0x814B0008, 0x915F0008)
    words(ctor, 0x48, 0x896B000C, 0x2B0B0000, 0x419A000C, 0x93DF0014, 0x4800000C, 0x3960FFFF, 0x917F0014)
    words(ctor, 0x9C, 0x817F0004, 0x7C832378, 0x1C8B00A4)
    relocation(ctor_refs, 0xA8, "?GetMemory@ICEMemory@", 0x06)
    words(ctor, 0xAC, 0x907F0030, 0x2B030000, 0x419A0038)
    words(ctor, 0xCC, 0x809F0008, 0x7C6BF214)
    relocation(ctor_refs, 0xD4, "?Prepare@ICEWidgetMenuItem@", 0x06)
    words(ctor, 0xE0, 0x3BDE00A4)
    assert x360rd.u32(0x82531024) == 0x1C8B00A4

    width, width_refs = widget.body("?GetWidth@ICEWidgetMenu@")
    relocation(width_refs, 0x0, "KF_ICE_MENU_MARGIN", 0x10)
    relocation(width_refs, 0xC, "KF_ICE_MENU_MARGIN", 0x11)
    assert widget.symbol("kaMenuStyles")[1] - widget.symbol("KF_ICE_MENU_MARGIN")[1] == 0x38
    words(width, 0x14, 0x392B0038, 0xC1AB0000, 0x550B2834)
    words(width, 0x7C, 0x39280004)
    render, render_refs = widget.body("?Render@ICEWidgetMenu@")
    relocation(render_refs, 0x44, "?GetWidth@ICEWidgetMenu@", 0x06)
    relocation(render_refs, 0x60, "KF_ICE_MENU_TEXT_SIZE", 0x11)
    assert widget.symbol("kaMenuStyles")[1] - widget.symbol("KF_ICE_MENU_TEXT_SIZE")[1] == 0x30
    words(render, 0x1C0, 0x817F0000, 0x395C0030)
    words(render, 0x1D0, 0x556B2834)
    words(render, 0x1EC, 0x3AEB0010)
    words(render, 0x334, 0xC0170000)
    words(render, 0x340, 0x3AF70004)

    menus, menu_refs = controller.body("?ConstructMenus@ICEController@")
    start_calls = [(at, name) for at, (name, kind) in menu_refs.items()
                   if kind == 6 and name.startswith("?Construct@ICEWidgetMenu@")]
    assert [at for at, _ in start_calls] == [0x40, 0x64, 0xBC, 0xE0, 0x104, 0x128, 0x14C]
    for (at, _), style in zip(start_calls, (1, 2, 0, 4, 3, 5, 6)):
        assert struct.pack(">I", 0x38A00000 | style) in menus[at - 0x14:at]
    relocation(menu_refs, 0x34, "KF_ICE_MENU_PAIR_A_X", 0x11)
    assert controller.symbol("KF_ICE_MENU_Y_SPACING")[1] - controller.symbol("KF_ICE_MENU_PAIR_A_X")[1] == 0x28
    assert controller.literal("KF_ICE_MENU_Y_SPACING", 4) == x360rd.rd(0x82004FD8, 4)
    words(menus, 0x8C, 0xC01E0028, 0x815F0F88, 0xC1AB0038, 0xEC0D002A, 0xD00A0038)
    words(menus, 0x18C, 0x39400000)
    words(menus, 0x198, 0xD01F7140, 0x915F7138)
    words(menus, 0x1A8, 0xD1BF713C)
    words(menus, 0x1E8, 0x915F71EC)
    words(menus, 0x21C, 0xD0FF71E4)
    words(menus, 0x224, 0xD0DF71E8)
    for offset, value in (
        (0x190, x360rd.f32(0x82F2884C) - x360rd.f32(0x82004FD8)),
        (0x1A0, x360rd.f32(0x82F28848) + x360rd.f32(0x8207B5F8)),
        (0x214, x360rd.f32(0x8207ACC4)),
        (0x220, x360rd.f32(0x8207ACC8)),
    ):
        bits = struct.pack(">f", value)
        symbol = "__real@" + bits.hex()
        relocation(menu_refs, offset, symbol, 0x11)
        assert controller.literal(symbol, 4) == bits

    receipt = {
        "head": args.expected_head, "tree": git("rev-parse", "HEAD^{tree}"), "base": BASE,
        "preserved_r153": PRESERVED, "compile_report_sha256": sha(report_path.read_bytes()),
        "objects_sha256": {FILES[key]: sha(obj.data) for key, obj in objects.items()},
        "style_table": {"artist": "0x8207ABB8", "bytes": len(style_data), "sha256": sha(style_data), "styles": styles},
        "construct": trace(ctor, 0x10, 0x24, 0x2C, 0x30, 0x48, 0x50, 0x54, 0x60, 0xA4, 0xA8, 0xD4, 0xE0),
        "get_width": trace(width, 0x14, 0x1C, 0x7C, 0x90),
        "render_widths": trace(render, 0x1D0, 0x1EC, 0x334, 0x340),
        "controller_geometry": trace(menus, 0x8C, 0x98, 0x9C, 0x198, 0x19C, 0x1A8, 0x1E8, 0x21C, 0x224),
        "scope": "Native XDK compile, ARTIST data equality and selected PPC ABI/store witnesses.",
    }
    path = ROOT / "PPC_EVIDENCE.json"
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(path, sha(path.read_bytes()))


if __name__ == "__main__":
    main()
