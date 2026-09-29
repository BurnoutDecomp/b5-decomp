"""Exercise GPU-page allocation with controlled asynchronous completion and failures."""
from pathlib import Path
from fxgs_common import compile_and_run,report
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCGeometryBufferPool.cpp','unused.inc','','PCGeometryBufferPool')
raise SystemExit(report('run_pc_geometry_buffer_pool',[],result,61))
