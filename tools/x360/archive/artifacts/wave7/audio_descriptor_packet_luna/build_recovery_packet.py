import hashlib,json,re,subprocess
from pathlib import Path

art=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\audio_descriptor_packet_luna')
repo=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\worktrees\dac_terra')
workflow=Path(r'D:\Reverse\BP-Decomp_Workflow')
scan=json.loads((art/'calibrated-source-homed-scan.json').read_text(encoding='utf-8'))
inv=json.loads(Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra\AUDIO_DEBT_INVENTORY.json').read_text(encoding='utf-8'))
rows={r['name']:r for r in scan['records']}
inv_rows={r['name']:r for r in inv['records']}
selected=['Rechannel','BandPassIir2','Gain','Resample','Pause','AiffWriter','GainFader','Route','Send']
source_callbacks={
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
param_names={0:'PARAMETERTYPE_ENUM',1:'PARAMETERTYPE_FLOAT32',2:'PARAMETERTYPE_FLOAT64',3:'PARAMETERTYPE_POINTERTOSTRING',4:'PARAMETERTYPE_POINTER',5:'PARAMETERTYPE_GUID'}
direction_names={0:'PARAMETERDIRECTION_INPUT',1:'PARAMETERDIRECTION_OUTPUT'}

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def source_block(source_path,descriptor_symbol):
    p=repo/source_path
    lines=p.read_text(encoding='utf-8',errors='replace').splitlines()
    needle='static PlugInDescRunTime '+descriptor_symbol
    start=next(i for i,x in enumerate(lines) if needle in x)
    end=next(i for i in range(start,len(lines)) if lines[i].strip()=='};')
    getter=[]
    for i,x in enumerate(lines):
        if 'GetPlugInDescRunTime' in x:
            ctx='\n'.join(lines[i:max(i+4,len(lines))])
            m=re.search(r'0x[0-9A-Fa-f]{8}',ctx)
            if m: getter.append({'line':i+1,'commented_x360_address':m.group(0)})
    return {'path':source_path,'sha256':sha(p),'git_diff_clean_for_file':subprocess.run(['git','diff','--quiet','--',source_path],cwd=repo).returncode==0,'descriptor_block_start_line':start+1,'descriptor_block':'\n'.join(lines[start:end+1]),'getter_refs':getter[:3]}

def fval(v):
    if v==int(v): return str(int(v))+'.0'
    return repr(v)
def initializer(name,r):
    arrays=[]
    if r['original_nonnull'] and 'channel_maps' in r['original_nonnull']:
        entries=r['channel_map_entries']
        body=', '.join('{ %d, %d }'%(x[0],x[1]) for x in entries)
        arrays.append('static ChannelMapPair s_%sChannelMaps[] = { %s };'%(name,body))
    if r['original_nonnull'] and 'parameters' in r['original_nonnull']:
        ps=[]
        for x in r['parameters']:
            ps.append('{ %s, %s, %s, %s, 0 }'%(direction_names.get(x['direction']),param_names.get(x['type']),fval(x['min_f64']),fval(x['max_f64'])))
        arrays.append('static ParameterDescRunTime s_%sParameterDescRunTime[] = { %s };'%(name,', '.join(ps)))
    if r['original_nonnull'] and 'events' in r['original_nonnull']:
        es=['{ %d, 0 }'%x['num_parameters'] for x in r['events']]
        arrays.append('static EventDescRunTime s_%sEventDescRunTime[] = { %s };'%(name,', '.join(es)))
    fields=[]
    for field in ('channel_maps','parameters','events'):
        if field in r['original_nonnull']:
            sym={'channel_maps':'ChannelMaps','parameters':'ParameterDescRunTime','events':'EventDescRunTime'}[field]
            fields.append('s_%s%s'%(name,sym))
        else: fields.append('0')
    fields.append('0 /* pPlugInDescToolSide remains null */')
    return {'static_array_suggestions':arrays,'descriptor_pointer_slots_after_pProcess':fields,'note':'Names are artifact suggestions only; the current shared header still declares these fields as void* and does not expose the metadata types.'}

def callback_detail(name):
    invr=inv_rows[name]
    out=[]
    for i,(addr,sym) in enumerate(zip(invr['callbacks'],source_callbacks[name])):
        artist_name=None; export_sha=None
        if int(addr,16):
            p=workflow/'.ida-exports'/'BURNOUT_X360_ARTIST.XEX'/(addr+'.json')
            if p.exists():
                x=json.loads(p.read_text(encoding='utf-8')); artist_name=x.get('name'); export_sha=sha(p)
        out.append({'slot':['get_size','create_instance','pre_process','process'][i],'source_symbol':sym,'artist_address':addr,'artist_export_name':artist_name,'artist_export_sha256':export_sha})
    return out

source_header=repo/'vendor/renderware/include/rw/audio/core/PlugIn.h'
ref_plugin=Path(r'D:\Reverse\BP-Decomp_Workflow\references\DecFIGS\dwarfdump\SDKs\EATech\include\rw\audio\core\plugin.h')
cal_record=rows[selected[0]]
records=[]
for name in selected:
    r=rows[name]
    src=source_block(r['source']['path'],r['source']['symbol'])
    assert src['git_diff_clean_for_file'],name
    records.append({**r,'current_source':src,'source_callback_symbols':callback_detail(name),'initializer_suggestion':initializer(name,r)})
remaining=[{'name':r['name'],'record':r['record'],'source':r.get('source',{}).get('path'),'source_symbol':r.get('source',{}).get('symbol'),'original_nonnull':r['original_nonnull'],'source_null_fields':r.get('source_null_fields'),'metadata_pointers':r['metadata_pointers'] if 'metadata_pointers' in r else {k:r[k] for k in ('channel_maps','parameters','events')}} for r in scan['records'] if r['name'] not in selected]
unhomed=[{'name':r['name'],'record':r['record'],'source':None,'original_nonnull':r['original_nonnull'],'metadata_pointers':{k:r[k] for k in ('channel_maps','parameters','events')},'status':'unhomed; no reachability/source initializer suggestion'} for r in inv['records'] if not r.get('source')]
# Record raw pointer records from the preserved descriptor inventory; do not extend them into arrays.
base_inventory_path=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra\stage-descriptor-original-inventory.json')
packet={
 'title':'ARTIST audio runtime descriptor metadata recovery packet',
 'scope':'Read-only data recovery; first 9 source-homed records only. No source edits or runtime registration claims.',
 'source_tree':{'path':str(repo),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip(),'status':subprocess.check_output(['git','status','--short'],cwd=repo,text=True).splitlines(),'note':'Audio owner has unrelated uncommitted profiler/System/Dac work. Selected source homes each have a clean per-file diff.'},
 'calibration':scan['calibration'],
 'inputs':{'debt_inventory':str(Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra\AUDIO_DEBT_INVENTORY.json')),'debt_inventory_sha256':sha(Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\dac-terra\AUDIO_DEBT_INVENTORY.json')),'original_descriptor_inventory':str(base_inventory_path),'original_descriptor_inventory_sha256':sha(base_inventory_path),'calibrated_scan':str(art/'calibrated-source-homed-scan.json'),'calibrated_scan_sha256':sha(art/'calibrated-source-homed-scan.json'),'x360rd':scan['reader'],'x360rd_sha256':scan['reader_sha256'],'artist_raw':scan['raw_image'],'artist_raw_sha256':scan['raw_image_sha256'],'ppcdis':str(Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re\ppcdis.py')),'ppcdis_sha256':sha(Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re\ppcdis.py'))},
 'typed_declaration_basis':{'current_source_header':str(source_header),'current_source_header_sha256':sha(source_header),'current_PluginDescRunTime_metadata_fields':'pChannelMaps/pParameterDescRunTime/pEventDescRunTime remain void* at +0x14/+0x18/+0x1C; no common metadata types are currently declared in this source header.','reference_header':str(ref_plugin),'reference_header_sha256':sha(ref_plugin),'reference_types':{'ChannelMapPair':'signed char input; signed char output; raw X360 stride 2','ParameterDescRunTime':'ParameterDirection parameterDirection; ParameterType parameterType; double minExtremeValue; double maxExtremeValue; ParameterDescToolSide* pParameterDescToolSide; raw X360 stride 32 (pointer at +0x18, trailing four bytes zero)','ParameterDescToolSide':'char* pNameCpp; char* pNameSaf; char* help; char* units; double defaultValue; double minValue; double maxValue; EnumDesc* pEnumDesc','EventDescRunTime':'int numParameters; EventDescToolSide* pEventDescToolSide; raw X360 stride 8','EventDescToolSide':'char* pNameCppEnum; char* pNameCppStruct; char* pNameSaf; char* help','EnumDesc':'char* name; char* help; int numMembers; EnumMemberDesc* pEnumMemberDesc','EnumMemberDesc':'char* pNameCpp; char* pNameSaf; char* help; int value','PlugInDescToolSide':'char* help'},'enum_values':{'ParameterDirection':{'0':'PARAMETERDIRECTION_INPUT','1':'PARAMETERDIRECTION_OUTPUT'},'ParameterType':{'0':'PARAMETERTYPE_ENUM','1':'PARAMETERTYPE_FLOAT32','2':'PARAMETERTYPE_FLOAT64','3':'PARAMETERTYPE_POINTERTOSTRING','4':'PARAMETERTYPE_POINTER','5':'PARAMETERTYPE_GUID'}},'extent_calibration':'Parameter counts use descriptor constructor/attribute fields plus the sum of event numParameters; channel maps end only at (-1,-1); event table size uses descriptor event count. Cross-checks: AiffWriter 1x32-byte parameter and 2x8-byte events; BandPassIir2 2x32-byte parameters; GainFader 5x32-byte parameters and 1 event; Route 4x32-byte parameters and 1 event. The calibration sentinels are not used for array extents.'},
 'calibration_data_sentinel_context':{'0x82815570':'first word 0x7C8A2378; ppcdis decodes mr r10,r4 at CgsHash12::CalculateHash','0x82F264A8':'raw u32 value 4; independent data sentinel documented for CrashedStuntHudState resource count, not a metadata extent'},
 'selected_records':records,
 'deferred_source_homed_records':remaining,
 'unhomed_records':unhomed,
 'strings_and_names':{'source_plugin_names':'Each selected ARTIST descriptor pName pointer resolves to the exact plugin name and matches current source literal.','metadata_tool_strings':'All selected ParameterDescToolSide, EventDescToolSide and PlugInDescToolSide pointers are zero; no parameter labels, units, help strings, or event names are present in these runtime metadata tables. Do not synthesize them.','channel_map_semantics':'Signed-byte pairs are recorded verbatim; negative channel codes are not assigned semantic names here.'},
 'identity_separation':{'descriptor_record_and_metadata_tables':'ARTIST data addresses and typed payload bytes; source initializer suggestions point at new source-local data symbols, never at the original XEX virtual addresses.','callback_slots':'Code pointers with separate source callback symbols and ARTIST export names; ICF aliases are explicitly retained, not renamed to data symbols.','event_metadata':'EventDescRunTime numParameters/tool-side pointers are metadata only and are not event handler callback addresses.'},
 'limits':['Code callback symbols/addresses and data-table pointers are separate identities. ICF aliases are retained in both source_symbol and artist_export_name columns.','No getters, registration functions, source arrays, headers, or other source files were created or edited.','The nine suggestions are pointer/table initializer recovery only; they do not establish descriptor registration, runtime reachability, or full native audio parity.','The five unhomed records remain unhomed and are not given source homes, callback bodies, or array initializers.'],
}
out=art/'recovery-packet.json'
out.write_text(json.dumps(packet,indent=2)+'\n',encoding='utf-8')
print('packet',out,'sha256',sha(out),'selected',len(records),'deferred',len(remaining),'unhomed',len(unhomed),'current HEAD',packet['source_tree']['head'])
