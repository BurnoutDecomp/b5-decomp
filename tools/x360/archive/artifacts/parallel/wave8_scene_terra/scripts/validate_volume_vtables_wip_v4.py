import sys,json,hashlib,struct
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeVTables-wip-v4.obj';o=F.Coff(obj,'?gVolumeHandler_82F91740@')
rows=[('Sphere',0x82f91740,[0x82ba8020,0x82ba8580,0x82bac980,0x82ba81b0,0x82ba8100,0x82ba82c8,0x82ad5078]),('Capsule',0x82f918c0,[0x82baf9c8,0x82baf6a8,0x82bac980,0x82baf808,0x82baf6f0,0x82bafcf8,0x82ad5078]),('Triangle',0x82f919a4,[0x82bba038,0x82bba110,0x82bac980,0x82bbacd0,0x82bbaa00,0x82bbb970,0x82ad5078]),('Box',0x82f9176c,[0x82ba9fc8,0x82ba8890,0x82bac980,0x82ba87f0,0x82ba92e8,0x82ba9478,0x82ad5078]),('Cylinder',0x82f91894,[0x82bad490,0x82bac7b8,0x82bac980,0x82bac988,0x82bac7f8,0x82baf688,0x82ad5078]),('Aggregate',0x82f919d0,[0x82bbba10,0x82bbbb58,0,0x82b0f1b8,0x82b0f1b8,0,0x82ad5078])]
slots=['GetBBox','GetBBoxDiag','GetInterval','GetMaximumFeature','CreateGPInstance','LineSegIntersect','Release'];report=[];bad=[]
for idx,(name,addr,expected) in enumerate(rows):
 syms=[(n,v,sec) for n,v,sec in o.symbols.values() if n.startswith('?gVolumeHandler_%08X@'%addr)]
 assert len(syms)==1,(name,syms)
 _,off,sec=syms[0];section=o.sections[sec];raw=o.data[section[4]+off:section[4]+off+0x28];orig=x360rd.rd(addr,0x28);original=list(struct.unpack('>10I',orig));actual=list(struct.unpack('>10I',raw));rel={at-off:o.symbols[ix][0] for at,ix,kind in o.relocations(sec) if off<=at<off+0x28 and kind==2}
 errs=[]
 if original[0] != idx+1 or actual[0] != idx+1 or original[9] != 0 or actual[9] != 0:errs.append('type or flags')
 for j,(slot,ea) in enumerate(zip(slots,expected)):
  delta=4+j*4;sym=rel.get(delta)
  if original[j+1]!=ea:errs.append('original %s'%slot)
  if ea==0:
   if sym is not None or actual[j+1]!=0:errs.append('null %s'%slot)
  elif not sym or not sym.startswith('?%s@%sVolume@'%(slot,name)):errs.append('relocation %s: %s'%(slot,sym))
 if 0x20 not in rel:errs.append('name relocation')
 bad+= [(name,e) for e in errs];report.append({'type':name,'source_address':hex(addr),'expected_targets':[hex(e) for e in expected],'relocations':{hex(k):v for k,v in rel.items()},'errors':errs})
print('records',len(rows),'mismatches',len(bad),bad[:3]);assert not bad
(out/'volume-vtables-direct-v4-comparison.json').write_text(json.dumps({'records':report,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'six actual XDK data records: type, flags, seven original targets and exact direct member relocations/nulls; method bodies/link readiness separately gated'},indent=2))
