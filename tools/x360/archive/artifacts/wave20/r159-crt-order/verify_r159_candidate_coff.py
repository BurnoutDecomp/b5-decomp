"""Verify the focused R159 initializer-order COFF and merge it with R157 evidence.

This reads one XDK COFF produced by the recorded /c invocation. It does not
compile, link, launch, or change any source.
"""
import copy
import hashlib
import json
from pathlib import Path
import runpy
import struct

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
PREFLIGHT = HERE / "R159_CRT_ORDER_PREFLIGHT.json"
COMPILE_RECEIPT = HERE / "R159_FOCUSED_COMPILE.json"
CANDIDATE_ROOT = Path(r"C:\Users\adri1\.codex\worktrees\bp360\debug_format_luna")
CANDIDATE_BASE = "49484358f743078512f861ab783ee194c998744b"
CANDIDATE_BASE_TREE = "7794ea26f68bd5170fcf9a64f06cb9f82304271a"
CANDIDATE_HEAD = "655e9d73490a49d4c4487e3852d869035d2cc5c1"
CANDIDATE_TREE = "dc10bfea0cabd8380bce54d410beefb23305799e"
CANDIDATE_BRANCH = "codex/r159-crt-init-order"
PRESERVED_BRANCH = "codex/experimental-x360-r157-native-batch"
PRESERVED_BRANCH_HEAD = "ffff6742d77340a053ef9f2420068e6ca0919900"
SOURCE = "src/GameSource/Sound/Module/BrnRootSoundModule.cpp"
ALLOCATORS = {
    "Rwac": "?gRwacTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
    "Logic": "?gLogicTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
    "Playback": "?gPlaybackTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
    "Csis": "?gCsisTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",
}
INITIALIZERS = {
    "Rwac": "??__EgRwacTestBedAlloc@@YAXXZ",
    "Logic": "??__EgLogicTestBedAlloc@@YAXXZ",
    "Playback": "??__EgPlaybackTestBedAlloc@@YAXXZ",
    "Csis": "??__EgCsisTestBedAlloc@@YAXXZ",
}
DEFAULT_SEND = "??__EK_DefaultSendName@Playback@CgsSound@@YAXXZ"
NULL_RESOURCE = "??__ENULLResourcePtr@CgsResource@@YAXXZ"
R157_VERIFY = HERE / "verify_r159_crt_order.py"
R155_VERIFIER = WAVE / "r155-audio-regression/verify_linked.py"


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def file_pin(path):
    path = Path(path)
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data), "sha256": sha_bytes(data)}


def make_helpers():
    r155 = runpy.run_path(str(R155_VERIFIER))
    r154 = runpy.run_path(str(r155["R154"]))
    r153 = runpy.run_path(str(r154["R153"]))
    r152 = runpy.run_path(str(r153["PRIOR_VERIFIER"]))
    inherited, voice, front, h, r151 = r152["helpers"]()
    return front, h


def coff_section(coff, wanted):
    max_section = max(sym.section for sym in coff.symbols.values() if sym.section > 0)
    matches = []
    for index in range(1, max_section + 1):
        try:
            name, data, start, count = coff.section(index)
        except Exception:
            continue
        if name == wanted:
            matches.append((index, data, start, count))
    assert len(matches) == 1, (wanted, len(matches))
    return matches[0]


def crt_relocations(coff):
    index, data, start, count = coff_section(coff, b".CRT$XCU")
    rows = [
        {"offset": offset, "symbol": coff.symbols[symbol_index].name, "kind": kind}
        for offset, symbol_index, kind in coff.section_relocations(start, count)
        if kind == 2
    ]
    rows.sort(key=lambda row: row["offset"])
    return index, data, rows


def allocator_bss_layout(coff):
    offsets = {}
    section_indices = set()
    for label, symbol_name in ALLOCATORS.items():
        symbol = next(sym for sym in coff.symbols.values() if sym.name == symbol_name)
        section_name, data, raw_start, reloc_count = coff.section(symbol.section)
        assert section_name == b".bss"
        section_indices.add(symbol.section)
        offsets[label] = symbol.value
    assert len(section_indices) == 1
    index = next(iter(section_indices))
    section_name, section_data, raw_start, reloc_count = coff.section(index)
    assert section_name == b".bss"
    return {
        "section_index": index,
        "section_size": len(section_data),
        "raw_file_pointer": raw_start,
        "relative_offsets": offsets,
        "storage_order": [name for name, _ in sorted(offsets.items(), key=lambda row: row[1])],
    }


