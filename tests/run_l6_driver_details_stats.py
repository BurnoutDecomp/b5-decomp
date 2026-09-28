"""L6 (owner list 2026-09-28): "Lot of Driver details are missing or wrong" -- the Driver Details stat panel.

CrashNavDriverDetails::HandleStatData @0x824B8618 formats the pause screen's 33 stat fields and 3 x 5
district fields from GuiEventStatsResponse (GUI 436). Two of the record's fields are FLOATS (the producer,
TranslateGameActionsToGuiEvents case 180 @0x823EC8A0, stores them with stfs @0x823ECA6C / @0x823ECA84) and
the console loads them as floats:
    bestAirTime_cpt  `lfs f1, 0xF4(r31)` @0x824B8968 -> SetLocalisedText(f32, E_FORMAT_SECONDS_HUNDREDTHS_LONG)
    bestSpin_cpt     `lfs f0, 0xF8(r31) ; fctiwz ; stfiwx` @0x824B8978 -> "%d" of the truncated angle
The PC read both through an s32 byte cursor, so the screen showed the IEEE bits (the owner's 7.43 s air time
as "-21474836.-48 Seconds", his 373.1 degree spin as "1,136,300,654 Degrees").

NUMERIC: tests/L6DriverDetailsStats.cpp compiles the extracted production HandleStatData (plus the pre-fix
file-local byte cursor, in a revision that has one) against recording TextFields with the production
SetLocalisedText overload set and the production ParameterFormatType enum, feeds it the REAL
GuiEventStatsResponse filled by console offset, and checks every one of the 48 fields against the asm of
0x824B8618 (field, load, format id, key string), plus the record's member offsets.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l6_driver_details_stats.py [--rev <b5 rev>]
    (--src-dir DIR reads BrnCrashNavDriverDetails.cpp from DIR instead: the shadow gate before a copy-in)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, STRSTREAM_CPP, compile_and_run, definition, report

SCREEN_CPP = "src/GameSource/Gui/Flow/Screen/States/BrnCrashNavDriverDetails.cpp"
LANGUAGE_H = "src/GameShared/GameClasses/Language/CgsLanguageManager.h"
BODY = "void CrashNavDriverDetails::HandleStatData(const GuiEventStatsResponse* lpStatsEvent)"
NUMERIC_CHECKS = 99


class DirTree(Tree):
    """The screen's .cpp from a directory (the shadow copy); everything else as the working tree."""

    def __init__(self, directory):
        super().__init__(None)
        self.directory = Path(directory)

    def read(self, relative):
        local = self.directory / Path(relative).name
        if relative == SCREEN_CPP and local.exists():
            return local.read_text(encoding="utf-8-sig")
        return super().read(relative)


def numeric(tree):
    source = tree.read(SCREEN_CPP).replace("\r\n", "\n")
    language = tree.read(LANGUAGE_H).replace("\r\n", "\n")
    try:
        body = definition(source, BODY)
    except ValueError as error:
        print("NUMERIC: cannot build -- HandleStatData absent: " + str(error))
        return None
    enum = re.search(r"enum ParameterFormatType\s*\{[^}]*\};", language)
    if enum is None:
        print("NUMERIC: cannot build -- ParameterFormatType not found in CgsLanguageManager.h")
        return None
    parts = ["namespace BrnGui {", "namespace {"]
    try:
        # The pre-fix revision read the record through this file-local cursor.
        parts.append(definition(source, "struct StatsReader") + ";")
    except ValueError:
        pass
    parts += ["}", body, "}"]
    return compile_and_run(Path(__file__).with_name("L6DriverDetailsStats.cpp"), "l6_driver_details_stats.inc",
                           "\n".join(parts) + "\n", "L6DriverDetailsStats", extra_sources=(STRSTREAM_CPP,),
                           extra_files={"l6_driver_details_format_enum.inc": enum.group(0) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-dir", help="read BrnCrashNavDriverDetails.cpp from this directory (shadow gate)")
    args = parser.parse_args()
    tree = DirTree(args.src_dir) if args.src_dir else Tree(args.rev)
    return report("run_l6_driver_details_stats", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
