from pathlib import Path
import sys,struct,json,bisect
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');x360rd._init();s,e,c=next(t for t in x360rd._segs if t[0]==0x821f0000)
raw=x360rd._m[x360rd._base+c*4:x360rd._base+(c+e-s)*4:4]
known=ppcdis.names();keys=sorted(known);hits=[]
for off in range(0,len(raw)-3,4):
 w=struct.unpack_from('>I',raw,off)[0];imm=w&65535;op=w>>26
 if (imm==0x7200 or 0x7390<=imm<=0x73b4)and op in[14,32,34,36,38,40,44,48,52,58,62]:
  ea=s+off;k=bisect.bisect_right(keys,ea)-1;fn=keys[k]if k>=0 else 0
  hits.append({'pc':hex(ea),'word':hex(w),'offset':hex(imm),'base_register':(w>>16)&31,'op':op,'nearest_function':hex(fn),'name':known.get(fn)})
(OUT/'ICE-tail-global-code-access-scan.json').write_text(json.dumps({'code_range':[hex(s),hex(e)],'hits':hits},indent=2)+'\n')
print('hits',len(hits))
for h in hits:print(h)
