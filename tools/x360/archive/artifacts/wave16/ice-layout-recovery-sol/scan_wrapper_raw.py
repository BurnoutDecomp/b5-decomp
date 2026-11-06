from pathlib import Path
import json,re,sys,struct,bisect
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd as x
assert x.u32(0x82815570)==0x7C8A2378 and x.u32(0x82F264A8)==4
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
exp=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
rs=[]
covered=bytearray((0x82CD0000-0x821F0000)//4)
for p in exp.glob('*.json'):
 d=json.loads(p.read_text(encoding='utf-8'))
 ads=[int(z,16) for z in re.findall(r'^0x([0-9A-Fa-f]+)\s',d.get('assembly',''),re.M)]
 if ads:
  rs.append((min(ads),max(ads)+4,d['name'],d['address']))
  for va in ads:
   if 0x821F0000<=va<0x82CD0000 and va%4==0:covered[(va-0x821F0000)//4]=1
rs.sort();starts=[r[0] for r in rs]
# All parent base adjustments, reduced modulo signed D-form immediate encoding.
ranges=[(0x120E0,0x120E4,0),(0x12130,0x12134,0),(0x12C30,0x12C34,0),(0x6C3740,0x6C3744,0)]
targets={a & 65535 for l,h,b in ranges for a in range(l,h)}
opnames={14:'addi',32:'lwz',34:'lbz',36:'stw',38:'stb',40:'lhz',42:'lha',44:'sth',48:'lfs',50:'lfd',52:'stfs',54:'stfd'}
rows=[];count=0;uncovered=0
for s,e,c in x._segs:
 if not (0x821F0000<=s<0x82CD0000):continue
 b=x._m[x._base+c*4:x._base+(c+e-s)*4:4]
 for k in range(0,len(b)-3,4):
  w=int.from_bytes(b[k:k+4],'big');op=w>>26
  if op not in opnames or (w&65535) not in targets:continue
  a=s+k;i=bisect.bisect_right(starts,a)-1
  rr=rs[i] if i>=0 and rs[i][0]<=a<rs[i][1] else None
  count+=1
  if covered[(a-0x821F0000)//4]:continue
  uncovered+=1
  rows.append({'address':hex(a),'word':hex(w),'op':opnames[op],'rt_rs':(w>>21)&31,'ra':(w>>16)&31,'imm':hex(w&65535),'nearest_function':rs[i][2] if i>=0 else None,'nearest_start':hex(rs[i][0]) if i>=0 else None,'context':x.rd(a-16,36).hex()})
(out/'wrapper-word-raw-exactcoverage-candidates.json').write_text(json.dumps({'ranges':ranges,'raw_candidates_total':count,'outside_exported_function_ranges':uncovered,'candidates':rows},indent=2),encoding='utf-8')
print('raw immediate candidates',count,'outside exports',uncovered)
print('candidate addresses saved')
