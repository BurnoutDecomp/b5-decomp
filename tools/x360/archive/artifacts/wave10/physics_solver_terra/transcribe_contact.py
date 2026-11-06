import re,sys
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re')
import x360rd,vmx128
assert x360rd.u32(0x82815570)==0x7c8a2378 and x360rd.u32(0x82f264a8)==4
lines=open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave10\physics_solver_terra\contact_asm.txt').read().splitlines()
regs={'r0':('n',0),'r1':('s',0),'r11':('c',0xa0),'r10':('c',0)}
fields=['mPosA','mPosB','mRi','mUp','mAt','mVel','mComA','mComB','mIfullA','mIfullB','mIspltA','mIspltB','mForceA','mForceB','mTorqueA','mTorqueB']
def num(s):
 s=s.split(' #')[0].strip().rstrip(',')
 if s=='back_chain': return 0
 if s.startswith('var_'): return -int(s[4:],16)
 return int(s,0)
def operand(r): return regs[r]
def add(a,b):
 assert not(a[0]!='n' and b[0]!='n'), (a,b)
 return (a[0] if a[0]!='n' else b[0],a[1]+b[1])
def mem(a,b):
 t,n=add(operand(a),operand(b))
 if t=='s': return 'stack_%X'%(-n) if n<0 else 'stack_p%X'%n
 if t=='c':
  assert 0<=n<256 and n%16==0,hex(n)
  return 'lrC.'+fields[n//16]
 raise Exception(('mem',t,n,a,b))
out=[]; used=set(); stacks=set(); unsupported=[]
for line in lines:
 m=re.match(r'(0x[0-9A-F]+)\s+(\w+)\s*(.*)',line)
 if not m: continue
 ea=int(m[1],16)
 if ea<0x82bc1580 or ea>=0x82bc1a14: continue
 op=m[2]; args=[x.strip() for x in m[3].split('#')[0].split(',')]
 try:
  if op=='li': regs[args[0]]=('n',num(args[1])); continue
  if op=='addi': regs[args[0]]=add(operand(args[1]),('n',num(args[2]))); continue
  if op in ('dcbt','addic','addic.'): continue
  if op in ('lvx128','stvx128'):
   d=args[0]; addr=mem(args[1],args[2]); used.add(d)
   if addr.startswith('stack_'): stacks.add(addr)
   if op=='lvx128': expr=addr if addr.startswith('stack_') else '__lvx(&'+addr+', 0)'; code=f'{d} = {expr};'
   else: code=f'{addr} = {d};' if addr.startswith('stack_') else f'__stvx({d}, &{addr}, 0);'
   out.append(f'        {code} // {ea:08X}'); continue
  d=args[0]; assert d.startswith('v'); used.add(d)
  if op in ('vmr','vmr128'): expr=args[1]
  elif op=='vspltw': expr=f'__vspltw({args[1]}, {args[2]})'
  elif op in ('vaddfp','vsubfp','vand','vor','vcmpequw','vcmpgefp','vmrghw','vmrglw'):
   expr=f'__{op}({args[1]}, {args[2]})'
  elif op in ('vperm','vsel'):
   expr=f'__{op}({args[1]}, {args[2]}, {args[3]})'
  elif op=='vperm128': expr=f'__vperm({args[1]}, {args[2]}, {args[3]})'
  elif op in ('vmaddfp','vnmsubfp'):
   expr=f'__{op}({args[1]}, {args[3]}, {args[2]})'
  elif op=='vrefp': expr=f'__vrefp({args[1]})'
  elif op in ('vmaddfp128','vmaddcfp128','vnmsubfp128','vand128','vor128'):
   dec=vmx128.decode(x360rd.u32(ea)); aa='v'+str(dec['vA']); bb='v'+str(dec['vB']); dd='v'+str(dec['vD']); assert dd==d
   if op=='vmaddfp128': expr=f'__vmaddfp({aa}, {bb}, {dd})'
   elif op=='vmaddcfp128': expr=f'__vmaddfp({aa}, {dd}, {bb})'
   elif op=='vnmsubfp128': expr=f'__vnmsubfp({aa}, {bb}, {dd})'
   else: expr=f'__{op[:-3]}({aa}, {bb})'
  else: raise Exception('unsupported '+op)
  used.update(re.findall(r'\bv\d+\b',expr))
  out.append(f'        {d} = {expr}; // {ea:08X}')
 except Exception as e: unsupported.append((line,str(e)))
print('unsupported:',*unsupported,sep='\n')
print('registers', ' '.join(sorted(used,key=lambda a:int(a[1:])))); print('stack',sorted(stacks))
open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave10\physics_solver_terra\contact_transcription.txt','w').write('\n'.join(out)+'\n')

