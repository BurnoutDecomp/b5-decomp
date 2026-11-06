from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'][0];obj=Path(r['object']);b=F.Bundle([F.Coff(obj,'?HandleStuntPerformed@HudMessageAnalyzer@BrnGui@@')]);b.select('?HandleStuntPerformed@HudMessageAnalyzer@BrnGui@@');base=F.M.S.SELF;aux=F.M.S.AUX;rng=random.Random(166)
for i in range(24):
 fail=[0,0x40000,0x80000][i%3];awesome=[4,1,2,0x10000,0x10,0x20,0x40,0x80][(i//3)%8];ev=bytearray(24);struct.pack_into('>IIIIHHHH',ev,0,fail | (0x4000 if i%2 else 0),awesome,0,1,1+(i%3),1+(i%3),i%2,1+(i%3));outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.wr(aux,ev);F.put(c,base,aux+0x100);F.put(c,aux+0x104,aux+0x200);F.put(c,base+12,aux+0x300);c.mem.wr(aux+0x500,b'Text, \0');trace=[]
  def memset(x):
   x.mem.wr(x.g(3),bytes([x.g(4)&255])*x.g(5))
  def st(x,a):return F.cstring(x,a&0xffffffff)
  def construct(x):trace.append(('construct',st(x,x.g(4))))
  def find(x):
   key=st(x,x.g(4));trace.append(('find',key));x.setg(3,(aux+0x500) if i%2 else 0)
  def adds(x):trace.append(('param',x.g(4),x.g(5),st(x,x.g(6))))
  def addi(x):trace.append(('param-int',x.g(4),x.g(5),x.g(6)&0xffffffff))
  def cat_orig(x):
   dst=x.g(3);a=st(x,dst);z=st(x,x.g(4));x.mem.wr(dst,(a+z).encode()+b'\0');x.setg(3,dst)
  def cat_new(x):
   dst=x.g(3);a=st(x,dst);z=st(x,x.g(5));x.mem.wr(dst,(a+z).encode()+b'\0')
  def fmt(x):
   trace.append(('format',st(x,x.g(6)),x.g(8)));x.mem.wr(x.g(4),b'Fmt, \0');x.setg(3,1)
  def publish(x):trace.append(('publish',))
  hooks={0x82c0b180:memset,0x824f78b0:construct,0x828646a0:find,0x824eb2c8:adds,0x82c0cb20:cat_orig,0x82865710:fmt,0x824eb508:addi,0x82517af8:publish}
  c.hooks.update(hooks)
  for a,n in b.unresolved.items():
   if n=='memset':c.hooks[a]=memset
   elif '?Construct@GuiHudMessage' in n:c.hooks[a]=construct
   elif '?FindString@LanguageManager' in n:c.hooks[a]=find
   elif '?AddParam@GuiHudMessage' in n:c.hooks[a]=adds if 'HPBD' in n else addi
   elif '?StrCat@CgsCore' in n:c.hooks[a]=cat_new
   elif '?FormatTextFromInt' in n:c.hooks[a]=fmt
   elif '?AddMessage@HudMessageDirector' in n:c.hooks[a]=publish
  c.setg(3,base);c.setg(4,aux);c.run(b.entry if native else 0x8251b608);outs.append(trace)
 assert outs[0]==outs[1],(i,outs)
report={'passed':True,'cases':24,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual HandleStuntPerformed vs ARTIST8251B608; failure/scoring and reverse/singular/plural cases, found/missing language keys; localization, formatting, message constructors, string append and director services controlled. No full GUI-frame claim.'};(D/'stunt-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
