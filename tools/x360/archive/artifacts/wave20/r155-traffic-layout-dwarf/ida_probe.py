import ida_segment,ida_typeinf,ida_pro,json
from pathlib import Path
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r155-traffic-layout-dwarf'); seg=[]
for i in range(ida_segment.get_segm_qty()):
 s=ida_segment.getnseg(i); seg.append([ida_segment.get_segm_name(s),hex(s.start_ea),hex(s.end_ea)])
(out/'ida_probe.json').write_text(json.dumps({'segments':seg,'typeinf_api':[x for x in dir(ida_typeinf) if 'ordinal' in x],'til_api':[x for x in dir(ida_typeinf.get_idati()) if 'ordinal' in x]},indent=2))
ida_pro.qexit(0)
