"""Compare the production SSE2 colour-cube job with a scalar numerical oracle."""
from pathlib import Path
from fxgs_common import compile_and_run, report
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCTintBlend.cpp','unused.inc','','PCTintBlend')
raise SystemExit(report('run_pc_tint_blend',[],result,121))
