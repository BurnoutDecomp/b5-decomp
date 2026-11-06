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

assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')
p = WORK/PATH
src = p.read_bytes()
line = b'\r\n' if b'\r\n' in src else b'\n'
before = b'        s32  GetLength() const;' + line
after = line.join([
    b'        s32 GetLength() const',
    b'        {',
    b'            return CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::GetLength();',
    b'        }',
    b''])
assert src.count(before)==1
p.write_bytes(src.replace(before,after))
git('diff','--check','--',PATH)
template = json.loads((OUT.parent/'xam-schema-root/command.json').read_text())
args = template[:template.index('--source')]
args[args.index('--out')+1] = str(OUT/'native1')
for source in (CALLER,BASE):
    args += ['--source',source]
temp = OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path = OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==2 and not report['failed']
rows={r['source']:r for r in report['results']}
asm=pathlib.Path(rows[CALLER]['assembly']).read_text(errors='replace')
actual='?GetLength@?$VariableEventQueue@$0BAAA@$0BA@@CgsModule@@QBAHXZ'
old='?GetLength@?$GuiEventQueueBase@$0BAAA@$0BA@@CgsGui@@QBAHXZ'
assert old not in asm and actual in asm
assert any(actual in l and '\tbl ' in l for l in asm.splitlines()), actual
message=OUT/'COMMIT_MESSAGE.txt'
message.write_text('Restore inherited GUI event queue length forwarding\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',PATH)
assert git('diff','--cached','--name-only')==PATH
git('commit','--file',str(message))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'source':PATH,'source_sha256':sha(p),
         'artist_caller':'0x8252A0B0','artist_target':'CgsModule::VariableEventQueue<4096,16>::GetLength',
         'decfigs':'CgsGuiEvent.h:361 declares a GetLength member for each generic queue size',
         'missing_host_mangled':old,'native_call_mangled':actual,
         'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
