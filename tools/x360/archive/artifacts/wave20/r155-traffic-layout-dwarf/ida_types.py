import ida_typeinf,ida_pro,json
from pathlib import Path
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r155-traffic-layout-dwarf');til=ida_typeinf.get_idati();o=[]
try:
 for i in range(1,ida_typeinf.get_ordinal_limit(til)):
  n=ida_typeinf.get_numbered_type_name(til,i)
  if n and ('TrafficLightManager' in n or 'TrafficEntityModule' in n):
   t=ida_typeinf.tinfo_t()
   if t.get_numbered_type(til,i):
    udt=ida_typeinf.udt_type_data_t();m=[]
    if t.get_udt_details(udt):
     for member in udt: m.append({'name':member.name,'offset_bits':member.offset,'size_bits':member.size,'type':str(member.type)})
    o.append({'ordinal':i,'name':n,'size':t.get_size(),'members':m})
 (out/'ida_types.json').write_text(json.dumps(o,indent=2))
except Exception as e: (out/'ida_types_error.txt').write_text(repr(e))
ida_pro.qexit(0)
