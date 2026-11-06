import pathlib,re
p=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a\src\GameSource\GameState\Progression\BrnProgressionDebugComponent.cpp')
data=p.read_bytes()
pat=re.compile(rb'(?m)^(void MaybeDrawText[^\r\n]*?)[ \t]+(\r?)$')
for line in data.splitlines(keepends=True):
    if b'void MaybeDrawText' in line:
        print(repr(line),len(line),pat.findall(line))
print('total',pat.findall(data))
