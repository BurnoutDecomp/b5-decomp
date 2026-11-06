import hashlib, json, os, pathlib, subprocess, sys

OUT = pathlib.Path(__file__).resolve().parent
WORK = pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT = '56799b4f3e1ac5f25cbfa7a41d037444acf6aeac'
PATH = 'src/GameShared/GameClasses/Gui/CgsGuiEvent.h'
CALLER = 'src/GameSource/Gui/BrnGuiModule.cpp'
BASE = 'src/GameShared/GameClasses/Module/VariableEventQueue_4096_16.cpp'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=WORK).decode().strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


assert git('rev-parse','HEAD') == PARENT
assert git('diff','--name-only') == PATH
git('diff','--check','--',PATH)
p=WORK/PATH
report_path = OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==2 and not report['failed']
rows={r['source']:r for r in report['results']}
asm=pathlib.Path(rows[CALLER]['assembly']).read_text(errors='replace')
actual='?GetLength@?$VariableEventQueue@$0BAAA@$0BA@@CgsModule@@QBAHXZ'
old='?GetLength@?$GuiEventQueueBase@$0BAAA@$0BA@@CgsGui@@QBAHXZ'
body=asm.split('Begin code for function: '+old,1)[1].split('End code for function: '+old,1)[0]
assert any(actual in line and ' b ' in line for line in body.splitlines())
call=asm.split('if (lrProfileEvents.GetLength())',1)[1].split('lpOutput->AddGuiOutEvents',1)[0]
assert 'BeginAssert@Assert' in call and 'cmpwi' in call
assert not any(old in line for line in call.splitlines())
message=OUT/'COMMIT_MESSAGE.txt'
message.write_text('Restore inherited GUI event queue length forwarding\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',PATH)
assert git('diff','--cached','--name-only')==PATH
git('commit','--file',str(message))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'source':PATH,'source_sha256':sha(p),
         'artist_caller':'0x8252A0B0','artist_target':'CgsModule::VariableEventQueue<4096,16>::GetLength',
         'decfigs':'CgsGuiEvent.h:361 declares a GetLength member for each generic queue size',
         'missing_host_mangled':old,'native_base_mangled':actual,
         'listing':'Inline caller uses the base guard and count, and the COMDAT wrapper is a direct base tail branch.',
         'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])

