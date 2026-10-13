"""Check the CPU registry allocator contract and completed native-worker snapshots."""
from pathlib import Path
import argparse
import os
import tempfile
import fxgs_common as common

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev", help="read the production CPU source from this revision")
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
        mapped.append(str(repo.joinpath(*parts[1:]))
                      if len(parts) >= 2 and parts[:2] == ("b5-decomp", "src") else value)
    return mapped

common.settings = settings
relative = "src/GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.cpp"
with tempfile.TemporaryDirectory(prefix="brn_cpu_registry_source_") as directory:
    source = Path(directory) / "CgsPerfMonCpu.cpp"
    source.write_text(common.Tree(args.rev).read(relative), encoding="utf-8")
    result = common.compile_and_run(Path(__file__).with_name("PCPerfMonSnapshot.cpp"), "unused.inc", "",
                                    "PCPerfMonSnapshot", extra_sources=[source,
                                        workflow / "b5-decomp/vendor/renderware/src/rw/BaseResourceDescriptor.cpp",
                                        workflow / "b5-decomp/vendor/coreallocator/source/icoreallocator_interface.cpp"])
raise SystemExit(common.report("run_pc_perfmon_snapshot", [], result, 12))
