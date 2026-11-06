import ida_funcs,idc
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/worldentity-layout-terra');a=0x827E5358;f=ida_funcs.get_func(a);lines=['function '+(('%08X..%08X %s'%(f.start_ea,f.end_ea,idc.get_func_name(a))) if f else 'none')];lines += ['%08X %s'%(x,idc.generate_disasm_line(x,0)) for x in range(a,a+0x250,4)];(out/'worldentity-ctor-original.txt').write_text('\n'.join(lines));idc.qexit(0)
