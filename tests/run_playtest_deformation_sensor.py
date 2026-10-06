"""Production debug-sensor callback vs ARTIST825DF310 golden outputs.

Optional source snapshot runs the measured pre-fix callback. The +15 in original
pointer arithmetic supplies the0x1950 array base, not an extra15 sensor indices.
"""
from pathlib import Path
import json,os,subprocess,sys,tempfile
sys.dont_write_bytecode=True
from fxdeformlat_common import definition,settings,REPO,WORKFLOW
HERE=Path(__file__).resolve().parent
def main():
    gold=json.loads((HERE/'PlaytestDeformationSensorData.json').read_text(encoding='utf-8'))
    source=Path(sys.argv[1]) if len(sys.argv)>1 else REPO/'src/GameSource/Physics/DeformationManager/BrnDeformationDebugComponent_Construct.cpp'
    method=definition(source.read_text(encoding='utf-8-sig'),'void DeformationDebugComponent::OnSelectedSensorChange(')
    with tempfile.TemporaryDirectory(prefix='brn_deform_sensor_') as path:
        out=Path(path)
        (out/'deformation_sensor_methods.inc').write_text(method,encoding='utf-8')
        cases=['{'+str(r['case']['count'])+','+str(r['case']['index'])+','+str(r['case'].get('rig',True)).lower()+'}' for r in gold]
        (out/'deformation_sensor_cases.inc').write_text('static const Case K_CASES[]={'+','.join(cases)+'};',encoding='utf-8')
        includes=' '.join('/I"'+str(WORKFLOW/p)+'"' for p in settings('msvc_includes.txt'))
        cmd='cl '+' '.join(settings('msvc_flags.txt'))+' '+includes+' /I"'+str(out)+'" "'+str(HERE/'PlaytestDeformationSensor.cpp')+'" /Fe:regression.exe /link /OPT:REF'
        script=out/'run.cmd';script.write_text('@echo off\ncall "'+str(WORKFLOW/'tools/build/msvc_env.bat')+'" >nul 2>&1\n'+cmd+'\nif errorlevel 1 exit /b 1\nregression.exe\n',encoding='utf-8',newline='\r\n')
        env=dict(os.environ);env.pop('NoDefaultCurrentDirectoryInExePath',None)
        r=subprocess.run(['cmd','/c',str(script)],cwd=out,env=env,capture_output=True,text=True,check=True)
        rows=[line for line in r.stdout.splitlines() if line.startswith('CASE,')]
    checks=0;errors=[]
    assert len(rows)==len(gold)
    for i,line in enumerate(rows):
        fields=line.split(',')[2:];actual=[int(fields[0]),int(fields[1]),*[float(x) for x in fields[2:6]],*[int(x) for x in fields[6:]]]
        record=gold[i];expected=[record['selected'],record['selected'],*record['values'],2*len(record['calls'])]
        expected += [x for pair in record['calls'] for x in pair]
        for k,(w,g) in enumerate(zip(expected,actual)):
            checks+=1
            ok=abs(w-g)<1e-7 if isinstance(w,float) else w==g
            if not ok:errors.append((i,k,w,g))
        assert len(expected)==len(actual)
    print(f'Production debug-sensor/original: {len(rows)} cases, {checks} checks, {len(errors)} failures')
    for error in errors[:12]:print(error)
    return 1 if errors else 0
if __name__=='__main__':raise SystemExit(main())
