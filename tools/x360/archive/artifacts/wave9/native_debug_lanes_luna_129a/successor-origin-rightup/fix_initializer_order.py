from pathlib import Path
import hashlib
p=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a\src\GameShared\GameClasses\Graphics\ImmediateMode\CgsIm2dTransform.cpp")
raw=p.read_bytes(); bom=raw.startswith(b"\xef\xbb\xbf"); text=raw.decode("utf-8-sig" if bom else "utf-8"); nl="\r\n" if "\r\n" in text else "\n"; text=text.replace("\r\n","\n")
pairs=[
("lTransform.mOriginXYZ = rw::math::vpu::Vector4(2.0f, 0.0f, 0.0f, -2.0);","lTransform.mOriginXYZ = rw::math::vpu::Vector4(-1.0f, 1.0f, 0.0f, 0.0f);"),
("lTransform.mRightUp = rw::math::vpu::Vector4(-1.0f, 1.0f, 0.0f, 0.0f);","lTransform.mRightUp = rw::math::vpu::Vector4(2.0f, 0.0f, 0.0f, -2.0f);"),
]
# Keep the replacements tied to the observed current initializer text.
pairs[0]=(pairs[0][0].replace("-2.0);","-2.0f);"),pairs[0][1])
for old,new in pairs:
    n=text.count(old)
    if n!=1: raise SystemExit(f"expected one source match, got {n}: {old}")
    text=text.replace(old,new,1)
out=(b"\xef\xbb\xbf" if bom else b"")+text.replace("\n",nl).encode("utf-8")
p.write_bytes(out)
print(f"updated {p} sha256={hashlib.sha256(out).hexdigest()}")