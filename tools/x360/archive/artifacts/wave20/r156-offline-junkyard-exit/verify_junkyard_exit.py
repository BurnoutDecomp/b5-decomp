"""Read ARTIST, the sealed R154 link, and the targeted native candidate object."""

import json
from pathlib import Path
import struct
import subprocess


HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
BASE = "13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d"
CANDIDATE = "43ba142ff2345d82c11fcec94dd9ac025271bf1a"
R155 = "68208920c4ac83d0759649576cb2d0516cb57bf8"
SOURCE = "src/GameSource/GameState/CarSelect/BrnCarSelectManager.cpp"
EXIT = "?UpdateExitState@CarSelectManager@BrnGameState@@AAAXPAV?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@@Z"
ADD = "?AddEvent@?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z"
EVENTS = [(76, 8), (70, 1), (77, 32), (99, 1), (0, 80), (7, 48), (149, 4), (55, 1)]

helper = WAVE / "r128-batch/verify_matches.py"
readers = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], readers)
sha = readers["sha"]
branch_target = readers["branch_target"]


def git(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def word(code, offset):
    return struct.unpack_from(">I", code, offset)[0]


def immediates(code, calls):
    rows = []
    for call in calls:
        result = []
        for register in (5, 6):
            value = next(word(code, offset) & 0xffff
                         for offset in range(call - 4, max(-4, call - 0x64), -4)
                         if word(code, offset) >> 16 == 0x3800 + 32 * register)
            result.append(value)
        rows.append(tuple(result))
    return rows


def add_calls(obj, relocations):
    raw = obj.read_bytes()
    return [offset for offset, index, kind in relocations
            if kind == 6 and readers["object_symbol"](raw, index)[0] == ADD]


def main():
    link = WAVE / "r154-link"
    pe = link / "native-game.pe"
    mapping = link / "native-game.map"
    inventory = json.loads((link / "source-inventory.json").read_text())
    result = json.loads((link / "result.json").read_text())
    assert inventory["head"] == BASE and result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert sha(pe) == "8895808adbacbd4c6847d3c4f5322e5ce6421250ba8f02ddbb75942208669c45"
    assert sha(mapping) == "85ea268fb1c72e005910f97c36b9347be5c8fb545539e285c3700d471c393013"

    invocation = json.loads((link / "invocation.json").read_text())
    chosen, = [entry for entry in invocation["objects"] if entry["source"] == SOURCE]
    old_obj = Path(chosen["object"])
    assert sha(old_obj) == chosen["sha256"]
    old_body, old_relocs = readers["collect"](old_obj, EXIT)
    address, linked = readers["linked_reader"](link)
    old_va = address(EXIT)
    assert old_va == 0x82391448
    old_linked = linked(old_va, len(old_body))
    readers["fixed_equal"](old_body, old_linked, old_relocs)
    old_calls = add_calls(old_obj, old_relocs)
    assert all(branch_target(old_va + offset, word(old_linked, offset)) == address(ADD) for offset in old_calls)
    old_events = immediates(old_linked, old_calls)
    assert old_events == EVENTS[:2] + EVENTS[3:]

    original = readers["original"]
    original_va = 0x82398c20
    artist = original(original_va, 0x2fc)
    artist_calls = [offset for offset in range(0, len(artist), 4)
                    if word(artist, offset) & 0xfc000003 == 0x48000001
                    and branch_target(original_va + offset, word(artist, offset)) == 0x8233fae8]
    assert immediates(artist, artist_calls) == EVENTS
    assert word(artist, 0x14) == 0x3be00000
    assert word(artist, 0x120) == 0x83be0038
    assert artist[0x148:0x168].hex() == (
        "396100701000e8c338c0002038a0004d388100707f63db78100059c39be10080"
    )

    compile_path = HERE / "compile-results.json"
    compile_result = json.loads(compile_path.read_text())
    selected, = compile_result["results"]
    assert selected["source"] == SOURCE and selected["passed"] and selected["machine"] == 0x1f2
    obj = Path(selected["object"])
    body, relocs = readers["collect"](obj, EXIT)
    calls = add_calls(obj, relocs)
    assert immediates(body, calls) == EVENTS and calls[2] == 0x118
    assert word(body, 0x50) == 0x3ba00000
    assert word(body, 0xd0) == 0x839f0038
    assert body[0xf8:0x118].hex() == (
        "396100701000e0c338c0002038a0004d388100707fc3f378100059c39ba10080"
    )

    director_case = word(linked(0x822ef7f4 + 77 * 4, 4), 0)
    assert director_case == 0x822f0190
    assert linked(0x822f01b0, 36).hex() == (
        "897700102b0b0000419a000c7f1f79ae480001c83d6000037f1f992e616b39817f1f59ae"
    )
    race_car_case = word(linked(0x826a9964 + 77 * 4, 4), 0)
    assert race_car_case == 0x826a9fc8
    assert linked(race_car_case, 28).hex() == (
        "3d6000017c9fc02e7fe3fb787f7fe9ae616b86d07f7ff1ae7f7f59ae"
    )
    for pc, callee in [(0x826aa0b4, 0x826a10b0), (0x826aa0c0, 0x826a06f0)]:
        assert branch_target(pc, word(linked(pc, 4), 0)) == callee

    assert git("rev-parse", f"{CANDIDATE}^") == BASE
    assert git("diff", "--name-only", BASE, CANDIDATE) == SOURCE
    assert git("rev-parse", f"{BASE}:{SOURCE}") == git("rev-parse", f"{R155}:{SOURCE}")
    proof = {
        "base": BASE,
        "candidate": CANDIDATE,
        "candidate_tree": git("rev-parse", f"{CANDIDATE}^{{tree}}"),
        "source": SOURCE,
        "source_blob": git("rev-parse", f"{CANDIDATE}:{SOURCE}"),
        "source_unchanged_in_r155_batch": R155,
        "base_link": {
            "pe_sha256": sha(pe), "map_sha256": sha(mapping), "selected_object_sha256": sha(old_obj),
            "method": hex(old_va), "add_event_id_and_size": old_events,
        },
        "artist": {
            "method": hex(original_va), "add_event_id_and_size": EVENTS, "action_77_call": "0x82398d88",
        },
        "targeted_coff": {
            "sha256": sha(obj), "compile_results_sha256": sha(compile_path),
            "add_event_id_and_size": EVENTS, "action_77_call_offset": hex(calls[2]),
            "action_77_payload": {"exit_spawn_pointer_offset": "0x38", "position_offset": 0, "online_byte_offset": 16, "online_byte_value": 0},
        },
        "base_consumers": {
            "MainDirector::ProcessInputQueue": hex(director_case),
            "RaceCarEntityModule::HandleGameActions": hex(race_car_case),
        },
    }
    print(json.dumps(proof, indent=2))


if __name__ == "__main__":
    main()
