
"""Check R161 provider and consumer edges in the original-XDK listings."""
import json
from pathlib import Path
import re
import struct

from build_focus import OUT, REPO, BASE, git, sha

audit = OUT.parent / "r161-mainflow-audit/ARTIST_INPUT_PPC.txt"
compile_path = OUT / "compile-receipt.json"
artifact = {}


def read_listing(stem):
    return (OUT / f"{stem}.asm").read_text(errors="replace")


def body(stem, symbol):
    listing = read_listing(stem)
    match = re.search(r"^(" + re.escape(symbol) + r"\S*)\s+PROC NEAR.*?^\1\s+ENDP",
                      listing, re.MULTILINE | re.DOTALL)
    assert match, (stem, symbol)
    rows = {int(offset, 16): (int(word, 16), inst.strip()) for offset, word, inst in
            re.findall(r"^  ([0-9a-f]{5})\t([0-9a-f]{8})\t(.*)$", match[0], re.MULTILINE)}
    assert rows, symbol
    return rows


def call(rows, target, count=1):
    matches = [offset for offset, (word, instr) in rows.items()
               if word == 0x48000001 and target in instr]
    assert len(matches) == count, (target, matches)
    return matches[0] if count == 1 else matches


def at_disp(rows, opcode, displacement):
    return [offset for offset, (word, _) in rows.items()
            if word >> 26 == opcode and (word & 0xFFFF) == displacement]


def coff_body(stem, symbol):
    data = (OUT / f"{stem}.obj").read_bytes()
    machine, _, _, symbol_ptr, symbol_count, opt_size, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x01F2
    string_ptr = symbol_ptr + 18 * symbol_count

    def name_at(index):
        at = symbol_ptr + 18 * index
        name = data[at:at + 8]
        if name[:4] == bytes(4):
            at = string_ptr + struct.unpack_from("<I", name, 4)[0]
            name = data[at:data.index(bytes(1), at)]
        return name.rstrip(bytes(1)).decode("ascii")

    index = 0
    while index < symbol_count:
        at = symbol_ptr + 18 * index
        value, section, typ, _, aux = struct.unpack_from("<IhHBB", data, at + 8)
        if section > 0 and typ == 0x20 and name_at(index).startswith(symbol):
            break
        index += 1 + aux
    else:
        raise AssertionError((stem, symbol))

    header = 20 + opt_size + 40 * (section - 1)
    _, _, size, raw_ptr, rel_ptr, _, rel_count, _, _ = struct.unpack_from("<IIIIIIHHI", data, header + 8)
    code = data[raw_ptr + value:raw_ptr + size]
    relocations = {}
    for at in range(rel_ptr, rel_ptr + 10 * rel_count, 10):
        offset, target, kind = struct.unpack_from("<IIH", data, at)
        relocations[offset - value] = (kind, name_at(target))
    return code, relocations


def verify_coff_calls(stem, symbol, offsets, targets, rows):
    code, relocations = coff_body(stem, symbol)
    checked = {}
    for offset, target in zip(offsets, targets, strict=True):
        kind, decorated = relocations[offset]
        assert kind == 6 and target in decorated
        word = int.from_bytes(code[offset:offset + 4], "big")
        assert word >> 26 == 18 and word & 1 and decorated in rows[offset][1]
        checked[f"0x{offset:X}"] = {"type": kind, "symbol": decorated, "word": f"{word:08X}"}
    return code, checked


