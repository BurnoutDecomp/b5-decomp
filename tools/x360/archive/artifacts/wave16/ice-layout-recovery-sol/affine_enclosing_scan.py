import json,re,sys
from pathlib import Path
exp=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
# Linear expressions are symbolic: dict of register/load roots plus a constant.
def e(c=0,**terms):return (tuple(sorted((k,v) for k,v in terms.items() if v)),c)
def plus(a,b):
 d=dict(a[0])
 for k,v in b[0]:d[k]=d.get(k,0)+v
 return (tuple(sorted((k,v) for k,v in d.items() if v)),a[1]+b[1])
def scale(a,n):return (tuple((k,v*n) for k,v in a[0] if v*n),a[1]*n)
def fmt(a):return '+'.join(f'{v}*{k}' for k,v in a[0])+f'{a[1]:+#x}'
def imm(s):
 try:return int(s,0)
 except:return None
def field(s):
 # IDA emitted symbol-difference constants carry the actual two addresses.
 m=re.search(r'\((?:\w+_)?([0-9A-Fa-f]{8}) - 0x([0-9A-Fa-f]{8})\)',s)
 if m:return int(m[1],16)-int(m[2],16)
 m=re.search(r'([0-9A-Fa-f]{8})@([hl])',s)
 if m:
  v=int(m[1],16)
  return ((v+0x8000)>>16) if m[2]=='h' else ((v&65535)-65536 if v&32768 else v&65535)
 return imm(s)
rows=[]
for p in exp.glob('*.json'):
 d=json.loads(p.read_text(encoding='utf-8'))
 if not (d.get('name','').startswith(('ICE::ICEController::','ICE::ICEAuthor::','ICE::ICEManager::','BrnDirector::ICEWrapper::','BrnDirector::MainDirector::','BrnDirector::DirectorModule::','BrnGame::BrnGameModule::')) or d.get('name')=='BrnDirector::MainDirector::MainDirector'):continue
 r={f'r{i}':e(**{f'arg{i}':1}) for i in range(32)};r['r0']=e();r['r3']=e(this=1)
 hits=[]
 for line in d.get('assembly','').splitlines():
  m=re.match(r'0x([\dA-Fa-f]+)\s+(\w+)\s+(.*)',line)
  if not m:continue
  va,op,args=m.groups();args=args.split('#')[0].strip();a=[x.strip() for x in args.split(',')]
  get=lambda z:r.get(z,e(**{z:1}))
  value=None
  if op=='mr' and len(a)==2:value=get(a[1])
  elif op in ('li','lis') and len(a)==2:
   v=field(a[1]);value=e(v*(65536 if op=='lis' else 1)) if v is not None else e(**{f'imm_{va}':1})
  elif op in ('ori','oris') and len(a)==3:
   v=field(a[2]);q=get(a[1]);value=e(q[1] | (v*(65536 if op=='oris' else 1))) if v is not None and not q[0] else e(**{f'bitwise_{va}':1})
  elif op in ('addi','addis') and len(a)==3:
   v=field(a[2]);value=plus(get(a[1]),e(v*(65536 if op=='addis' else 1))) if v is not None else e(**{f'add_{va}':1})
  elif op in ('add','subf') and len(a)==3:value=plus(get(a[1]),scale(get(a[2]),1)) if op=='add' else plus(get(a[2]),scale(get(a[1]),-1))
  elif op in ('mulli','slwi') and len(a)==3:
   v=field(a[2]);value=scale(get(a[1]),(2**v if op=='slwi' else v)) if v is not None else e(**{f'mul_{va}':1})
  elif op in ('lwz','lbz','lhz','lha','lfs','lfd','stw','stb','sth','stfs','stfd','std','ld') and len(a)==2:
   mm=re.match(r'(.+)\((r\d+)\)',a[1])
   if mm:
    v=field(mm[1]);addr=plus(get(mm[2]),e(v)) if v is not None else None
    if addr and dict(addr[0]).get('this')==1:hits.append({'va':'0x'+va,'op':op,'address':fmt(addr),'offset':addr[1],'source':line})
   if op.startswith('l') and a[0].startswith('r'):value=e(**{f'load_{va}':1})
  elif op in ('lwzx','lfsx','stwx','stfsx','lvx','stvx','lvx128','stvx128') and len(a)==3:
   addr=plus(e() if a[1]=='r0' else get(a[1]),get(a[2]))
   if dict(addr[0]).get('this')==1:hits.append({'va':'0x'+va,'op':op,'address':fmt(addr),'offset':addr[1],'source':line})
   if op.startswith('l') and a[0].startswith('r'):value=e(**{f'load_{va}':1})
  elif op in ('bl','bctrl'):
   vals={k:fmt(v) for k,v in r.items() if k in [f'r{i}' for i in range(3,11)] and dict(v[0]).get('this')==1}
   if vals:hits.append({'va':'0x'+va,'op':op,'callee':args,'arguments':vals,'source':line})
   if not args.startswith(('__savegprlr','__restgprlr','__savefpr','__restfpr','__savevmx','__restvmx')):
    for i in range(3,13):r[f'r{i}']=e(**{f'call_{va}_r{i}':1})
  elif len(a)>0 and a[0].startswith('r') and op not in ('cmpwi','cmpw','cmplwi','cmplw','mtctr','mtlr','mflr','mcrf','sync','b','blr') and not op.startswith(('b','st')):
   value=e(**{f'{op}_{va}':1})
  if value is not None and a and a[0].startswith('r'):r[a[0]]=value
 rows.append({'name':d['name'],'address':d['address'],'hits':hits})
(out/'affine-enclosing-accesses.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
bases={'ICE::ICEController::':0,'ICE::ICEAuthor::':0,'ICE::ICEManager::':0x1d10,'BrnDirector::ICEWrapper::':0x2750,'BrnDirector::MainDirector::':0x27a0,'BrnDirector::DirectorModule::':0x32a0,'BrnGame::BrnGameModule::':0x6b3db0}
print('enclosing functions',len(rows))
for row in rows:
 base=next((b for n,b in bases.items() if row['name'].startswith(n)),0)
 sel=[h for h in row['hits'] if 0x7200<=h.get('offset',-1)-base<0x724c or 0x7390<=h.get('offset',-1)-base<0x73b8]
 if sel:
  print(row['name'],'controller base',hex(base))
  for h in sel:print(h)
