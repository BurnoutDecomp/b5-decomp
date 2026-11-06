from pathlib import Path
import sys,json,capstone,re
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import rd,u32
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
p=json.loads((out/'raw-unexported-exactcoverage-allwidth-candidates.json').read_text(encoding='utf-8'))
md=capstone.Cs(capstone.CS_ARCH_PPC,capstone.CS_MODE_32|capstone.CS_MODE_BIG_ENDIAN)
summary={}
for row in p['candidates']:
 a=int(row['address'],16);w=u32(a);rA=(w>>16)&31
 ww=[u32(k) for k in range(a-12,a+16,4)]
 if 0x821f0000<=w<0x82cd0000 and sum(0x821f0000<=v<0x82cd0000 and v%4==0 for v in ww)>=5:
  row['classification']='jump_table_data';row['base']=None
 else:
  regs={i:None for i in range(32)};regs[1]='stack';ctx=[]
  for k in range(a-256,a+4,4):
   q=list(md.disasm(rd(k,4),k))
   if not q:continue
   ins=q[0];op=ins.mnemonic;args=[z.strip() for z in ins.op_str.split(',')]
   ctx.append(f'{k:08X} {op} {ins.op_str}')
   if k==a:
    val=regs[rA]
    row['base']=hex(val) if isinstance(val,int) else val
    row['classification']='absolute_global' if isinstance(val,int) and 0x82000000<=val<0x83400000 else ('stack' if val=='stack' else 'unresolved_base')
    row['context']=ctx[-20:]
    break
   get=lambda z:regs.get(int(z[1:])) if re.fullmatch(r'r\d+',z) else None
   dst=int(args[0][1:]) if args and re.fullmatch(r'r\d+',args[0]) else None
   def imm(z):
    try:return int(z,0)
    except:return None
   value=None
   if op in ('lis','li') and len(args)==2:
    v=imm(args[1]);value=(v*(65536 if op=='lis' else 1))&0xffffffff if v is not None else None
   elif op=='mr':value=get(args[1])
   elif op in ('addi','addis'):
    v=get(args[1]);n=imm(args[2]);value=(v+n*(65536 if op=='addis' else 1))&0xffffffff if isinstance(v,int) and n is not None else v if v=='stack' else None
   elif op in ('ori','oris'):
    v=get(args[1]);n=imm(args[2]);value=v | (n*(65536 if op=='oris' else 1)) if isinstance(v,int) and n is not None else None
   elif op in ('bl','bctrl'):
    for reg in range(13):regs[reg]=None
   elif op.startswith(('cmp','cmpl','st','b','mt')):dst=None
   if dst is not None:regs[dst]=value
 summary[row['classification']]=summary.get(row['classification'],0)+1
p['classification_counts']=summary
(out/'raw-unexported-exactcoverage-allwidth-classified.json').write_text(json.dumps(p,indent=2),encoding='utf-8')
print(summary)
for r in p['candidates']:
 if r['classification']=='unresolved_base':print(r)
