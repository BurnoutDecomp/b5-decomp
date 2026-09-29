"""Verify traffic tick snapshots and render-only sampling with the real blend."""
from pathlib import Path
from fxgs_common import Tree, definition, code_only, compile_and_run, report

HERE = Path(__file__).resolve().parent
tree = Tree()
root = 'src/GameSource/World/EntityModules/TrafficEntityModule/'
module = tree.read(root + 'BrnTrafficEntityModule.cpp')
render = tree.read(root + 'BrnTrafficEntityModule_Render.cpp')
post = code_only(definition(module, 'void TrafficEntityModule::PostPhysicsUpdate('))
draw = code_only(definition(render, 'TrafficEntityModule::RenderTrafficCar('))
wiring = [
    ('traffic snapshots follow all post-physics producers',
     post.count('LatchRenderPosesPC();') == 1 and post.index('LatchRenderPosesPC();') > post.index('ProcessTrafficTypeRequests(')),
    ('body, suspension/steering/spin and physical wheels sample render-local history',
     all(x in draw for x in ('SampleBody(', 'SampleAngles(', 'SampleWheel(', 'FrameInterpolation::IsEnabled()'))),
    ('spawn and module reset invalidate previous vehicle history',
     'maRenderPosesPC[luVehicle].Reset();' in code_only(module) and
     'maRenderPosesPC[luParam].Reset();' in code_only(module) and
     'lrRenderPose.Reset();' in code_only(definition(module, 'void TrafficEntityModule::Reset('))),
]
numeric = compile_and_run(HERE/'TrafficRenderInterpolation.cpp', 'unused.inc', '',
    'TrafficRenderInterpolation', extra_sources=[HERE.parent/'src/GameShared/GameClasses/System/Timer/CgsFrameInterpolation.cpp'])
raise SystemExit(report('run_traffic_render_interpolation', wiring, numeric, 47))
