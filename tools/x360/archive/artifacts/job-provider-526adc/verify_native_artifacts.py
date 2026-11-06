"""Bind native static lifetime, getter identity, field layout and actual consumers."""
from pathlib import Path
import hashlib,json,re,struct,sys
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT));from validate_startup_phase import Obj
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/tools/re');import x360rd
assert x360rd.u32(0x82815570)==0x7c8a2378
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
r=json.loads((OUT/'native-provider/compile-results.json').read_text());assert r['passed']==7 and r['failed']==0
rows={Path(x['source']).name:x for x in r['results']};obj=Obj(rows['CgsHardwareInitPS3.cpp']['object'])
def definition(o,prefix):
 a=[(i,s)for i,s in o.symbols.items()if s[0].startswith(prefix)and s[2]>0];assert len(a)==1,(prefix,a);return a[0]
def code(o,prefix):
 _,(name,value,section)=definition(o,prefix);s=o.sections[section];return name,bytearray(o.data[s[4]+value:s[4]+s[3]]),o.rels(section),section
map_targets={
 '?mJobManager@HardwareInit@':0x830ea650,
 '?mJobManagerAllocator@HardwareInit@':0x82f34160,
 '??__F?mJobManager@HardwareInit@':0x82c757a0,
 '??__F?mJobManagerAllocator@HardwareInit@':0x82c757b0,
 '??0JobScheduler@Jobs@EA@@':0x82bc9aa0,
 '??1JobScheduler@Jobs@EA@@':0x82bca5e0,
 '??0GeneralAllocator@Allocator@EA@@':0x82b4ff58,
 'atexit':0x82c09120,
 '??0ThreadParameters@Thread@EA@@':0x82b42d58,
}
job_name=((x360rd.u32(0x828d60ac)&0xffff)<<16)+(lambda x:x-0x10000 if x&0x8000 else x)(x360rd.u32(0x828d60bc)&0xffff)
def map_symbol(o,name,value,section):
 if name.startswith('??_C@'):
  s=o.sections[section];string=o.data[s[4]+value:].split(b'\0',1)[0]
  if string==b'd:\\':return 0x82046464
  if string==b'Job Manager - Job Thread':return job_name
  raise AssertionError((name,string))
 targets=[v for p,v in map_targets.items()if name.startswith(p)];assert len(targets)==1,(name,targets);return targets[0]
def relocate(o,prefix,origin):
 name,body,rels,section=code(o,prefix);mapped=[]
 for at,ix,typ in rels:
  if typ==0x12:mapped.append({'offset':at,'type':typ,'metadata':True});continue
  symbol,value,target_section=o.symbols[ix];target=map_symbol(o,symbol,value,target_section)
  word=struct.unpack_from('>I',body,at)[0]
  if typ==6:
   signed=word&0x3fffffc;signed=signed-0x4000000 if signed&0x2000000 else signed;assert signed==-at
   word=(word&0xfc000003)|((target-origin-at)&0x3fffffc)
  elif typ in(0x10,0x11):
   assert word&0xffff==0;imm=((target+0x8000)>>16)if typ==0x10 else target;word=(word&0xffff0000)|(imm&0xffff)
  else:raise AssertionError((typ,symbol))
  struct.pack_into('>I',body,at,word);mapped.append({'offset':at,'type':typ,'symbol':symbol,'native_target':target})
 native=x360rd.rd(origin,len(body));assert bytes(body)==native,(name,len(body),[(i,body[i:i+4].hex(),native[i:i+4].hex())for i in range(0,len(body),4)if body[i:i+4]!=native[i:i+4]])
 return{'symbol':name,'original':origin,'bytes':len(body),'mismatches':0,'sha256':hashlib.sha256(native).hexdigest(),'relocations':mapped}
