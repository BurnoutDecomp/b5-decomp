from pathlib import Path
p=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a/vendor/dirtysdk/src/x360/connapi_x360.cpp")
s=p.read_text(encoding="utf-8")
old='#include "dirtylib.h"\n#include "voip.h"'
new='#include "dirtylib.h"\n#include "platform.h"\n#include "voip.h"'
if old not in s: raise SystemExit("include insertion point missing")
s=s.replace(old,new,1)
old='extern "C" s32 ProtoMangleComplete(ProtoMangleRefT*, const DirtyAddrT*, s32);'
new='extern "C" s32 ProtoMangleComplete(ProtoMangleRefT*, s32*, s32*);'
if old not in s: raise SystemExit("ProtoMangleComplete declaration missing")
s=s.replace(old,new,1)
old='extern "C" s32 VoipConnect4(VoipRefT*, s32, const DirtyAddrT*, s32, s32, s32, u32, s32);'
new='extern "C" s32 VoipConnect4(VoipRefT*, s32, const char*, u32, u32, u32, u32, u32);'
if old not in s: raise SystemExit("VoipConnect4 declaration missing")
s=s.replace(old,new,1)
old='extern "C" s32 VoipRemove(VoipRefT*, s32);'
new='extern "C" void VoipRemove(VoipRefT*, s32);'
if old not in s: raise SystemExit("VoipRemove declaration missing")
s=s.replace(old,new,1)
old='extern "C" s32 NetGameUtilControl(NetGameUtilRefT*, s32, s32);'
new='extern "C" void NetGameUtilControl(NetGameUtilRefT*, s32, s32);'
if old not in s: raise SystemExit("NetGameUtilControl declaration missing")
s=s.replace(old,new,1)
old='extern "C" char* ds_snzprintf(char*, s32, const char*, ...);'
if old not in s: raise SystemExit("local ds_snzprintf declaration missing")
s=s.replace(old,'',1)
old='''            const s32 complete = ProtoMangleComplete(
                ref->pProtoMangle, &client->UserInfo.DirtyAddr, 0);'''
new='''            const s32 complete = ProtoMangleComplete(
                ref->pProtoMangle,
                reinterpret_cast<s32*>(&client->UserInfo.uAddr), 0);'''
if old not in s: raise SystemExit("ProtoMangleComplete call missing")
s=s.replace(old,new,1)
old='''        const s32 connId = VoipConnect4(ref->pVoipRef, clientIndex,
            &client->UserInfo.DirtyAddr, address,
            client->VoipInfo.uLocalPort, client->VoipInfo.uMnglPort,
            client->UserInfo.uClientId, 0);'''
new='''        const s32 connId = VoipConnect4(ref->pVoipRef, clientIndex,
            client->UserInfo.DirtyAddr.strMachineAddr,
            static_cast<u32>(address), client->VoipInfo.uMnglPort,
            client->VoipInfo.uLocalPort, client->UserInfo.uClientId, 0);'''
if old not in s: raise SystemExit("VoipConnect4 call missing")
s=s.replace(old,new,1)
p.write_text(s,encoding="utf-8",newline="\r\n")
