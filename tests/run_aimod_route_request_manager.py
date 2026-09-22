"""FX-AIMOD G04-D1 / G04-D3: replay the production RouteRequestManager Construct and block sections.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_route_request_manager.py
(The pre-fix tree has neither SetBlockSections nor ClearBlockSections and a different slot type, so
this unit test only builds on the fixed tree; the RED side of these defects is the structural
tests/run_aimod_module_wiring.py --rev <parent>.)
"""
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition, constants

RRM = "src/GameSource/World/AI/BrnRouteRequestManager.cpp"


def main():
    source = Tree(parse_args().rev).read(RRM)
    chunks = ["namespace BrnAI {",
              constants(source, r"^static CgsNumeric::Random mRandom;"),
              definition(source, "void RouteRequestManager::Construct("),
              definition(source, "void RouteRequestManager::SetBlockSections("),
              definition(source, "void RouteRequestManager::ClearBlockSections("),
              "}"]
    sys.exit(compile_and_run("AIModRouteRequestManager.cpp", chunks, prefix="brn_aimod_rrm_"))


if __name__ == "__main__":
    main()
