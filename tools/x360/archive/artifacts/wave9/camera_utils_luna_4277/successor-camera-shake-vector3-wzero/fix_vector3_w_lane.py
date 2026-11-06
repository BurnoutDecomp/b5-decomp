from pathlib import Path
import hashlib
p=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277\src\GameSource\Director\Camera\Utils\BrnCameraShakeICEController.cpp")
raw=p.read_bytes();bom=raw.startswith(b"\xef\xbb\xbf");text=raw.decode("utf-8-sig" if bom else "utf-8");nl="\r\n" if "\r\n" in text else "\n";text=text.replace("\r\n","\n")
old="""    Vector3 lTargetCameraAnglesRandom;
    lTargetCameraAnglesRandom.SetX(
        mfBumpValue * 0.01f * (1.0f / kfShakeFocalDistance) * lfShakeAmplitude);
    lTargetCameraAnglesRandom.SetY(0.0f);
    lTargetCameraAnglesRandom.SetZ(0.0f);"""
new="""    Vector3 lTargetCameraAnglesRandom(
        mfBumpValue * 0.01f * (1.0f / kfShakeFocalDistance) * lfShakeAmplitude,
        0.0f, 0.0f);"""
if text.count(old)!=1:raise SystemExit(f"expected one initializer block, got {text.count(old)}")
text=text.replace(old,new,1)
out=(b"\xef\xbb\xbf" if bom else b"")+text.replace("\n",nl).encode("utf-8");p.write_bytes(out)
print(f"updated {p} sha256={hashlib.sha256(out).hexdigest()}")