"""Check the two actual caller objects, including complete Effects bridge relocation parity."""
from pathlib import Path
import hashlib,json,re,struct,sys
OUT=Path(__file__).resolve().parent
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/boot-callers-526adc')
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(PARENT/'tools/re'))
import x360rd
assert x360rd.u32(0x82815570)==0x7c8a2378
r=json.loads((OUT/'native-two-callers/compile-results.json').read_text());assert r['state']=='complete'and r['passed']==2 and r['failed']==0

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
def function_object(row,prefix):
 b=Path(row['object']).read_bytes();machine,ns,_,ptr,nsy,opt,_=struct.unpack_from('<HHIIIHH',b);assert machine==0x1f2
 sp=ptr+nsy*18;st=b[sp:sp+struct.unpack_from('<I',b,sp)[0]];syms={};i=0
 while i<nsy:
  name,value,sect,ty,storage,aux=struct.unpack_from('<8sIhHBB',b,ptr+i*18)
  if name[:4]==b'\0'*4:name=st[struct.unpack_from('<I',name,4)[0]:].split(b'\0',1)[0]
  name=name.split(b'\0',1)[0].decode(errors='replace');syms[i]=(name,value,sect,ty,storage,aux);i+=1+aux
 fn=next(v for v in syms.values()if v[0].startswith(prefix)and v[2]>0 and v[3]==0x20)
 sp=20+opt+(fn[2]-1)*40;size,raw,rel=struct.unpack_from('<III',b,sp+16);nrel=struct.unpack_from('<H',b,sp+32)[0]
 assert fn[1]==0
 code=bytearray(b[raw:raw+size]);relocs=[]
 for i in range(nrel):
  off,symi,typ=struct.unpack_from('<IIH',b,rel+i*10);relocs.append({'offset':off,'type':typ,'symbol':syms[symi][0]})
 return fn[0],code,relocs
controller=next(x for x in r['results']if x['source'].endswith('GameBridgeGameStateToX_Controller.cpp'))
name,code,rels=function_object(controller,'?BridgeGameStateToController@')
assert struct.unpack_from('>I',code,0xcc)[0]==0x39600003
assert struct.unpack_from('>I',code,0xd0)[0]==0x7d79d12e
assert struct.unpack_from('>I',code,0x114)[0]==0x39600006
assert struct.unpack_from('>I',code,0x118)[0]==0x7d79d12e
controller_evidence={'symbol':name,'object_sha256':sha(controller['object']),'compiled_bind_li_store_offsets':[0xcc,0xd0],'compiled_unbind_li_store_offsets':[0x114,0x118],'values':[3,6],'original_bind_instructions':['823C0BC4 li r11,3','823C0BC8 stwx r11,r27,r30'],'original_unbind_instructions':['823C0C1C li r11,6','823C0C20 stwx r11,r27,r30'],'limits':'Exact original enum values and ordering, using actual current Main field. This does not prove Main layout; actual field displacement remains the documented +8B00.'}
renderer=next(x for x in r['results']if x['source'].endswith('GameBridgeRendererToX.cpp'))
name,code,rels=function_object(renderer,'?BridgeRendererToEffects@');assert len(code)==0x8c
original=json.loads((OUT/'original-823C1168.json').read_text());targets={x['name']:int(x['address'],16)for x in original['xrefs_from']}
rest_path=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82C08F38.json')
rest=json.loads(rest_path.read_text());assert rest['name']=='__restgprlr_28'
targets[rest['name']]=int(rest['address'],16)
map_names={
 '__savegprlr_28':'__savegprlr_28','__restgprlr_28':'__restgprlr_28',
 'GetDispatchFrame@OutputBuffer':'RendererIO::OutputBuffer::GetDispatchFrame',
 'GetBaseEffectsFrame@OutputBuffer':'RendererIO::OutputBuffer::GetBaseEffectsFrame',
 'SetBaseEffectsFrame@DispatchInputBuffer':'BrnEffects::EffectsIO::DispatchInputBuffer::SetBaseEffectsFrame',
 'GetFXEventsEffectsFrame@OutputBuffer':'RendererIO::OutputBuffer::GetFXEventsEffectsFrame',
 'SetFXEventsEffectsFrame@DispatchInputBuffer':'BrnEffects::EffectsIO::DispatchInputBuffer::SetFXEventsEffectsFrame',
 'SetEnvironmentMap@DispatchInputBuffer':'BrnEffects::EffectsIO::DispatchInputBuffer::SetEnvironmentMap'}
