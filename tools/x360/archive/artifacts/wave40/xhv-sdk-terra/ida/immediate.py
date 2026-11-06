import idaapi,ida_funcs,ida_bytes,ida_name,json
names=['XHVpCreateEngine','XHVEngineCreate','RTpFindProcMode','RemoteTalkerSetOutputVolumes','RemoteTalkerSetOutputVoice','VoiceChatDestroy','LoopbackDestroy']
out=[]
for n in names:
 a=ida_name.get_name_ea(idaapi.BADADDR,n); f=ida_funcs.get_func(a)
 if f: out.append({'name':n,'addr':hex(a),'size':f.end_ea-f.start_ea,'bytes':ida_bytes.get_bytes(f.start_ea,f.end_ea-f.start_ea).hex()})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave40/xhv-sdk-terra/artist-immediate.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
