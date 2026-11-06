from pathlib import Path
import hashlib,json,struct,subprocess
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def git(*a):return subprocess.check_output(['git','-C',str(ROOT),*a])
b,a=read(HERE/'source-before.json'),read(HERE/'source-after.json')
assert b['hashes']==a['hashes'] and len(b['hashes'])==6277
assert all(sha(ROOT/p)==v for p,v in b['hashes'].items())
assert git('rev-parse','HEAD^').decode().strip()==b['base']
assert git('diff-tree','--no-commit-id','--name-only','-r','HEAD').decode().splitlines()==[b['source']]
assert not git('diff','--name-only').strip()
r=read(HERE/'native/compile-results.json');assert r['state']=='complete' and r['passed']==1 and r['failed']==0
row=r['results'][0];assert row['machine']==0x1f2 and struct.unpack_from('<H',Path(row['object']).read_bytes())[0]==0x1f2
headers={}
for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
 if 'including file:' in line:
  p=Path(line.split('including file:',1)[1].strip()).resolve();headers[str(p)]=sha(p)
assert not any('wave4-intake' in p.lower() for p in headers)
original=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82675868.json')
d=read(original);asm=d['assembly'];assert 'cLionFX__BinLoad' in asm and 'stw       r11, 4(r31)' in asm
assert 'snprintf' not in asm and 'WriteToLog' not in asm
code=(ROOT/b['source']).read_text(encoding='utf-8')
assert all(s not in code for s in ('std::snprintf','sbLogged','CgsDev::Log','lef_transcode.py'))
tools=read(HERE.parent/'wave6/full-stage1-terminal-audit.json')['external_inputs']['compiler_and_runtime']
assert len(tools)==7 and all(sha(p)==v for p,v in tools.items())
head=git('rev-parse','HEAD').decode().strip();(HERE/'owned.patch').write_bytes(git('diff','--binary','HEAD^','HEAD'))
result={'base':b['base'],'head':head,'source':b['source'],'source_sha256':sha(ROOT/b['source']),
 'git_blob':git('rev-parse','HEAD:'+b['source']).decode().strip(),'patch_sha256':sha(HERE/'owned.patch'),
 'report':str(HERE/'native/compile-results.json'),'report_sha256':sha(HERE/'native/compile-results.json'),
 'passed':1,'failed':0,'stable_source_hashes':6277,'actual_headers':headers,'header_count':len(headers),
 'artifacts':{row[k]:sha(row[k]) for k in ('object','assembly','log')},'compiler_and_runtime':tools,
 'original_export':str(original),'original_export_sha256':sha(original),'original_assembly':asm,
 'scope':'Remove only the host logging/inspection failure path and its includes. Keep native BinLoad, definition store and true post-load tail; no API/layout/provider change.',
 'limits':'Whole CPP compilation and direct original control/call/store inspection. No full method machine-code/runtime comparison, whole particle subsystem, link/image or launch claim.',
 'verifier_sha256':sha(__file__)}
with (HERE/'DELIVERY.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps({'head':head,'passed':1,'headers':len(headers),'delivery_sha256':sha(HERE/'DELIVERY.json')},indent=2))