def constructor_literals(coff, expected_source_literals):
    rows = {}
    for label, initializer in INITIALIZERS.items():
        symbol = next(sym for sym in coff.symbols.values() if sym.name == initializer)
        section_name, data, start, count = coff.section(symbol.section)
        assert section_name == b".text$yc" and symbol.value == 0
        relocs = {}
        for offset, symbol_index, kind in coff.section_relocations(start, count):
            if kind != 0x12:
                relocs[offset] = (kind, coff.symbols[symbol_index])
        literal_kind, literal_symbol = relocs[0x0C]
        global_kind, global_symbol = relocs[0x18]
        assert literal_kind == 0x10 and global_kind == 0x10
        assert global_symbol.name == ALLOCATORS[label]
        literal = coff.literal(literal_symbol, len(expected_source_literals[label]) + 1)
        assert literal == expected_source_literals[label].encode("ascii") + b"\0"
        rows[label] = {
            "initializer_symbol": symbol.name,
            "constructor_literal": literal[:-1].decode("ascii"),
            "allocator_symbol": global_symbol.name,
            "code_sha256": sha_bytes(data[symbol.value:symbol.value + 64]),
        }
    return rows


def main():
    front, h = make_helpers()
    preflight = json.loads(PREFLIGHT.read_text(encoding="utf-8"))
    assert preflight["state"] == "source_order_candidate_substantiated_layout_gate_pending"

    head = h.git(CANDIDATE_ROOT, "rev-parse", "HEAD").decode().strip()
    tree = h.git(CANDIDATE_ROOT, "rev-parse", "HEAD^{tree}").decode().strip()
    parent = h.git(CANDIDATE_ROOT, "rev-parse", "HEAD^").decode().strip()
    branch = h.git(CANDIDATE_ROOT, "branch", "--show-current").decode().strip()
    old_branch = h.git(CANDIDATE_ROOT, "rev-parse", PRESERVED_BRANCH).decode().strip()
    assert (head, tree, parent, branch, old_branch) == (
        CANDIDATE_HEAD, CANDIDATE_TREE, CANDIDATE_BASE, CANDIDATE_BRANCH, PRESERVED_BRANCH_HEAD
    )
    changed = h.git(
        CANDIDATE_ROOT, "diff", "--name-only", CANDIDATE_BASE, CANDIDATE_HEAD
    ).decode().splitlines()
    status = h.git(CANDIDATE_ROOT, "status", "--porcelain=v1").decode().splitlines()
    numstat = h.git(
        CANDIDATE_ROOT, "diff", "--numstat", CANDIDATE_BASE, CANDIDATE_HEAD, "--", SOURCE
    ).decode().strip().split("\t")
    assert changed == [SOURCE] and status == []
    assert numstat == ["3", "3", SOURCE]
    base_blob = h.git(CANDIDATE_ROOT, "rev-parse", f"{CANDIDATE_BASE}:{SOURCE}").decode().strip()
    commit_blob = h.git(CANDIDATE_ROOT, "rev-parse", f"{CANDIDATE_HEAD}:{SOURCE}").decode().strip()
    assert base_blob == preflight["r157"]["source"]["git_blob"] and commit_blob != base_blob

    candidate_source = CANDIDATE_ROOT / SOURCE
    source_bytes = candidate_source.read_bytes()
    source_text = source_bytes.decode(errors="replace")
    expected_declarations = [
        'CgsSound::TestBed::Allocator gRwacTestBedAlloc("RWAC", 0);',
        'CgsSound::TestBed::Allocator gLogicTestBedAlloc("Logic", 0);',
        'CgsSound::TestBed::Allocator gPlaybackTestBedAlloc("Playback", 0);',
        'CgsSound::TestBed::Allocator gCsisTestBedAlloc("Csis", 0);',
    ]
    extracted = [
        line.strip() for line in source_text.splitlines()
        if "CgsSound::TestBed::Allocator g" in line and "TestBedAlloc(" in line
    ]
    assert extracted == expected_declarations
    assert source_text.index('const Name K_DefaultSendName("Send01");') < \
        source_text.index(expected_declarations[0])

    compile_receipt = json.loads(COMPILE_RECEIPT.read_text(encoding="utf-8"))
    assert compile_receipt["schema"] == "r159-focused-xdk-compile-v1"
    assert compile_receipt["returncode"] == 0
    assert compile_receipt["target"] == "xbox360-ppc-be"
    assert compile_receipt["machine"] == "0x1f2"
    assert compile_receipt["branch"] == branch
    assert compile_receipt["base"] == CANDIDATE_BASE
    assert compile_receipt["tree"] == CANDIDATE_BASE_TREE
    assert Path(compile_receipt["source"]["path"]) == candidate_source
    assert compile_receipt["source"]["sha256"] == sha_bytes(source_bytes)
    committed_source = h.git(CANDIDATE_ROOT, "show", f"{CANDIDATE_HEAD}:{SOURCE}")
    assert source_bytes.replace(b"\r\n", b"\n") == committed_source.replace(b"\r\n", b"\n")
    compile_args = compile_receipt["arguments"]
    assert "/c" in compile_args and "/FAcs" in compile_args
    assert compile_args[-1] == str(candidate_source)
    assert all(not arg.lower().startswith("/link") for arg in compile_args)
    object_pin = compile_receipt["outputs"]["object"]
    candidate_object_path = Path(object_pin["path"])
    candidate_object_bytes = candidate_object_path.read_bytes()
    assert sha_bytes(candidate_object_bytes) == object_pin["sha256"]
    assert len(candidate_object_bytes) == object_pin["bytes"]
    assert struct.unpack_from("<H", candidate_object_bytes)[0] == 0x1f2

    r157_obj = preflight["r157"]["source"]["selected_object"]
    baseline_coff = h.Coff(front["pinned"](r157_obj["object"], r157_obj["sha256"]))
    candidate_coff = h.Coff(candidate_object_bytes)
    baseline_crt_index, baseline_crt_data, baseline_crt = crt_relocations(baseline_coff)
    candidate_crt_index, candidate_crt_data, candidate_crt = crt_relocations(candidate_coff)
    baseline_order = [row["symbol"] for row in baseline_crt]
    candidate_order = [row["symbol"] for row in candidate_crt]
    expected_baseline = [
        NULL_RESOURCE, DEFAULT_SEND,
        INITIALIZERS["Rwac"], INITIALIZERS["Csis"],
        INITIALIZERS["Playback"], INITIALIZERS["Logic"],
    ]
    expected_candidate = [
        NULL_RESOURCE, DEFAULT_SEND,
        INITIALIZERS["Rwac"], INITIALIZERS["Logic"],
        INITIALIZERS["Playback"], INITIALIZERS["Csis"],
    ]
    assert baseline_order == expected_baseline
    assert candidate_order == expected_candidate
    assert len(baseline_crt_data) == len(candidate_crt_data) == 24
    assert [row["offset"] for row in baseline_crt] == \
        [row["offset"] for row in candidate_crt] == [0, 4, 8, 12, 16, 20]

    baseline_bss = allocator_bss_layout(baseline_coff)
    candidate_bss = allocator_bss_layout(candidate_coff)
    assert baseline_bss["section_size"] == candidate_bss["section_size"]
    assert baseline_bss["raw_file_pointer"] == candidate_bss["raw_file_pointer"] == 0
    assert baseline_bss["relative_offsets"] == candidate_bss["relative_offsets"]
    assert baseline_bss["storage_order"] == candidate_bss["storage_order"] == [
        "Rwac", "Logic", "Csis", "Playback"
    ]

    baseline_literals = {
        label: row["constructor_name"]
        for label, row in preflight["r157"]["coff"]["constructor_names"].items()
    }
    candidate_literals = constructor_literals(candidate_coff, {
        "Rwac": "RWAC", "Logic": "Logic", "Playback": "Playback", "Csis": "Csis"
    })
    assert baseline_literals == {
        "Rwac": "Rwac", "Logic": "Logic", "Playback": "Playback", "Csis": "Csis"
    }
    assert {label: row["constructor_literal"] for label, row in candidate_literals.items()} == {
        "Rwac": "RWAC", "Logic": "Logic", "Playback": "Playback", "Csis": "Csis"
    }
    assert {label: row["allocator_symbol"] for label, row in candidate_literals.items()} == \
        ALLOCATORS

    compile_pin = file_pin(COMPILE_RECEIPT)
    final = copy.deepcopy(preflight)
    final["state"] = "focused_candidate_coff_pass_link_pending"
    final["scope"] = (
        "Static ARTIST/R157 evidence plus one focused candidate XDK COFF compile. "
        "Final candidate link addresses and runtime remain pending."
    )
    final["focused_compile"] = {
        "receipt": compile_pin,
        "compiler": compile_receipt["compiler"],
        "target": compile_receipt["target"],
        "machine": compile_receipt["machine"],
        "returncode": compile_receipt["returncode"],
        "command_sha256": compile_receipt["command_sha256"],
        "arguments": compile_args,
        "source": compile_receipt["source"],
        "outputs": compile_receipt["outputs"],
    }
    final["candidate"] = {
        "branch": branch,
        "base": CANDIDATE_BASE,
        "base_tree": CANDIDATE_BASE_TREE,
        "commit": head,
        "tree": tree,
        "parent": parent,
        "preserved_existing_branch": {
            "name": PRESERVED_BRANCH,
            "head": old_branch,
        },
        "working_source": file_pin(candidate_source),
        "committed_source_blob": commit_blob,
        "working_tree_clean": status == [],
        "source_changes_from_base": changed,
        "numstat": {
            "added": int(numstat[0]),
            "deleted": int(numstat[1]),
            "path": numstat[2],
        },
        "source_declarations": expected_declarations,
        "coff": {
            "object": object_pin,
            "machine": hex(struct.unpack_from("<H", candidate_object_bytes)[0]),
            "crt_section_index": candidate_crt_index,
            "crt_section_size": len(candidate_crt_data),
            "crt_initializers": candidate_crt,
            "allocator_bss": candidate_bss,
            "allocator_bss_offsets_match_r157": True,
            "constructor_names": candidate_literals,
        },
        "final_link_address_layout": "pending link",
    }
    final["comparison"]["r157_source_order"] = [
        "Rwac", "Csis", "Playback", "Logic"
    ]
    final["comparison"]["candidate_coff_order"] = [
        "Rwac", "Logic", "Playback", "Csis"
    ]
    final["comparison"]["r157_source_rwac_literal"] = "Rwac"
    final["comparison"]["candidate_rwac_literal"] = "RWAC"
    final["comparison"]["artist_rwac_literal"] = "RWAC"
    final["comparison"]["r157_rwac_name_case_delta"] = True
    final["comparison"]["candidate_rwac_name_case_delta"] = False
    final["comparison"]["allocator_bss_relative_offsets_match"] = True
    final["comparison"]["names_and_types_unchanged_by_candidate"] = (
        "The four global symbols and Allocator type remain. The three non-Rwac "
        "constructor literals and all null backing arguments remain; Rwac literal "
        "case is corrected to ARTIST RWAC."
    )
    final["startup_consequences"]["rwac_diagnostic_name"] = (
        "The candidate changes the stored Rwac name from Rwac to ARTIST RWAC; "
        "other allocator names stay unchanged. The order also changes the atexit "
        "registration and reverse teardown sequence."
    )
    final["abi_verdict"] = {
        "source_level": (
            "The declaration move produces the ARTIST allocator initializer order "
            "while preserving each global symbol and Allocator type."
        ),
        "candidate_coff_storage": (
            "The focused candidate COFF keeps the same .bss section size and all "
            "four allocator global relative offsets as the R157 object."
        ),
        "final_link_addresses": (
            "Pending link. The candidate was not linked, so final absolute global "
            "addresses and final PE CRT slots are not claimed."
        ),
        "status": "object-level storage layout preserved; final linked address layout pending",
    }
    final["limits"] = [
        "One focused XDK compile produced PPC COFF only; no full link was run.",
        "Candidate COFF has the ARTIST CRT initializer order and unchanged allocator .bss offsets.",
        "Final candidate PE/map addresses and runtime behavior remain unverified.",
    ]
    final["verifier"] = file_pin(Path(__file__))

    out = HERE / "R159_CRT_ORDER_EVIDENCE.json"
    markdown = f"""# R159 CRT initializer order

ARTIST's actual loaded-image initializer-pointer fragment is DefaultSendName, Rwac, Logic, Playback, Csis. The exact R157 linked PE instead runs DefaultSendName, Rwac, Csis, Playback, Logic.

A private branch commit {CANDIDATE_HEAD} (tree {CANDIDATE_TREE}) changes only BrnRootSoundModule.cpp over base {CANDIDATE_BASE}. Its single focused XDK compile passed for xbox360-ppc-be. The candidate COFF now orders the records DefaultSendName, Rwac, Logic, Playback, Csis. All four allocator .bss relative offsets and the section size match the R157 object; final linked absolute addresses remain pending a link.

The four global symbols and Allocator type are unchanged. The source correction also restores the raw-verified Rwac constructor label from Rwac to ARTIST RWAC; Logic, Playback, and Csis names and null backing arguments are unchanged. Reordering changes the atexit registration order and reverse teardown sequence. No full link, runtime, or gameplay run was performed.
"""
    evidence_path = HERE / "EVIDENCE.md"
    out.write_text(json.dumps(final, indent=2) + "\n", encoding="utf-8")
    evidence_path.write_text(markdown, encoding="utf-8")
    print(json.dumps({
        "receipt": file_pin(out),
        "evidence": file_pin(evidence_path),
        "candidate_coff": object_pin,
        "candidate_order": ["Rwac", "Logic", "Playback", "Csis"],
        "bss_offsets_match": True,
    }, indent=2))


if __name__ == "__main__":
    main()