"""Exercise the actual query recorder and complete engine GPU reporting chain."""
import os
from pathlib import Path
import fxgs_common as common

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
common.WORKFLOW = next(p for p in Path(__file__).resolve().parents
                       if (p / "tools/build/msvc_env.bat").is_file())
root = common.REPO
sources = [
    root / "src/GameShared/GameClasses/Development/PerfMon/Gpu/PS3/CgsPerfMonGpuPS3.cpp",
    root / "src/GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.cpp",
    root / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
    root / "src/GameShared/GameClasses/RenderWare/RwRGBA.cpp",
]
numeric = common.compile_and_run(Path(__file__).with_name("PCGpuMonitorTiming.cpp"),
    "pc_gpu_monitor_unused.inc", "", "PCGpuMonitorTiming",
    extra_flags=f'/I"{root / "src"}" d3d9.lib user32.lib', extra_sources=sources)
raise SystemExit(common.report("run_pc_gpu_monitor_timing", [], numeric, 26))
