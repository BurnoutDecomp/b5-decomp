"""R154 replay/readiness carry: object provenance and machine-code binding."""
import argparse,hashlib,json,re,runpy,struct,subprocess
from pathlib import Path
W=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20'); OUT=W/'r155-replay-linked'; OLD=W/'r154-link'; NEW=W/'r155-link'; GIT='C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'; sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
ap=argparse.ArgumentParser();
for name in ('head','seal','invocation','result','xex','pe','map'):ap.add_argument('--'+name,required=True)
a=ap.parse_args();assert a.head
for name,digest in [('seal.json',a.seal),('invocation.json',a.invocation),('result.json',a.result),('native-game.exe',a.xex),('native-game.pe',a.pe),('native-game.map',a.map)]:assert sha(NEW/name)==digest,name
assert sha(W/'r154-replay-linked/RECEIPT.json')=='7c3f30284dfce583009c471dca3ae6a8ac50e8e3749e886a39fda702852d417d';assert sha(OLD/'invocation.json')=='8f86905da0eeb6aab9ccbca54b791b945fa73f608c4b25213a96c03bcad0e875'
helper=W/'r148-boot-presentation-linked/verify_linked.py';assert sha(helper)=='434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba';h=runpy.run_path(str(helper));seal=json.loads((NEW/'seal.json').read_text());inv=json.loads((NEW/'invocation.json').read_text());result=json.loads((NEW/'result.json').read_text());assert seal['head']==a.head and inv['sealed_sha256']==a.seal and result['exit_code']==0 and result['all_post_checks_pass'];tree=subprocess.check_output(['git','--no-optional-locks','-C',GIT,'rev-parse',a.head+'^{tree}'],text=True).strip();assert tree=='6bdcd7954fa913359b809b7506984f0dc05d90ee'
for filename,digest in [('native-game.exe',a.xex),('native-game.pe',a.pe),('native-game.map',a.map)]:assert result['outputs'][str(NEW/filename)]['sha256']==digest
prior=json.loads((W/'r153-replay-linked/RECEIPT.json').read_text());oldinv=json.loads((OLD/'invocation.json').read_text());old={x['source']:x for x in oldinv['objects']};new={x['source']:x for x in inv['objects']};assert len(prior['functions'])==16
mapdata=(NEW/'native-game.map').read_text(errors='replace');mapping={}
for line in mapdata.splitlines():
 m=re.match(r'\s+\w+:\w+\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\b.*?\s+(\S+\.obj)\s*$',line)
 if m:mapping[m[1]]=(int(m[2],16),m[3])
pe=(NEW/'native-game.pe').read_bytes();nt=struct.unpack_from('<I',pe,0x3c)[0];assert pe[nt:nt+4]==b'PE\0\0';machine,nsec,_,_,_,opt,_=struct.unpack_from('<HHIIIHH',pe,nt+4);base=struct.unpack_from('<I',pe,nt+52)[0];assert machine==0x1f2 and base==0x82000000;secs=[struct.unpack_from('<8sIIII',pe,nt+24+opt+i*40)[2:] for i in range(nsec)]
def image(va,n):
 rva=va-base
 for start,size,raw in secs:
  if start<=rva and rva+n<=start+size:return pe[raw+rva-start:raw+rva-start+n]
 raise ValueError(hex(va))
def source_blob(head,source):return subprocess.check_output(['git','--no-optional-locks','-C',GIT,'rev-parse',head+':'+source],text=True).strip()
def check_source(head,source):
 data=subprocess.check_output(['git','--no-optional-locks','-C',GIT,'show',head+':'+source]);records=[r for r in seal['sources'] if r['path'].replace('\\','/').lower().endswith('/'+source.lower())];assert len(records)==1,(source,records); raw=Path(records[0]['path']).read_bytes();digest=hashlib.sha256(raw).hexdigest();assert records[0]['sha256'].lower()==digest and raw.replace(b'\r\n',b'\n')==data.replace(b'\r\n',b'\n'),source;return digest
def obj(row):
 p=Path(row['object']);assert sha(p)==row['sha256'];return h['Coff'](p.read_bytes())
def refs_equal(ca,sa,ra,cb,sb,rb,allowed):
 x,y=h['ref_signature'](sa,ra),h['ref_signature'](sb,rb);diff={k for k in set(x)|set(y) if x.get(k)!=y.get(k)};assert diff<=set(allowed),(sa.name,diff,allowed)
 for off in diff:
  expected=allowed[off]
  for c,r in ((ca,ra),(cb,rb)):
   kind,sym=r[off];assert kind in (0x10,0x11);_,data,_,_=c.section(sym.section);text=data[sym.value:data.index(0,sym.value)].replace(b'\\',b'/');assert text.endswith(expected.encode()),(off,text)
 return diff
def bind(sym,code,refs,src):
 va,owner=mapping[sym.name];assert Path(new[src]['object']).name.lower()==owner.lower(),(sym.name,owner);payload=image(va,len(code)); masks={2:0xffffffff,6:0x03fffffc,0x10:0xffff,0x11:0xffff,0x12:0}; mask={}
 for off,(kind,_) in refs.items():mask[off]=mask.get(off,0)|masks[kind]
 for off in range(0,len(code),4):
  x,y=struct.unpack_from('>I',code,off)[0],struct.unpack_from('>I',payload,off)[0];assert not ((x^y)&~mask.get(off,0)),(sym.name,hex(off))
 return va,payload,mask

