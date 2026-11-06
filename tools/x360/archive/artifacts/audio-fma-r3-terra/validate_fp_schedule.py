import hashlib,json,re
from pathlib import Path
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/audio-fma-r3-terra');d=p/'native';rows={}
for n in ['Xas1Enc','XasEnc','Butterworth']:
 f=d/(n+'.asm');s=f.read_text(encoding='utf-8');lines=s.splitlines();fused=[x.strip() for x in lines if re.search(r'\bfmadds\b',x)];rows[n]={'asm_sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'fmadds_count':len(fused),'fmadds_sample':fused[:8]};assert fused
s=(d/'Xas1Enc.asm').read_text(encoding='utf-8');begin=s.index('; 225  :');end=s.index('; 227  :',begin);assert 'fmuls' in s[begin:end];end2=s.index('; 228  :',end);assert 'fmuls' in s[end:end2];end3=s.index('; 227  :                 if',end2) if '; 227  :                 if' in s[end2:] else s.index('$LN39@EncodeBloc',end2);assert 'fadds' in s[end2:end3] and 'fmadds' not in s[begin:end3];rows['Xas1Enc']['closed_loop']='two fmuls, volatile f32 stores/loads, fadds; no fmadds in prediction window'
(p/'native-fp-schedule.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(rows,indent=2))

