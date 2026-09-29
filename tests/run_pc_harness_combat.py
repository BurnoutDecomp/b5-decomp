from pathlib import Path
import os
from fxgs_common import compile_and_run, report
os.environ.pop("NoDefaultCurrentDirectoryInExePath",None)
result = compile_and_run(Path(__file__).with_name("PCHarnessCombat.cpp"), "unused.inc", "", "PCHarnessCombat")
raise SystemExit(report("run_pc_harness_combat", [], result, 8))
