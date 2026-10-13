"""Check the complete production member frustum predicate against ARTIST branches."""
import argparse
import os
from pathlib import Path
import tempfile
import fxgs_common as common

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev", help="read the whole production Frustum.cpp from this revision")
args = parser.parse_args()
repo = common.REPO
workflow = next(parent for parent in repo.parents if (parent / "tools/build/msvc_flags.txt").is_file())
common.WORKFLOW = workflow
original_settings = common.settings


def settings(name):
    values = original_settings(name)
    if name != "msvc_includes.txt":
        return values
    mapped = []
    for value in values:
        parts = Path(value).parts
        private = len(parts) >= 2 and parts[:2] == ("b5-decomp", "src")
        private = private or (len(parts) >= 3 and parts[:3] == ("b5-decomp", "vendor", "renderware"))
        mapped.append(str(repo.joinpath(*parts[1:])) if private else value)
    return mapped


common.settings = settings
with tempfile.TemporaryDirectory(prefix="brn_member_frustum_source_") as directory:
    source = Path(directory) / "Frustum.cpp"
    source.write_text(common.Tree(args.rev).read("src/vendor/renderware/collision/Frustum.cpp"), encoding="utf-8")
    result = common.compile_and_run(Path(__file__).with_name("CollisionFrustumPlanes.cpp"),
        "unused.inc", "", "CollisionFrustumPlanes", extra_sources=[source])
raise SystemExit(common.report("run_collision_frustum_planes", [], result, 6))
