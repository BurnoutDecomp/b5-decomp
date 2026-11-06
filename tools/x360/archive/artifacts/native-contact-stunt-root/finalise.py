from pathlib import Path
import hashlib,json,struct,subprocess,sys
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE=Path(__file__).resolve().parent
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def git(*a):return subprocess.check_output(['git','-C',str(ROOT),*a])
before,after=read(HERE/'source-before.json'),read(HERE/'source-after.json')
assert before['hashes']==after['hashes'] and len(before['hashes'])==6277
assert all(sha(ROOT/p)==v for p,v in before['hashes'].items())
assert git('rev-parse','HEAD^').decode().strip()==before['base']
assert set(git('diff-tree','--no-commit-id','--name-only','-r','HEAD').decode().splitlines())==set(before['owned'])
assert not git('diff','--name-only').strip()
r=read(HERE/'native/compile-results.json');assert r['state']=='complete' and r['passed']==2 and r['failed']==0
headers,artifacts={},{}
for row in r['results']:
 assert row['passed'] and row['machine']==0x1f2 and struct.unpack_from('<H',Path(row['object']).read_bytes())[0]==0x1f2
 for key in ('object','assembly','log'):artifacts[row[key]]=sha(row[key])
 for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
  if 'including file:' in line:
   path=Path(line.split('including file:',1)[1].strip()).resolve();headers[str(path)]=sha(path)
assert not any('wave4-intake' in p.lower() for p in headers)
tools=read(HERE.parent/'wave6/full-stage1-terminal-audit.json')['external_inputs']['compiler_and_runtime']
assert len(tools)==7 and all(sha(p)==v for p,v in tools.items())
sys.path.insert(0,str(PARENT/'tools/re'));import x360rd
assert x360rd.u32(0x82815570)==0x7c8a2378 and x360rd.u32(0x82f264a8)==4
assert x360rd.u32(0x8210168c)==x360rd.u32(0x82020afc)==0x7f7fffff
source=(ROOT/before['owned'][0]).read_text(encoding='utf-8')
assert all(s not in source for s in ('BRN_PROP_DIAG','sbFirstPass','sbFirstContact','getenv'))
head=git('rev-parse','HEAD').decode().strip();(HERE/'owned.patch').write_bytes(git('diff','--binary','HEAD^','HEAD'))
result={'base':before['base'],'head':head,'sources':{p:sha(ROOT/p) for p in before['owned']},
 'git_blobs':{p:git('rev-parse','HEAD:'+p).decode().strip() for p in before['owned']},
 'patch_sha256':sha(HERE/'owned.patch'),'report':str(HERE/'native/compile-results.json'),
 'report_sha256':sha(HERE/'native/compile-results.json'),'passed':2,'failed':0,
 'stable_source_count':6277,'actual_headers':headers,'header_count':len(headers),'artifacts':artifacts,'compiler_and_runtime':tools,
 'original_limits':{'0x8210168c':'0x7f7fffff','0x82020afc':'0x7f7fffff'},'reader_sha256':sha(x360rd.__file__),
 'scope':'Two CPP-only C++03/float-limit fixes and removal of empty PC diagnostic remnants; no headers, layout, API or collision algorithms changed.',
 'limits':'Existing collision scalar sqrt/reciprocal approximations remain unrecovered. Online reference-vector values are preserved, but its current scalar-POD versus native VMX ABI still belongs to the vector-family work. No full method parity, link/image or runtime claim.',
 'verifier_sha256':sha(__file__)}
with (HERE/'DELIVERY.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps({'head':head,'passed':2,'headers':len(headers),'delivery_sha256':sha(HERE/'DELIVERY.json')},indent=2))
