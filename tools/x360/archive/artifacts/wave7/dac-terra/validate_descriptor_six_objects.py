import json,sys,hashlib
from pathlib import Path
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/tools/re');from validate_render_states import Coff;import x360rd as x
assert x.u32(0x82815570)==0x7c8a2378 and x.u32(0x82f264a8)==4
scan=json.loads((base/'descriptor-deferred-readonly-scan.json').read_text());pk=json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/audio_descriptor_packet_luna/recovery-packet.json').read_text());deferred={r['name']:r for r in pk['deferred_source_homed_records']};names={'Dac','SndPlayer1','SubMix','GinsuPlayer','SndPlayer1_CgsStreamMod','GainArray'};results=[];errors=[]
for r in scan:
 if r['name'] not in names:continue
 rec=deferred[r['name']];obj=base/'descriptor-six-native-build'/(Path(r['source']).stem+'.obj');o=Coff(obj,rec['source_symbol']);symbols={n:(v,sec) for n,v,sec in o.symbols.values() if sec>0};dv,ds=symbols[rec['source_symbol']];rel={off:o.symbols[ix][0] for off,ix,k in o.relocations(ds)};comp={}
 for i,(key,field,length) in enumerate([('ChannelMaps','channel_maps',len(r['maps'])*2),('ParameterDescRunTime','parameters',len(r['parameters'])*32)] + ([('EventDescRunTime','events',len(r['events'])*8)] if r['events'] else [])):
  name='s_'+r['name']+key;value,sec=symbols[name];section=o.sections[sec];actual=o.data[section[4]+value:section[4]+value+length] if section[4] else bytes(length);expected=x.rd(int(rec['metadata_pointers'][field],16),length);target=rel.get(dv+0x14+4*i);match=actual==expected and target==name;comp[key]={'expected':expected.hex(),'actual':actual.hex(),'relocation':target,'match':match}
  if not match:errors.append((r['name'],key))
 if not r['events']:
  slot=dv+0x1c;section=o.sections[ds];raw=o.data[section[4]+slot:section[4]+slot+4];ok=raw==bytes(4) and slot not in rel;comp['event_null']=ok
  if not ok:errors.append((r['name'],'event'))
 results.append({'record':r['name'],'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'comparisons':comp})
report={'records':len(results),'mismatches':errors,'results':results,'limits':'Actual native object map and parameter bytes and descriptor pointer relocations compared directly to calibrated ARTIST raw image; event records or null slot. No runtime metadata use, registration, full link or hardware claim.'};(base/'descriptor-six-object-comparison.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('records',len(results),'errors',errors);assert len(results)==6 and not errors

