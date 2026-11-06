from pathlib import Path
import json,os,re,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='fa84cd2a4423d5258a88eff385c5ad61e1707a88'
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
assert git('rev-parse','HEAD')=='717bd5556277d1fe9394504ed09e31ce86e4ce42'
assert not git('status','--porcelain=v1')
git('switch','-c','codex/experimental-x360-r30-native-gui-states',base)
gui='src/GameShared/GameClasses/Gui/View/'
render='src/GameSource/Gui/CustomRenderer/Renderers/'
billboard=gui+'ParticleSystem2d/CgsBillboardRenderer'
sources=[gui+'CgsGuiViewModule.cpp',gui+'CustomRenderer/CgsCustomRenderer.cpp',billboard+'.cpp']
sources += [render+name+'.cpp' for name in ('BrnAboveCarRenderer','BrnBlackBarRenderer',
             'BrnBoostBarRenderer','BrnCrashNavIconRenderer','BrnInGameMessageRenderer','BrnMainMapRenderer')]
header=billboard+'.h'
def edit(path,callback):
    p=w/path
    before=p.read_bytes()
    is_crlf=b'\r\n' in before
    data=before.decode('utf-8').replace('\r\n','\n')
    new=callback(data)
    assert new!=data,path
    p.write_bytes((new.replace('\n','\r\n') if is_crlf else new).encode('utf-8'))

def clear_header(text):
    start=text.index('    // The X360 GUI render-state-table entries')
    end=text.index('    struct BillboardInfo;',start)
    return text[:start]+text[end:]
edit(header,clear_header)

slots={
    'gpGuiBlendStateStandard':'mpBlendState_Standard',
    'gpGuiBlendStateAdditive':'mpBlendState_Additive',
    'gpBillboardDepthStencilState':'mpDepthStencilState_ZBufferOff',
    'gpGuiDepthStencilStateZOn':'mpDepthStencilState_ZBufferOn',
    'gpGuiWhiteTexture':'mpTexture_White',
    'gpGuiRasterizerStateCullNone':'mpRasterizerState_CullNone',
}
lookup={k:'CgsGraphics::ImRendererBase::GetStateLibrary().'+v for k,v in slots.items()}
lookup['gBillboardScreenTransform']='CgsGraphics::Im2dTransform::GetAspectCorrected()'
pat=re.compile(r'\b(?:CgsGui::)?('+'|'.join(lookup)+r')\b')
include='#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"'
for path in sources:
    def native_state(text):
        if path==billboard+'.cpp':
            marker='// The X360 data-segment globals the GUI billboard/boost-bar draws bind'
            start=text.rfind('// ---------------------------------------------------------------------------------------------',0,text.index(marker))
            end=text.index('// The shared full-texture default UV set',start)
            text=text[:start]+text[end:]
            old="""    // (The X360 data-segment globals the draw binds -- dword_83010F54 and unk_83011090 -- are
    // defined at namespace scope below as their PC folds; see the block comment there.)

"""
            assert text.count(old)==1
            text=text.replace(old,'')
        if path.endswith('/BrnBoostBarRenderer.cpp'):
            old="""// publish the shared screen transform (&unk_83011090 == gBillboardScreenTransform's console
// original), submit as a 4-vertex strip (primitive 6). The corner positions stay in the
// console's 0..1 screen proportions -- the shared transform carries the screen mapping, exactly
// as on the console (see gBillboardScreenTransform's PC-fold note in CgsBillboardRenderer.cpp)."""
            assert text.count(old)==1
            text=text.replace(old,"""// publish the shared aspect-corrected transform (83011090), and submit as a
// 4-vertex strip (primitive 6). The corners stay in 0..1 screen proportions.""")
            text=text.replace('SetTransform(&unk_83011090 == gBillboardScreenTransform)', 'SetTransform(83011090)')
        text, count=pat.subn(lambda m:lookup[m.group(1)],text)
        assert count>0,path
        if include not in text and 'ImRendererBase::GetStateLibrary()' in text:
            lines=text.splitlines(keepends=True)
            idx=next(i for i,line in enumerate(lines) if line.startswith('#include '))
            lines.insert(idx,include+'\n')
            text=''.join(lines)
        return text
    edit(path,native_state)
git('diff','--check')
expected={header,*sources}
assert set(git('diff','--name-only').splitlines())==expected
old=json.loads((d.parent/'r26-stream-lock-root/command.json').read_text())
args=old[:old.index('--source')]
args[args.index('--out')+1]=str(d/'native')
args[args.index('--jobs')+1]='4'
for path in sources:args+=['--source',path]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
