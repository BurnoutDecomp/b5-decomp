"""Validate shared world/immediate/GUI constant shadowing on real D3D9."""
from pathlib import Path
from fxgs_common import compile_and_run,report
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCShaderConstantCache.cpp','unused.inc','',
                       'PCShaderConstantCache',extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_shader_constant_cache',[],result,43))
