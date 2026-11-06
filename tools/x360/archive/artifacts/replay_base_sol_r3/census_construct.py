from pathlib import Path
import bisect,hashlib,json,struct,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd,ppcdis
OUT=Path(__file__).resolve().parent
EX=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
x360rd._init();assert x360rd.u32(0x82815570)==0x7C8A2378
s,e,c=next(x for x in x360rd._segs if x[0]==0x821F0000)
data=x360rd._m[x360rd._base+c*4:x360rd._base+(c+e-s)*4:4]
starts=sorted(int(p.stem,16)for p in EX.glob('0x*.json'));calls=[]
for i in range(0,len(data)-3,4):
    word=struct.unpack_from('>I',data,i)[0]
    if word>>26!=18:continue
    disp=word&0x3FFFFFC
    if disp&0x2000000:disp-=0x4000000
    pc=s+i;target=(disp if word&2 else pc+disp)&0xFFFFFFFF
    if target!=0x8264C280:continue
    start=starts[bisect.bisect_right(starts,pc)-1];p=EX/('0x%08X.json'%start);d=json.loads(p.read_text())
    calls.append({'pc':hex(pc),'nearest_function':d['name'],'export':str(p),'export_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'context':ppcdis.disasm(pc-48,15)})
report={'target':'0x8264C280','scope':'Calibrated aligned direct branches in mapped text; computed/unloaded references are not covered. Caller contexts must be read for actual register provenance.','calls':calls,'complete_constructor_assembly':ppcdis.disasm(0x8264C280,124),'constructor_bytes_sha256':hashlib.sha256(x360rd.rd(0x8264C280,0x1F0)).hexdigest(),'default_name_bytes':x360rd.rd(0x8209DF88,9).hex(),'type_boundary':'Retained PS3 BaseSerialiser type has no Xbox18/1C fields. Original semantic names/types beyond two word stores are not recovered; do not invent bit-buffer roles.'}
p=OUT/'construct-primary-inventory.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n')
print('direct Base Construct branches',len(calls));print([(c['pc'],c['nearest_function'])for c in calls])
