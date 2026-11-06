from pathlib import Path
import json,re
src=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX');rows=[];n=0
for f in src.glob('*.json'):
 try:
  d=json.loads(f.read_text(encoding='utf8')); name=d.get('name','')
  if not name.startswith('BrnWorld::RaceCarEntityModule::'):continue
  n+=1; regs={3:('this',0)}
  for l in d.get('assembly','').splitlines():
   z=re.match(r'(0x[0-9A-Fa-f]+)\s+(\w+)\s+(.*)',l)
   if not z:continue
   addr,op,args=z.groups();args=args.split('#')[0].strip(); parts=[v.strip() for v in args.split(',')];
   def reg(v):
    m=re.fullmatch(r'r(\d+)',v);return int(m.group(1)) if m else None
   def num(v):
    try:return int(v,0)
    except:return None
   dest=reg(parts[0]) if parts else None
   mem=re.search(r'(-?(?:0x[\da-fA-F]+|\d+))\(r(\d+)\)',args)
   if mem:
    off,base=num(mem.group(1)),int(mem.group(2));v=regs.get(base)
    if v and v[0]=='this' and 0x185f6<=v[1]+off<0x186c1:rows.append({'function':name,'line':l,'offset':hex(v[1]+off),'base_origin':hex(v[1])})
   handled=False
   if op=='mr' and len(parts)>=2 and dest is not None:
    src2=reg(parts[1]);regs[dest]=regs.get(src2);handled=True
   elif op in ('addi','addis') and len(parts)>=3 and dest is not None:
    a=reg(parts[1]);v=regs.get(a);imm=num(parts[2]);regs[dest]=(v[0],v[1]+imm*(65536 if op=='addis' else 1)) if v and imm is not None else None;handled=True
   elif op=='lis' and dest is not None and len(parts)>=2:
    imm=num(parts[1]);regs[dest]=('const',(imm<<16)&0xffffffff) if imm is not None else None;handled=True
   elif op=='li' and dest is not None and len(parts)>=2:
    imm=num(parts[1]);regs[dest]=('const',imm) if imm is not None else None;handled=True
   elif op=='ori' and len(parts)>=3:
    # PPC ori rA,rS,imm
    a=reg(parts[0]);b=reg(parts[1]);v=regs.get(b);imm=num(parts[2]);regs[a]=(v[0],v[1]|imm) if v and v[0]=='const' and imm is not None else None;handled=True
   elif op=='add' and len(parts)>=3 and dest is not None:
    v=regs.get(reg(parts[1]));w=regs.get(reg(parts[2]));regs[dest]=(v[0],v[1]+w[1]) if v and w and w[0]=='const' else (w[0],w[1]+v[1]) if v and w and v[0]=='const' else None;handled=True
   if not handled and dest is not None and not op.startswith(('st','cmp','b','mt','lv','v','f')):regs.pop(dest,None)
   if op in ('bl','bctrl') and '__save' not in args and '__rest' not in args:
    for j in range(3,13):regs.pop(j,None)
 except Exception:continue
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume/racecar-member-scan.json');out.write_text(json.dumps({'functions_scanned':n,'hits':rows,'limits':'Simple linear GPR propagation only; no CFG join, indirect indexing or callee analysis. Not a complete absence proof.'},indent=2),encoding='utf8');print('functions',n,'hits',len(rows));print('\n'.join(str(x) for x in rows[:30]))
