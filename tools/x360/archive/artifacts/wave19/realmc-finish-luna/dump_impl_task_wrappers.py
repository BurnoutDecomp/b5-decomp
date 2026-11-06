import ida_auto, ida_funcs, ida_lines, ida_hexrays, idc, pathlib
ida_auto.auto_wait()
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave19\realmc-finish-luna\ida-impl-task-wrappers.txt')
with out.open('w',encoding='utf-8') as f:
  for ea in [0x82B52E90,0x82B52FA0,0x82B530A8]:
    fn=ida_funcs.get_func(ea)
    f.write(f'### {ea:08X} {idc.get_name(ea)} {idc.get_type(ea)} range={fn.start_ea:08X}..{fn.end_ea:08X}\n')
    cur=fn.start_ea
    while cur<fn.end_ea:
      line=idc.generate_disasm_line(cur,0)
      if line: f.write(f'{cur:08X} {ida_lines.tag_remove(line)}\n')
      cur=idc.next_head(cur,fn.end_ea)
    try: f.write('PSEUDOCODE:\n'+str(ida_hexrays.decompile(ea))+'\n')
    except Exception as e: f.write(f'decompile-error={e}\n')
    f.write('\n')
