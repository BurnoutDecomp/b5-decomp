"""Exercise concurrent production line assembly and numeric formatting."""
from pathlib import Path
import os
import sys
import tempfile
from fxgs_common import REPO, STRSTREAM_CPP, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
source = REPO / "src/GameShared/GameClasses/Development/Log/CgsLog.cpp"
with tempfile.TemporaryDirectory(prefix="brn_log_threading_") as directory:
    shadow = None
    if "--shared-state" in sys.argv:
        path = "src/GameShared/GameClasses/Development/Log/CgsLog.h"
        shadow = {path: (REPO/path).read_text(encoding="utf-8").replace("static thread_local PrintMode", "static PrintMode")}
        copy = Path(directory) / "CgsLog.cpp"
        copy.write_text(source.read_text(encoding="utf-8").replace("static thread_local", "static"), encoding="utf-8")
        source = copy
    result = compile_and_run(Path(__file__).with_name("PCLogThreading.cpp"), "unused.inc", "",
        "PCLogThreading", shadow=shadow, extra_flags="/Gy /Gw user32.lib", extra_sources=[STRSTREAM_CPP, source])
raise SystemExit(report("run_pc_log_threading", [], result, 5))
