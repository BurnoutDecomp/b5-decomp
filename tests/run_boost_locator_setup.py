"""Regression for exhausted boost effects caused by an empty live locator table.

Compiles the production PrepareLocators body with the real resource/output types.
Use --rev HEAD to demonstrate failure before the fix.
"""
import argparse
from pathlib import Path
import sys

from fxgs_common import Tree, compile_and_run, definition, report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    source = Tree(args.rev).read(
        "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_Lifecycle.cpp")
    body = definition(source, "void DeformableObject::PrepareLocators()")
    body = body.replace("DeformableObject::", "LocatorSetupFixture::")
    result = compile_and_run(Path(__file__).with_name("BoostLocatorSetup.cpp"),
                             "boost_locator_setup.inc", body, "BoostLocatorSetup")
    return report("run_boost_locator_setup", [], result, 88)


if __name__ == "__main__":
    sys.exit(main())
