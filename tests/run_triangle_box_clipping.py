"""Link the whole production clipping implementation and exercise ARTIST counterexamples."""
import argparse
import os
from pathlib import Path
import tempfile
import fxgs_common as common

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev", help="read the whole production source from this Git revision")
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
relative = "src/GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.cpp"
with tempfile.TemporaryDirectory(prefix="brn_triangle_clip_source_") as directory:
    source = Path(directory) / "CgsTriangleBox.cpp"
    source.write_text(common.Tree(args.rev).read(relative), encoding="utf-8")
    result = common.compile_and_run(Path(__file__).with_name("TriangleBoxClipping.cpp"),
        "unused.inc", "", "TriangleBoxClipping", extra_sources=[source,
            repo / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"])
raise SystemExit(common.report("run_triangle_box_clipping", [], result, 8))
