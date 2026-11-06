import ida_name,ida_bytes,ida_nalt,ida_typeinf,ida_pro,json
name='gCallBackWriteBuffer';a=ida_name.get_name_ea(0xffffffffffffffff,name);t=ida_typeinf.tinfo_t();ok=ida_nalt.get_tinfo(t,a);out={'name':name,'address':hex(a),'item_size':ida_bytes.get_item_size(a),'type_found':ok,'type':str(t) if ok else None,'type_size':t.get_size() if ok else None}
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/rawpuller-pdb-scratch.json','w').write(json.dumps(out,indent=2));ida_pro.qexit(0)
