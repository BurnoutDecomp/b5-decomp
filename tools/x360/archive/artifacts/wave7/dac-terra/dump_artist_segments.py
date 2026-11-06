import ida_segment,ida_pro,json
out=[];s=ida_segment.get_first_seg()
while s:out.append({'start':hex(s.start_ea),'end':hex(s.end_ea),'name':ida_segment.get_segm_name(s),'perm':s.perm});s=ida_segment.get_next_seg(s.start_ea)
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/artist-segments.json','w').write(json.dumps(out,indent=2));ida_pro.qexit(0)
