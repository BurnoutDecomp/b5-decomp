"""Recheck the R144 launch-data instructions in a sealed successor image."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import re

import pefile


HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
BASE = WAVE / "r144-link"
PROOF = WAVE / "r144-launch-codegen-proof" / "R144_CODEGEN_LINKED.json"
PROOF_SHA = "a5ac54f4401d76fc5a6aec7c771b07d18b4dd4980ab6883818a416cb4f732274"
SECTIONS = {
    "hardware_and_getter": (
        (0x8235AF38, "?GetSoftRebootData@BrnGameModule@"),
        (0x8237D568, "?Prepare@System360HW@"),
    ),
    "invite_classification": ((0x8237D800, "?HasGameBeenRebootedDueToInvite@System360HW@"),),
    "game_over_roundtrip": ((0x8235EAC8, "?BridgeGuiToGame@BrnGameModule@"),),
    "network_loader": ((0x82380640, "?LoadNetworkModule@LoadingScriptedState@"),),
    "buddy_read_and_write": (
        (0x82539B80, "?Prepare@BuddyManagerX360@BrnNetwork@"),
        (0x82539E10, "?DoInvite@BuddyManagerX360@BrnNetwork@"),
    ),
    "boot_legal_controller": ((0x82465050, "?Update@BootLegal@"),),
}
MAP_ROW = re.compile(r"\s+[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+")


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def symbols(path):
    by_addr, by_name = {}, {}
    for line in path.read_text(errors="replace").splitlines():
        match = MAP_ROW.match(line)
        if match:
            name, address = match[1], int(match[2], 16)
            by_addr.setdefault(address, set()).add(name)
            by_name.setdefault(name, set()).add(address)
    return by_addr, by_name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--output", type=Path, default=HERE / "R145_LAUNCH_DATA_LINKED.json")
    args = parser.parse_args()
    target = args.link.resolve()
    assert sha(PROOF) == PROOF_SHA
    old = read(PROOF)
    assert sha(BASE / "native-game.exe") == old["xex_sha256"]
    assert sha(BASE / "native-game.map") == old["map_sha256"]
    seal = read(target / "seal.json")
    inventory = read(target / "source-inventory.json")
    invocation = read(target / "invocation.json")
    image = read(target / "image-verification.json")
    result = read(target / "result.json")
    assert inventory["head"] == seal["head"] == image["head"] == args.expected_head
    assert seal["source_inventory"]["sha256"] == sha(target / "source-inventory.json")
    assert invocation["sealed_sha256"] == sha(target / "seal.json")
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    for name in ("native-game.exe", "native-game.map", "native-game.pe"):
        path = target / name
        assert result["outputs"][str(path)]["sha256"] == sha(path)
    assert image["magic"] == "XEX2" and image["image_sha256"] == sha(target / "native-game.exe")
    baseline_symbols, _ = symbols(BASE / "native-game.map")
    _, target_symbols = symbols(target / "native-game.map")
    pe = pefile.PE(str(target / "native-game.pe"), fast_load=True)
    assert pe.FILE_HEADER.Machine == 0x1F2 and pe.OPTIONAL_HEADER.ImageBase == 0x82000000

    def current_symbol(old_addr, prefix=None):
        names = sorted(baseline_symbols[old_addr])
        if prefix:
            names = [n for n in names if n.startswith(prefix)]
            assert len(names) == 1, (hex(old_addr), names)
        new_addrs = {a for n in names for a in target_symbols.get(n, ())}
        assert len(new_addrs) == 1, (hex(old_addr), names, new_addrs)
        return new_addrs.pop()

    def word(addr):
        return struct.unpack(">I", pe.get_data(addr - 0x82000000, 4))[0]

    results = {}
    for section, entries in old["selected_linked_checks"].items():
        anchors = [(address, current_symbol(address, prefix)) for address, prefix in SECTIONS[section]]
        checked = []
        for entry in entries:
            prior = int(entry["address"] if isinstance(entry, dict) else entry[0], 16)
            origin, new_origin = max((a, b) for a, b in anchors if a <= prior)
            current = new_origin + prior - origin
            if isinstance(entry, dict):
                values = [word(current + i * 4) for i in range(len(entry["words"]))]
                actual = [f"{value:08x}" for value in values]
                assert actual == entry["words"], (section, hex(current), actual, entry["words"])
                checked.append({"old_address": hex(prior), "address": hex(current), "words": actual})
            else:
                dest = current_symbol(int(entry[1], 16))
                value = word(current)
                assert value & 0xFC000003 == 0x48000001, (section, hex(current), hex(value))
                offset = value & 0x03FFFFFC
                if offset & 0x02000000:
                    offset -= 0x04000000
                assert current + offset == dest, (section, hex(current), hex(dest), hex(value))
                checked.append({"old_call": hex(prior), "call": hex(current), "target": hex(dest)})
        results[section] = checked
    report = {
        "head": args.expected_head,
        "xex_sha256": image["image_sha256"],
        "pe_sha256": sha(target / "native-game.pe"),
        "map_sha256": sha(target / "native-game.map"),
        "seal_sha256": sha(target / "seal.json"),
        "r144_receipt_sha256": PROOF_SHA,
        "selected_linked_checks": results,
        "scope": "Selected Xbox launch-data code words and direct linked target symbols",
    }
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(report, output, indent=2)
        output.write("\n")
    print(args.output, sha(args.output))


if __name__ == "__main__":
    main()