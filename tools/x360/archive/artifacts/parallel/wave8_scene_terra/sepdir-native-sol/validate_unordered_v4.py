from pathlib import Path
import ast,hashlib,json,struct
p=Path(__file__).with_name('validate_drivers_v3.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='reports'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent
final=json.loads((OUT/'candidate-v4/compile-results.json').read_text())['results'][0]
objects[0]=PackedCoff(Path(final['object']),'?FindBestSeparatingDirection@');bundle=ns['Bundle'](objects);bundle.select('?FindBestSeparatingDirection@')
table=F.address(bundle,'?g_aGPVolumeMethods@');records=[];coverage={'flip_unordered':0,'best_unordered':0}
for word in [0x7fc12345,0xffc54321,0x7f800000,0xff800000,0x7fa12345,0xffa12345]:
 region=0x57000000;image=bytearray(b'\xA5'*0x600);target=region+0x400
 for j,ty in [(0,1),(0x100,4)]:
  pos=[frombits(word),.5,-.3,.1]if j==0 else[1.,2.,3.,4.]
  for off,v in [(0,pos),(0x10,[1,0,0,.2]),(0x20,[0,1,0,-.4]),(0x30,[0,0,1,.6]),(0x40,[1,0,0,.2]),(0x50,[0,1,0,-.4]),(0x60,[0,0,1,.6]),(0x70,[1,2,3,.4])]:struct.pack_into('>4f',image,j+off,*v)
  image[j+0x8c]=0 if j==0 else 3;image[j+0x8d]=0 if j==0 else 3;struct.pack_into('>I',image,j+0x90,ty)
 outputs=[]
 for compiled in [False,True]:
  c=basecpu();initial=bytearray(image)
  for j,ty in [(0,1),(0x100,4)]:initial[j+0xa4:j+0xb4]=c.mem.rd(table+ty*16,16)if compiled else F.M.V.x360rd.rd(0x82F918F0+ty*16,16)
  c.mem.add(region,len(initial),initial);c.setg(3,target);c.setg(4,region);c.setg(5,region+0x100)
  class Observe(type(c)):
   def fetch(s,pc):
    if not compiled and pc in [0x82BB4770,0x82BB4790]and s.cr[6]&1:coverage['flip_unordered'if pc==0x82BB4770 else'best_unordered']+=1
    return super().fetch(pc)
  c.__class__=Observe;c.run(bundle.entry if compiled else 0x82BB4660)
  outputs.append((list(c.vr[1]),c.mem.rd(target,16)))
 assert outputs[0]==outputs[1],(hex(word),outputs)
 assert outputs[0][1]==struct.pack('>4f',0,0,1,.6),outputs
 records.append({'input_word':hex(word),'return_v1':list(map(hex,outputs[0][0])),'direction':outputs[0][1].hex()})
p=OUT/'unordered-generic-v4-comparison.json';assert not p.exists();p.write_text(json.dumps({'cases':records,'native_coverage':coverage,'object_sha256':hashlib.sha256(objects[0].path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'scope':'Actual original and final generic driver with real sphere/box interval providers and PMF table. Unordered CR.GT=0 at both original ble sites must retain first axis and avoid sign flip. Six specific NaN/infinity counterexamples; same software IEEE/VMX model, no hardware NaN-payload certification.'},indent=2)+'\n');print('PASS',len(records),coverage)
