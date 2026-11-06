from pathlib import Path
import hashlib,json,struct,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd,ppcdis
OUT=Path(__file__).resolve().parent;x360rd._init();assert x360rd.u32(0x82815570)==0x7C8A2378
rows={}
for a in [0x82014AF0,0x82014AE0,0x82014AD0,0x82014AC0]:
 b=x360rd.rd(a,16);rows[hex(a)]={'words':list(map(lambda n:'%08X'%n,struct.unpack('>4I',b))),'floats':list(struct.unpack('>4f',b)),'C_float_literals':[format(f,'.9g')+'f'for f in struct.unpack('>4f',b)]}
s,e,c=next(x for x in x360rd._segs if x[0]==0x821F0000);data=x360rd._m[x360rd._base+c*4:x360rd._base+(c+e-s)*4:4];sites=[]
for i in range(0,len(data)-3,4):
 w=struct.unpack_from('>I',data,i)[0]
 if w>>26==14 and w&0xFFFF==0x9AF0:sites.append({'pc':hex(s+i),'context':ppcdis.disasm(s+i-24,18)})
r={'polynomial_rows':rows,'sixty_address':'82FB9AF0','low_immediate_sites':sites,'scope':'Primary read-only raw aligned instruction/data inventory; effective address must be confirmed from context, not assumed from the low immediate.'}
p=OUT/'math-data-primary.json';assert not p.exists();p.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(rows,indent=2));print('sixty candidate sites',len(sites));print('\n'.join('\n'.join(x['context'])for x in sites))
