import hashlib,importlib.util,json,struct,sys
from pathlib import Path
reader_path=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re\x360rd.py')
sys.path.insert(0,str(reader_path.parent))
import x360rd
base=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra')
records=json.loads((base/'AUDIO_DEBT_INVENTORY.json').read_text(encoding='utf-8'))['records']
homed=[r for r in records if r.get('source')]
# Mandatory sentinels before any stage-record or metadata-table reads.
cal1=x360rd.u32(0x82815570); cal2=x360rd.u32(0x82F264A8)
assert cal1==0x7C8A2378,hex(cal1)
assert cal2==4,hex(cal2)

def rd(addr,n): return x360rd.rd(addr,n)
def u32(b,off=0): return struct.unpack_from('>I',b,off)[0]
def s8(b): return struct.unpack('b',b)[0]
def cstr(addr,limit=128):
 if not addr:return None
 b=rd(addr,limit); end=b.find(b'\0'); end=len(b) if end<0 else end
 return {'address':hex(addr),'text':b[:end].decode('latin1'),'hex':b[:end].hex(),'terminated':end<len(b)}
all_rows=[]
for inv in homed:
 name=inv['name']; rec=int(inv['record'],16); d=rd(rec,0x34); assert len(d)==0x34
 ptr={'channel_maps':u32(d,0x14),'parameters':u32(d,0x18),'events':u32(d,0x1c),'tool_side':u32(d,0x20)}
 assert ptr['channel_maps']==int(inv['channel_maps'],16),(name,'maps')
 assert ptr['parameters']==int(inv['parameters'],16),(name,'params')
 assert ptr['events']==int(inv['events'],16),(name,'events')
 nctor,nattr,nevent=d[0x2d],d[0x2e],d[0x2f]
 maps=[]; mapraw=b''; mapterm=False
 if ptr['channel_maps']:
  for i in range(64):
   p=rd(ptr['channel_maps']+i*2,2); pair=(s8(p[:1]),s8(p[1:])); maps.append(pair); mapraw+=p
   if pair==(-1,-1):mapterm=True;break
 events=[];eventn=0;eventraw=b''
 if ptr['events'] and nevent:
  eventraw=rd(ptr['events'],nevent*8); assert len(eventraw)==nevent*8
  for i in range(nevent):
   count,tool=struct.unpack_from('>II',eventraw,i*8);eventn+=count;events.append({'num_parameters':count,'tool_side':hex(tool)})
 elif nevent: raise AssertionError((name,'missing event pointer'))
 nparam=nctor+nattr+eventn
 params=[];paramraw=b''
 if ptr['parameters'] and nparam:
  paramraw=rd(ptr['parameters'],nparam*32);assert len(paramraw)==nparam*32,(name,len(paramraw),nparam)
  for i in range(nparam):
   q=paramraw[i*32:(i+1)*32];direction,ptype=struct.unpack_from('>II',q,0);tool=u32(q,24)
   params.append({'direction':direction,'type':ptype,'min_bits':q[8:16].hex(),'min_f64':struct.unpack_from('>d',q,8)[0],'max_bits':q[16:24].hex(),'max_f64':struct.unpack_from('>d',q,16)[0],'tool_side':hex(tool),'padding':q[28:32].hex(),'raw':q.hex()})
 elif nparam: raise AssertionError((name,'missing parameter pointer'))
 ptools=[p['tool_side'] for p in params if p['tool_side']!='0x0']
 etools=[e['tool_side'] for e in events if e['tool_side']!='0x0']
 all_rows.append({'name':name,'record':inv['record'],'source':inv['source'],'source_null_fields':inv['source_null_fields'],'original_nonnull':inv['original_nonnull'],'record_raw':d.hex(),'name_string':cstr(u32(d,0)),'metadata_pointers':{k:hex(v) for k,v in ptr.items()},'tail_counts':{'constructor_parameters':nctor,'attributes':nattr,'events':nevent},'channel_map_entries':maps,'channel_map_sentinel_found':mapterm if ptr['channel_maps'] else None,'channel_map_raw':mapraw.hex(),'events':events,'event_raw':eventraw.hex(),'parameter_count_calibrated':nparam,'parameters':params,'parameter_raw':paramraw.hex(),'nonzero_parameter_toolside_pointers':ptools,'nonzero_event_toolside_pointers':etools})
packet={'reader':str(reader_path),'reader_sha256':hashlib.sha256(reader_path.read_bytes()).hexdigest(),'raw_image':str(Path(x360rd.RAW)),'raw_image_sha256':hashlib.sha256(Path(x360rd.RAW).read_bytes()).hexdigest(),'calibration':{'0x82815570':hex(cal1),'0x82F264A8':cal2},'scope':'all 27 source-homed records; this scan is for candidate selection, not a native registration/reachability claim','records':all_rows,'tool_side_pointer_summary':{'nonzero_parameter_toolside':[(r['name'],r['nonzero_parameter_toolside_pointers']) for r in all_rows if r['nonzero_parameter_toolside_pointers']],'nonzero_event_toolside':[(r['name'],r['nonzero_event_toolside_pointers']) for r in all_rows if r['nonzero_event_toolside_pointers']],'nonzero_plugin_toolside':[r['name'] for r in all_rows if r['metadata_pointers']['tool_side']!='0x0']}}
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\audio_descriptor_packet_luna\calibrated-source-homed-scan.json')
out.write_text(json.dumps(packet,indent=2)+'\n',encoding='utf-8')
print('wrote',out,'homed',len(all_rows),'nonzero tool-side arrays',packet['tool_side_pointer_summary'])
