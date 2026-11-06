from pathlib import Path
import sys,struct,json,hashlib
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/eventinfo-constants-sol');OUT.mkdir(exist_ok=True)
assert x360rd.u32(0x82815570)==0x7c8a2378
assert x360rd.u32(0x82f264a8)==4
x360rd._init()
targets=[0x82fb29e0,0x82fb2ac0,0x82fb2a00,0x82fb2a90,0x82fb2be0,0x82fb2d70]
# Read only bounded raw CRT code; identify the exact six low16 addresses,
# plus the independently attested RoadRule example as a control.
lo,hi=0x82c50000,0x82c54000
raw=x360rd.rd(lo,hi-lo);hits=[]
for i in range(0,len(raw),4):
 w=struct.unpack_from('>I',raw,i)[0];imm=w&65535;op=w>>26
 if imm in[(t&65535)for t in targets+[0x82fb2ad0]] and op in[14,24,32,36,48,52,58,62]:
  ea=lo+i;hits.append({'pc':hex(ea),'word':hex(w),'matching_low16':hex(imm),'op':op})
  name='%08X-context.asm'%ea
  (OUT/name).write_text('\n'.join(ppcdis.disasm(ea-24,26))+'\n')
report={'raw_reader':str(Path(x360rd.__file__)),'raw_reader_sha256':hashlib.sha256(Path(x360rd.__file__).read_bytes()).hexdigest(),'raw_image':x360rd.RAW,'sentinels':{'82815570':hex(x360rd.u32(0x82815570)),'82F264A8':x360rd.u32(0x82f264a8)},'mapped_segments':x360rd._segs,'scan_range':[hex(lo),hex(hi)],'scan_bytes_sha256':hashlib.sha256(raw).hexdigest(),'hits':hits,'BSS_snapshot_only_not_runtime':{hex(t):x360rd.rd(t,16).hex()for t in targets}}
(OUT/'INITIAL_SCAN.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(hits,indent=2));print('segments',x360rd._segs)
