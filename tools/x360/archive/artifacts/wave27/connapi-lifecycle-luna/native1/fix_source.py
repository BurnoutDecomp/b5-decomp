from pathlib import Path
p=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a/vendor/dirtysdk/src/x360/connapi_x360.cpp")
s=p.read_text(encoding="utf-8")
old="""        if (i != ref->iSelf &&\n            (ref->eState == 3 || (client->GameInfo.uGameServerFallback & 1)))\n"""
new="""        if (i != ref->iSelf &&\n            (ref->eState == 3 || (client->uConnFlags & 1)))\n"""
if old not in s: raise SystemExit("missing update-connection selection")
s=s.replace(old,new,1)
old="""        NetPrintfCode("connapi: can't start game unless in connected state\\n");\n"""
new="""        NetPrintfCode("connapi: can't start game when not in hosting or joining state\\n");\n"""
if old not in s: raise SystemExit("missing ConnApiStart diagnostic")
s=s.replace(old,new,1)
p.write_text(s, encoding="utf-8", newline="\r\n")
