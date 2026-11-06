import ida_typeinf,ida_funcs,ida_ua,ida_lines,idautils,idc,json,os
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\network-providers-terra\ida-batch5';os.makedirs(out,exist_ok=True)
r={'types':{},'displacement_refs':[]}
for n in ['HLBApiRefT','HLBBudT','HLBPresenceT']:
 try:
  t=ida_typeinf.tinfo_t();ok=t.get_named_type(ida_typeinf.get_idati(),n);r['types'][n]=str(t) if ok else None
 except Exception as e:r['types'][n]=str(e)
for ea in idautils.Heads(0x82B27C00,0x82B28500):
 ins=ida_ua.insn_t()
 if not ida_ua.decode_insn(ins,ea):continue
 for op in ins.ops:
  if op.type in (ida_ua.o_displ,ida_ua.o_phrase) and op.addr in [0x1c,0x20,0x24,0x28,0x38]:
   f=ida_funcs.get_func(ea);r['displacement_refs'].append({'address':'0x%08X'%ea,'function':idc.get_func_name(ea),'instruction':ida_lines.tag_remove(idc.generate_disasm_line(ea,0) or ''),'displacement':'0x%X'%op.addr});break
with open(os.path.join(out,'HLB_TYPES_AND_DISPLACEMENTS.json'),'w',encoding='utf-8') as o:json.dump(r,o,indent=2)
idc.qexit(0)
