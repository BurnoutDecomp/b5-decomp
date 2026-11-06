from pathlib import Path
import ast,hashlib,json,re,struct,sys
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-corona-r1-a26')
PATHS={}
for folder in ['core-native1','native2','manager-native4']:
 for r in json.loads((OUT/folder/'compile-results.json').read_text())['results']:
  if r['passed']:PATHS[Path(r['source']).name]=Path(r['object'])
# Reuse only the frozen instruction model class, not its test execution.
s=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-sun-retail-917/validate_draws.py')
t=ast.parse(s.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='SunCPU'],type_ignores=[]),str(s),'exec'),globals())
CPU=SunCPU
PREFIXES={'Curves.cpp':'?Evaluate@SmoothStep@Curves@BrnEffects@@SAM','rwgcoronabuffer.cpp':'?Initialize@CoronaBuffer@','rwgcoronabufferiterator.cpp':'?Write@Iterator@CoronaBuffer@','BaseResourceDescriptor.cpp':'??0BaseResourceDescriptor@','RwRGBA.cpp':'??0RGBA@rw@@','vector2.cpp':'??0Vector2@vpu@math@rw@@QAA@MM@Z','core_probe.cpp':'?Curve@CoronaAbi@','BrnCoronaManager.cpp':'?Construct@BrnCoronaManager@@','BrnCoronaTypeParams.cpp':'?GetCoronaTypeParams@'}
def bundle(names):return F.Bundle([F.Coff(PATHS[n],PREFIXES[n]) for n in names])
def ptr(c,n):return c.g(n)&0xffffffff
def put(c,a,v):F.put(c,a,v)
def select(b,prefix,rebase_globals=False):
 b.select(prefix)
 for (oi,sec),base in b.bases.items():
  obj=b.objects[oi];body=b.images[base]
  for at,ix,kind in obj.relocations(sec):
   name=obj.symbols[ix][0];target=None
   m=re.fullmatch(r'__(save|rest)fpr_(\d+)',name)
   if m:target=(0x82c0a5c4 if m[1]=='save' else 0x82c0a610)+4*(int(m[2])-27)
   if rebase_globals:
    for part,address in [('?kfCoronaFadeDistance@',0x82f242cc),('?gPropCoronaColour@',0x82f242d0),('?kfPropCoronaCurveMin@',0x82f242fc),('?kfPropCoronaCurveMid@',0x82f24300),('?kfPropCoronaCurveMax@',0x82f24304),('?lColour@',0x82fb0040),('?$S1@',0x82fb0044),('?mParams@BrnCoronaTypeParams@',0x82f24310)]:
     if name.startswith(part):target=address
   if target is None:continue
   raw=struct.unpack_from('>I',obj.data,obj.sections[sec][4]+at)[0]
   if kind in [0x10,0x11]:value=(raw&0xffff0000)|(((target+0x8000)>>16 if kind==0x10 else target)&0xffff)
   elif kind==6:value=(raw&0xfc000003)|((target-base-at)&0x03fffffc)
   elif kind==2:value=(raw+target)&0xffffffff
   else:raise AssertionError((name,kind))
   struct.pack_into('>I',body,at,value)
def cpu(b):
 c=F.cpu(b);c.__class__=CPU
 for address,size in [(0x82f24000,0x10000),(0x82fb0000,0x1000)]:c.mem.add(address,size,F.M.V.x360rd.rd(address,size))
 return c
def entry(b,prefix):return F.address(b,prefix)
def original_branch(b,object_index,callee,target):
 obj=b.objects[object_index]
 for (oi,sec),base in b.bases.items():
  if oi!=object_index:continue
  for at,ix,kind in obj.relocations(sec):
   if obj.symbols[ix][0].startswith(callee):
    assert kind==6;raw=struct.unpack_from('>I',obj.data,obj.sections[sec][4]+at)[0]
    struct.pack_into('>I',b.images[base],at,(raw&0xfc000003)|((target-base-at)&0x03fffffc))
def pin(p):p=Path(p);return {'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
def emit(name,value):
 p=OUT/name;assert not p.exists(),str(p);p.write_text(json.dumps(value,indent=2)+'\n');return p
