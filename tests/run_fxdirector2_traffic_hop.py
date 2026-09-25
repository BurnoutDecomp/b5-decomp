"""FX-DIRECTOR2 (crash parity 2026-09-25): THE WORLD -> DIRECTOR TRAFFIC HOP AND THE TEAM WORDS.

  The director's AllVehicleData::Update @0x8221D938 takes FIVE arguments on the console (MainDirector::
  PreSceneQueryUpdate 0x8225BC28..0x8225BC70): the used race-car bits, the VehicleInfo[8], the player index, the
  traffic records (director input + 0x6AC0 + 0x10) and the per-car team words (GetVehicleInfoArray 0x82206EF0,
  input + 0x3238). The traffic records arrive through BrnGameModule::BridgeWorldToDirector step 4 (0x823E3F5C..
  0x823E3F78, the inlined InputBuffer::SetTrafficOutputInterface); the team words through BridgeGameStateToDirector's
  last leg (SetVehicleTeam 0x823CD514..0x823CD56C) next to the player's team word @0x7AB0 (0x823CD428..0x823CD454),
  both out of OnlineScoringOutputInterface::maePlayerTeam. On the PC the input carried +0x6AC0 as opaque bytes, step 4
  was dropped, neither team leg existed, and the director ran an extracted UpdateRaceCarsBringUp that stored no
  traffic pointer and wrote team 0 on every nearest-car row.

Numeric: tests/FxDirector2TrafficHop.cpp runs the revision's step-4 statement, its two team blocks and its first
PreSceneQueryUpdate block (all extracted) against the revision's real InputBuffer (header shadowed, TU linked:
Construct, the lock-asserted getters, SetVehicleTeam) and real AllVehicleData (header shadowed): the typed span,
Construct's two new seeds, the copy (and its replacement next frame), the team words (the -1 slot included), the
AllVehicleData stores and rows (teams non-zero where the input has them) and the queries that read them, the
five-argument signature and its :66 tripwire. A revision lacking a piece compiles an empty block or a detection
fallback, so it fails on behaviour. Wiring: the call sites and their console order, the header / layout / Construct
text, ProcessInputQueue's renamed copy. The live witness is tests/FxDirector2TrafficHopLive.ps1.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_traffic_hop.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, code_only, compile_and_run, report

WORLD_BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToX.cpp"
GAME_MODULE_CPP = "src/GameSource/Game/BrnGameModule.cpp"
DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
INPUT_H = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
INPUT_CPP = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIOInputBuffer.cpp"
AVD_H = "src/GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
TRAFFIC_IO_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.cpp"
SHADOW_HEADERS = (INPUT_H, AVD_H)
EXTRA_SOURCES = (STRSTREAM_CPP, REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
                 REPO / "src/GameSource/Director/Camera/SharedIO/BrnPlayerInfo.cpp",   # VehicleInfo::operator=
                 REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp")   # RaceCarState copy
NUMERIC_CHECKS = 3 + 3 + 5 + 5 + 11 + 2

WORLD_BRIDGE = "void BrnGameModule::BridgeWorldToDirector("
STATE_BRIDGE = "void BrnGameModule::BridgeGameStateToDirector("
PRESQ = "void MainDirector::PreSceneQueryUpdate(const DirectorInputOutput* lpIO)"
PIQ = "void MainDirector::ProcessInputQueue(const DirectorInputOutput* lpIO)"
CONSTRUCT = "    void InputBuffer::Construct()"
ACCESSOR = ("const Array<TrafficDirectorEntity, 32u>& TrafficDirectorOutputInterface::GetTrafficDirectorEntityArray()"
            " const")
STEP4 = re.compile(r"lpDirectorInput->SetTrafficOutputInterface\([^;]*\);")
LINE1_ANCHOR = "const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;"
TEAM_WORD_MARKER = "// ---- the player's TEAM word"
TEAM_LOOP_MARKER = "// ---- the per-car TEAMS"
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]')


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def body(tree, relative, signature):
    try:
        return definition(tree.read(relative), signature)
    except ValueError:
        return ""


def block_after(text, marker):
    """The first brace block after the comment line holding `marker` (comments skipped), or None."""
    at = text.find(marker)
    if at < 0:
        return None
    line_end = text.find("\n", at)
    for token in TOKEN.finditer(text, line_end if line_end >= 0 else len(text)):
        if token[0] == "{":
            return definition(text[token.start():], "{")
    return None


def line1_block(tree):
    """PreSceneQueryUpdate's first block: the brace block holding the input-buffer fetch."""
    presq = body(tree, DIRECTOR_CPP, PRESQ)
    at = presq.find(LINE1_ANCHOR)
    if at < 0:
        return None
    return definition(presq[presq.rfind("{", 0, at):], "{")


