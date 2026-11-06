from pathlib import Path
p=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a/vendor/dirtysdk/src/x360/connapi_x360.cpp")
s=p.read_text(encoding="utf-8")
repls=[
('extern "C" s32 ProtoMangleConnect(ProtoMangleRefT*, u16, const DirtyAddrT*);',
 'extern "C" void ProtoMangleConnect(ProtoMangleRefT*, s32, const char*);'),
('extern "C" void VoipSetLocalUser(VoipRefT*, const DirtyAddrT*);',
 'extern "C" void VoipSetLocalUser(VoipRefT*, const char*);'),
('VoipSetLocalUser(ref->pVoipRef, addr);',
 'VoipSetLocalUser(ref->pVoipRef, addr->strMachineAddr);'),
('''            ProtoMangleConnect(ref->pProtoMangle, ref->uGamePort,\n                               &client->UserInfo.DirtyAddr);''',
 '''            ProtoMangleConnect(ref->pProtoMangle, ref->uGamePort,\n                               client->UserInfo.DirtyAddr.strMachineAddr);''')]
for old,new in repls:
    if old not in s: raise SystemExit("missing fragment: "+old.splitlines()[0])
    s=s.replace(old,new,1)
p.write_text(s,encoding="utf-8",newline="\r\n")
