"""Numeric ARTIST animation oracle plus both production render-call checks.

--rev supplies the old attachment body and draw paths. The recovered CPU shader
is kept as the fixture's consumer, so a deferred attachment fails numerically.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, REPO, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser=argparse.ArgumentParser()
parser.add_argument('--rev')
args=parser.parse_args()
tree=Tree(args.rev)
assembly=tree.read('src/GameShared/GameClasses/Graphics/CgsMaterialAssembly.cpp')
draws=tree.read('src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp')
constants=Tree().read('src/GameShared/GameClasses/Graphics/CgsShaderConstants.cpp')
animation=Tree().read('src/GameShared/GameClasses/Graphics/Dispatch/CgsMaterialAnimation.cpp')
# definition excludes the struct's trailing semicolon.
helpers=definition(constants,'struct SerialisedCPU')+';\n'+ '\n'.join(
    definition(constants,s) for s in ('inline u32* SlotArray(','inline const char* SlotString('))
body=(animation+'\nnamespace {\n'+helpers+'\n}\n'
      +definition(constants,'bool ShaderConstantsCPU::GetValue(')
      +'\nnamespace CgsGraphics {\n'
      +definition(assembly,'void MaterialAssembly::FixupAnimatedMaterial(')
      +'\n'+definition(assembly,'MaterialTechnique* MaterialAssembly::GetMaterial(')+'\n}\n')
color=definition(draws,'s32 DispatchList::DispatchAllMeshes(')
depth=definition(draws,'void DrawRenderableMeshZOnly::Interpret(')
render=definition(tree.read('src/GameSource/Graphics/BrnRendererModule.cpp'),
                  'void BrnRendererModule::Render(')
wiring=[('color path dispatches at renderer time after object constants',
         'lpCPU->Dispatch(lpInterpreter->GetTime()' in color and
         color.index('SetMeshObjectConstantsPC')<color.index('lpCPU->Dispatch')),
        ('depth path dispatches at the same interpreter time',
         'lpCPU->Dispatch(lfTime' in depth and
         depth.index('SetMeshObjectConstantsPC')<depth.index('lpCPU->Dispatch')),
        ('mesh draw clock comes from the completed shader frame after the draw gate',
         'mpInterpreter->SetTime(maShaderConstantsFrames[mu8ShaderConstantsFrameInternal].GetGameTime())' in render
         and render.index('if (!lbDrawFrame)') < render.index('mpInterpreter->SetTime(')
         < render.index('RenderShadowMapPasses('))]
shadow={
 'src/GameShared/GameClasses/Core/CgsAssert.h':
  '#pragma once\n#include <cstdlib>\n#undef CGS_ASSERT\n#define CGS_ASSERT(ok, ...) do {if (!(ok)) std::abort();} while(0)\n',
 'src/GameShared/GameClasses/Development/Log/CgsLog.h':
  '#pragma once\nnamespace CgsDev {struct QuietLog {template<class T> QuietLog& operator<<(const T&){return *this;}};'
  'namespace Message {inline unsigned gxMessageFilterFlags=0;} namespace Log {'
  'inline QuietLog stream;inline QuietLog* gpDebugPrint=&stream;}}\n'
}
result=compile_and_run(Path(__file__).with_name('MaterialAnimation.cpp'),
  'material_animation.inc',body,'MaterialAnimation',shadow=shadow,
  extra_sources=[REPO/'src/GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.cpp'])
raise SystemExit(report('run_material_animation',wiring,result,40))
