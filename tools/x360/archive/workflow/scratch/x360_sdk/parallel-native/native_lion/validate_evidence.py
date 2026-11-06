from pathlib import Path
import json,re,hashlib
out=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/native_lion')
exports=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
ranges={'0x8290F730':[(0x8290F968,0x8290F9D0)],'0x82913228':[(0x82913228,0x82913330)],'0x82910118':[(0x82910540,0x82910570)],'0x82911E98':[(0x82911F04,0x82911FAC)],'0x82913928':[(0x82913A44,0x82913A54),(0x82913B1C,0x82913B2C),(0x82913BE0,0x82913BF0)]}
evidence={}
for addr,windows in ranges.items():
 p=exports/(addr+'.json');j=json.loads(p.read_text(encoding='utf8'));lines=[]
 for line in j['assembly'].splitlines():
  m=re.match(r'0x([0-9A-Fa-f]+)',line)
  if m and any(a<=int(m[1],16)<b for a,b in windows):lines.append(line)
 evidence[addr]={'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'name':j['name'],'assembly':lines}
probe_dir=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/parallel/native_lion/header-probe')
probe=next(probe_dir.glob('*.asm')).read_text(encoding='utf8',errors='replace')
checks={}
for name in ['ConstructTime','ConstructValueTime','InitialiseTime','SlotHasValue','SlotNoValue','SlotExplicitTest','SlotRead','SlotRelocate']:
 b=re.search(r'^'+name+r'\s+PROC.*?^'+name+r'\s+ENDP',probe,re.M|re.S)
 assert b,name
 lines=[x.strip() for x in b[0].splitlines() if re.match(r'\s+[0-9a-f]{5}\s+[0-9a-f]{8}\s',x,re.I)]
 checks[name]=lines
assert len(checks['ConstructTime'])==1 and checks['ConstructTime'][0].endswith('blr')
assert len(checks['ConstructValueTime'])==1 and checks['ConstructValueTime'][0].endswith('blr')
assert any('stw' in l for l in checks['InitialiseTime'])
emitter=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/parallel/native_lion/final-owned/ParticleEmitter-62480770.asm').read_text(errors='replace')
assert 'vrfin' in emitter
assert '?Lerp@cParticleBehaviour@@' in emitter
for name in ['MatrixSimulationHelper','VectorSimulationHelper','LocalSimulationHelper']:
 assert '??$SimulateParticlesInBucketGeneral@V'+name in emitter,name
(out/'native-evidence.json').write_text(json.dumps({'artist':evidence,'probe_instructions':checks,'emitter_checks':{'vrfin':True,'behaviour_Lerp':True,'three_helper_variants':True}},indent=2)+'\n',encoding='utf8')
print('Recorded ARTIST windows, native constructor/slot streams, Lerp and vrfin and three helper instantiations.')
