"""L6 (owner list 2026-09-28): the Driver Details panel read "5,657.6 Kilometres" and "$6.180.200" where the English
console reads miles / yards and "$6,180,200".

CgsLanguage::LanguageManager::LoadStringTable @0x828664B8 calls PrepareFormattingStrings @0x82865B70 right after
installing a table. It reads 25 ids (.data pointers off_82F33340 .. off_82F333A0) out of the LOADED table into the
separator / template members (+0x6100 .. +0x6150) and switches on DISTANCE_FORMAT_ISMETRIC's first character: '1' ->
the metric templates, factors 0.001 / 1.0, metric flag 1; '0' -> the imperial templates, 0.00062137126 (miles) /
1.0936133 (yards), flag 0. The PC re-stamped the English-default literals instead ("the boot language (English)
formats identically through the defaults"). It does not: the English tables the PC loads carry currency separator
',' (default '.') and ISMETRIC '0' (default metric), among other differences.

  1. WIRING -- LoadStringTable calls PrepareFormattingStrings() where the console does and no longer re-stamps the
     defaults there; the false comment is gone; the header declares the DWARF signature.
  2. NUMERIC -- tests/L6LanguageFormatting.cpp runs the extracted production body (+ its ids / factors) on a fixture
     whose FindString hashes with the production CgsHash::CalculateHash, over the REAL LANGUAGE bundles
     (0002 = English, the table the PC loads; 0003 = French), plus a table whose switch is neither '0' nor '1'.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l6_language_formatting.py [--rev <b5 rev>]
    (--src-dir DIR reads CgsLanguageManager.cpp/.h from DIR instead: the shadow gate before a copy-in)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, WORKFLOW, REPO, code_only, compile_and_run, definition, report

LANGUAGE_CPP = "src/GameShared/GameClasses/Language/CgsLanguageManager.cpp"
LANGUAGE_H = "src/GameShared/GameClasses/Language/CgsLanguageManager.h"
BODY = "void LanguageManager::PrepareFormattingStrings()"
IDS = "namespace\n    {\n        // PrepareFormattingStrings' ids"
HASH_CPP = REPO / "src/GameShared/GameClasses/Containers/CgsHash.cpp"
BUNDLES = WORKFLOW / "build/game/LANGUAGE"
NUMERIC_CHECKS = 47


class DirTree(Tree):
    """The manager's .cpp/.h from a directory (the shadow copy); everything else as the working tree."""

    def __init__(self, directory):
        super().__init__(None)
        self.directory = Path(directory)

    def read(self, relative):
        local = self.directory / Path(relative).name
        if relative in (LANGUAGE_CPP, LANGUAGE_H) and local.exists():
            return local.read_text(encoding="utf-8-sig")
        return super().read(relative)


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(LANGUAGE_CPP).replace("\r\n", "\n")
    try:
        load = squash(definition(source, "void LanguageManager::LoadStringTable("))
    except ValueError:
        load = ""
    yield ("LoadStringTable calls PrepareFormattingStrings() between the mpResource store and the font pick "
           "(0x828664B8's `bl PrepareFormattingStrings`)",
           "mpResource=lpResource;constu32luLanguageId=lpResource->meLanguageID;PrepareFormattingStrings();" in load)
    yield ("LoadStringTable no longer re-stamps the English defaults",
           "PrepareDefaultFormattingStrings();" not in load)
    yield ("the false 'English formats identically through the defaults' claim is gone",
           "formats identically through the" not in source)
    header = squash(tree.read(LANGUAGE_H).replace("\r\n", "\n"))
    yield ("CgsLanguageManager.h declares void PrepareFormattingStrings() (DWARF signature)",
           "voidPrepareFormattingStrings();" in header)


def numeric(tree):
    source = tree.read(LANGUAGE_CPP).replace("\r\n", "\n")
    try:
        ids = definition(source, IDS)
        body = definition(source, BODY)
    except ValueError as error:
        print("NUMERIC: cannot build -- PrepareFormattingStrings / its ids absent: " + str(error))
        return None
    english = (BUNDLES / "0002.bundle").as_posix()
    french = (BUNDLES / "0003.bundle").as_posix()
    paths = ('static const char* const KAC_BUNDLE_ENGLISH = "%s";\n'
             'static const char* const KAC_BUNDLE_FRENCH  = "%s";\n' % (english, french))
    text = "\n".join(["namespace CgsLanguage {", ids, body, "}"]) + "\n"
    return compile_and_run(Path(__file__).with_name("L6LanguageFormatting.cpp"), "l6_language_formatting.inc",
                           text, "L6LanguageFormatting", extra_sources=(HASH_CPP,),
                           extra_files={"l6_language_formatting_paths.inc": paths})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-dir", help="read CgsLanguageManager.cpp/.h from this directory (shadow gate)")
    args = parser.parse_args()
    tree = DirTree(args.src_dir) if args.src_dir else Tree(args.rev)
    return report("run_l6_language_formatting", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
