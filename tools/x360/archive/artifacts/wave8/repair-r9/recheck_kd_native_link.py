"""Recheck the actual R8/R9 two-object native constant collision, no FORCE."""
from datetime import datetime,timezone
import hashlib,json,os,struct,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;C=json.loads((HERE/'campaign.json').read_text());WORK=Path(C['workspace'])
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
base=read(C['baseline_report']);new=read(HERE/'targeted-stage1/compile-results.json');assert new['passed']==new['requested']==14
sdk=Path(new['sdk']);link=sdk/'bin/win32/link.exe';assert link.is_file()
names=['KdTreeLineQuery.cpp','CgsFineIntersectionTestModule_wSQ1.cpp'];symbol='?KU_NUM_STACK_ENTRIES@KdTreeLineQuery@collision@rw@@2IB'
def constant(p):
 b=Path(p).read_bytes();machine,n,_,off,ns,opt,_=struct.unpack_from('<HHLLLHH',b);assert machine==0x1f2;st=off+ns*18;i=0;records=[]
 while i<ns:
  at=off+i*18;raw=b[at:at+8]
  if raw[:4]==bytes(4):start=st+struct.unpack_from('<I',raw,4)[0];name=b[start:b.index(0,start)].decode()
  else:name=raw.split(bytes(1),1)[0].decode()
  value,sec=struct.unpack_from('<Ih',b,at+8)
  if name==symbol and sec>0:
   sh=20+opt+(sec-1)*40;size,data=struct.unpack_from('<II',b,sh+16);flags=struct.unpack_from('<I',b,sh+36)[0];assert value+4<=size
   words=b[data+value:data+value+4];assert words==bytes.fromhex('00000020');records.append({'symbol':name,'bytes':words.hex(),'COMDAT':bool(flags&0x1000)})
  i+=1+b[at+17]
 assert len(records)==1,(p,records)
 return records[0]
libs=['xboxkrnl.lib','xapilib.lib','libcMT.lib','libcpMT.lib'];tools={str(link):sha(link),**{str(sdk/'lib/xbox'/n):sha(sdk/'lib/xbox'/n)for n in libs}};results={}
for label,report in [('baseline_R8',base),('successor_R9',new)]:
 rows=[next(r for r in report['results']if Path(r['source']).name==n)for n in names];assert all(r['passed']for r in rows)
 out=HERE/('kd-link-'+label);out.mkdir();temp=out/'temp';temp.mkdir();target=out/'narrow-constant-diagnostic.exe'
 args=['/NOLOGO','/MACHINE:PPCBE','/SUBSYSTEM:XBOX','/ENTRY:mainCRTStartup','/OUT:'+str(target),'/LIBPATH:'+str(sdk/'lib/xbox'),*[r['object']for r in rows],*libs]
 assert not any('FORCE'in a.upper()for a in args);rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+a+'"'for a in args)+'\n',encoding='utf-8')
 env={**os.environ,'TEMP':str(temp),'TMP':str(temp),'PATH':str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32')}
 for k in ['LINK','_LINK_','CL','_CL_']:env.pop(k,None)
 p=subprocess.run([str(link),'@'+str(rsp)],cwd=out,env=env,capture_output=True,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW);log=out/'link.log';log.write_bytes(p.stdout+p.stderr)
 text=log.read_text(errors='replace');dupes=[l for l in text.splitlines()if'LNK2005'in l and'KU_NUM_STACK_ENTRIES'in l];all_dupes=[l for l in text.splitlines()if'LNK2005'in l]
 assert p.returncode==1120 and not target.exists()and ('LNK2019'in text or'LNK2001'in text)
 assert len(dupes)==(1 if label=='baseline_R8'else 0)
 symbols=[{'object':r['object'],'object_sha256':sha(r['object']),**constant(r['object'])}for r in rows]
 if label=='successor_R9':assert all(s['COMDAT']for s in symbols)
 results[label]={'returncode':p.returncode,'constant_duplicates':dupes,'all_duplicates':all_dupes,'unrelated_unresolved_remain':True,'EXE_emitted':False,'symbols':symbols,'response_file':str(rsp),'response_sha256':sha(rsp),'log':str(log),'log_sha256':sha(log)}
 print(label,p.returncode,'constant_dupes',len(dupes),flush=True)
assert all(sha(p)==h for p,h in tools.items())
result={'head':read(HERE/'repair-r9-receipts.json')['head'],'actual_new14_report_sha256':sha(HERE/'targeted-stage1/compile-results.json'),'baseline_R8_report_sha256':C['baseline_report_sha256'],
 'results':results,'native_linker_and_libs':tools,'no_FORCE':True,'scope':'Only actual KdTreeLineQuery and unchanged Fine consumer products. Native linker normally folds matching compiler COMDAT32 after redundant strong definition removal. Both narrow links retain unrelated unresolved dependencies and emit no EXE; no full game/image/launch claim.','created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'kd-actual-two-object-link-audit.json',result);print('KD_AUDIT',sha(HERE/'kd-actual-two-object-link-audit.json'),flush=True)
