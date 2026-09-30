"""Read completed profiling data while native worker threads update/register counters."""
from pathlib import Path
import os
from fxgs_common import REPO, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
source = REPO / "src/GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.cpp"
result = compile_and_run(Path(__file__).with_name("PCPerfMonSnapshot.cpp"), "unused.inc", "",
                         "PCPerfMonSnapshot", extra_sources=[source])
raise SystemExit(report("run_pc_perfmon_snapshot", [], result, 8))