checks=[]
for prefix,origin in [('??__E?mJobManager@HardwareInit@',0x82c6f908),('??__E?mJobManagerAllocator@HardwareInit@',0x82c6f940),('??__F?mJobManager@HardwareInit@',0x82c757a0),('?GetFOPENDirectory@HardwareInit@',0x828d60f8)]:checks.append(relocate(obj,prefix,origin))
jtp=Obj(rows['job_thread_parameters.cpp']['object']);checks.append(relocate(jtp,'??0JobThreadParameters@',0x828d6090))
_,getter,getter_relocs,_=code(obj,'?JobManager@CgsSystem@@');assert len(getter)==12
assert [t for _,_,t in getter_relocs if t!=0x12]==[0x10,0x11]
assert all(obj.symbols[ix][0].startswith('?mJobManager@HardwareInit@')for _,ix,t in getter_relocs if t!=0x12)
assert struct.unpack_from('>I',getter,8)[0]==0x4e800020
# The actual CRT table preserves the native manager-before-allocator order.
crt=[]
for sec,s in obj.sections.items():
 if s[0].rstrip(b'\0')==b'.CRT$XCU':
  for at,ix,typ in obj.rels(sec):crt.append((at,typ,obj.symbols[ix][0]))
assert len(crt)==2 and 'E?mJobManager@'in crt[0][2]and'E?mJobManagerAllocator@'in crt[1][2],crt
layout_obj=Obj(OUT/'native-hardware-layout-probe.obj');_,(n,v,s)=definition(layout_obj,'native_hardware_layout');sec=layout_obj.sections[s];layout=list(struct.unpack_from('>18I',layout_obj.data,sec[4]+v))
assert layout[2:6]==[0,8,0x28,0x29];assert layout[6:13]==[24,0,4,8,12,16,20];assert layout[15:]==[4,8,0x64000]
# Real caller undefined symbols resolve to the one provider definition by exact mangling.
provider=definition(obj,'?JobManager@CgsSystem@@')[1][0]
consumer_rows=[rows['CgsRelocator.cpp'],rows['CgsResourceBundleLoaderModule.cpp']]
scene=json.loads((OUT/'native-scene-callers/compile-results.json').read_text());assert scene['passed']==3 and scene['failed']==0
consumer_rows+=[next(x for x in scene['results']if x['source'].endswith('CgsLooseOctree.cpp'))]
consumers=[]
for row in consumer_rows:
 o=Obj(row['object']);uses=[(i,n)for i,(n,v,s)in o.symbols.items()if s==0 and n==provider];assert len(uses)==1,(row['source'],uses)
 ix=uses[0][0];references=[(sec,at,typ)for sec in o.sections for at,sym,typ in o.rels(sec)if sym==ix]
 assert references
 consumers.append({'source':row['source'],'object_sha256':sha(row['object']),'symbol':provider,'reference_count':len(references)})
headers={}
for row in r['results']+scene['results']:
 for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
  m=re.match(r'\s*Note: including file:\s*(.+?)\s*$',line)
  if m:
   p=Path(m.group(1)).resolve();ss=p.as_posix().lower();assert'/src/pc/'not in ss
   if'/src/'in ss:assert ss.startswith('d:/reverse/bp-x360-experimental-worktrees/boot-callers-526adc/')or'/vendor/'in ss or'/xdk/'in ss,ss
   headers[str(p)]=sha(p)
save('actual-header-inputs.json',{'unique_headers':len(headers),'hashes':headers})
save('native-artifact-checks.json',{'complete_relocated_functions':checks,'total_matching_bytes':sum(x['bytes']for x in checks),'getter':{'symbol':provider,'code_bytes':12,'returns_only_real_manager_symbol':True,'standalone_original_identity':'No separate X360 function export; DecFIGS free API and direct native singleton references attest the accessor.'},'crt_initializer_order':crt,'actual_layout_words':layout,'layout_scope':'Actual compiler sizes/alignments; hard equality only for independently attested fields, parameter stride and backing capacity. No guessed padding or original-size claim from neighbor spacing.','consumer_symbol_closure':consumers,'whole_cpp_passes':10,'actual_header_count':len(headers),'objects':{str(o.path):sha(o.path)for o in(obj,jtp)},'limits':'Object/caller symbol proof and explicit original-address relocation comparisons; no complete game link, worker execution or physical Xenon launch.'})
print('PASS:10 whole CPPs;3 genuine accessor consumers;CRT order and native fields;relocated bytes',sum(x['bytes']for x in checks),'headers',len(headers))
print('layout',','.join(hex(v)for v in layout))
