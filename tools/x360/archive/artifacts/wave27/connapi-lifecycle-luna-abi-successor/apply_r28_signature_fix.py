from pathlib import Path
p=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a/vendor/dirtysdk/src/x360/connapi_x360.cpp")
s=p.read_text(encoding="utf-8")
old='''extern "C" s32 NetGameUtilStatus(NetGameUtilRefT*, s32, void*, s32);\n\n\nnamespace'''
new='''extern "C" s32 NetGameUtilStatus(NetGameUtilRefT*, s32, void*, s32);\n\nnamespace'''
if old not in s: raise SystemExit("extra extern spacing not found")
s=s.replace(old,new,1)
old='''            const s32 complete = ProtoMangleComplete(\n                ref->pProtoMangle,\n                reinterpret_cast<s32*>(&client->UserInfo.uAddr), 0);'''
new='''            // ARTIST writes the secure address to UserInfo.uAddr (+0); no port output.\n            const s32 complete = ProtoMangleComplete(\n                ref->pProtoMangle,\n                reinterpret_cast<s32*>(&client->UserInfo.uAddr), 0);'''
if old not in s: raise SystemExit("ProtoMangleComplete output call not found")
s=s.replace(old,new,1)
old='''        const s32 connId = VoipConnect4(ref->pVoipRef, clientIndex,\n            client->UserInfo.DirtyAddr.strMachineAddr,\n            static_cast<u32>(address), client->VoipInfo.uMnglPort,\n            client->VoipInfo.uLocalPort, client->UserInfo.uClientId, 0);'''
new='''        // VoipConnect4 takes the XUID text and the mangle port before the local port.\n        const s32 connId = VoipConnect4(ref->pVoipRef, clientIndex,\n            client->UserInfo.DirtyAddr.strMachineAddr,\n            static_cast<u32>(address), client->VoipInfo.uMnglPort,\n            client->VoipInfo.uLocalPort, client->UserInfo.uClientId, 0);'''
if old not in s: raise SystemExit("VoipConnect4 call not found")
s=s.replace(old,new,1)
p.write_text(s,encoding="utf-8",newline="\r\n")
