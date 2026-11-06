import hashlib, importlib.util, json, struct, subprocess, sys
from pathlib import Path

artifact=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\audio_descriptor_packet_luna')
repo=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\worktrees\dac_terra')
workflow=Path(r'D:\Reverse\BP-Decomp_Workflow')
reader_path=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re\x360rd.py')
sys.path.insert(0,str(reader_path.parent))
import x360rd

inventory_path=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra\AUDIO_DEBT_INVENTORY.json')
original_inventory_path=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra\stage-descriptor-original-inventory.json')
records=json.loads(inventory_path.read_text(encoding='utf-8'))['records']
orig_records=json.loads(original_inventory_path.read_text(encoding='utf-8'))
orig_by_name={r['name']:r for r in orig_records}
by_name={r['name']:r for r in records}

# Mandatory independent mapping sentinels; do not read stage descriptor bytes before these pass.
sentinel_insn=x360rd.u32(0x82815570)
sentinel_word=x360rd.u32(0x82F264A8)
assert sentinel_insn==0x7C8A2378,hex(sentinel_insn)
assert sentinel_word==4,hex(sentinel_word)

SELECTED=['Rechannel','BandPassIir2','Gain','Resample','Pause','AiffWriter','GainFader','Route','Send']
CALLBACKS={
 'AiffWriter':['AiffWriter::GetSize','AiffWriter::CreateInstance',None,'AiffWriter::Process'],
 'BandPassIir2':['BandPassIir2GetSizeThunk','BandPassIir2::CreateInstance',None,'BandPassIir2ProcessThunk'],
 'Gain':['GainGetSizeThunk','Gain::CreateInstance',None,'Gain::Process'],
 'Resample':['Resample::GetSize','Resample::CreateInstance','Resample::PreProcess','Resample::Process'],
 'Pause':['Pause::GetSize','Pause::CreateInstance','Pause::PreProcess','Pause::Process'],
 'Rechannel':['Rechannel::GetSize','Rechannel::CreateInstance','Rechannel::PreProcess','Rechannel::Process'],
 'GainFader':['GainFader::GetSize','GainFader::CreateInstance',None,'GainFader::Process'],
 'Route':['Route::GetSize','RouteCreateInstanceThunk',None,'Route::Process'],
 'Send':['Send::GetSize','Send::CreateInstance',None,'Send::Process'],
}
PARAM_TYPE={0:'ENUM',1:'FLOAT32',2:'FLOAT64',3:'POINTERTOSTRING',4:'POINTER',5:'GUID'}
DIRECTION={0:'INPUT',1:'OUTPUT'}

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(addr,n): return x360rd.rd(addr,n)
def u32(b,off=0): return struct.unpack_from('>I',b,off)[0]
def s8(b): return struct.unpack('b',b)[0]
def cstring(addr,limit=128):
    if not addr: return None
    data=read(addr,limit)
    end=data.find(b'\0')
    if end<0: end=len(data)
    return {'address':hex(addr),'text':data[:end].decode('latin1'),'hex':data[:end].hex(),'terminated':end<len(data)}

def ptool(addr):
    if not addr: return None
    b=read(addr,48)
    if len(b)<44: return {'address':hex(addr),'read_bytes':len(b),'raw_hex':b.hex(),'complete':False}
    ptrs=[u32(b,off) for off in (0,4,8,12)]
    return {'address':hex(addr),'raw_hex':b.hex(),'name_cpp':cstring(ptrs[0]),'name_saf':cstring(ptrs[1]),'help':cstring(ptrs[2]),'units':cstring(ptrs[3]),'default':struct.unpack_from('>d',b,16)[0],'min':struct.unpack_from('>d',b,24)[0],'max':struct.unpack_from('>d',b,32)[0],'enum_ptr':hex(u32(b,40))}

