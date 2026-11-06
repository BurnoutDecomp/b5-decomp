#!/usr/bin/env python3
"""Verify startup routing in the sealed R152 image against bound XDK and R151 evidence."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

import pefile


ARTIFACTS = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20")
OUTDIR = ARTIFACTS / "r152-startup-routing-linked"
COMPOSITION = ARTIFACTS / "r152-batch-v2"
PREFLIGHT = ARTIFACTS / "r152-composed-preflight"
BASE = ARTIFACTS / "r151-link"
OLD_INTAKE = ARTIFACTS / "r149-intake" / "native-selected"
SOURCE_REPO = Path(r"C:\Users\adri1\.codex\worktrees\bp360\native_resources")

FROZEN_HEAD = "bf4deeec085ff4cefaba8eeb4cfdc2df8384409b"
FROZEN_TREE = "64d1fe0fb7713085ce642f38892db1ea51dba53e"
MANIFEST_SHA256 = "163ff7068ddf24b9f37475ddae2e2fa1ec148a8bcd58c90e012900e588b8ae34"
BINDING_SHA256 = "2eb69dcc2bb1e241ad6ac7fa509d966eec0d4ac7169d4b9515aa7a48d4f44705"
COMPILE_REPORT_SHA256 = "8f82e6af443b9ada907125f9f5c7b034bee7430223898fe6ccfa830b926d8b26"

OFFICIAL_HEAD = "5159415953b6b1c2e9cf9c82bfa67dfa5d5e2ed9"
OFFICIAL_SEAL_SHA256 = "98e8f322bdaa6ff58e2db938962c5bbbfe479f5e9db8ac58b6f20e7e24144ca0"
OFFICIAL_XEX_SHA256 = "c99cfab9c8aeca5c692a84491b88840e8260fbb24267c335008d9783ee120daa"
OFFICIAL_PE_SHA256 = "506a33ce5c18c5c8ec2b2c606b2106c3fbf29f3689cab41c13ecd490b57319f1"
OFFICIAL_MAP_SHA256 = "254be758b540e3bf8abe09631d7656a41336ba398e2f6ef94bb26822ffe24e52"
BASE_HEAD = "a632bc6495f5accac5dd267ddadea49c2072f7f7"
BASE_TREE = "e5a7b8962ff39181fd7ccc77f8708851418bbd44"
BASE_SEAL_SHA256 = "6a73f783a235dc99aaa3914eef0f721311d128a0b4c81eacb002ac19780b6be0"
BASE_RESULT_SHA256 = "117ea004dbe2879266fdd07b2c0e65cfe9375a5d02fc6fbd01bb9634a90b4692"
BASE_XEX_SHA256 = "852197623e70c6347e7591d39f5d152b1039ca15eebed5385bfd1b3ab0854b56"
BASE_PE_SHA256 = "7e5b2a6e6eaba9703c74727a1e4fe7debc32b63ba0badbc37dfb56cffcf701ce"
BASE_MAP_SHA256 = "d200ddfc2685a98b9f8c0bf62be01d63d4d02a5576c0d17b4395a029f562c04e"

REC = "?RecEvent@GuiCache@BrnGui@@QAAXPBUEvent@CgsModule@@H@Z"
NETWORK_BRIDGE = (
    "?TranslateNetworkEventsToGuiEvents@BrnGameModule@BrnGame@@QAAX"
    "PAUInputBuffer@CgsGuiModuleIO@CgsGui@@PBUOutputBuffer@BrnNetworkModuleIO@BrnNetwork@@@Z"
)
GUI_UPDATE = (
    "?Update@GuiModule@BrnGui@@UAAXGPAUIOBufferStack@CgsModule@@0"
    "PAUInputBuffer@CgsGuiModuleIO@CgsGui@@PAUOutputBuffer@67@PAU8ModelIO@7@"
    "PAU5ViewIO@7@PAU5GameDataIO@BrnResource@@PBU8GameDataIO@BrnResource@@_N@Z"
)
SCREEN_UPDATE = "?Update@ScreenLoading@BrnGui@@UAAXXZ"
SEND_STATE = "?SendStateEvent@State@CgsGui@@QAAXPBD@Z"
APPLY_PROFILE = "?ApplyOptionsDataProfileSettings@ScreenLoading@BrnGui@@AAAXXZ"
GUI_QUEUE_GET = "?GetGuiEvents@InputBuffer@CgsGuiModuleIO@CgsGui@@QAAPAV?$GuiEventQueueBase@$0IAAA@$0BA@@3@XZ"
GUI_QUEUE_ADD = "?AddEvent@?$VariableEventQueue@$0IAAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z"

R152_ASM = {
    "rec": PREFLIGHT / "BrnGuiCache-b13d1757.asm",
    "screen": PREFLIGHT / "BrnScreenLoading-a0a66f8c.asm",
}
R151_ASM = {
    "rec": OLD_INTAKE / "BrnGuiCache-b13d1757.asm",
    "screen": OLD_INTAKE / "BrnScreenLoading-a0a66f8c.asm",
    "network": OLD_INTAKE / "GameBridgeNetworkToX-df33a80f.asm",
    "gui": OLD_INTAKE / "BrnGuiModule-c9029ba8.asm",
}
PINNED_ARTIFACTS = {
    COMPOSITION / "MANIFEST.json": MANIFEST_SHA256,
    COMPOSITION / "TARGETED_OBJECT_BINDING.json": BINDING_SHA256,
    PREFLIGHT / "compile-results.json": COMPILE_REPORT_SHA256,
    R152_ASM["rec"]: "927be1ed10c7941f5797ba4f84f0b6764293a39ba04452d64b0c73237adf389f",
    PREFLIGHT / "BrnGuiCache-b13d1757.obj": "69f4eeb9445e02983cdc2d35540c5096f261d88b4523955a5d1d32d6697ca792",
    R152_ASM["screen"]: "8fa398443ecc4e7877683af5032b7212f713af0f3c782d981d936bc53fc3683f",
    PREFLIGHT / "BrnScreenLoading-a0a66f8c.obj": "5f7c899981b058bc2dcf34b37d63088ce8e786b1440b92828f6a62c87d9149b2",
    R151_ASM["rec"]: "54bd7c15bf92a76d504ab1364b4a4b97f2528cb43300d09737ea257d86c63bc3",
    OLD_INTAKE / "BrnGuiCache-b13d1757.obj": "cbf7e5ed721e8d4b73a7eed338a7ead618d74d030bed9b3aa50ea5b4e578d0c1",
    R151_ASM["screen"]: "7b8e9f72a615d4faad20eaad7c8a2874022478ef14037a6af83a402bb8a42025",
    OLD_INTAKE / "BrnScreenLoading-a0a66f8c.obj": "1fac25106bbaf37d7495d6e25ce095862393858df16803ed9b269fedabfa1bde",
    R151_ASM["network"]: "8bdc85c31ec8380ee177940a6ee0aad095dfe54afa73ba858b0d29de6bd47518",
    OLD_INTAKE / "GameBridgeNetworkToX-df33a80f.obj": "7e1ad3c5d1a8dfaebeaeeafbaf5ad0d44dfea658c3c40bd97c3b6765e0d76a6e",
    R151_ASM["gui"]: "c8a4ea96660b42df2dfeeea68a2f73b1397cf03c4a0c2e7ebb94ed293374ac0d",
    OLD_INTAKE / "BrnGuiModule-c9029ba8.obj": "34d2f67d86a465a2128815c6b1c63f7a0dbb586fa0842f10cb6c3db07eff04bf",
}
COMPILE_OBJECTS = {
    "src/GameSource/Gui/BrnGuiCache.cpp":
        ("69f4eeb9445e02983cdc2d35540c5096f261d88b4523955a5d1d32d6697ca792",
         "927be1ed10c7941f5797ba4f84f0b6764293a39ba04452d64b0c73237adf389f"),
    "src/GameSource/Gui/Flow/Screen/States/BrnScreenLoading.cpp":
        ("5f7c899981b058bc2dcf34b37d63088ce8e786b1440b92828f6a62c87d9149b2",
         "8fa398443ecc4e7877683af5032b7212f713af0f3c782d981d936bc53fc3683f"),
}
PROVIDER_SOURCE_SHA256 = {
    "src/GameSource/Game/GameBridgeNetworkToX.cpp":
        "7e011ad86965308ec8663751742ddbfc6404b0c22dc809b99e609676c0dc8107",
    "src/GameSource/Gui/BrnGuiModule.cpp":
        "afb5eedc4ec55805fe29fbc2a79011e2eb481a95a78134533cd59f28c3b1c741",
}

EVENT_CASES = {
    109: {"index": 105, "artist": "0x8250EF90", "compiled_label": "$LN302@RecEvent"},
    128: {"index": 124, "artist": "0x8250EEE8", "compiled_label": "$LN301@RecEvent"},
    129: {"index": 125, "artist": "0x8250EEE8", "compiled_label": "$LN301@RecEvent"},
    131: {"index": 127, "artist": "0x8250EF04", "compiled_label": "$LN300@RecEvent"},
    132: {"index": 128, "artist": "0x8250EF78", "compiled_label": "$LN293@RecEvent"},
}
REC_CODE_OFFSETS = {
    109: [0x590, 0x594],
    128: [0x5AC, 0x5B0, 0x5B4, 0x5B8],
    129: [0x5AC, 0x5B0, 0x5B4, 0x5B8],
    131: list(range(0x5D0, 0x620, 4)),
    132: [0x634, 0x638, 0x63C],
}
MAP_RE = re.compile(
    r"^\s*[0-9a-fA-F]{4}:([0-9a-fA-F]{8})\s+(\S+)\s+"
    r"([0-9a-fA-F]{8})\s+(.*?)\s*$"
)
ASM_INSN_RE = re.compile(r"^\s+([0-9a-fA-F]{5})\s+([0-9a-fA-F]{8})\s+(.*)$")
ASM_LABEL_RE = re.compile(r"^\s*(\$\S+):\s*$")
DCD_RE = re.compile(r"\bDCD\s+\|([^|]+)\|")


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def git_bytes(repo, revision_path):
    return subprocess.check_output(
        ["git", "-C", str(repo), "show", revision_path], stderr=subprocess.STDOUT
    )


def git_tree(repo, revision):
    return subprocess.check_output(
        ["git", "-C", str(repo), "show", "-s", "--format=%T", revision],
        text=True, stderr=subprocess.STDOUT
    ).strip()


def git_parents(repo, revision):
    return subprocess.check_output(
        ["git", "-C", str(repo), "show", "-s", "--format=%P", revision],
        text=True, stderr=subprocess.STDOUT
    ).strip().split()


def git_blob_id(repo, revision_path):
    return subprocess.check_output(
        ["git", "-C", str(repo), "rev-parse", revision_path],
        text=True, stderr=subprocess.STDOUT
    ).strip()


def norm(path):
    return str(path).replace("\\", "/").casefold()


def parse_map(path):
    rows = []
    for line in Path(path).read_text(errors="replace").splitlines():
        match = MAP_RE.match(line)
        if not match:
            continue
        rest = match.group(4)
        object_match = re.search(r"([^\s]+\.obj)\s*$", rest, re.IGNORECASE)
        rows.append({
            "name": match.group(2),
            "address": int(match.group(3), 16),
            "object": object_match.group(1) if object_match else None,
            "line": line,
        })
    return rows


def map_symbol(rows, exact=None, prefix=None):
    matches = [r for r in rows if (r["name"] == exact if exact else r["name"].startswith(prefix))]
    unique = {(r["address"], r["object"]): r for r in matches}
    assert len(unique) == 1, (exact, prefix, [r["line"] for r in matches[:8]])
    return next(iter(unique.values()))


def asm_function(path, decorated, prefix=False):
    lines = Path(path).read_text(errors="replace").splitlines()
    start = None
    for i, line in enumerate(lines):
        if "PROC NEAR" not in line:
            continue
        name = line.split(" PROC NEAR", 1)[0].strip()
        if (name.startswith(decorated) if prefix else name == decorated):
            start = i
            break
    assert start is not None, (path, decorated)
    name = lines[start].split(" PROC NEAR", 1)[0].strip()
    end = next((i for i in range(start + 1, len(lines)) if name + " ENDP" in lines[i]), None)
    assert end is not None, (path, name)
    words, labels, dcd = {}, {}, {}
    pending = None
    for line in lines[start + 1:end]:
        label_match = ASM_LABEL_RE.match(line)
        if label_match:
            pending = label_match.group(1)
            continue
        insn_match = ASM_INSN_RE.match(line)
        if not insn_match:
            continue
        offset = int(insn_match.group(1), 16)
        word = int(insn_match.group(2), 16)
        tail = insn_match.group(3)
        if pending:
            labels[pending] = offset
            pending = None
        words[offset] = word
        dcd_match = DCD_RE.search(tail)
        if dcd_match:
            dcd[offset] = dcd_match.group(1)
    return {"name": name, "words": words, "labels": labels, "dcd": dcd}


def pe_bytes(pe, va, size):
    rva = va - pe.OPTIONAL_HEADER.ImageBase
    matches = [section for section in pe.sections
               if section.VirtualAddress <= rva < section.VirtualAddress + max(
                   section.Misc_VirtualSize, section.SizeOfRawData
               )]
    assert len(matches) == 1, (hex(va), hex(rva), len(matches))
    section = matches[0]
    delta = rva - section.VirtualAddress
    assert delta + size <= section.SizeOfRawData, (hex(va), size, section.Name)
    offset = section.PointerToRawData + delta
    return pe.__data__[offset:offset + size]


def pe_word(pe, va):
    return int.from_bytes(pe_bytes(pe, va, 4), "big")


def branch_target(word, pc):
    opcode = word >> 26
    if opcode == 18:
        disp = word & 0x03FFFFFC
        if disp & 0x02000000:
            disp -= 0x04000000
        return disp if word & 2 else pc + disp
    if opcode == 16:
        disp = word & 0x0000FFFC
        if disp & 0x00008000:
            disp -= 0x00010000
        return disp if word & 2 else pc + disp
    raise AssertionError((hex(word), hex(pc), opcode))


def check_code_words(pe, function_va, asm, offsets):
    checked = []
    for offset in offsets:
        assert offset in asm["words"], (asm["name"], hex(offset))
        expected = asm["words"][offset]
        actual = pe_word(pe, function_va + offset)
        if expected >> 26 in (16, 18):
            # These are local branches here; external relocations are checked by symbol below.
            expected_disp = expected & (0x0000FFFC if expected >> 26 == 16 else 0x03FFFFFC)
            assert expected_disp != 0, (asm["name"], hex(offset), hex(expected))
            expected_target = function_va + branch_target(expected, offset)
            actual_target = branch_target(actual, function_va + offset)
            assert actual_target == expected_target, (
                asm["name"], hex(offset), hex(actual), hex(actual_target), hex(expected_target)
            )
        else:
            assert actual == expected, (asm["name"], hex(offset), hex(actual), hex(expected))
        checked.append({"offset": hex(offset), "word": f"{actual:08x}"})
    return checked


def check_call(pe, function_va, offset, target_row):
    actual = pe_word(pe, function_va + offset)
    assert actual >> 26 == 18 and actual & 1, (hex(function_va + offset), hex(actual))
    target = branch_target(actual, function_va + offset)
    assert target == target_row["address"], (
        hex(function_va + offset), hex(target), target_row["name"], hex(target_row["address"])
    )
    return {"call": hex(function_va + offset), "target": target_row["name"], "address": hex(target)}


def check_table(pe, function_va, asm, table_label, index, expected_label):
    table_base = asm["labels"][table_label]
    slot = table_base + 4 * index
    assert asm["dcd"].get(slot) == expected_label, (
        asm["name"], hex(slot), asm["dcd"].get(slot), expected_label
    )
    expected_target = function_va + asm["labels"][expected_label]
    actual_target = pe_word(pe, function_va + slot)
    assert actual_target == expected_target, (
        asm["name"], hex(slot), hex(actual_target), hex(expected_target)
    )
    return {"slot_offset": hex(slot), "target_label": expected_label,
            "target": hex(actual_target)}


def selected_provider(seal, source, expected_sha=None):
    entries = [e for e in seal["selected"] if norm(e["source"]) == norm(source)]
    assert len(entries) == 1, (source, len(entries))
    entry = entries[0]
    assert Path(entry["object"]).is_file(), entry["object"]
    actual = sha(entry["object"])
    assert actual == entry["sha256"], (source, actual, entry["sha256"])
    if expected_sha:
        assert actual == expected_sha, (source, actual, expected_sha)
    return entry


def source_inventory_hash(inventory, source_root, source):
    suffix = "/" + norm(source).lstrip("/")
    root_prefix = norm(source_root).rstrip("/")
    rows = [r for r in inventory["files"]
            if norm(r["path"]).startswith(root_prefix + "/")
            and norm(r["path"]).endswith(suffix)]
    assert len(rows) == 1, (source, [r["path"] for r in rows])
    return rows[0]["sha256"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--link-dir", type=Path, required=True)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--expected-tree", required=True)
    parser.add_argument("--expected-xex-sha256", required=True)
    parser.add_argument("--expected-seal-sha256", required=True)
    parser.add_argument("--expected-pe-sha256", required=True)
    parser.add_argument("--expected-map-sha256", required=True)
    parser.add_argument("--expected-selected-objects-sha256")
    parser.add_argument("--source-repo", type=Path, default=SOURCE_REPO)
    parser.add_argument("--output", type=Path, default=OUTDIR / "R152_STARTUP_ROUTING_LINKED.json")
    args = parser.parse_args()

    assert args.expected_head == OFFICIAL_HEAD
    assert args.expected_tree == FROZEN_TREE
    assert args.expected_xex_sha256 == OFFICIAL_XEX_SHA256
    assert args.expected_seal_sha256 == OFFICIAL_SEAL_SHA256
    assert args.expected_pe_sha256 == OFFICIAL_PE_SHA256
    assert args.expected_map_sha256 == OFFICIAL_MAP_SHA256
    assert git_tree(args.source_repo, FROZEN_HEAD) == FROZEN_TREE
    assert git_tree(args.source_repo, args.expected_head) == args.expected_tree
    assert git_parents(args.source_repo, args.expected_head) == [BASE_HEAD]
    assert git_tree(args.source_repo, BASE_HEAD) == BASE_TREE

    for path, expected in PINNED_ARTIFACTS.items():
        assert sha(path) == expected, (str(path), sha(path), expected)
    manifest = read_json(COMPOSITION / "MANIFEST.json")
    binding = read_json(COMPOSITION / "TARGETED_OBJECT_BINDING.json")
    compile_report = read_json(PREFLIGHT / "compile-results.json")
    assert manifest["candidate"] == FROZEN_HEAD and manifest["projected_tree"] == FROZEN_TREE
    for receipt in manifest["composed_receipts"]:
        assert sha(receipt["path"]) == receipt["sha256"], receipt["path"]
    assert binding["head"] == FROZEN_HEAD and binding["tree"] == FROZEN_TREE
    receipt_hashes = {norm(r["path"]): r["sha256"] for r in manifest["composed_receipts"]}
    assert receipt_hashes[norm(str(COMPOSITION / "TARGETED_OBJECT_BINDING.json"))] == BINDING_SHA256
    assert receipt_hashes[norm(str(ARTIFACTS / "r152-batch" / "TARGETED_OBJECT_BINDING.json"))] == "808a96b984b94d172b0db48965d74e1b0a86c5eff843c9787913ac5ad475e46f"

    binding_rows = {norm(r["source"]): r for r in binding["results"]}
    compile_rows = {norm(r["source"]): r for r in compile_report["results"]}
    for source, (obj_sha, asm_sha) in COMPILE_OBJECTS.items():
        key = norm(source)
        bound = binding_rows[key]
        compiled = compile_rows[key]
        assert bound["source_sha256"] and bound["machine"] == 498
        assert compiled["passed"] and compiled["returncode"] == 0
        assert bound["artifacts"]["object"]["sha256"] == obj_sha
        assert bound["artifacts"]["assembly"]["sha256"] == asm_sha
        assert sha(bound["artifacts"]["object"]["path"]) == obj_sha
        assert sha(bound["artifacts"]["assembly"]["path"]) == asm_sha

    base_seal = read_json(BASE / "seal.json")
    base_inventory = read_json(BASE / "source-inventory.json")
    base_result = read_json(BASE / "result.json")
    base_image = read_json(BASE / "image-verification.json")
    assert sha(BASE / "seal.json") == BASE_SEAL_SHA256
    assert sha(BASE / "result.json") == BASE_RESULT_SHA256
    assert sha(BASE / "native-game.exe") == BASE_XEX_SHA256
    assert sha(BASE / "native-game.pe") == BASE_PE_SHA256
    assert sha(BASE / "native-game.map") == BASE_MAP_SHA256
    assert base_seal["head"] == base_image["head"] == BASE_HEAD
    assert base_result["exit_code"] == 0 and base_result["all_post_checks_pass"]
    assert base_image["image_sha256"] == BASE_XEX_SHA256 and base_image["magic"] == "XEX2"

    target = args.link_dir.resolve()
    seal_path = target / "seal.json"
    seal = read_json(seal_path)
    inventory = read_json(target / "source-inventory.json")
    invocation = read_json(target / "invocation.json")
    result = read_json(target / "result.json")
    image = read_json(target / "image-verification.json")
    assert sha(seal_path) == args.expected_seal_sha256
    assert seal["head"] == inventory["head"] == args.expected_head
    assert seal["source_inventory"]["sha256"] == sha(target / "source-inventory.json")
    assert invocation["sealed_sha256"] == sha(seal_path)
    if args.expected_selected_objects_sha256:
        assert seal["selected_objects_sha256"] == args.expected_selected_objects_sha256
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    for basename in ("native-game.exe", "native-game.pe", "native-game.map"):
        path = target / basename
        output_entry = next((v for k, v in result["outputs"].items()
                             if Path(k).name.casefold() == basename.casefold()), None)
        assert output_entry and output_entry["sha256"] == sha(path), basename
    xex_sha = sha(target / "native-game.exe")
    pe_sha = sha(target / "native-game.pe")
    map_sha = sha(target / "native-game.map")
    assert pe_sha == args.expected_pe_sha256
    assert map_sha == args.expected_map_sha256
    assert xex_sha == args.expected_xex_sha256 == image["image_sha256"]
    assert image["head"] == args.expected_head and image["magic"] == "XEX2"
    pe = pefile.PE(str(target / "native-game.pe"), fast_load=True)
    assert pe.FILE_HEADER.Machine == 0x1F2
    assert pe.OPTIONAL_HEADER.ImageBase == 0x82000000

    source_root = seal["source_root"]
    required_sources = {
        "src/GameSource/Gui/BrnGuiCache.cpp": binding_rows[norm("src/GameSource/Gui/BrnGuiCache.cpp")]["source_sha256"],
        "src/GameSource/Gui/Flow/Screen/States/BrnScreenLoading.cpp": binding_rows[norm("src/GameSource/Gui/Flow/Screen/States/BrnScreenLoading.cpp")]["source_sha256"],
        **PROVIDER_SOURCE_SHA256,
    }
    providers = {}
    for source, expected_source_sha in required_sources.items():
        current = source_inventory_hash(inventory, source_root, source)
        frozen_blob = git_blob_id(args.source_repo, f"{args.expected_head}:{source}")
        if norm(source) in binding_rows:
            bound_source = binding_rows[norm(source)]
            assert bound_source["git_blob"] == frozen_blob
            assert current == bound_source["source_sha256"] == expected_source_sha
        else:
            base_source_sha = source_inventory_hash(base_inventory, base_seal["source_root"], source)
            base_blob = git_blob_id(args.source_repo, f"{BASE_HEAD}:{source}")
            assert frozen_blob == base_blob and current == base_source_sha == expected_source_sha
        provider = selected_provider(seal, source)
        providers[source] = {
            "source_sha256": current,
            "object": provider["object"],
            "object_sha256": provider["sha256"],
        }

    for source in COMPILE_OBJECTS:
        bound = binding_rows[norm(source)]
        providers[source]["targeted_preflight_object"] = {
            "path": bound["artifacts"]["object"]["path"],
            "sha256": bound["artifacts"]["object"]["sha256"],
        }
        providers[source]["targeted_preflight_assembly"] = {
            "path": bound["artifacts"]["assembly"]["path"],
            "sha256": bound["artifacts"]["assembly"]["sha256"],
        }
        # The sealed object can differ in debug/file provenance; the final linked
        # instructions below must still match the pinned target assembly.

    final_map = parse_map(target / "native-game.map")
    base_map = parse_map(BASE / "native-game.map")
    r152_rec_asm = asm_function(R152_ASM["rec"], REC)
    r152_screen_asm = asm_function(R152_ASM["screen"], SCREEN_UPDATE)
    r151_rec_asm = asm_function(R151_ASM["rec"], REC)
    r151_screen_asm = asm_function(R151_ASM["screen"], SCREEN_UPDATE)
    r151_network_asm = asm_function(R151_ASM["network"], NETWORK_BRIDGE)
    r151_gui_asm = asm_function(R151_ASM["gui"], GUI_UPDATE)

    function_specs = [
        (final_map, REC, providers["src/GameSource/Gui/BrnGuiCache.cpp"]),
        (final_map, SCREEN_UPDATE, providers["src/GameSource/Gui/Flow/Screen/States/BrnScreenLoading.cpp"]),
        (final_map, NETWORK_BRIDGE, providers["src/GameSource/Game/GameBridgeNetworkToX.cpp"]),
        (final_map, GUI_UPDATE, providers["src/GameSource/Gui/BrnGuiModule.cpp"]),
    ]
    mapped = {}
    for rows, name, provider in function_specs:
        symbol = map_symbol(rows, exact=name)
        assert symbol["object"] and Path(symbol["object"]).name.casefold() == Path(provider["object"]).name.casefold(), (
            name, symbol["object"], provider["object"]
        )
        mapped[name] = symbol

    base_rec = map_symbol(base_map, exact=REC)
    base_screen = map_symbol(base_map, exact=SCREEN_UPDATE)
    base_network = map_symbol(base_map, exact=NETWORK_BRIDGE)
    base_gui = map_symbol(base_map, exact=GUI_UPDATE)
    for sym, source in (
        (base_rec, "src/GameSource/Gui/BrnGuiCache.cpp"),
        (base_screen, "src/GameSource/Gui/Flow/Screen/States/BrnScreenLoading.cpp"),
        (base_network, "src/GameSource/Game/GameBridgeNetworkToX.cpp"),
        (base_gui, "src/GameSource/Gui/BrnGuiModule.cpp"),
    ):
        assert sym["object"], sym["line"]

    # R151 linked baseline: all five event ids still selected the default RecEvent arm.
    base_pe = pefile.PE(str(BASE / "native-game.pe"), fast_load=True)
    base_rec_cases = {}
    base_table = r151_rec_asm["labels"]["$LN497@RecEvent"]
    default_label = "$LN1@RecEvent"
    for event_id, data in EVENT_CASES.items():
        slot = base_table + 4 * (event_id - 14)
        assert r151_rec_asm["dcd"].get(slot) == default_label
        expected = base_rec["address"] + r151_rec_asm["labels"][default_label]
        actual = pe_word(base_pe, base_rec["address"] + slot)
        assert actual == expected
        base_rec_cases[event_id] = {"slot_offset": hex(slot), "target": hex(actual), "label": default_label}

    # Verify the R152 native dispatch table and all five case bodies in the final linked PE.
    rec_cases = {}
    for event_id, data in EVENT_CASES.items():
        rec_cases[event_id] = check_table(
            pe, mapped[REC]["address"], r152_rec_asm, "$LN513@RecEvent",
            event_id - 14, data["compiled_label"]
        )
        rec_cases[event_id]["artist_index"] = data["index"]
        rec_cases[event_id]["artist_target"] = data["artist"]
        rec_cases[event_id]["code"] = check_code_words(
            pe, mapped[REC]["address"], r152_rec_asm, REC_CODE_OFFSETS[event_id]
        )

    # Exact generated event-131 loop: eight disconnected bytes, then per-player
    # eliminated (+8 from in-car-select base) and in-car-select bytes.
    event131 = rec_cases[131]
    event131["array_order"] = [
        {"field": "maOnlinePlayerDisconnected", "offset": "0xB84C", "count": 8},
        {"field": "maOnlinePlayerEliminated", "offset": "0xB85C", "count": 8, "per_player_order": 1},
        {"field": "maOnlinePlayerInCarSelect", "offset": "0xB854", "count": 8, "per_player_order": 2},
    ]
    event131["invite_flags"] = [{"field": "mbInviteInProgress", "offset": "0x4B4D", "value": 0},
                                {"field": "mbPerformingInvite", "offset": "0x4B4F", "value": 1}]

    # Verify network event 42 reaches GUI event 109, then AddEvent, and that GuiModule
    # feeds the dequeued event id to GuiCache::RecEvent.
    bridge = mapped[NETWORK_BRIDGE]
    bridge_asm = r151_network_asm
    bridge_table = "$LN424@TranslateN@3"
    bridge_case = check_table(
        pe, bridge["address"], bridge_asm, bridge_table, 42 - 1, "$LN48@TranslateN@3"
    )
    bridge_words = check_code_words(
        pe, bridge["address"], bridge_asm,
        [0x9BC, 0x9C0, 0x9C4, 0x9C8, 0x9CC, 0x9D0, 0x9F0, 0x9F4, 0x9FC, 0xA00, 0xF78, 0xF7C]
    )
    bridge_queue_get = map_symbol(final_map, prefix=GUI_QUEUE_GET)
    bridge_queue_add = map_symbol(final_map, exact=GUI_QUEUE_ADD)
    bridge_get_call = check_call(pe, bridge["address"], 0x9F8, bridge_queue_get)
    bridge_add_call = check_call(pe, bridge["address"], 0xF80, bridge_queue_add)
    gui = mapped[GUI_UPDATE]
    gui_words = check_code_words(pe, gui["address"], r151_gui_asm, [0x4B8, 0x4BC, 0x4C0])
    gui_dispatch_call = check_call(pe, gui["address"], 0x4C4, mapped[REC])

    # Verify ScreenLoading reads both cached flags before event 137 and sends the
    # native destination names through CgsGui::State::SendStateEvent.
    screen = mapped[SCREEN_UPDATE]
    latch_offsets = [
        0x0A4, 0x0A8, 0x0AC, 0x0B0, 0x0B4, 0x0B8, 0x0BC,
        0x0C0, 0x0C4, 0x0C8, 0x0CC, 0x0D0, 0x144, 0x148,
        0x14C, 0x154, 0x158, 0x15C, 0x160, 0x170, 0x174,
        0x178, 0x17C, 0x180, 0x184, 0x18C,
    ]
    screen_words = check_code_words(pe, screen["address"], r152_screen_asm, latch_offsets)
    apply_row = map_symbol(final_map, exact=APPLY_PROFILE)
    send_row = map_symbol(final_map, exact=SEND_STATE)
    apply_call = check_call(pe, screen["address"], 0x150, apply_row)
    send_invite_call = check_call(pe, screen["address"], 0x168, send_row)
    send_join_or_game_call = check_call(pe, screen["address"], 0x188, send_row)
    route_strings = {}
    for name, reg, lis_off, addi_off, artist_addr in (
        ("ENTER_GAME", 24, 0x124, 0x128, "0x8206A054"),
        ("TO_GAME_ROOM", 23, 0x12C, 0x130, "0x82062164"),
        ("TO_ON_PLAY", 22, 0x134, 0x138, "0x8206363C"),
    ):
        lis_word = pe_word(pe, screen["address"] + lis_off)
        addi_word = pe_word(pe, screen["address"] + addi_off)
        assert lis_word >> 26 == 15 and ((lis_word >> 21) & 31) == 11
        assert addi_word >> 26 == 14 and ((addi_word >> 21) & 31) == reg and ((addi_word >> 16) & 31) == 11
        lo = addi_word & 0xFFFF
        if lo & 0x8000:
            lo -= 0x10000
        address = ((((lis_word & 0xFFFF) << 16) + lo) & 0xFFFFFFFF)
        assert pe_bytes(pe, address, len(name) + 1) == name.encode("ascii") + b"\0"
        route_strings[name] = {"image_address": hex(address), "artist_address": artist_addr}

    # Confirm route argument registers select those exact strings in the compiled control flow.
    assert pe_word(pe, screen["address"] + 0x160) == r152_screen_asm["words"][0x160]
    assert pe_word(pe, screen["address"] + 0x174) == r152_screen_asm["words"][0x174]
    assert pe_word(pe, screen["address"] + 0x180) == r152_screen_asm["words"][0x180]

    # R151 baseline proof for the previous false latches in the actual linked image.
    old_screen_words = check_code_words(base_pe, base_screen["address"], r151_screen_asm, [0x0B8, 0x0C8])
    assert r151_screen_asm["words"][0x0B8] == 0x9B930038
    assert r151_screen_asm["words"][0x0C8] == 0x9B930039
    assert r152_screen_asm["words"][0x0B8] == 0x894B4B4F
    assert r152_screen_asm["words"][0x0CC] == 0x896B4B4E

    report = {
        "schema": "r152-startup-routing-linked-v1",
        "frozen_composition": {"head": FROZEN_HEAD, "tree": FROZEN_TREE,
                               "manifest_sha256": MANIFEST_SHA256,
                               "targeted_object_binding_sha256": BINDING_SHA256,
                               "compile_report_sha256": COMPILE_REPORT_SHA256},
        "linked_output": {
            "head": args.expected_head, "tree": args.expected_tree,
            "seal_sha256": sha(seal_path), "selected_objects_sha256": seal["selected_objects_sha256"],
            "xex_sha256": xex_sha, "pe_sha256": pe_sha, "map_sha256": map_sha,
            "machine": "PPCBE", "image_base": "0x82000000",
        },
        "r151_baseline": {
            "head": BASE_HEAD, "tree": BASE_TREE,
            "seal_sha256": BASE_SEAL_SHA256, "xex_sha256": BASE_XEX_SHA256,
            "pe_sha256": BASE_PE_SHA256, "map_sha256": BASE_MAP_SHA256,
            "rec_event_cases": base_rec_cases,
            "screen_loading_false_latches": old_screen_words,
        },
        "providers": providers,
        "native_rec_event": {
            "artist_jump_table": "0x8250DE40; index = event id - 4",
            "compiled_jump_table": "RecEvent switch table at function offset 0xAC; index = event id - 14",
            "cases": rec_cases,
        },
        "network_to_gui_dispatch": {
            "network_event": 42,
            "gui_event": 109,
            "artist_gui109_producer": "0x823D0B10",
            "network_switch_case": bridge_case,
            "network_event42_words": bridge_words,
            "gui_queue_get_call": bridge_get_call,
            "gui_queue_add_call": bridge_add_call,
            "gui_module_update_words": gui_words,
            "gui_module_calls_rec_event": gui_dispatch_call,
        },
        "screen_loading": {
            "flag_reads_before_event137": [
                {"cache_field": "mbPerformingInvite", "offset": "0x4B4F", "code_offset": "0xB8"},
                {"cache_field": "mbIsStartingGameDueToPlayerJoin", "offset": "0x4B4E", "code_offset": "0xCC"},
            ],
            "event137_code_offset": "0x144",
            "code_words": screen_words,
            "apply_profile_call": apply_call,
            "send_state_calls": [send_invite_call, send_join_or_game_call],
            "routes": route_strings,
            "artist_update": "0x824DA770",
            "artist_send_state_event": "0x82848550",
        },
        "scope": "Static linked-image proof for the five incoming GuiCache events, event 42 to GUI 109 dispatch, and ScreenLoading routing. No runtime claim.",
    }
    out = args.output.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(str(out), sha(out))


if __name__ == "__main__":
    main()

