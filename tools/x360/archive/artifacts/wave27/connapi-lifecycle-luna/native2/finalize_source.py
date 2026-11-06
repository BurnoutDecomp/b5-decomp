from pathlib import Path
p=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a/vendor/dirtysdk/src/x360/connapi_x360.cpp")
s=p.read_text(encoding="utf-8")
old='static const char ConnApiTunnelKey[] = "";'
new='static const char ConnApiTunnelKey[] = ""; // ARTIST passes 0x8324E919, whose first byte is NUL.'
if old not in s: raise SystemExit("missing ConnApiTunnelKey")
p.write_text(s.replace(old,new,1), encoding="utf-8", newline="\r\n")
