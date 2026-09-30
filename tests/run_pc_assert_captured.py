"""Exercise production captured-assert logging, with map/UI sinks isolated."""
from pathlib import Path
import os
from fxgs_common import REPO, STRSTREAM_CPP, compile_and_run, definition, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
source=(REPO/"src/GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.cpp").read_text(encoding="utf-8")
body="\n".join(definition(source, signature) for signature in (
    "Manager::Manager()", "void Manager::SetMapFileReader(", "void Manager::HandleAssertCapturedPC(",
    "void Manager::LogCallstackPC("))
result=compile_and_run(Path(__file__).with_name("PCAssertCaptured.cpp"), "assert_captured.inc", body,
    "PCAssertCaptured", extra_flags="/Gy /Gw",
    extra_sources=[STRSTREAM_CPP, REPO/"src/GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.cpp"])
raise SystemExit(report("run_pc_assert_captured", [], result, 6))