def etool(addr):
    if not addr: return None
    b=read(addr,16)
    if len(b)<16: return {'address':hex(addr),'raw_hex':b.hex(),'complete':False}
    ptrs=[u32(b,off) for off in (0,4,8,12)]
    return {'address':hex(addr),'raw_hex':b.hex(),'name_cpp_enum':cstring(ptrs[0]),'name_cpp_struct':cstring(ptrs[1]),'name_saf':cstring(ptrs[2]),'help':cstring(ptrs[3])}

def callback_details(record,source_names):
    rows=[]
    for i,addr in enumerate(record['callbacks']):
        sym=source_names[i] if i<len(source_names) else None
        original_name=None
        export_path=None
        export_hash=None
        if int(addr,16)!=0:
            path=workflow/'.ida-exports'/'BURNOUT_X360_ARTIST.XEX'/(addr+'.json')
            export_path=str(path)
            if path.exists():
                data=json.loads(path.read_text(encoding='utf-8'))
                original_name=data.get('name')
                export_hash=sha(path)
        rows.append({'slot':['get_size','create_instance','pre_process','process'][i], 'artist_address':addr,'source_symbol':sym,'artist_export_name':original_name,'artist_export_path':export_path,'artist_export_sha256':export_hash})
    return rows

def decode_record(name):
    inv=by_name[name]
    record=int(inv['record'],16)
    b=read(record,0x34)
    assert len(b)==0x34
    fields={
      'name_ptr':u32(b,0),'get_size':u32(b,4),'create_instance':u32(b,8),'pre_process':u32(b,12),'process':u32(b,16),
      'channel_maps':u32(b,0x14),'parameters':u32(b,0x18),'events':u32(b,0x1c),'plugin_tool_side':u32(b,0x20),'next':u32(b,0x24),'guid':u32(b,0x28),
      'plugin_type':b[0x2c],'num_constructor_parameters':b[0x2d],'num_attributes':b[0x2e],'num_events':b[0x2f],
      'variable_input_channels':b[0x30],'variable_output_channels':b[0x31],'sequence':b[0x32],'tail_pad':b[0x33]
    }
    inv_ptrs={k:int(inv[k],16) for k in ('channel_maps','parameters','events')}
    assert fields['channel_maps']==inv_ptrs['channel_maps'],(name,'maps',hex(fields['channel_maps']),inv['channel_maps'])
    assert fields['parameters']==inv_ptrs['parameters'],(name,'parameters')
    assert fields['events']==inv_ptrs['events'],(name,'events')
    assert fields['name_ptr']!=0
    name_string=cstring(fields['name_ptr'])
    assert name_string['text']==name,(name,name_string)

    maps_ptr=fields['channel_maps']
    maps=None
    if maps_ptr:
        entries=[]; raw=bytearray()
        for i in range(64):
            pair=read(maps_ptr+i*2,2)
            if len(pair)!=2: break
            s=(s8(pair[0:1]),s8(pair[1:2]))
            entries.append({'index':i,'input_s8':s[0],'output_s8':s[1],'raw_hex':pair.hex()})
            raw.extend(pair)
            if s==(-1,-1): break
        maps={'pointer':hex(maps_ptr),'entries':entries,'sentinel_found':bool(entries and entries[-1]['input_s8']==-1 and entries[-1]['output_s8']==-1),'raw_hex':raw.hex()}
    event_ptr=fields['events']; event_entries=[]; event_params=0; event_raw=b''
    if event_ptr and fields['num_events']:
        event_raw=read(event_ptr,fields['num_events']*8)
        assert len(event_raw)==fields['num_events']*8,(name,'event bytes',len(event_raw))
        for i in range(fields['num_events']):
            nparams,toolptr=struct.unpack_from('>II',event_raw,i*8)
            event_params+=nparams
            event_entries.append({'index':i,'num_parameters':nparams,'tool_side_ptr':hex(toolptr),'tool_side':etool(toolptr),'raw_hex':event_raw[i*8:(i+1)*8].hex()})
    elif fields['num_events']:
        raise AssertionError((name,'nonzero event count with null pointer'))
    params_ptr=fields['parameters']
    nparams=fields['num_constructor_parameters']+fields['num_attributes']+event_params
    param_raw=b''; param_entries=[]
    if nparams and params_ptr:
        param_raw=read(params_ptr,nparams*32)
        assert len(param_raw)==nparams*32,(name,'parameter bytes',len(param_raw),nparams)
        for i in range(nparams):
            d=param_raw[i*32:(i+1)*32]
            direction,ptype=struct.unpack_from('>II',d,0)
            toolptr=u32(d,24)
            param_entries.append({'index':i,'direction_code':direction,'direction':DIRECTION.get(direction,'UNKNOWN'),'type_code':ptype,'type':PARAM_TYPE.get(ptype,'UNKNOWN'),'min_extreme':struct.unpack_from('>d',d,8)[0],'max_extreme':struct.unpack_from('>d',d,16)[0],'tool_side_ptr':hex(toolptr),'tool_side':ptool(toolptr),'pad_hex':d[28:32].hex(),'raw_hex':d.hex()})
    elif nparams and not params_ptr:
        raise AssertionError((name,'nonzero calibrated parameter count with null pointer',nparams))
    if param_entries:
        assert all(x['pad_hex']=='00000000' for x in param_entries),(name,'nonzero param padding')
    # Type/order calibration cross-checks use explicit fields and count tables, not adjacency alone.
    return {
      'name':name,'record':inv['record'],'source':inv['source'],'source_current_sha256':sha(repo/inv['source']['path']),
      'source_descriptor_symbol':inv['source']['symbol'],'source_metadata_fields_current':inv['source']['source_metadata'],
      'source_null_fields':inv['source_null_fields'],'original_nonnull_fields':inv['original_nonnull'],
      'original_name_string':name_string,'descriptor_raw_hex':b.hex(),'descriptor_fields':{k:(hex(v) if isinstance(v,int) and k in ('name_ptr','get_size','create_instance','pre_process','process','channel_maps','parameters','events','plugin_tool_side','next','guid') else v) for k,v in fields.items()},
      'callbacks':callback_details(inv,CALLBACKS[name]),'channel_map_table':maps,
      'event_table':{'pointer':hex(event_ptr),'count':fields['num_events'],'raw_hex':event_raw.hex(),'entries':event_entries},
      'parameter_table':{'pointer':hex(params_ptr),'count_calibrated':nparams,'formula':'num_constructor_parameters + num_attributes + sum(EventDescRunTime.numParameters)','runtime_record_size':32,'raw_hex':param_raw.hex(),'entries':param_entries},
      'suggested_descriptor_field_values':{'pChannelMaps':f'&s_{name}ChannelMaps' if maps_ptr else '0','pParameterDescRunTime':f'&s_{name}ParameterDescRunTime' if params_ptr else '0','pEventDescRunTime':f'&s_{name}EventDescRunTime' if event_ptr else '0'},
      'unresolved':['native channel-code meanings are not assigned from signed-byte values alone','parameter/event labels/help/units absent where tool-side pointers are null']
    }

