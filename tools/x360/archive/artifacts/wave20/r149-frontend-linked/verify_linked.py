"""Bind R149 frontend PPC witnesses to the sealed native link."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import runpy
import struct
import subprocess

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
LINK = WAVE / "r149-link"
HEAD = "28769133a5e88f5cc530003d41b059022fb557f6"
BASE = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
PRIVATE = "33d20058fb914df3c56cf5fe507029ba64b174b4"
TREE = "bd200df2bf44260dee9d59f4cef5dd7dceb63c46"
MANIFEST_SHA = "5f11112152fddbb6291be08c4635e622ea3b3dd08acda4723f1329907e6ce9e3"
FROZEN_SHA = "10692971f27af2d6601506e2954f96e109680e5b9847e5b1cede03074fd615d1"
PRIOR_SEAL_SHA = "1e0641bd74316d8270d32ea43e3c407570c062c84cbbeee40d359b73a3bb0b17"
PROVIDER_SHA = "c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833"
FROZEN = WAVE / "r149-composed-proof/R149_COMPOSED_PPC.json"
MANIFEST = WAVE / "r149-batch/MANIFEST.json"
READER = WAVE / "r148-boot-presentation-linked/verify_linked.py"
CANON = WAVE / "r143-linked-proof/verify_linked.py"
READER_SHA = "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba"
CANON_SHA = "bcccaf6d775764472f93c662b2dbb19caeceabdca7c04ecbcfa958ad957b81b1"
PRIVATE_TEXT = {
    "boot": ("??$PostCommand16@",),
    "title": ("??$PostCommand16@",),
    "add": ("?RemoveStringPointerByHash@",),
    "remove": ("?RemoveStringPointerByHash@",),
}
PRIVATE_DATA = {
    "boot": ("KF_ACCEPT_DWELL",),
    "play": ("?KF_STREAM_STOP_FADE_OUT@", "__real@3dcccccd"),
}
QUEUE = "?AddEvent@?$VariableEventQueue@$0BAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z"
HASH = "?HashString@ID@CgsResource@@SA_KPBE@Z"


def sha(data):
    return sha256(data).hexdigest()


def pin(path):
    return {"path": str(path), "sha256": sha(path.read_bytes())}


def pinned(path, digest):
    data = Path(path).read_bytes()
    assert sha(data) == digest, path
    return data


def load(path, digest=None):
    return json.loads(pinned(path, digest) if digest else Path(path).read_bytes())


def from_pin(row):
    return load(row["path"], row["sha256"])


def norm(path):
    return Path(path).as_posix().lower()


def git(root, *args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(root), *args])


def sealed(head, manifest, frozen):
    result = load(LINK / "result.json")
    image = load(LINK / "image-verification.json")
    invocation = load(LINK / "invocation.json")
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] is True
    assert image["head"] == head
    seal_path = LINK / "seal.json"
    assert Path(invocation["sealed"]) == seal_path
    seal = load(seal_path, invocation["sealed_sha256"])
    assert seal["schema"] == "r149-native-link-seal-v1" and seal["head"] == head
    release_path = LINK / "ROOT_RELEASE.json"
    assert Path(invocation["release"]) == release_path
    assert load(release_path, invocation["release_sha256"]) == {
        "action": "one strict R149 native link", "sealed_sha256": invocation["sealed_sha256"]
    }
    for invocation_key, seal_key in (("objects", "selected"), ("excluded", "excluded"), ("sources", "sources"),
                                     ("libraries", "libraries"), ("tool_files", "tool_files")):
        assert invocation[invocation_key] == seal[seal_key], invocation_key
    rsp_path = LINK / "r149-strict.rsp"
    assert invocation["command"][1:] == ["@" + str(rsp_path)]
    args = pinned(rsp_path, invocation["rsp_sha256"]).decode().splitlines()
    assert [arg for arg in args if arg.lower().endswith('.obj"')] == [
        f'"{row["object"]}"' for row in seal["selected"]
    ]
    assert pin(LINK / "link.log")["sha256"] == result["log_sha256"]
    diagnostics = load(LINK / "diagnostic-parsed.json")
    assert diagnostics["unresolved_messages"] == diagnostics["unique_symbols"] == 0
    assert not diagnostics["duplicate_messages"]

    assert (len(seal["selected"]), len(seal["excluded"])) == (3589, 121)
    assert len({norm(row["source"]) for row in seal["selected"] + seal["excluded"]}) == 3710
    ordering = sorted((norm(row["source"]), norm(row["object"]), row["sha256"]) for row in seal["selected"])
    digest = sha(json.dumps(ordering, separators=(",", ":")).encode())
    assert digest == seal["selected_objects_sha256"]
    boundary = from_pin(seal["boundary"])
    assert (boundary["schema"], boundary["head"], boundary["selected_objects_sha256"]) == (
        "r149-link-boundary-v1", head, digest
    )
    assert boundary["no_excluded_cohort_outside_references"] and not boundary["errors"]
    prior_path = WAVE / "r148-link/seal.json"
    selection_path = WAVE / "r147-xmedia-provider-proof/PROVIDER_SELECTION.json"
    assert seal["prior_successful_seal"] == {"path": str(prior_path), "sha256": PRIOR_SEAL_SHA}
    assert seal["provider_selection"] == {"path": str(selection_path), "sha256": PROVIDER_SHA}
    prior = load(prior_path, PRIOR_SEAL_SHA)
    cohort = load(selection_path, PROVIDER_SHA)["sources"]
    assert prior["head"] == BASE and seal["provider_cohort"] == prior["provider_cohort"] == cohort
    assert len(cohort) == 15
    for key in ("selected", "excluded"):
        assert {row["source"] for row in seal[key]} == {row["source"] for row in prior[key]}
    excluded = {row["source"]: row for row in seal["excluded"]}
    for provider in cohort:
        row = excluded[provider["source"]]
        assert (row["object"], row["sha256"]) == (provider["object"], provider["sha256"])
        pinned(row["object"], row["sha256"])

    root = Path(seal["source_root"])
    for revision in (head, PRIVATE):
        assert git(root, "rev-parse", f"{revision}^{{tree}}", f"{revision}^").decode().splitlines() == [TREE, BASE]
    binding_path = WAVE / "r149-intake/commit-binding.json"
    binding = next(row for row in seal["provenance"] if Path(row["path"]) == binding_path)
    commit = from_pin(binding)
    assert (commit["head"], commit["base"], commit["manifest_sha256"]) == (head, BASE, MANIFEST_SHA)
    diff = git(root, "diff", "--name-only", BASE, head).decode().splitlines()
    assert sorted(diff) == sorted(commit["paths"]) == sorted(row["path"] for row in manifest["final_paths"])
    inventory = from_pin(seal["source_inventory"])
    assert inventory["head"] == head and inventory["files"] == seal["sources"]
    report = from_pin(seal["report"])
    assert report["passed"] == len(report["results"]) == 3710 and report["failed"] == 0
    all_selected = {row["source"]: row for row in seal["selected"]}
    all_report = {row["source"]: row for row in report["results"]}
    source_hashes = {norm(row["path"]): row["sha256"] for row in seal["sources"]}
    selected = {}
    for source, proof in frozen["artifacts"].items():
        row, built = all_selected[source], all_report[source]
        assert norm(row["object"]) == norm(built["object"]) and built["passed"] and built["machine"] == 0x1f2
        blob_id = git(root, "rev-parse", f"{head}:{source}").decode().strip()
        assert blob_id == proof["source_blob"], source
        work_file = root / source
        work_data = pinned(work_file, source_hashes[norm(work_file)])
        git_data = git(root, "show", f"{head}:{source}")
        assert work_data.replace(b"\r\n", b"\n") == git_data.replace(b"\r\n", b"\n"), source
        selected[source] = row
    provenance = {
        "seal": pin(seal_path), "result": pin(LINK / "result.json"), "image_verification": pin(LINK / "image-verification.json"),
        "release": pin(release_path), "response_file": pin(rsp_path), "commit_binding": binding,
        "boundary": seal["boundary"], "source_inventory": seal["source_inventory"],
        "full_compile_report": seal["report"], "prior_seal": seal["prior_successful_seal"],
        "provider_selection": seal["provider_selection"], "selected_objects_sha256": digest,
        "selection_counts": [3589, 121],
    }
    return result, image, selected, provenance


def body_proof(read, entries, coff, method, symbol, code, refs, address, helpers, readers):
    u32, sign = readers["u32"], readers["sign"]
    linked = read(address, len(code), executable=True)
    expected = bytearray(code)
    bindings = {}
    private_targets = {}
    for offset, (kind, target) in sorted(refs.items()):
        word = u32(code, offset)
        location = address + offset
        source = "map"
        if target.section == symbol.section:
            assert target.name.startswith("$LN"), target
            dest = address + target.value
            source = "local jump-table label"
        elif target.name in entries:
            dest = readers["map_addr"](entries, target.name)
        elif kind == 6:
            assert target.section > 0 and target.name.startswith(PRIVATE_TEXT.get(method, ())), (method, target)
            op = u32(linked, offset)
            assert op & 0xfc000002 == 0x48000000
            dest = location + sign(op & 0x03fffffc, 26)
            source = "private helper body checked"
            helper_key = (target.name, dest)
            if helper_key not in helpers:
                child, child_code, child_refs = coff.function(target.name)
                helpers[helper_key] = body_proof(
                    read, entries, coff, "private helper", child, child_code, child_refs, dest, helpers, readers
                )
        else:
            assert kind in (0x10, 0x11) and target.section > 0, (method, offset, target)
            assert target.name.startswith(("??_C@", *PRIVATE_DATA.get(method, ()))), (method, target)
            if target not in private_targets:
                bases = set()
                for low, item in refs.items():
                    if item != (0x11, target):
                        continue
                    reg = (u32(code, low) >> 16) & 31
                    high = max(at for at, pair in refs.items()
                               if at < low and pair == (0x10, target) and ((u32(code, at) >> 21) & 31) == reg)
                    op = u32(linked, high)
                    assert op >> 26 == 15 and (op >> 16) & 31 == 0
                    bases.add((((op & 0xffff) << 16) + sign(u32(linked, low) & 0xffff, 16)) & 0xffffffff)
                [dest] = bases
                _, data, _, _ = coff.section(target.section)
                if target.name.startswith("??_C@"):
                    size = data.index(b"\0", target.value) - target.value + 1
                else:
                    size = 4
                assert read(dest, size) == coff.literal(target, size)
                private_targets[target] = dest
            dest = private_targets[target]
            source = "private readonly payload checked"
        if kind == 6:
            assert word & 0xfc000002 == 0x48000000
            word = word & 0xfc000003 | ((dest - location) & 0x03fffffc)
            assert location + sign(word & 0x03fffffc, 26) == dest
        elif kind in (0x10, 0x11):
            assert word & 0xffff == 0, (method, offset, target)
            word |= (((dest + 0x8000) >> 16) if kind == 0x10 else dest) & 0xffff
        else:
            assert kind == 2 and word == 0, (method, offset, target)
            word = dest
        assert u32(linked, offset) == word, (method, hex(offset), target.name, hex(word), hex(u32(linked, offset)))
        struct.pack_into(">I", expected, offset, word)
        bindings[hex(offset)] = {"symbol": target.name, "target": hex(dest), "kind": hex(kind), "basis": source}
    assert linked == expected, (method, hex(address), symbol.name)
    return {"symbol": symbol.name, "address": hex(address), "bytes": len(linked), "sha256": sha(linked), "bindings": bindings}


def verify_functions(read, entries, selected, frozen, readers, canon):
    checks, helpers, object_pins = {}, {}, {}
    read_functions = canon["coff_functions"]
    for source, proof in frozen["artifacts"].items():
        selected_row = selected[source]
        selected_path = Path(selected_row["object"])
        current = pinned(selected_path, selected_row["sha256"])
        old = pinned(proof["object"]["path"], proof["object"]["sha256"])
        assert current[:2] == old[:2] == b"\xf2\x01"
        object_pins[source] = {
            "source_blob": proof["source_blob"], "selected_object": pin(selected_path),
            "composed_object": proof["object"],
        }
        current_coff = readers["Coff"](current)
        for method, witness in proof["compared_functions"].items():
            name = witness["symbol"]
            read_functions.__globals__["PREFIX"][method] = name
            old_name, old_code, old_refs = read_functions(old, [method])[method]
            now_name, now_code, now_refs = read_functions(current, [method])[method]
            old_bindings = canon["canonical_bindings"](old_refs)
            assert (old_name, old_code) == (now_name, now_code)
            assert sha(old_code) == witness["body_sha256"] and len(old_code) == witness["bytes"]
            assert sha(json.dumps(old_bindings, sort_keys=True).encode()) == witness["relocations_sha256"]
            assert old_bindings == canon["canonical_bindings"](now_refs), (source, method)
            symbol, code, refs = current_coff.function(name)
            if method == "queue":
                assert name == QUEUE
                owners = {owner for _, owner in entries[name]}
                assert len(owners) == 1
                owner = owners.pop()
                [mapped] = [row for row in selected.values() if Path(row["object"]).name == owner]
                assert Path(mapped["source"]).stem in {"CgsGuiViewModule", "VariableEventQueue_256_16"}, mapped
            else:
                owner = selected_path.name
            address = readers["map_addr"](entries, name, owner)
            check = body_proof(read, entries, current_coff, method, symbol, code, refs, address, helpers, readers)
            check.update({"map_owner": owner, "checked_coff": str(selected_path), "source": source})
            key = method if method != "queue" else f"queue:{Path(source).stem}"
            assert key not in checks
            checks[key] = check
    assert len(checks) == 16 and len(object_pins) == 15
    queues = [row for key, row in checks.items() if key.startswith("queue:")]
    assert len(queues) == 2
    assert len({(row["address"], row["sha256"], row["map_owner"]) for row in queues}) == 1
    return checks, list(helpers.values()), object_pins


def verify_witnesses(read, checks, frozen):
    def w(method, offset, words):
        addr = int(checks[method]["address"], 16) + offset
        data = bytes.fromhex(words)
        assert read(addr, len(data), executable=True) == data, (method, hex(offset))

    def at(method, offset, prefix):
        row = checks[method]["bindings"][hex(offset)]
        assert row["symbol"].startswith(prefix), (method, hex(offset), row)
        return row

    def literal(method, hi, lo, payload):
        high = at(method, hi, "")
        low = at(method, lo, "")
        assert (high["symbol"], high["target"]) == (low["symbol"], low["target"])
        assert read(int(low["target"], 16), len(payload)) == payload
        return low

    w("view", 0x1c, "2f06000a 419a00b0 2f06000b 419a0084 2f06000c 419a0028")
    w("view", 0x58, "817f0000 387e7c0c 2f0b0000 419a0038")
    w("view", 0x80, "3c7e0001 38c00004 38a0000d 38810050 3863ddc0")
    w("view", 0xb4, "38a0029f")
    language_calls = {
        hex(offset): at("view", offset, symbol) for offset, symbol in (
            (0x70, "?RemoveStringPointer@"), (0x7c, "?SetStringRemoved@"), (0x94, QUEUE),
            (0xa4, "?AddStringPointer@"), (0xac, "?BeginAssert@"), (0xc4, "?FireAssert@"),
            (0xc8, "?EndAssert@"), (0xec, "?FormatMinutesAndSecondsString@"),
            (0x10c, "?FormatSecondsString@"), (0x14c, "?UpdateFlashComponent@"),
        )
    }
    assert language_calls["0x94"]["target"] == checks["queue:CgsGuiViewModule"]["address"]
    assert at("dispatch", 0x190, "?ProcessIncomingLanguageEvent@")["target"] == checks["view"]["address"]
    w("payload", 0x40, "907f0000")

    w("hash", 0x48, "38600000")
    w("hash", 0x184, "7d4b50f8 79630020")
    hash_calls = {f"{key}:{offset:#x}": at(key, offset, HASH) for key, offset in
                  (("sku", 0x70), ("sku", 0xd4), ("title", 0xf8), ("boot", 0x824))}
    assert all(row["target"] == checks["hash"]["address"] for row in hash_calls.values())
    w("sku", 0x7c, "796b0020 7fa3eb78 f9610070")
    w("sku", 0xe0, "796b0020 7fa3eb78 f9610090")

    menu = {}
    for key, hi, lo, hash_at, post_at in (("title", 0x60, 0x64, 0x68, 0x84), ("boot", 0x54c, 0x550, 0x554, 0x56c)):
        menu[key] = {
            "literal": literal(key, hi, lo, b"Guns_And_Roses\0"),
            "make_hash": at(key, hash_at, "?MakeHash@Name@Playback@"),
            "post": at(key, post_at, "??$OutputGuiEvent@VGuiEventPlayMusicOnMenuStream@"),
        }
    menu["notify_lookup"] = at("notify", 0x33c, "?GetStreamFromVideoName@")
    w("notify", 0x340, "57cb063e 907f025c 9bdf0275 2b0b0000 9bbf0276")
    assert not any(row["symbol"].startswith("?MakeHash@Name@Playback@") for off, row in checks["notify"]["bindings"].items()
                   if 0x330 <= int(off, 16) < 0x358)

    w("play", 0x14, "a17e0014 2b0bfffe 409a00a0")
    w("play", 0x64, "3bfd0038 38610050 7fe4fb78")
    w("play", 0x8c, "38a00004 7fe4fb78 38610050")
    w("play", 0xac, "897e0019 997d02bc")
    w("play", 0x12c, "2b0bffff 91410090 409a003c")
    w("get_params", 0, "38630244 4e800020")
    w("find_voice", 0x34, "394a0088")
    presentation = {
        "stop_fade": literal("play", 0x60, 0x70, bytes.fromhex("3e800000")),
        "play_lag": literal("play", 0x88, 0x98, bytes.fromhex("3dcccccd")),
        "calls_in_order": [
            at("play", 0x78, "??0StreamStopRequest@"),
            at("play", 0x84, "?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@QAAXABUStreamStopRequest@"),
            at("play", 0x9c, "??0StreamRequest@"),
            at("play", 0xa8, "?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@QAAXABUStreamRequest@"),
        ],
        "resident_wave": literal("play", 0x140, 0x144, b"StereoWavVoiceSpec\0"),
        "resident_wave_calls": [at("play", 0x150, "?MakeHash@"), at("play", 0x1fc, "?Create@VoiceWrapper@"),
                                at("play", 0x218, "?Play@VoiceWrapper@")],
        "remaining_create_params_layout": frozen["presentation"]["remaining_create_params_layout"],
    }
    return {"language_calls": language_calls, "removal_event": {"id": 13, "size": 4},
            "resource_hash_calls": hash_calls, "menu_music": menu, "presentation": presentation}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--head", default=HEAD, choices=(HEAD,))
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    readers = runpy.run_path(str(READER))
    canon = runpy.run_path(str(CANON))
    assert pin(READER)["sha256"] == READER_SHA and pin(CANON)["sha256"] == CANON_SHA
    frozen = load(FROZEN, FROZEN_SHA)
    manifest = load(MANIFEST, MANIFEST_SHA)
    assert (frozen["state"], frozen["head"], frozen["tree"], frozen["base"]) == ("pass", PRIVATE, TREE, BASE)
    assert (manifest["candidate"], manifest["projected_tree"], manifest["base"]) == (PRIVATE, TREE, BASE)
    result, image, selected, provenance = sealed(args.head, manifest, frozen)
    readers["load_image"].__globals__["LINK"] = LINK
    read, entries, image_pins = readers["load_image"](result, image)
    checks, helpers, objects = verify_functions(read, entries, selected, frozen, readers, canon)
    witnesses = verify_witnesses(read, checks, frozen)
    receipt = {
        "state": "pass", "head": args.head, "tree": TREE, "base": BASE, "manifest": pin(MANIFEST),
        "frozen_composed": pin(FROZEN), "verifier": pin(Path(__file__)), "readers": [pin(READER), pin(CANON)],
        "provenance": provenance, "image": image_pins, "xex_sha256": image_pins["xex_sha256"],
        "objects": objects, "functions": checks,
        "private_helpers": helpers, "witnesses": witnesses,
        "scope": "Static PPC verification of the sealed PE and XEX. Runtime was not exercised.",
    }
    if args.write:
        out = HERE / "R149_FRONTEND_LINKED.json"
        with out.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
        print(pin(out))
    print(f"PASS {args.head}: {len(checks)} PPC body checks and {len(helpers)} private helper checks")


if __name__ == "__main__":
    main()
