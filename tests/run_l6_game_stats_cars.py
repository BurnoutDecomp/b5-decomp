"""L6 (owner list 2026-09-28): "Lot of Driver details are missing or wrong" -- CARS OWNED read 171/86.

ProgressionManager::GetGameStats @0x8238A6A0 counts CARS_COLLECTED over the profile's car list. A sponsor car
(CarData unlock type 5) counts once the progression rank reaches the vehicle's unlock rank (entry+0x99);
any other car counts when its vehicle-list flags word has bit 0 set (entry+0x94) AND its livery tag
(entry+0xE9) is NOT 1 / 3 / 4:
    0x8238A870  cmplwi r11, 0      ; r11 = (livery in {1,3,4})
    0x8238A874  li     r11, 1
    0x8238A878  beq    0x8238A880  ; not in the set -> count
    0x8238A87C  mr     r11, r18    ; in the set -> r11 = 0 -> no count
The PC counted the set instead: the colour / silver variants of the owner's cars (171 of his 241) where the
console counts the 70 base and pattern cars, against a total of 86.

NUMERIC: tests/L6GameStatsCars.cpp compiles the extracted production GetGameStats (with its TU's
anonymous-namespace constants), GameStats::Construct and the VehicleListEntry accessors it reads, against the
real GameStats record, the real VehicleListEntry and CarData, and stand-in managers; it checks every arm of
the car loop, the owner's slot-0 mix (241 cars -> 70), and the rest of the record against the store list of
0x8238A6A0.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l6_game_stats_cars.py [--rev <b5 rev>]
    (--src-dir DIR reads BrnProgressionManager_GameStats.cpp from DIR instead: the shadow gate)
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, STRSTREAM_CPP, compile_and_run, definition, report

STATS_CPP = "src/GameSource/GameState/Progression/BrnProgressionManager_GameStats.cpp"
RECORD_CPP = "src/GameSource/GameState/SharedIO/BrnGameActionData.cpp"
ENTRY_CPP = "src/SharedClasses/DataLists/VehicleListEntry.cpp"
BODY = "void ProgressionManager::GetGameStats("
NUMERIC_CHECKS = 52


class DirTree(Tree):
    """GetGameStats' .cpp from a directory (the shadow copy); everything else as the working tree."""

    def __init__(self, directory):
        super().__init__(None)
        self.directory = Path(directory)

    def read(self, relative):
        local = self.directory / Path(relative).name
        if relative == STATS_CPP and local.exists():
            return local.read_text(encoding="utf-8-sig")
        return super().read(relative)


def numeric(tree):
    stats = tree.read(STATS_CPP).replace("\r\n", "\n")
    record = tree.read(RECORD_CPP).replace("\r\n", "\n")
    entry = tree.read(ENTRY_CPP).replace("\r\n", "\n")
    try:
        parts = [
            "namespace BrnGameState { namespace GameStateModuleIO {",
            definition(record, "void GameStats::Construct()"),
            "} }",
            "namespace BrnResource {",
            definition(entry, "namespace\n{"),
            definition(entry, "CgsID VehicleListEntry::GetId() const"),
            definition(entry, "bool VehicleListEntry::IsTrophyCar() const"),
            definition(entry, "u8 VehicleListEntry::GetUnlockRank() const"),
            definition(entry, "u8 VehicleListEntry::GetLiveryType() const"),
            "}",
            "namespace BrnProgression {",
            definition(stats, "namespace\n{"),
            definition(stats, BODY),
            "}",
        ]
    except ValueError as error:
        print("NUMERIC: cannot build -- a production body is absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("L6GameStatsCars.cpp"), "l6_game_stats_cars.inc",
                           "\n".join(parts) + "\n", "L6GameStatsCars", extra_sources=(STRSTREAM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-dir", help="read BrnProgressionManager_GameStats.cpp from this directory (shadow gate)")
    args = parser.parse_args()
    tree = DirTree(args.src_dir) if args.src_dir else Tree(args.rev)
    return report("run_l6_game_stats_cars", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
