"""Extract real PC input delivery; exercise actual gated Win32 hold/release."""
from pathlib import Path
import argparse,os,re,subprocess,tempfile
from fxdeformlat_common import definition,settings
BASE=Path(__file__).resolve().parent
ROOT=BASE.parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path);args=parser.parse_args()
source=(args.source or ROOT/'b5-decomp/src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.cpp').read_text(encoding='utf-8-sig')
names=[('struct XInputGamepad',True),('struct XInputState',True),('f32 ApplyStickDeadzone(',False),
       ('f32 NormaliseThumb(',False),('f32 ClampAxis(',False),('enum EHarnessSteerChannel',True),('bool HarnessSteerChannelHeld(',False)]
if 'void ApplyHarnessCameraStick(' in source:names.append(('void ApplyHarnessCameraStick(',False))
helpers=[re.search(r'const f32 '+name+r'\s*=[^;]+;',source)[0] for name in ['KF_STICK_SATURATION','KF_STICK_DEADZONE']]
helpers += [definition(source,name)+(';' if semicolon else '') for name,semicolon in names]
marker='        f32 lfStickRX = lbXPad ?' if '        f32 lfStickRX = lbXPad ?' in source else '        lrPad.mfStickRX = lbXPad ?'
start=source.index(marker);end=source.index('        lrPad.mfAxis10',start)
delivery=source[start:end]
with tempfile.TemporaryDirectory(prefix='brn_camera_harness_') as path:
    out=Path(path);(out/'camera_harness_helpers.inc').write_text('\n'.join(helpers),encoding='utf-8')
    (out/'camera_harness_delivery.inc').write_text(delivery,encoding='utf-8')
    includes=' '.join('/I"'+str(ROOT/p)+'"' for p in settings('msvc_includes.txt'))
    cmd='cl '+' '.join(settings('msvc_flags.txt'))+' '+includes+' /I"'+str(out)+'" "'+str(BASE/'PlaytestCameraStickHarness.cpp')+'" /Fe:regression.exe /link /OPT:REF'
    script=out/'run.cmd';script.write_text('@echo off\ncall "'+str(ROOT/'tools/build/msvc_env.bat')+'" >nul 2>&1\n'+cmd+'\n',encoding='utf-8',newline='\r\n')
    env=dict(os.environ);env.pop('NoDefaultCurrentDirectoryInExePath',None)
    result=subprocess.run(['cmd','/c',str(script)],cwd=out,env=env,capture_output=True,text=True)
    if result.returncode:print(result.stdout,result.stderr);raise SystemExit(result.returncode)
    failed=False
    for mode in ['disabled','enabled']:
        result=subprocess.run([str(out/'regression.exe'),mode],cwd=out,env=env,capture_output=True,text=True)
        print(result.stdout.strip());failed |= result.returncode!=0
    raise SystemExit(1 if failed else 0)
