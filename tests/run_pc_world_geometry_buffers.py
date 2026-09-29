"""Draw through production static geometry uploads, reuse and streaming retirement."""
from pathlib import Path
from fxgs_common import compile_and_run, report
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCWorldGeometryBuffers.cpp','unused.inc','',
                       'PCWorldGeometryBuffers',extra_flags='d3d9.lib user32.lib d3dcompiler.lib')
raise SystemExit(report('run_pc_world_geometry_buffers',[],result,31))
