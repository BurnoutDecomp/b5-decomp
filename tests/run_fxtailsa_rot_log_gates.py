"""FX-TAILS-A item 8 (crash parity 2026-09-24): log parity of the ResetOnTrack Strategies TU, and a stale comment.

The console prints "<AI> routeless fail" (0x8278450C..0x8278451C), "<AI> Car tried to start ahead of player..."
(0x82784738..0x82784748), "<AI> Nodes in same place" (0x82785E9C..0x82785EB4), "Lerping t from a to b"
(0x82785ED4..0x82785F38) and "Converting nodes to position & direction" (0x82790380..0x82790390) only under
`CgsDev::Message::gxMessageFilterFlags & 1`; the tree printed the first four whenever gpDebugPrint was set and never
printed the fifth.

Numeric: FxTailsARotLogGates.cpp runs the PRODUCTION ResetNearRoutelessPlayer, DeterminePositionBetweenNodes and
ConvertNodesToPositionAndDirection (with the TU helper namespace and GetAICar) against a capturing gpDebugPrint with
bit 0 of the filter clear and set. Wiring: BrnModeManager_WorldTick.cpp no longer says three Showtime hops are
unreconstructed (they landed in b72520a2 / f2e66b94 / d6f7aba1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_rot_log_gates.py [--rev <rev>]
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

MANAGER = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
STRATEGIES = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp"
WORLDTICK = "src/GameSource/GameState/ModeManager/BrnModeManager_WorldTick.cpp"
EXTRA = [REPO / "src/GameSource/Math/BrnMathUtils.cpp",
         REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]


def main():
    args = parse_args()
    tree = Tree(args.rev)
    manager, strategies, worldtick = tree.read(MANAGER), tree.read(STRATEGIES), tree.read(WORLDTICK)

    stale = "Three of those hops are still" in worldtick and "unreconstructed -- see the stack's banner" in worldtick
    named = all(sha in worldtick for sha in ("b72520a2", "f2e66b94", "d6f7aba1"))
    wiring_ok = (not stale) and named
    print(("PASS" if wiring_ok else "FAIL") + "  BrnModeManager_WorldTick.cpp: the stale 'three hops unreconstructed' "
          "comment is corrected and names b72520a2 / f2e66b94 / d6f7aba1")

    chunks = ["namespace BrnAI {",
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              definition(strategies, "namespace\n{"),
              definition(strategies, "Vector3 ResetOnTrackManager::DeterminePositionBetweenNodes("),
              definition(strategies, "bool ResetOnTrackManager::ConvertNodesToPositionAndDirection("),
              definition(strategies, "bool ResetOnTrackManager::ResetNearRoutelessPlayer("),
              "}"]
    rc = compile_and_run("FxTailsARotLogGates.cpp", chunks, extra_sources=EXTRA, prefix="brn_fxtailsa_rotlog_")
    print(f"run_fxtailsa_rot_log_gates: wiring {'1/1' if wiring_ok else '0/1'}, numeric rc={rc}")
    sys.exit(0 if (rc == 0 and wiring_ok) else 1)


if __name__ == "__main__":
    main()
