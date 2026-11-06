"""Execute the actual ARTIST CRT permutation-table initializer; no seeded masks."""
from pathlib import Path
import hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc');from validate_startup_phase import F
ENTRY=0x82c74120;END=0x82c74368;TABLE=0x8327f140;STACK=0x40000000
class Memory(F.M.V.Mem):
 def wr(self,a,data):
  if hasattr(self,'writes')and a<0x8327f250 and a+len(data)>0x8327f130:self.writes.append([a,len(data)])
  return super().wr(a,data)
def initialise(cpu):cpu.run(ENTRY)
def main():
 rd=F.M.V.x360rd;assert rd.u32(0x82815570)==0x7c8a2378;assert rd.u32(0x82cd3cc0)==ENTRY
 errors=[];first=None;footprint=None
 for i in range(128):
  rng=random.Random(0x74120+i);m=Memory();m.add(STACK,0x10000,rng.randbytes(0x10000));m.add(TABLE-16,0x120,rng.randbytes(0x120));before=m.rd(TABLE-16,0x120);m.writes=[]
  c=F.AtomicCPU(m);c.setg(1,STACK+0x8000);c.max_steps=1000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0
  for r in range(3,32):c.setg(r,rng.getrandbits(64))
  initialise(c);after=m.rd(TABLE-16,0x120);table=m.rd(TABLE,0x100)
  assert before[:16]==after[:16]and before[-16:]==after[-16:]
  assert m.writes==[[TABLE+16*j,16]for j in range(16)],m.writes
  if first is None:first=table;footprint=m.writes
  if table!=first:errors.append({'case':i,'table':table.hex()});break
 result={'cases':i+1,'mismatches':errors,'scope':__doc__,'original_entry':hex(ENTRY),'original_end':hex(END),'crt_entry_cell':'0x82CD3CC0','crt_caller':'_cinit82934620 table82CD0010..82CD3D34','original_instruction_sha256':hashlib.sha256(rd.rd(ENTRY,END-ENTRY)).hexdigest(),'table_address':hex(TABLE),'extent_bytes':256,'table_sha256':hashlib.sha256(first).hexdigest(),'table_hex':first.hex(),'writes':[[hex(a),n]for a,n in footprint],'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Original instruction execution and extent proof in private software PPC model; no source provider, whole CRT execution or hardware claim.'};(OUT/'vpu-mask-startup-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('Actual CRT masks',i+1,'cases',len(errors),'mismatches, tableSHA',result['table_sha256']);assert not errors
if __name__=='__main__':main()
