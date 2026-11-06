from pathlib import Path
root=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a")
header=root/"vendor/dirtysdk/include/connapi.h"
s=header.read_text(encoding="utf-8")
if "DIRTYSDK_CONNAPI_X360_NATIVE_ABI" not in s: raise SystemExit("selector macro missing")
s=s.replace("defined(DIRTYSDK_CONNAPI_X360_NATIVE_ABI)","defined(_XBOX)")
header.write_text(s,encoding="utf-8",newline="\r\n")
for rel in ("vendor/dirtysdk/src/x360/connapi_x360.cpp","vendor/dirtysdk/src/x360/gamemanager.cpp"):
    p=root/rel
    s=p.read_text(encoding="utf-8")
    old='#define DIRTYSDK_CONNAPI_X360_NATIVE_ABI 1\n#include "connapi.h"\n#undef DIRTYSDK_CONNAPI_X360_NATIVE_ABI'
    if old not in s: raise SystemExit("wrapper not found in "+rel)
    s=s.replace(old,'#include "connapi.h"',1)
    p.write_text(s,encoding="utf-8",newline="\r\n")
