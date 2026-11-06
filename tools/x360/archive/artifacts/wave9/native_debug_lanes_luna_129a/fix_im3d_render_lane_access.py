from pathlib import Path
p = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a\src\GameShared\GameClasses\Graphics\ImmediateMode\CgsIm3d.cpp")
raw = p.read_bytes()
bom = raw.startswith(b"\xef\xbb\xbf")
text = raw.decode("utf-8-sig" if bom else "utf-8")
nl = "\r\n" if "\r\n" in text else "\n"
text = text.replace("\r\n", "\n")
repls = {
    "lpOutput[luVertex].mPosition.x = lpVertices[luVertex].mv3Pos.x;":
    "lpOutput[luVertex].mPosition.x = lpVertices[luVertex].mv3Pos.GetX().GetFloat();",
    "lpOutput[luVertex].mPosition.y = lpVertices[luVertex].mv3Pos.y;":
    "lpOutput[luVertex].mPosition.y = lpVertices[luVertex].mv3Pos.GetY().GetFloat();",
    "lpOutput[luVertex].mPosition.z = lpVertices[luVertex].mv3Pos.z;":
    "lpOutput[luVertex].mPosition.z = lpVertices[luVertex].mv3Pos.GetZ().GetFloat();",
}
for old, new in repls.items():
    n = text.count(old)
    if n != 1:
        raise SystemExit(f"expected one occurrence for {old!r}, got {n}")
    text = text.replace(old, new, 1)
out = (b"\xef\xbb\xbf" if bom else b"") + text.replace("\n", nl).encode("utf-8")
p.write_bytes(out)
print(f"updated {p} sha256={__import__('hashlib').sha256(out).hexdigest()}")