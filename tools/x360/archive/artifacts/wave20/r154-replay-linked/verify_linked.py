"""R154 replay/readiness carry: object provenance and machine-code binding."""
import argparse,hashlib,json,re,runpy,struct,subprocess
from pathlib import Path
W=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20'); OUT=W/'r154-replay-linked'; OLD=W/'r153-link'; NEW=W/'r154-link'; GIT='C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'; sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
ap=argparse.ArgumentParser();
for name in ('head','seal','invocation','result','xex','pe','map'):ap.add_argument('--'+name,required=True)
a=ap.parse_args();assert a.head
for name,digest in [('seal.json',a.seal),('invocation.json',a.invocation),('result.json',a.result),('native-game.exe',a.xex),('native-game.pe',a.pe),('native-game.map',a.map)]:assert sha(NEW/name)==digest,name
assert sha(W/'r153-replay-linked/RECEIPT.json')=='cd6fb09171dd1258f9d5c2ab6c973c62bac0a81fb13126df3009ad7460e6ff9f';assert sha(OLD/'invocation.json')=='6625bfaac881b5300e2e2a92f316a95fb40b4d916933165b8c027f03e5dedca9';assert sha(W/'r154-batch/TARGETED_OBJECT_BINDING.json')=='f494ffae5f31fc9b3ec103a2cdac142e487df526687aa109e69d78ed60ee6434'
helper=W/'r148-boot-presentation-linked/verify_linked.py';assert sha(helper)=='434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba';h=runpy.run_path(str(helper));seal=json.loads((NEW/'seal.json').read_text());inv=json.loads((NEW/'invocation.json').read_text());result=json.loads((NEW/'result.json').read_text());assert seal['head']==a.head and inv['sealed_sha256']==a.seal and result['exit_code']==0 and result['all_post_checks_pass'];tree=subprocess.check_output(['git','--no-optional-locks','-C',GIT,'rev-parse',a.head+'^{tree}'],text=True).strip();assert tree=='ae764bcba446118b5bc8bda12ac461f5ede4a9a5'
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
allowed_refs={'?UpdateStreaming@RaceCarEntityModule':{0x7c:'/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h',0x88:'/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'},'?UpdateOutputInterfaces@RaceCarEntityModule':{0x134:'/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h',0x138:'/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'}}
proof={};prescene=None
for name,prev in prior['r152_function_carry'].items():
 source=prev['source'];assert source in old and source in new;before,after=obj(old[source]),obj(new[source]);sa,ca,ra=before.function(name);sb,cb,rb=after.function(name);assert sa.name==sb.name==name
 if name.startswith('?PreSceneUpdate@RaceCarEntityModule'):
  prescene=(name,source,sb,cb,rb);continue
 diffs={i:(x,y) for i,(x,y) in enumerate(zip(struct.unpack('>'+str(len(ca)//4)+'I',ca),struct.unpack('>'+str(len(cb)//4)+'I',cb))) if x!=y};assert len(ca)==len(cb)
 expected={0x3c//4:(0x38a0188b,0x38a01882),0x60//4:(0x38a0188c,0x38a01883)} if name.startswith('?PostPhysicsUpdate@RaceCarEntityModule') else {};assert diffs==expected,(name,diffs)
 allow=next((v for k,v in allowed_refs.items() if name.startswith(k)),{});observed_path_refs=refs_equal(before,sa,ra,after,sb,rb,allow)
 oldblob=source_blob('bcc3e02a41cdb7bba938b91d7ab638c46962540d',source);newblob=source_blob(a.head,source);source_sha=check_source(a.head,source)
 if source not in ('src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp','src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp'):assert oldblob==newblob,source
 va,payload,mask=bind(sb,cb,rb,source);entry=prior['functions'][name]
 for off,word in entry['fixed_words'].items():off=int(off,16);word=int(word,16);assert off not in mask and struct.unpack_from('>I',payload,off)[0]==word,(name,hex(off))
 if name.startswith('?SetActiveRaceCarInterface@InputBuffer_PreScene@WorldEntityIO'):
  words=struct.unpack('>'+str(len(payload)//4)+'I',payload);assert any(w>>26==14 and w&0xffff==0x28f0 for w in words),name
 actual=[]
 for off,(kind,_) in sorted(rb.items()):
  if kind==6:
   op=struct.unpack_from('>I',payload,off)[0];assert op>>26==18;disp=op&0x03fffffc;actual.append(va+off+disp-(0x4000000 if disp&0x2000000 else 0))
 pos=0
 for prefix in entry['checked_calls']:
  matches={mapping[prefix][0]} if prefix in mapping else {address for sym,(address,_) in mapping.items() if sym.startswith(prefix)};assert len(matches)==1,(prefix,matches);pos=actual.index(next(iter(matches)),pos)+1
 proof[name]={'source':source,'r153_blob':oldblob,'r154_blob':newblob,'source_sha256':source_sha,'r153_object_sha256':old[source]['sha256'],'r154_object_sha256':new[source]['sha256'],'body_sha256':hashlib.sha256(cb).hexdigest(),'linked_va':hex(va),'linked_body_sha256':hashlib.sha256(payload).hexdigest(),'expected_line_word_changes':{hex(k*4):[hex(x),hex(y)] for k,(x,y) in expected.items()},'path_relocation_offsets':[hex(k) for k in sorted(observed_path_refs)],'checked_calls':list(entry['checked_calls'])}
assert len(proof)==15 and prescene
# Bind changed PreScene to the selected candidate source/object and PE, not to old body.
name,source,sym,code,refs=prescene; binding=json.loads((W/'r154-batch/TARGETED_OBJECT_BINDING.json').read_text());assert binding['head']=='6cb50ec8d0d34831a15440739b5c943817c7634a' and binding['tree']==tree; row,=[r for r in binding['results'] if r['source']==source]; assert source_blob(a.head,source)==row['git_blob'];assert check_source(a.head,source)==row['source_sha256'];candidate_path=Path(row['artifacts']['object']['path']);assert sha(candidate_path)==row['artifacts']['object']['sha256'];candidate=h['Coff'](candidate_path.read_bytes());cs,cc,cr=candidate.function(name);assert code==cc
x,y=h['ref_signature'](sym,refs),h['ref_signature'](cs,cr);diff={k for k in set(x)|set(y) if x.get(k)!=y.get(k)}; allowed={}
for off in diff:
 r=refs[off];_,data,_,_=obj(new[source]).section(r[1].section);s=data[r[1].value:data.index(0,r[1].value)].replace(b'\\',b'/').decode();suffix=s[s.index('/src/'):];assert suffix in ('/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h','/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h','/src/GameSource/BurnoutConstants.h');allowed[off]=suffix
refs_equal(obj(new[source]),sym,refs,candidate,cs,cr,allowed);va,payload,_=bind(sym,code,refs,source)
# Bind independent PreScene-focused verification to the same sealed image.
volume=W/'r154-racecar-prescene-linked/R154_LINKED.json';assert sha(volume)=='4ec2b7695e987fb5929d86bf60f0fce1a70042c8c2e83780d5fb175c34916551';v=json.loads(volume.read_text());assert v['head']==a.head and v['tree']==tree and v['seal_sha256']==a.seal and v['hashes']=={'pe':a.pe,'map':a.map,'xex':a.xex};volume_checks=v['checks'];assert all(volume_checks[key]['pass'] for key in ('controls','replay_drain','world_queue','cache_case','render_writer','downstream','lock_order'))
receipt={'head':a.head,'tree':tree,'pins':{'seal':a.seal,'invocation':a.invocation,'result':a.result,'xex':a.xex,'pe':a.pe,'map':a.map},'prior_receipt_sha256':sha(W/'r153-replay-linked/RECEIPT.json'),'targeted_binding_sha256':sha(W/'r154-batch/TARGETED_OBJECT_BINDING.json'),'unaffected_functions':proof,'changed_prescene':{'source':source,'candidate_blob':row['git_blob'],'candidate_object_sha256':sha(candidate_path),'official_object_sha256':new[source]['sha256'],'body_sha256':hashlib.sha256(code).hexdigest(),'linked_va':hex(va),'linked_body_sha256':hashlib.sha256(payload).hexdigest(),'assert_path_relocation_offsets':[hex(x) for x in sorted(allowed)],'independent_receipt_sha256':sha(volume),'focused_checks':volume_checks},'scope':'static linked code and scoped data-copy/readiness route, not runtime proof','copy_witnesses':['WorldMap2D six 8-byte copies via exact linked words','active race-car interface memcpy length 0x28f0 and linked target']}
(OUT/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');print('PASS',sha(OUT/'RECEIPT.json'))




