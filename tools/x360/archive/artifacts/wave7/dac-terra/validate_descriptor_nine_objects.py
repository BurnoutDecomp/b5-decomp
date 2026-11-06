import json,sys,hashlib,re
from pathlib import Path
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');from validate_render_states import Coff
packet=json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/audio_descriptor_packet_luna/recovery-packet.json').read_text());results=[];errors=[]
for rec in packet['selected_records']:
 obj=base/'descriptor-nine-native-build'/(Path(rec['source']['path']).stem+'.obj');o=Coff(obj,rec['source']['symbol']);symbols={n:(v,sec) for n,v,sec in o.symbols.values() if sec>0};descval,descsec=symbols[rec['source']['symbol']];rel={off:o.symbols[ix][0] for off,ix,k in o.relocations(descsec)};comparison={}
 for i,(key,rawkey) in enumerate([('ChannelMaps','channel_map_raw'),('ParameterDescRunTime','parameter_raw'),('EventDescRunTime','event_raw')]):
  expected=bytes.fromhex(rec[rawkey]);name='s_'+rec['name']+key;slot=descval+0x14+4*i;actualtarget=rel.get(slot)
  if expected:
   if name not in symbols:errors.append((rec['name'],name,'missing'));continue
   value,sec=symbols[name];section=o.sections[sec];got=o.data[section[4]+value:section[4]+value+len(expected)];same=got==expected and actualtarget==name;comparison[key]={'name':name,'expected':expected.hex(),'actual':got.hex(),'relocation':actualtarget,'match':same}
   if not same:errors.append((rec['name'],key,comparison[key]))
  else:
   section=o.sections[descsec];got=o.data[section[4]+slot:section[4]+slot+4];same=got==bytes(4) and actualtarget is None;comparison[key]={'null':same,'relocation':actualtarget}
   if not same:errors.append((rec['name'],key,comparison[key]))
 results.append({'record':rec['name'],'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'comparisons':comparison})
r={'records':len(results),'mismatches':errors,'results':results,'limits':'Compares actual emitted native object table bytes and descriptor relocations to calibrated ARTIST packet. Type sizes are XDK compile gates; no registration reachability, runtime consumption, link or hardware claim.'};(base/'descriptor-nine-object-comparison.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print('records',len(results),'errors',errors);assert not errors
