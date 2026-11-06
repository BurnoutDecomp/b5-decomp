import json, struct
from pathlib import Path
import ida_bytes, idc

artifact = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\audio_descriptor_packet_luna')
raw_records = json.loads((artifact/'ida-data-windows.json').read_text(encoding='utf-8'))['records']
inv = json.loads(Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra\AUDIO_DEBT_INVENTORY.json').read_text(encoding='utf-8'))
inv_by_name = {r['name']: r for r in inv['records']}

def read(addr, size):
    data = ida_bytes.get_bytes(addr, size)
    return bytes(data) if data else b''

def u32(data, off=0):
    return struct.unpack_from('>I', data, off)[0]

def s8(byte):
    return struct.unpack('b', byte)[0]

def f64(data, off):
    return struct.unpack_from('>d', data, off)[0]

def cstr(addr, limit=192):
    if addr == 0: return None
    data = read(addr, limit)
    if not data: return {'address':hex(addr),'bytes':None}
    end = data.find(b'\0')
    if end < 0: end = len(data)
    return {'address':hex(addr),'text_latin1':data[:end].decode('latin1'),'bytes_hex':data[:end].hex(),'terminated':end < len(data)}

def ptool(addr):
    data=read(addr,48)
    if len(data)<44: return {'address':hex(addr),'read_hex':data.hex(),'size':len(data),'complete':False}
    ptrs=[u32(data,off) for off in (0,4,8,12)]
    enum=u32(data,40)
    return {'address':hex(addr),'raw_hex':data.hex(),'name_cpp':cstr(ptrs[0]),'name_saf':cstr(ptrs[1]),'help':cstr(ptrs[2]),'units':cstr(ptrs[3]),'default':f64(data,16),'min':f64(data,24),'max':f64(data,32),'enum_ptr':hex(enum),'size_read':len(data)}

def etool(addr):
    data=read(addr,16)
    if len(data)<16:return {'address':hex(addr),'raw_hex':data.hex(),'complete':False}
    ptrs=[u32(data,off) for off in (0,4,8,12)]
    return {'address':hex(addr),'raw_hex':data.hex(),'name_cpp_enum':cstr(ptrs[0]),'name_cpp_struct':cstr(ptrs[1]),'name_saf':cstr(ptrs[2]),'help':cstr(ptrs[3])}

out=[]
for row in raw_records:
    name=row['name']; source=inv_by_name[name].get('source')
    desc=bytes.fromhex(row['descriptor_hex'])
    if len(desc)!=0x34:
        out.append({'name':name,'source':source,'error':'descriptor read incomplete','raw_hex':desc.hex()}); continue
    pointers={'channel_maps':u32(desc,0x14),'parameters':u32(desc,0x18),'events':u32(desc,0x1c),'tool_side':u32(desc,0x20)}
    plugin_type,ctor_count,attr_count,event_count=desc[0x2c:0x30]
    entry={'name':name,'source':source,'record':row['record'],'descriptor_raw_hex':desc.hex(),'descriptor_pointers':{k:hex(v) for k,v in pointers.items()},'tail_counts':{'plugin_type':plugin_type,'constructor_parameters':ctor_count,'attributes':attr_count,'events':event_count,'variable_input_channels':desc[0x30],'variable_output_channels':desc[0x31]},'channel_maps':None,'events_data':None,'parameters':None,'tool_side':None}
    if pointers['tool_side']:
        data=read(pointers['tool_side'],4)
        entry['tool_side']={'address':hex(pointers['tool_side']),'raw_hex':data.hex(),'help':cstr(u32(data)) if len(data)>=4 else None}
    maps_ptr=pointers['channel_maps']
    if maps_ptr:
        data=read(maps_ptr,128)
        pairs=[]; sentinel=False
        for off in range(0,len(data)-1,2):
            pair=(s8(data[off:off+1]),s8(data[off+1:off+2]))
            pairs.append(pair)
            if pair==(-1,-1): sentinel=True; break
        entry['channel_maps']={'address':hex(maps_ptr),'pairs':pairs,'sentinel_found':sentinel,'raw_through_sentinel':data[:len(pairs)*2].hex() if sentinel else data.hex()}
    event_count_total=0
    event_details=[]
    if pointers['events'] and event_count:
        data=read(pointers['events'],event_count*8)
        if len(data)==event_count*8:
            for i in range(event_count):
                nparams,toolptr=struct.unpack_from('>II',data,i*8)
                event_count_total += nparams
                event_details.append({'index':i,'num_parameters':nparams,'tool_side_ptr':hex(toolptr),'tool_side':etool(toolptr) if toolptr else None})
        else:
            event_details=[{'error':'event array read incomplete','read_bytes':len(data),'expected_bytes':event_count*8}]
    elif event_count:
        event_details=[{'error':'event pointer null despite nonzero descriptor count'}]
    if event_count or pointers['events']:
        entry['events_data']={'address':hex(pointers['events']),'count_from_descriptor':event_count,'raw_hex':read(pointers['events'],event_count*8).hex() if pointers['events'] and event_count else '', 'entries':event_details}
    # Calibrated from AiffWriter, BandPassIir2, GainFader and Route: runtime parameter
    # records cover constructor params + attributes + the sum of event parameter counts.
    # ParameterDescRunTime is 32 bytes in ARTIST (fields through +0x1B, 8-byte struct stride).
    param_count=ctor_count+attr_count+event_count_total
    params_ptr=pointers['parameters']
    param_details=[]
    raw_params=b''
    if params_ptr and param_count:
        raw_params=read(params_ptr,param_count*32)
        if len(raw_params)==param_count*32:
            for i in range(param_count):
                d=raw_params[i*32:(i+1)*32]
                direction,ptype=struct.unpack_from('>II',d,0)
                toolptr=u32(d,24)
                param_details.append({'index':i,'direction':direction,'type':ptype,'min_extreme':f64(d,8),'max_extreme':f64(d,16),'tool_side_ptr':hex(toolptr),'tool_side':ptool(toolptr) if toolptr else None,'pad_hex':d[28:32].hex(),'raw_hex':d.hex()})
        else:
            param_details=[{'error':'parameter array read incomplete','read_bytes':len(raw_params),'expected_bytes':param_count*32}]
    elif param_count:
        param_details=[{'error':'parameter pointer null despite calibrated nonzero count','expected_count':param_count}]
    entry['parameters']={'address':hex(params_ptr),'count_from_descriptor_and_events':param_count,'runtime_record_stride':32,'raw_hex':raw_params.hex(),'entries':param_details}
    out.append(entry)

output=artifact/'descriptor-tables-decoded.json'
output.write_text(json.dumps({'records':out,'method_note':'Reads are direct ARTIST IDA memory reads from the copied IDB. Counted structures are read only to counts inferred from typed descriptor fields and event parameter counts, calibrated on multiple records. Channel maps stop only at the native (-1,-1) sentinel. No source or IDB was modified.'},indent=2)+'\n',encoding='utf-8')
print('wrote',output,'records',len(out))
idc.qexit(0)
