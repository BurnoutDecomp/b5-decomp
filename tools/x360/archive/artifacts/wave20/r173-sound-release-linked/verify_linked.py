"""Bind the R173 manager Release slot and scene teardown to the strict linked image."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import sys

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
PARENT = "b44620b618ed440be3fcb3770002131be370f198"
PARENT_PINS = "cb675722d8d0a4cb857bb47c399d18c301d2734dcb84ea424e87843beb4db68d"
TOOLS = WAVE / "r168-director-carry/verify_carry.py"
assert hashlib.sha256(TOOLS.read_bytes()).hexdigest() == "258a2f24a9529bd0e6d2e577839891fa124966131fa46671f9e8d2ecef605cd5"
V = runpy.run_path(str(TOOLS))
checked, sha, git, bind, calls, load_build = (V[name] for name in ("checked", "sha", "git", "bind", "calls", "load_build"))
BASE = "src/GameShared/GameClasses/Sound/Logic/CgsStateManager.cpp"
PASSBY = "src/GameSource/Sound/Passby/BrnPassbyStateManager.cpp"
EMITTER = "src/GameSource/Sound/Module/LogicModule/BrnEmitterStateManager.cpp"
PLAYER = "src/GameSource/Sound/Vehicles/BrnPlayerVehicleStateManager.cpp"
STREAMING = "src/GameSource/Sound/Streaming/BrnStreamingStateManager.cpp"
SCENE = "src/SharedClasses/Sound/World/BrnSoundWorldScene.cpp"
DONOR = WAVE / "r173-sound-manager-release"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins-sha256", required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--tree", required=True)
    args = parser.parse_args()
    tools = V["H"]
    checked(tools["PE_TOOL"], "d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78")
    checked(tools["COFF_TOOL"], "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba")
    helper = runpy.run_path(str(tools["COFF_TOOL"]))
    Image = runpy.run_path(str(tools["PE_TOOL"]))["Image"]
    old_root, _, _, _, old = load_build("r172-link", PARENT_PINS, helper, Image)
    root, seal, pe, entries, new = load_build("r173-link", args.root_pins_sha256.lower(), helper, Image)
    assert old_root["head"] == PARENT
    assert (root["head"], root["tree"], root["parent"]) == (args.head, args.tree, PARENT)
    assert git("rev-parse", f"{args.head}^{{tree}}", f"{args.head}^").decode().split() == [args.tree, PARENT]

    donor = json.loads(checked(DONOR / "PPC_RELEASE_PROOF.json",
                               "bf27beb5ef6c9234ea3ed94e849efa3ddc77dda0b3172416ae786780fafb6004"))
    base = helper["Coff"](checked(WAVE / "r173-state-manager-vtable/CgsStateManager.obj",
                                  "d1e189adfcaf3dd2c51399367787960f845e5e2866a4c8c33ada5cfa2b94dfd1"))
    donors = json.loads(checked(DONOR / "compile-results.json",
                                "e9c15319d688f867bf8a63ef7e18e1f78a7b28af4d0baa5dbff69d9b631992c4"))
    frozen = {row["source"]: helper["Coff"](checked(row["object"], row["object_sha256"])) for row in donors["results"]}
    frozen[BASE] = base
    frozen[PASSBY] = helper["Coff"](checked(old[PASSBY]["object"], old[PASSBY]["sha256"]))
    by_object = {Path(row["object"]).name.lower(): source for source, row in new.items()}
    owners = {}

    def object_for(source):
        if source not in owners:
            row = new[source]
            [source_pin] = [record for record in seal["sources"]
                            if Path(record["path"]).as_posix().lower().endswith("/" + source.lower())]
            sealed = checked(source_pin["path"], source_pin["sha256"])
            committed = git("show", f"{args.head}:{source}")
            assert sealed.replace(b"\r\n", b"\n") == committed.replace(b"\r\n", b"\n")
            owners[source] = helper["Coff"](checked(row["object"], row["sha256"]))
        return owners[source]

    functions = {}
    for source, prefix, label in (
        (BASE, "?Release@StateManager@", "base"),
        (PASSBY, "?Release@PassbyStateManager", "passby"),
        (STREAMING, "?Release@StreamingStateManager", "streaming"),
        (PLAYER, "?Release@PlayerVehicleStateManager", "player"),
        (EMITTER, "?Release@EmitterStateManager", "emitter"),
        (SCENE, "?Release@SoundWorldScene", "scene_release"),
        (SCENE, "?Destruct@SoundWorldScene", "scene_destruct"),
    ):
        obj = object_for(source)
        va, linked, symbol, refs = bind(obj, frozen[source], pe, entries, new[source], prefix, helper)
        if prefix in donor["functions"]:
            evidence = donor["functions"][prefix]
            _, code, _ = obj.function(prefix)
            assert evidence["symbol"] == symbol.name
            assert list(struct.iter_unpack(">I", code)) == [(int(word, 16),) for word in evidence["instruction_words"]]
            assert {hex(at): dst.name for at, (kind, dst) in refs.items() if kind == 6} == evidence["relocations"]
        edges = [(at, dst.name) for at, (kind, dst) in refs.items() if kind == 6]
        functions[label] = {
            "source": source, "symbol": symbol.name, "va": hex(va), "body_sha256": sha(linked),
            "size": len(linked), "selected_object": new[source], "edges": calls(pe, entries, va, edges),
        }

    def slot(prefix, offset, expected):
        matches = [symbol for symbol in entries if symbol.startswith(prefix)]
        [vtable] = [symbol for symbol in matches if len(matches) == 1 or "6BStateManager@" in symbol]
        [(address, owner)] = entries[vtable]
        source = by_object[owner.lower()]
        obj = object_for(source)
        name = obj.vtable_slot(vtable, offset)
        assert name == expected
        target = pe.word(address + offset)
        assert target in {va for va, _ in entries[name]}
        return {"vtable": vtable, "vtable_va": hex(address), "owner": owner, "slot": hex(offset),
                "target": name, "target_va": hex(target)}

    slots = {}
    for cls, effect in (
        ("StateManager", "base"), ("BrnStateManager", "base"), ("GlobalStateManager", "base"),
        ("AIVehicleStateManager", "base"), ("TrafficStateManager", "base"), ("CollisionStateManager", "base"),
        ("PassbyStateManager", "passby"), ("StreamingStateManager", "streaming"),
        ("PlayerVehicleStateManager", "player"), ("EmitterStateManager", "emitter"),
    ):
        prefix = "??_7StateManager@Logic@CgsSound@@6B@" if cls == "StateManager" else f"??_7{cls}@"
        slots[cls] = slot(prefix, 0x10, functions[effect]["symbol"])
        assert slots[cls]["target_va"] == functions[effect]["va"]
    slots["State::Detach"] = slot("??_7State@Logic@CgsSound@@6B@", 0x18, "?Detach@State@Logic@CgsSound@@UAA_NXZ")
    slots["State deleting destructor"] = slot("??_7State@Logic@CgsSound@@6B@", 0, "??_EState@Logic@CgsSound@@UAAPAXI@Z")
    slots["Scene requester"] = slot("??_7SoundWorldScene@World@Logic@BrnSound@@6B@", 4,
                                    "?GetResourceRegistrar@SoundWorldScene@World@Logic@BrnSound@@UAAAAVResourceRegistrar@34@XZ")

    hits = git("grep", "-n", "-E", r"mSoundScene\.(Release|Destruct)\(|SoundWorldScene::(Release|Destruct)\(",
               args.head, "--", "src").decode().splitlines()
    external = [line for line in hits if "mSoundScene." in line]
    assert len(external) == 3
    assert sum(EMITTER + ":" in line and "mSoundScene.Release()" in line for line in external) == 1
    assert sum(EMITTER + ":" in line and "mSoundScene.Destruct()" in line for line in external) == 1
    assert sum(PLAYER + ":" in line and "mSoundScene.Release()" in line for line in external) == 1

    report = {
        "head": args.head, "tree": args.tree, "parent": PARENT, "root_pins_sha256": args.root_pins_sha256.lower(),
        "functions": functions, "slots": slots, "scene_source_users": external,
        "scope": "Static linked Release bodies and vtable slots only. Runtime and RTTI are separate.",
    }
    path = OUT / "RECEIPT.json"
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("PASS", len(functions), "bodies", len(slots), "slots", sha(path.read_bytes()))


if __name__ == "__main__":
    main()


