import sys, struct, hashlib, json, random
from pathlib import Path
sys.path.insert(0, r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import validate_native_math as M
obj=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\debugui_std_luna_c0d955\vector2-provider\objects\vector2-bcf3b066.obj')
data=obj.read_bytes()
_, nsec, _, symoff, nsyms, optsize, _=struct.unpack_from('<HHIIIHH',data)
sections={i+1:struct.unpack_from('<8sIIIIIIHHI',data,20+optsize+i*40) for i in range(nsec)}
strings=data[symoff+nsyms*18:]
symbols={}; i=0
while i<nsyms:
 raw,value,sec,kind,storage,aux=struct.unpack_from('<8sIhHBB',data,symoff+i*18)
 name=(strings[struct.unpack_from('<I',raw,4)[0]:].split(b'\0',1)[0].decode() if raw[:4]==b'\0'*4 else raw.rstrip(b'\0').decode())
 symbols[i]=(name,value,sec); i+=1+aux
matches=[v for v in symbols.values() if v[0]=='??0Vector2@vpu@math@rw@@QAA@MM@Z' and v[2]>0]
assert len(matches)==1,matches
name,off,sec=matches[0]
s=sections[sec]; assert s[7]==0, ('constructor unexpectedly has relocations',s[7])
base=0x84000000
body=bytearray(data[s[4]:s[4]+s[3]])
report={'object':str(obj),'object_sha256':hashlib.sha256(data).hexdigest(),'symbol':name,'cases':[],'mismatches':[],'claim':'compiled PPC Vector2(float,float) full 16-byte object compared against ARTIST 0x821F0DB0; deterministic interpreter only'}
for idx,(x,y) in enumerate([(14.0,14.0),(0.0,-0.0),(-3.25,9.5),(1.0e20,-1.0e-20),(float('inf'),float('-inf'))]):
 outs=[]
 for compiled in (False,True):
  c=M.new_cpu(); c.mem.add(base,len(body),body); c.setg(3,M.S.SELF); c.setf(1,x); c.setf(2,y); c.run(base+off if compiled else 0x821F0DB0); outs.append(c.mem.rd(M.S.SELF,16))
 a,b=outs
 report['cases'].append({'index':idx,'input_f32_bits':[struct.pack('>f',x).hex(),struct.pack('>f',y).hex()],'original':a.hex(),'compiled':b.hex(),'equal':a==b,'upper_lanes_zero':a[8:]==b'\0'*8 and b[8:]==b'\0'*8})
 if a!=b: report['mismatches'].append(idx)
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\debugui_std_luna_c0d955\vector2-lane-comparison.json'); out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
assert not report['mismatches']