origin=0x823c1168;resolved=[]
for rel in rels:
 off,kind,symbol=rel['offset'],rel['type'],rel['symbol'];word=struct.unpack_from('>I',code,off)[0]
 if kind==0x12:
  resolved.append({**rel,'kind':'PPC PAIR metadata; no separate instruction'});continue
 if kind==6:
  keys=[k for k in map_names if k in symbol];assert len(keys)==1,(symbol,keys)
  identity=map_names[keys[0]];target=targets[identity]
  disp=word&0x3fffffc;disp=disp-0x4000000 if disp&0x2000000 else disp
  assert disp==-off
  word=(word&0xfc000003)|((target-origin-off)&0x3fffffc)
 elif kind in (0x10,0x11):
  assert symbol=='?gTextureScopeTable@CgsGraphics@@3UTextureScopeTable@1@A'
  target=0x83011a78;identity='CgsGraphics::gTextureScopeTable (ARTIST registry83011A78)'
  assert word&0xffff==0
  immediate=((target+0x8000)>>16)&0xffff if kind==0x10 else target&0xffff
  word=(word&0xffff0000)|immediate
 else:raise AssertionError(rel)
 struct.pack_into('>I',code,off,word);resolved.append({**rel,'identity':identity,'original_address':target})
native=x360rd.rd(origin,0x8c);assert bytes(code)==native
(OUT/'effects-bridge-relocated.bin').write_bytes(code);(OUT/'effects-bridge-original.bin').write_bytes(native)
renderer_evidence={'symbol':name,'object_sha256':sha(renderer['object']),'original_origin':origin,'bytes_compared':len(native),'instruction_words':len(native)//4,'mismatches':0,'original_and_relocated_sha256':hashlib.sha256(native).hexdigest(),'relocations':resolved,'symbol_address_sources':'Original function xrefs_from plus exact named restgprlr export82C08F38 and the actual83011A78 registry used by original instructions.','limits':'One complete140-byte caller function after resolving object relocations to original symbol addresses. Not a game-provider link or a claim the shared callee bodies/layouts match.'}
inputs={}
for row in r['results']:
 for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
  m=re.match(r'\s*Note: including file:\s*(.+?)\s*$',line)
  if m:
   p=Path(m.group(1)).resolve();s=p.as_posix().lower()
   assert '/src/pc/'not in s
   if '/src/'in s and not s.startswith(ROOT.as_posix().lower()+'/'):
    assert '/vendor/'in s or '/xdk/'in s,s
   inputs[str(p)]=sha(p)
 for kind in ('object','assembly','log'):assert Path(row[kind]).is_file()
assert inputs
save('actual-header-inputs.json',{'unique_headers':len(inputs),'hashes':inputs})
save('native-artifact-checks.json',{'controller':controller_evidence,'renderer_to_effects':renderer_evidence,'source_before_after_equal':json.loads((OUT/'compiler-source-before.json').read_text())['hashes']==json.loads((OUT/'compiler-source-after.json').read_text())['hashes'],'actual_header_input_count':len(inputs),'whole_cpp_passes':2,'whole_main_compiled':False,'production_layout_guards_modified':False})
print('PASS: two whole native CPPs; enum3/6 stores; complete relocated Effects bridge140/140 bytes; actual includedheaders',len(inputs))
