"""Production credits notification/layout/scroll/draw contracts; VMX-derived oracle."""
from pathlib import Path
from fxgs_common import Tree, REPO, STRSTREAM_CPP, definition, compile_and_run, report
tree=Tree()
text=tree.read('src/GameShared/GameClasses/Graphics/Font/CgsFontRenderer.cpp')
code='namespace CgsGraphics { static const RGBA KU_DEFAULT_COLOUR=0xFFFFFFFFu;\n'
code+=definition(text,'void TextObject::Construct(')+'\n}\n'
result=compile_and_run(Path(__file__).with_name('PlaytestCredits.cpp'),'playtest_credits_textobject.inc',code,
    'PlaytestCredits',extra_flags='/Gy',extra_sources=[STRSTREAM_CPP,
        REPO/'src/GameSource/Gui/CustomRenderer/Renderers/BrnCreditsTextRenderer.cpp',
        REPO/'src/GameShared/GameClasses/Core/CgsID.cpp',
        REPO/'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
raise SystemExit(report('run_playtest_credits',[],result,20))
