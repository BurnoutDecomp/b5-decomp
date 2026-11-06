import json,random,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff
from validate_resource_pointer import Bundle
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-apt-place-inspection'
r=json.loads((OUT/'compile-results.json').read_text(encoding='utf-8'))
row=r['results'][0];assert row['passed']
b=Bundle([Coff(Path(row['object']),'??0AptPseudoData_t@@')]);b.select('??0AptPseudoData_t@@')
rng=random.Random(36028);errors=[];cases=0
for flags in range(256):
 for alignment in (0,4):
  src=M.S.AUX+0x100+alignment;dest=M.S.SELF
  source=bytearray(rng.randbytes(0x80));struct.pack_into('>I',source,4,flags)
  ratio=[0,0x80000000,0x3f000000,0x3f800000,0x7f800000,0xff800000,0x7fc00000,0x41200000][flags%8]
  struct.pack_into('>I',source,0x30,ratio)
  old=rng.randbytes(0x80);character=rng.randint(-32768,32767);data=rng.getrandbits(32)
  outputs=[]
  for compiled in (False,True):
   c=b.cpu();c.mem.wr(src,source);c.mem.wr(dest,old)
   c.setg(3,dest);c.setg(4,src);c.setg(5,character);c.setg(6,data)
   c.run(b.entry if compiled else 0x82ad9910)
   outputs.append((c.mem.rd(dest,0x80),c.mem.rd(src,0x80)))
  cases+=1
  if outputs[0]!=outputs[1]:errors.append({'flags':flags,'alignment':alignment,'destination_offsets':[i for i,(x,y) in enumerate(zip(outputs[0][0],outputs[1][0])) if x!=y]})
result={'cases':cases,'mismatches':errors,'original':'82AD9910','object':row['object'],'scope':'ignored candidate native place-body layout and actual existing constructor body;all256 flags at both4/8 alignments;sentinels and source bytes checked','limits':'not yet a production source change;no hardware/boot proof;constructor return ignored'}
(OUT/'candidate-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('Apt place candidate:',cases,'cases;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:4],indent=2))
raise SystemExit(bool(errors))
