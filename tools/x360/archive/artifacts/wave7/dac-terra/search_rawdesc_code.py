import ida_segment,ida_bytes,ida_funcs,ida_name,ida_pro,json,struct
out=[];seg=ida_segment.get_first_seg()
while seg:
 if seg.perm & ida_segment.SEGPERM_EXEC:
  data=ida_bytes.get_bytes(seg.start_ea,seg.end_ea-seg.start_ea) or b''
  for pos in range(0,len(data)-32,4):
   w=struct.unpack_from('>I',data,pos)[0]
   if w>>26!=15 or w&0xffff!=0x82f9 or ((w>>16)&31)!=0:continue
   reg=(w>>21)&31
   for d in range(4,32,4):
    z=struct.unpack_from('>I',data,pos+d)[0]
    if z>>26==14 and ((z>>16)&31)==reg and z&0xffff==0xf768:
     a=seg.start_ea+pos;f=ida_funcs.get_func(a);out.append({'at':hex(a),'next':hex(a+d),'function':ida_name.get_name(f.start_ea) if f else None})
 seg=ida_segment.get_next_seg(seg.start_ea)
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/rawpuller-descriptor-code-search.json','w').write(json.dumps(out,indent=2));ida_pro.qexit(0)
