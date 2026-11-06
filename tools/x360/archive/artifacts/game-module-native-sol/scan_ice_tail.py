from pathlib import Path
import sys,struct,json,hashlib
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');assert x360rd.u32(0x82815570)==0x7c8a2378 and x360rd.u32(0x82f264a8)==4
lo,hi=0x8252c000,0x82543000;raw=x360rd.rd(lo,hi-lo);hits=[]
for off in range(0,len(raw),4):
 w=struct.unpack_from('>I',raw,off)[0];imm=w&65535;op=w>>26
 if (imm==0x7200 or 0x7390<=imm<=0x73b4) and op in [14,32,34,36,38,40,44,48,52,58,62]:
  ea=lo+off;hits.append({'pc':hex(ea),'word':hex(w),'offset':hex(imm),'base_register':(w>>16)&31,'op':op})
  print('\n'.join(ppcdis.disasm(ea-8,5)))
(OUT/'ICE-tail-access-scan.json').write_text(json.dumps({'range':[hex(lo),hex(hi)],'bytes_sha256':hashlib.sha256(raw).hexdigest(),'hits':hits},indent=2)+'\n')