selected=['Rechannel','BandPassIir2','Gain','Resample','Pause','AiffWriter','GainFader','Route','Send']
homed=[r for r in records if r.get('source')]
unhomed=[r for r in records if not r.get('source')]
selected_data=[decode_record(n) for n in selected]
remaining=[{'name':r['name'],'record':r['record'],'source':r.get('source',{}).get('path'),'source_symbol':r.get('source',{}).get('symbol'),'original_nonnull':r['original_nonnull'],'source_null_fields':r.get('source_null_fields'),'metadata_pointers':{k:r[k] for k in ('channel_maps','parameters','events')}} for r in homed if r['name'] not in selected]
unhomed_data=[{'name':r['name'],'record':r['record'],'source':None,'original_nonnull':r['original_nonnull'],'metadata_pointers':{k:r[k] for k in ('channel_maps','parameters','events')},'status':'unhomed; no reachability or source-array inference'} for r in unhomed]
raw=Path(x360rd.RAW)
packet={
 'scope':'Read-only artifact recovery for first 9 source-homed metadata records; no source modifications or runtime/registration claims.',
 'source_workspace':str(repo),'source_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip(),'source_worktree_status':subprocess.check_output(['git','status','--short'],cwd=repo,text=True).splitlines(),
 'calibration':{'reader':str(reader_path),'reader_sha256':hashlib.sha256(reader_path.read_bytes()).hexdigest(),'raw_image':str(raw),'raw_image_sha256':hashlib.sha256(raw.read_bytes()).hexdigest(),'instruction_sentinel':{'address':'0x82815570','read_u32':hex(sentinel_insn),'expected':'0x7c8a2378','ppcdis':'mr r10,r4'},'data_sentinel':{'address':'0x82f264a8','read_u32':sentinel_word,'expected':4}},
 'typed_schema':{'decfigs_plugin_h':str(Path(r'D:\Reverse\BP-Decomp_Workflow\references\DecFIGS\dwarfdump\SDKs\EATech\include\rw\audio\core\plugin.h')),'decfigs_plugin_h_sha256':hashlib.sha256(Path(r'D:\Reverse\BP-Decomp_Workflow\references\DecFIGS\dwarfdump\SDKs\EATech\include\rw\audio\core\plugin.h').read_bytes()).hexdigest(),'current_source_header':str(repo/'vendor/renderware/include/rw/audio/core/PlugIn.h'),'current_source_header_sha256':sha(repo/'vendor/renderware/include/rw/audio/core/PlugIn.h'),'current_metadata_pointer_fields_are_void':True,'reference_types':{'ChannelMapPair':'signed char input; signed char output; 2-byte pair; terminated by (-1,-1)','ParameterDescRunTime':'ParameterDirection(4), ParameterType(4), double minExtremeValue, double maxExtremeValue, ParameterDescToolSide*(4); 32-byte X360 stride including +0x1C..+0x1F padding','EventDescRunTime':'u32 numParameters; EventDescToolSide*(4); 8-byte stride','ParameterDirection':{'0':'INPUT','1':'OUTPUT'},'ParameterType':{'0':'ENUM','1':'FLOAT32','2':'FLOAT64','3':'POINTERTOSTRING','4':'POINTER','5':'GUID'}},'layout_calibration_records':{'AiffWriter':'0 ctor + 0 attrs + event parameters (1) = one 32-byte parameter record; parameter pointer to event-table pointer delta = 0x20','BandPassIir2':'2 attrs = two 32-byte parameter records; parameter start to following record pointer delta = 0x40 corroborates stride','GainFader':'1 attr + event parameters (4) = five 32-byte records; parameter start to event table pointer delta = 0xA0','Route':'0 attrs + event parameters (4) = four 32-byte records; parameter start to event table pointer delta = 0x80'},'extent_rule':'Table extents use descriptor constructor/attribute/event counts, each event numParameters, the sentinel for channel maps, and typed strides. The two mapping sentinels are calibration only and are not used as table extents.'},
 'selected_records':selected_data,
 'deferred_homed_records':remaining,
 'unhomed_records':unhomed_data,
 'coverage_summary':{'runtime_records':len(records),'source_homed':len(homed),'selected':len(selected_data),'deferred_homed':len(remaining),'unhomed':len(unhomed_data),'unhomed_names':[r['name'] for r in unhomed]},
 'limitations':['No new registration/getter/API functions were created.','No metadata table is asserted to be runtime-reachable; pointer tables and typed bytes are recovered from ARTIST memory only.','For selected records all ParameterDescToolSide and EventDescToolSide pointers were read null; therefore parameter/event names, help, units and event struct names are unresolved/not present in these runtime descriptors.','The five unhomed records remain unhomed and are not assigned source homes or metadata initializers.']
}
out=artifact/'calibrated-audio-metadata.json'
out.write_text(json.dumps(packet,indent=2)+'\n',encoding='utf-8')
print('packet',out,'selected',len(selected_data),'homed',len(homed),'unhomed',len(unhomed),'sentinels',hex(sentinel_insn),sentinel_word)
idc.qexit(0)
