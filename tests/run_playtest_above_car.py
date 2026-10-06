"""Actual AboveCar lifecycle/events/cash draw and raw-VMX transform fixtures.

Font metrics, text submission, camera projection refresh, and 3D command recording
are boundaries. The renderer logic, cache payload copies, and replay copies are real.
"""
from pathlib import Path
import os,re
from fxgs_common import Tree,definition,compile_and_run,report,REPO,STRSTREAM_CPP
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
tree=Tree(None)
above=tree.read('src/GameSource/Gui/CustomRenderer/Renderers/BrnAboveCarRenderer.cpp')
cache=tree.read('src/GameSource/Gui/BrnGuiCache.cpp')
font=tree.read('src/GameShared/GameClasses/Graphics/Font/CgsFontRenderer.cpp')
code='namespace CgsGraphics { static const RGBA KU_DEFAULT_COLOUR=0xFFFFFFFFu;\n'
code+=definition(font,'void TextObject::Construct(')+'\n}\nnamespace BrnGui {\n'
for sig in ['void AboveCarRenderer::Construct(', 'bool AboveCarRenderer::Prepare(',
            'bool AboveCarRenderer::Release(', 'CgsID AboveCarRenderer::GetID(',
            'void AboveCarRenderer::Update(', 'void AboveCarRenderer::RecvEvent(',
            'f32 AboveCarRenderer::SetTransformMatrixForCar(',
            'void AboveCarRenderer::RenderTrafficCarScores(']:
    code+=definition(above,sig)+'\n'
for sig in ['GuiCache::GuiCache(', 'u32 GuiCache::GetScoringTrafficCount(',
            'const BrnTraffic::BrnTrafficIO::VehicleScoreData* GuiCache::GetScoringTrafficData(']:
    code+=definition(cache,sig)+'\n'
code+='void GuiCache::RecEvent(const CgsModule::Event* lpEvent,s32 liEventId) { switch(liEventId) {\n'
for event in [208,210]:
    arm=re.search(r'        case '+str(event)+r':\n[\s\S]*?            break;',cache)
    if not arm:raise RuntimeError('Missing production cache arm '+str(event))
    code+=arm[0]+'\n'
code+='} }\n}\n'
serialiser=tree.read('src/GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.cpp')
code+='namespace BrnReplays {\n'+definition(serialiser,'GuiModuleStaticLayout* GuiModuleSerialiser::GetStaticLayout(')+'\n}\n'
result=compile_and_run(Path(__file__).with_name('PlaytestAboveCar.cpp'),
    'playtest_above_car.inc',code,'PlaytestAboveCar',extra_flags='/Gy /Gw',
    extra_sources=[STRSTREAM_CPP,REPO/'src/GameSource/Replays/BrnGuiModuleAboveCarObjectLayout.cpp'])
raise SystemExit(report('run_playtest_above_car',[],result,1))