def wiring(tree):
    world = squash(body(tree, WORLD_BRIDGE_CPP, WORLD_BRIDGE))
    player_assert = world.find('CGS_ASSERT(lpActiveRaceCars->IsPlayerCarActive(),"Playerracecarindexnotactive");')
    step4 = world.find("lpDirectorInput->SetTrafficOutputInterface(lpWorldOutput->GetTrafficDirectorOutputInterface());")
    step6 = world.find("lpDirectorInput->GetVehicleInputInterface()->Append(")
    yield ("BridgeWorldToDirector step 4 copies the world's traffic records straight after the :66 player assert and "
           "before step 6 (0x823E3F5C..0x823E3F78)",
           0 <= player_assert < step4 < step6)
    header = squash(tree.read(INPUT_H))
    yield ("the director input types +0x6AC0 as BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface and declares "
           "the DWARF :290 / :291 accessors inline (the console inlines both, no lock test)",
           "BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterfacemTrafficOutputInterface;" in header
           and "GetTrafficOutputInterface()const{return&mTrafficOutputInterface;}" in header
           and "{mTrafficOutputInterface=*lpTrafficOutputInterface;}" in header)
    layout = squash(tree.read(INPUT_CPP))
    yield ("_AssertLayout pins +0x6AC0, the 0xE20 interface and the 0x70 record",
           "offsetof(InputBuffer,mTrafficOutputInterface)==0x6AC0" in layout
           and "sizeof(BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface)==0x78E0-0x6AC0" in layout
           and "sizeof(BrnTraffic::BrnTrafficIO::TrafficDirectorEntity)==0x70" in layout)
    construct = squash(body(tree, INPUT_CPP, CONSTRUCT))
    yield ("InputBuffer::Construct empties the traffic array (0x82239444) and zeroes the eight team words "
           "(0x82239530..0x82239560)",
           "mTrafficOutputInterface.GetTrafficDirectorEntityArray().Construct();" in construct
           and "maVehicleInfoArray[luCar]=0u;" in construct)
    presq = squash(body(tree, DIRECTOR_CPP, PRESQ))
    yield ("PreSceneQueryUpdate hands AllVehicleData::Update the console's five arguments (0x8225BC28..0x8225BC70): the "
           "used bits, GetRaceCarInfo, the live index, the input's traffic array, GetVehicleInfoArray",
           "constu32*lpaVehicleTeams=lpInput->GetVehicleInfoArray();" in presq
           and ("mAllVehicleData.Update(*lpUsedRaceCars,lpRaceCars,static_cast<EActiveRaceCarIndex>(liPlayerCarIndex),"
                "&lpInput->GetTrafficOutputInterface()->GetTrafficDirectorEntityArray(),lpaVehicleTeams);") in presq
           and "UpdateRaceCarsBringUp" not in presq)
    avd = squash(tree.read(AVD_H))
    yield ("AllVehicleData: the extracted UpdateRaceCarsBringUp leg is retired and Update carries the console's :66 / "
           ":134 tripwires",
           "UpdateRaceCarsBringUp(" not in avd and '"lpTrafficVehicleArray!=NULL"' in avd and '"lpaVehicleTeams"' in avd)
    state = squash(body(tree, GAME_MODULE_CPP, STATE_BRIDGE))
    append = state.find("lpDirectorInput->GetGameActionQueue()->Append(*lpGameStateOutput->GetGameActionQueue());")
    word = state.find("lpDirectorInput->SetPlayerTeam(liPlayerTeam);")
    pair = state.find("lpDirectorInput->SetPlayerEliminated(")
    loop = state.find("lpDirectorInput->SetVehicleTeam(")
    yield ("BridgeGameStateToDirector: the player's team word follows the game-action Append (0x823CD428..0x823CD454) "
           "and precedes the end-of-event pair; the SetVehicleTeam loop is the last leg (0x823CD514..0x823CD56C)",
           0 <= append < word < pair < loop)
    piq = squash(body(tree, DIRECTOR_CPP, PIQ))
    yield ("ProcessInputQueue copies the player's team (+0x7AB0 -> GameState +0x1CC, 0x8223740C / 0x82237428) by its "
           "name, miPlayerTeam",
           "maGameState.miPlayerTeam=lpInput->GetPlayerTeam();" in piq)


def numeric(tree):
    match = STEP4.search(code_only(body(tree, WORLD_BRIDGE_CPP, WORLD_BRIDGE)))
    step4 = match.group(0) if match else "/* [this revision drops BridgeWorldToDirector step 4] */"
    state = body(tree, GAME_MODULE_CPP, STATE_BRIDGE)
    word = block_after(state, TEAM_WORD_MARKER) or "/* [this revision publishes no player team word] */"
    loop = block_after(state, TEAM_LOOP_MARKER) or "/* [this revision publishes no per-car teams] */"
    line1 = line1_block(tree) or "/* [this revision has no PreSceneQueryUpdate line 1] */"
    for name, piece in (("step 4", match), ("team word", word.startswith("{")), ("team loop", loop.startswith("{")),
                        ("PreSceneQueryUpdate line 1", line1.startswith("{"))):
        if not piece:
            print(f"NUMERIC: {name} is absent in this revision -- an empty block runs in its place")
    accessor = body(tree, TRAFFIC_IO_CPP, ACCESSOR)
    input_cpp = tree.read(INPUT_CPP)
    if not accessor or not input_cpp:
        print("NUMERIC: cannot build -- the revision lacks the traffic accessor body or the input-buffer TU")
        return None
    with tempfile.TemporaryDirectory(prefix="brn_fxd2hop_") as directory:
        input_source = Path(directory) / "BrnDirectorModuleIOInputBuffer.cpp"
        input_source.write_text(input_cpp, encoding="utf-8")
        shadow = {header: tree.read(header) for header in SHADOW_HEADERS} if tree.rev else None
        return compile_and_run(Path(__file__).with_name("FxDirector2TrafficHop.cpp"), "traffic_hop_step4.inc",
                               step4 + "\n", "FxDirector2TrafficHop", shadow=shadow,
                               extra_sources=EXTRA_SOURCES + (input_source,),
                               extra_files={"traffic_hop_team_word.inc": word + "\n",
                                            "traffic_hop_team_loop.inc": loop + "\n",
                                            "traffic_hop_presq.inc": line1 + "\n",
                                            "traffic_hop_accessor.inc": accessor + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_traffic_hop", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