def main():
    head = git("rev-parse", "HEAD")
    assert git("merge-base", BASE, head) == BASE and not git("status", "--porcelain=v1")
    assert git("rev-parse", "refs/heads/codex/experimental-x360-r159-gamedata-bind-order") == (
        "d46884e57316b0707526c9bf0cf086c2fd0e8b57")
    compiled = json.loads(compile_path.read_bytes())
    assert compiled["head"] == head and compiled["tree"] == git("rev-parse", "HEAD^{tree}")
    assert len(compiled["results"]) == 10 and not compiled["status"]
    for source, digest in compiled["input_sha256"].items():
        assert sha(REPO / source) == digest, source
    for row in compiled["results"]:
        name = Path(row["source"]).stem
        assert row["exit_code"] == 0 and row["machine"] == "0x01F2"
        assert sha(OUT / f"{name}.obj") == row["ppc_coff_sha256"]
        assert sha(OUT / f"{name}.asm") == row["listing_sha256"]

    pre = body("CgsInputModule", "?PreWorldUpdate@InputModule@CgsInput@@")
    order = [call(pre, "?LockForWrite@IOBuffer"), call(pre, "?LockForRead@IOBuffer"),
             call(pre, "?ProcessRumbleRequests@InputModule"), call(pre, "?UnlockForRead@IOBuffer"),
             call(pre, "?Update@InputPads@CgsInput"), call(pre, "?GetBindResultQueue@OutputBuffer"),
             call(pre, "?GetUnbindResultQueue@OutputBuffer"), call(pre, "?UnlockForWrite@IOBuffer")]
    assert order == sorted(order), order
    assert at_disp(pre, 14, 0x230)  # addi receiver, InputModule, mControllers offset
    artifact["input_module_call_offsets"] = [f"0x{value:X}" for value in order]
    code, module_relocations = verify_coff_calls(
        "CgsInputModule", "?PreWorldUpdate@InputModule@CgsInput@@", order,
        ("?LockForWrite", "?LockForRead", "?ProcessRumbleRequests", "?UnlockForRead",
         "?Update@InputPads", "?GetBindResultQueue", "?GetUnbindResultQueue", "?UnlockForWrite"), pre)
    assert code[0x44:0x58] == bytes.fromhex("578B063E 2B0B0000 419A0010 7FC4F378 387F0230")
    artifact["pre_world_coff_relocations"] = module_relocations
    artifact["pre_world_coff_gate_bytes"] = code[0x44:0x58].hex()

    pads = body("CgsInputPads", "?Update@InputPads@CgsInput@@")
    pad_order = [call(pads, "?UpdateConnectedDevices@ManagerX360"),
                 call(pads, "?UnbindFromPort@ManagerX360"), call(pads, "?BindToPort@ManagerX360"),
                 call(pads, "?Update@ManagerX360"), call(pads, "?GetPadInfo@OutputBuffer"),
                 call(pads, "?FillKeyBindings@AutoTestManager")]
    assert pad_order == sorted(pad_order), pad_order
    assert at_disp(pads, 32, 0xF0)  # previous bound port
    assert at_disp(pads, 36, 0x398) and at_disp(pads, 36, 0x39C)
    assert at_disp(pads, 38, 0x3A0)
    assert at_disp(pads, 14, 0x648)
    assert any("GetPadDisconnectedQueue@OutputBuffer" in instr for _, instr in pads.values())
    assert any("fcmpu" in instr for _, instr in pads.values())
    artifact["pad_update_call_offsets"] = [f"0x{value:X}" for value in pad_order]
    code, pad_relocations = verify_coff_calls(
        "CgsInputPads", "?Update@InputPads@CgsInput@@", pad_order,
        ("?UpdateConnectedDevices", "?UnbindFromPort", "?BindToPort", "?Update@ManagerX360",
         "?GetPadInfo", "?FillKeyBindings"), pads)
    assert code[0x2F4:0x2FC] == bytes.fromhex("389A0018 386B0648")
    artifact["pad_update_coff_relocations"] = pad_relocations

    manager = body("CgsInputManagerX360", "?Update@ManagerX360@CgsInput@@")
    manager_call = call(manager, "?Update@DeviceX360Pad@CgsInput")
    artifact["manager_device_update_offset"] = f"0x{manager_call:X}"
    _, artifact["manager_update_coff_relocations"] = verify_coff_calls(
        "CgsInputManagerX360", "?Update@ManagerX360@CgsInput@@", [manager_call],
        ["?Update@DeviceX360Pad"], manager)

    brn = body("BrnGameModule", "?DoUpdate_InputPreWorld@BrnGameModule@BrnGame@@")
    assert any(word == 0x816B0044 for word, _ in brn.values())
    assert any(word == 0x4E800421 for word, _ in brn.values())
    assert call(brn, "?GetPadInfo@OutputBuffer@InputIO@CgsInput@@QB") > 0
    assert at_disp(brn, 32, 0x5C)
    listing = read_listing("BrnGameModule")
    table = re.search(r"^\?\?_7InputModule@CgsInput@@6B@\s+DD\s+(.*?)(?:^; Function compile flags)",
                      listing, re.MULTILINE | re.DOTALL)
    assert table
    slots = [table[1].splitlines()[0]] + re.findall(r"^\s*DD\s+(\S+)", table[1], re.MULTILINE)
    assert len(slots) == 19 and "?PreWorldUpdate@" in slots[17] and "?PostWorldUpdate@" in slots[18]
    artifact["input_module_virtual_slots"] = {"0x44": slots[17], "0x48": slots[18]}

    artifact.update(commit=head, tree=compiled["tree"], parent=git("rev-parse", "HEAD^"), base=BASE,
                    compile_receipt_sha256=sha(compile_path), artist_ppc_sha256=sha(audit),
                    focused_ppc_compiles=len(compiled["results"]),
                    scope="Original Xbox SDK PPC COFF plus ARTIST source evidence. Strict link and runtime pending.")
    (OUT / "RECEIPT.json").write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    print("Verified native pad update, IO lock order, type and field writes, and the title input dispatch")


if __name__ == "__main__":
    main()

