from pathlib import Path
import sys,json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/massive-socket-native-terra/final');row=json.loads((out/'compile-results.json').read_text())['results'][0];obj=F.Coff(Path(row['object']),'??0CMassiveSocket@MassiveAdClient3@@');b=F.Bundle([obj]);SELF=F.M.S.SELF;AUX=F.M.S.AUX;MASK=0xffffffff
specs=[('ctor','??0CMassiveSocket@MassiveAdClient3@@',0x82bd8220),('dtor','??1CMassiveSocket@MassiveAdClient3@@',0x82bd8278),('send','?Send@CMassiveSocket@MassiveAdClient3@@',0x82bd8340),('recv','?Receive@CMassiveSocket@MassiveAdClient3@@',0x82bd8370),('select','?IsConnected@CMassiveSocket@MassiveAdClient3@@',0x82bd82c8),('error','?ProcessError@CMassiveSocket@MassiveAdClient3@@',0x82bd83a0),('disconnect','?Disconnect@CMassiveSocket@MassiveAdClient3@@',0x82bd8598)]
def branch(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x3fffffc;return (pc+(d-0x4000000 if d&0x2000000 else d))&MASK
orig={'base_ctor':branch(0x82bd823c),'base_dtor':branch(0x82bd82ac),'close':branch(0x82bd82a4),'send':branch(0x82bd8354),'recv':branch(0x82bd8384),'select':branch(0x82bd8308),'error':branch(0x82bd83b4)}
def fixture(kind,comp,handle,rv,err,seed):
 c=F.cpu(b);data=bytearray(random.Random(seed).randbytes(0x100));struct.pack_into('>I',data,0x14,handle&MASK);c.mem.wr(SELF,data);c.mem.wr(AUX,bytes(range(64)));trace=[]
 def hook(name):
  def f(x):
   if name=='base_ctor':trace.append([name,x.g(3)&MASK,F.cstring(x,x.g(4)&MASK)]);F.put(x,SELF+4,0x13579bdf);x.setg(3,SELF)
   elif name=='base_dtor':trace.append([name,x.g(3)&MASK]);F.put(x,SELF,0xabcdef01)
   elif name=='close':trace.append([name,x.g(3)&MASK]);x.setg(3,rv)
   elif name in ('send','recv'):trace.append([name]+[x.g(i)&MASK for i in range(3,7)]);x.setg(3,rv)
   elif name=='error':trace.append([name]);x.setg(3,err)
   elif name=='select':
    a=x.g(5)&MASK;t=x.g(7)&MASK;trace.append([name,x.g(3)&MASK,x.g(4)&MASK,x.g(6)&MASK,int.from_bytes(x.mem.rd(a,4),'big'),int.from_bytes(x.mem.rd(a+4,4),'big'),x.mem.rd(t,8).hex()]);x.setg(3,rv)
  return f
 for n,a in orig.items():c.hooks[a]=hook(n)
 m={'??0CMassiveBaseObject@':'base_ctor','??1CMassiveBaseObject@':'base_dtor','closesocket':'close','send':'send','recv':'recv','select':'select','WSAGetLastError':'error'}
 for a,n in b.unresolved.items():
  for k,v in m.items():
   if n==k or n.startswith(k):c.hooks[a]=hook(v);break
 c.setg(3,SELF);c.setg(4,AUX);c.setg(5,37);c.run(b.entry if comp else next(z for k,p,z in specs if k==kind));state=bytearray(c.mem.rd(SELF,0x100))
 if comp and kind=='ctor':
  vt=F.address(b,'??_7CMassiveSocket@MassiveAdClient3@@');assert int.from_bytes(state[:4],'big')==vt;struct.pack_into('>I',state,0,0x82186aac)
 return {'return':c.g(3)&MASK if kind not in ('dtor',) else None,'state':state.hex(),'buffer':c.mem.rd(AUX,64).hex(),'calls':trace}
errors=[];groups=[]
for kind,prefix,address in specs:
 b.select(prefix);cases=0
 for handle in (0xffffffff,0,0x12345678):
  for rv in (-1,0,1,4):
   for err in (0,10035,10038,10053,10054,10056):
    seed=cases+900;left=fixture(kind,False,handle,rv,err,seed);right=fixture(kind,True,handle,rv,err,seed);cases+=1
    if left!=right:errors.append({'kind':kind,'handle':handle,'rv':rv,'error':err,'original':left,'native':right});break
   if errors:break
  if errors:break
 groups.append({'kind':kind,'original':hex(address),'cases':cases})
 if errors:break
result={'scope':'Actual native object versus ARTIST constructor, destructor, send, receive, select, ProcessError and Disconnect. Base constructor/destructor and OS socket calls controlled; ctor vtable address normalized. Complete owner bytes, buffers, service arguments/order and non-void returns compared. No real network or full vendor ABI claim.','groups':groups,'cases':sum(g['cases'] for g in groups),'mismatches':errors,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'socket-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('cases',result['cases'],'errors',len(errors));print(json.dumps(errors[:1],indent=2)[:5000]);assert not errors
