import pathlib,re
repo=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a')
paths=['src/GameSource/GameState/Progression/BrnProgressionDebugComponent.cpp','src/GameSource/Gui/BrnGuiGuiCacheDebugComponent.cpp','src/GameSource/Massive/BrnMassiveDebugComponent.cpp']
pat=re.compile(rb'(?m)^(void MaybeDrawText[^\r\n]*?)[ \t]+(\r?)$')
for rel in paths:
    p=repo/rel; data=p.read_bytes(); updated,n=pat.subn(rb'\1\2',data)
    if n!=1: raise SystemExit('%s: expected one prototype line trim, got %d' % (rel,n))
    p.write_bytes(updated)
    print('%s: trimmed one trailing-space run' % rel)
