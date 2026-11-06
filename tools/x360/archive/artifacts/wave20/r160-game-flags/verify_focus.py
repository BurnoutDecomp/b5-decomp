"""Verify the focused native bridge, its caller relocations and profile accessors."""
import json
from pathlib import Path
import re

from build_focus import OUT, REPO, BASE, git, sha

ARTIST = Path("D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX")
SOURCE = "src/GameSource/Game/GameBridgeGUIToX_GameState.cpp"
SIGNATURE = ("?BridgeGuiToGameState@BrnGameModule@BrnGame@@QAAHPAUPostWorldInputBuffer"
             "@GameStateModuleIO@BrnGameState@@PBUOutputBuffer@CgsGuiModuleIO@CgsGui@@@Z")


def block(listing, signature):
    match = re.search(r"^(" + re.escape(signature) + r"\S*)\s+PROC NEAR.*?^\1\s+ENDP",
                      listing, re.MULTILINE | re.DOTALL)
    assert match, signature
    return {int(offset, 16): (int(word, 16), inst.strip()) for offset, word, inst in
            re.findall(r"^  ([0-9a-f]{5})\t([0-9a-f]{8})\t(.*)$", match[0], re.MULTILINE)}


def destination(rows, offset):
    word = rows[offset][0]
    assert word >> 26 == 16 and not word & 3
    displacement = word & 0xFFFC
    return offset + (displacement - 0x10000 if displacement & 0x8000 else displacement)


def main():
    head = git("rev-parse", "HEAD")
    assert git("rev-parse", "HEAD^") == BASE and not git("status", "--porcelain=v1")
    assert git("rev-parse", "refs/heads/codex/r159-live-revenge-version-log") == (
        "ce69aa8c092e532638e8c2bc40bc4b4e2e35feac")
    compile_path = OUT / "compile-receipt.json"
    compiled = json.loads(compile_path.read_bytes())
    assert compiled["head"] in (head, BASE) and len(compiled["results"]) == 5
    for source, digest in compiled["input_sha256"].items():
        assert sha(REPO / source) == digest, source
    for row in compiled["results"]:
        name = Path(row["source"]).stem
        assert row["exit_code"] == 0
        assert sha(OUT / f"{name}.obj") == row["ppc_coff_sha256"]
        assert sha(OUT / f"{name}.asm") == row["listing_sha256"]
    source = (REPO / SOURCE).read_text()
    before = git("show", f"{BASE}:{SOURCE}")
    original = json.loads((ARTIST / "0x823DDB78.json").read_bytes())
    cases = lambda s: {int(v) for v in re.findall(r"^\s*case (\d+):", s, re.MULTILINE)}
    assert cases(source) == cases(before) == cases(original["pseudocode"]) | {192, 328}
    assert "result == 192" in original["pseudocode"] and "result == 328" in original["pseudocode"]
    assert all(marker not in source for marker in ("TranslateGuiEventsToGameState", "liResult == 40",
                                                     "lpuRecord", "lu64GameFlags"))

    listing = (OUT / "GameBridgeGUIToX_GameState.asm").read_text(errors="replace")
    assert len(re.findall(r"^\?BridgeGuiToGameState@.*\sPROC NEAR", listing, re.MULTILINE)) == 1
    bridge = block(listing, SIGNATURE)
    for offset, target in ((0x1C, "GetOutEventQueue@OutputBuffer@CgsGuiModuleIO"),
                           (0x2C, "GetGameEventQueue@PostWorldInputBuffer"),
                           (0x5C, "GetFirstEvent@"),
                           (0x3CC, "GetProfile@ProgressionManager"),
                           (0x3D4, "HasPlayerSeenTrainingType@Profile"),
                           (0x3EC, "RequestTraining@TrainingManager")):
        assert target in bridge[offset][1]
    assert bridge[0x3AC][0] == 0x897C0000  # lbz the raw payload
    assert bridge[0x3C4][0] == 0x3C790067 and bridge[0x3C8][0] == 0x38635030
    assert 0x669500 + 0xBB30 == 0x675030
    assert 0x675030 + 0x170 + 0x3998 * 8 == 0x691E60
    assert bridge[0x3D0][0] == 0x38800029 and bridge[0x3E4][0] == 0x38800021
    assert destination(bridge, 0x3C0) == destination(bridge, 0x3E0) == 0x3F0
    assert bridge[0x3F0][0] == 0x3BE00063  # event 99
    assert any("GetNextEvent@" in inst for _, inst in bridge.values())
    assert any("AddEvent@" in inst for _, inst in bridge.values())

    caller_offsets = {}
    for name in ("BrnGameModule", "BrnGameMainFlowStates"):
        caller = (OUT / f"{name}.asm").read_text(errors="replace")
        matches = list(re.finditer(r"^  ([0-9a-f]{5})\t48000001\t bl\s+" + re.escape(SIGNATURE),
                                   caller, re.MULTILINE))
        assert len(matches) == 1
        call = matches[0]
        assert "?LockBuffersForIO@CgsModule" in caller[call.start() - 900:call.start()]
        caller_offsets[name] = "0x" + call[1]

    progression = (OUT / "BrnProgressionManager.asm").read_text(errors="replace")
    getter = block(progression, "?GetProfile@ProgressionManager@BrnProgression@@")
    assert getter[0][0] == 0x38630170
    profile = (OUT / "BrnProfile.asm").read_text(errors="replace")
    seen = block(profile, "?HasPlayerSeenTrainingType@Profile@BrnProgression@@")
    assert any(word == 0x396B3998 for word, _ in seen.values())
    assert any("ldx" in inst for _, inst in seen.values())
    assert any("sld" in inst for _, inst in seen.values())
    record = {"commit": head, "tree": git("rev-parse", "HEAD^{tree}"), "parent": BASE,
              "compile_receipt_sha256": sha(compile_path), "focused_ppc_compiles": 5,
              "artist_bridge_sha256": sha(ARTIST / "0x823DDB78.json"),
              "case_count_including_nested_selectors": len(cases(source)),
              "case_94": {"coff_case": "0x3AC", "coff_event_99": "0x3F0",
                          "artist_bit_load": "0x823DDFC8", "profile_word": "0x691E60",
                          "seen_bit": 41, "training_request": 33},
              "typed_caller_offsets": caller_offsets,
              "scope": "Focused Xbox SDK PPC COFF, native layout and ARTIST behavior. Link and runtime pending."}
    (OUT / "RECEIPT.json").write_text(json.dumps(record, indent=2) + "\n")
    print("Verified direct GUI tags, native profile gate, and both caller edges")


if __name__ == "__main__":
    main()