receipt154=json.loads((W/'r154-replay-linked/RECEIPT.json').read_text()); assert receipt154['head']=='13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d'
proof={}
for name,prev in prior['r152_function_carry'].items():
 source=prev['source'];assert source in old and source in new;before,after=obj(old[source]),obj(new[source]);sa,ca,ra=before.function(name);sb,cb,rb=after.function(name);assert sa.name==sb.name==name and ca==cb,name
 refs_equal(before,sa,ra,after,sb,rb,{})
 oldblob=source_blob('13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d',source);newblob=source_blob(a.head,source);assert oldblob==newblob,source;source_sha=check_source(a.head,source)
 va,payload,mask=bind(sb,cb,rb,source);entry=prior['functions'][name]
 # PreScene deliberately changed in R154: its old R153 windows do not apply.
 if not name.startswith('?PreSceneUpdate@RaceCarEntityModule'):
  for off,word in entry['fixed_words'].items():off=int(off,16);word=int(word,16);assert off not in mask and struct.unpack_from('>I',payload,off)[0]==word,(name,hex(off))
  actual=[]
  for off,(kind,_) in sorted(rb.items()):
   if kind==6:
    op=struct.unpack_from('>I',payload,off)[0];assert op>>26==18;disp=op&0x03fffffc;actual.append(va+off+disp-(0x4000000 if disp&0x2000000 else 0))
  pos=0
  for prefix in entry['checked_calls']:
   matches={mapping[prefix][0]} if prefix in mapping else {address for sym,(address,_) in mapping.items() if sym.startswith(prefix)};assert len(matches)==1,(prefix,matches);pos=actual.index(next(iter(matches)),pos)+1
 if name.startswith('?SetActiveRaceCarInterface@InputBuffer_PreScene@WorldEntityIO'):
  words=struct.unpack('>'+str(len(payload)//4)+'I',payload);assert any(w>>26==14 and w&0xffff==0x28f0 for w in words),name
 proof[name]={'source':source,'git_blob':newblob,'source_sha256':source_sha,'r154_object_sha256':old[source]['sha256'],'r155_object_sha256':new[source]['sha256'],'instruction_sha256':hashlib.sha256(cb).hexdigest(),'canonical_relocations':len(rb),'linked_va':hex(va),'linked_body_sha256':hashlib.sha256(payload).hexdigest()}
assert len(proof)==16
volume=W/'r154-racecar-prescene-linked/verify_linked.py';assert sha(volume)=='d9162ed22d19a454c85c2ae961605e5a58e5fad9f7b658da704d8734d8dcfd78';v=runpy.run_path(str(volume));im=v['Image'](NEW/'native-game.pe',NEW/'native-game.map');checks={}
for key in ('controls','replay_drain','world_queue','cache_case','render_writer','downstream','lock_order'):
 pcs=v[key](im);checks[key]=[f'{pc:#010x}:{im.word(pc):08x}' for pc in pcs]
boundary={}
for key in ('PRE','WORLD','BRIDGE','GUI_BRIDGE','GUI_CACHE','GUI_UPDATE','RENDER'):
 prefix=v[key]; names=[n for n in mapping if n.startswith(prefix)];assert len(names)==1,(key,names);name=names[0];owner=mapping[name][1]; current,=[r for r in inv['objects'] if Path(r['object']).name.lower()==owner.lower()];source=current['source'];previous=old[source];before,after=obj(previous),obj(current);sa,ca,ra=before.function(name);sb,cb,rb=after.function(name);assert ca==cb and h['ref_signature'](sa,ra)==h['ref_signature'](sb,rb),key;blob=source_blob(a.head,source);assert blob==source_blob('13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d',source);source_sha=check_source(a.head,source);va,payload,_=bind(sb,cb,rb,source);boundary[key]={'symbol':name,'source':source,'git_blob':blob,'source_sha256':source_sha,'r154_object_sha256':previous['sha256'],'r155_object_sha256':current['sha256'],'body_sha256':hashlib.sha256(cb).hexdigest(),'linked_va':hex(va),'linked_body_sha256':hashlib.sha256(payload).hexdigest(),'canonical_relocations':len(rb)}
receipt={'head':a.head,'tree':tree,'pins':{'seal':a.seal,'invocation':a.invocation,'result':a.result,'xex':a.xex,'pe':a.pe,'map':a.map},'prior_receipt_sha256':sha(W/'r154-replay-linked/RECEIPT.json'),'functions':proof,'focused_prescene_verifier_sha256':sha(volume),'focused_checks':checks,'boundary_owners':boundary,'copy_witnesses':['WorldMap2D six 8-byte copies via exact linked words','active race-car interface memcpy length 0x28f0 and linked target'],'scope':'static linked replay and readiness carry; no runtime proof'}
(OUT/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');print('PASS',sha(OUT/'RECEIPT.json'))
