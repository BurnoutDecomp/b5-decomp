from pathlib import Path
import hashlib
p=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277\src\GameSource\Director\Camera\Utils\BrnCameraShakeICEController.cpp")
raw=p.read_bytes();bom=raw.startswith(b"\xef\xbb\xbf");text=raw.decode("utf-8-sig" if bom else "utf-8");nl="\r\n" if "\r\n" in text else "\n";text=text.replace("\r\n","\n");before=text
remove=[
('#include "GameShared/GameClasses/Development/Log/CgsLog.h"        // [diag] CgsDev::Log::gpDebugPrint\n','\n'),
("""    static u32 lines = 0;
    if (trace && lfShakeAmplitude > 0.1f && lines++ < 32 && CgsDev::Log::gpDebugPrint)
        *CgsDev::Log::gpDebugPrint << "[camera-rig] boost-shake type=" << static_cast<u32>(lu8ShakeType)
            << " keys=" << count << " frame=" << frame << " angles=" << angles.GetX().GetFloat() << ","
            << angles.GetY().GetFloat() << "," << angles.GetZ().GetFloat()
            << " lift=" << mMatrix.Pos().GetY().GetFloat() << "\\n";
""", "")]
for old,new in remove:
 n=text.count(old)
 if n!=1:raise SystemExit(f"expected one exact diagnostics block, got {n}: {old[:90]!r}")
 text=text.replace(old,new,1)
out=(b"\xef\xbb\xbf" if bom else b"")+text.replace("\n",nl).encode("utf-8");p.write_bytes(out)
print(f"updated {p} sha256={hashlib.sha256(out).hexdigest()}")