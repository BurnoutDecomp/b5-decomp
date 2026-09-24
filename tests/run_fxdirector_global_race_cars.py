"""FX-DIRECTOR (crash parity 2026-09-24): the director input's global race-car table at +0x10.

  BrnGameModule::BridgeWorldToDirector step 8 (0x823E3FD8..0x823E3FEC) copies the world's
  RCEntityGlobalRaceCarOutputInterface into the director input, `XMemCpy(input + 0x10, src, 0x970)` -- the
  inlined DWARF InputBuffer::SetGlobalRaceCarInterface (:231). MainDirector::ProcessInputQueue's cases 113
  (a car reached a checkpoint, @0x822385D4) and 223 (a car joined, @0x82238278) convert their GLOBAL race-car
  index to an ACTIVE one through it. The PC input modelled the span as opaque bytes from +0x1 and step 8 was
  dropped, so neither arm could exist.

Numeric: tests/FxDirectorGlobalRaceCars.cpp runs the production step-8 statement against the revision's real
InputBuffer (shadowed header) and the REAL RCEntityGlobalRaceCarOutputInterface TU: offset / size, the 35-slot
map after a publish, the rest of the table, the neighbour at +0x980 untouched, a re-publish. A revision whose
input has no typed table cannot build it (every numeric check then counts as failed). Wiring: step 8 sits
between the contact-spy publish and SetPlayerCarIndex; the header declares the two DWARF accessors; the layout
pins. The arms' numbers are tests/run_fxdirector_input_queue.py sections 22 / 23.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_global_race_cars.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, REPO, code_only, definition, compile_and_run, report, STRSTREAM_CPP

BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToX.cpp"
INPUT_H = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
INPUT_CPP = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIOInputBuffer.cpp"
GLOBAL_TABLE_CPP = REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityGlobalRaceCarOutputInterface.cpp"
BRIDGE = "void BrnGameModule::BridgeWorldToDirector("
STEP8 = re.compile(r"lpDirectorInput->SetGlobalRaceCarInterface\([^;]*\);")
NUMERIC_CHECKS = 10


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def bridge_body(tree):
    try:
        return code_only(definition(tree.read(BRIDGE_CPP), BRIDGE))
    except ValueError:
        return ""


def wiring(tree):
    body = squash(bridge_body(tree))
    contacts = body.find("lpDirectorInput->AppendContacts(lpWorldOutput->GetContactSpyInterface());")
    step8 = body.find("lpDirectorInput->SetGlobalRaceCarInterface(lpWorldOutput->GetRaceCarGlobalOutputInterface());")
    player = body.find("lpDirectorInput->SetPlayerCarIndex(lePlayerIndex);")
    yield ("BridgeWorldToDirector step 8 publishes the world's table between AppendContacts and SetPlayerCarIndex "
           "(0x823E3FD8..0x823E3FEC, before 0x823E3FF8)",
           0 <= contacts < step8 < player)
    header = squash(tree.read(INPUT_H))
    yield ("InputBuffer types mGlobalRaceCarInterface and declares the DWARF accessors GetGlobalRaceCarInterface (:230) / "
           "SetGlobalRaceCarInterface (:231)",
           "BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterfacemGlobalRaceCarInterface;" in header
           and "GetGlobalRaceCarInterface()const{return&mGlobalRaceCarInterface;}" in header
           and "{mGlobalRaceCarInterface=*lpGlobalRaceCarInterface;}" in header)
    layout = squash(tree.read(INPUT_CPP))
    yield ("_AssertLayout pins the table at +0x10 and its size at 0x970",
           "offsetof(InputBuffer,mGlobalRaceCarInterface)==0x0010" in layout
           and "sizeof(BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface)==0x970" in layout)


def numeric(tree):
    match = STEP8.search(bridge_body(tree))
    if match is None:
        print("NUMERIC: this revision's BridgeWorldToDirector has no step 8 -- empty body")
        statement = "/* [this revision does not publish the global race-car table] */"
    else:
        statement = match.group(0)
    shadow = {INPUT_H: tree.read(INPUT_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxDirectorGlobalRaceCars.cpp"), "fxdirector_step8.inc",
                           statement + "\n", "FxDirectorGlobalRaceCars", shadow=shadow,
                           extra_sources=[STRSTREAM_CPP, GLOBAL_TABLE_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_global_race_cars", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
