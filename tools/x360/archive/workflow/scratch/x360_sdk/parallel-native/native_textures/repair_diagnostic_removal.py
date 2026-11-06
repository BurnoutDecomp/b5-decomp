from pathlib import Path
import subprocess,re
r=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_textures')
p=r/'src/GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.cpp'
s=p.read_text(encoding='utf-8')
old=subprocess.check_output(['git','show','HEAD:src/GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.cpp'],cwd=r).decode('utf-8')
a=old.index('    // ---- RecvEvent'); b=old.index('    // ---- GetID',a)
block=old[a:b]
a1=block.index('            // [netimg]'); b1=block.index('            if (mbRenderTexture)',a1);block=block[:a1]+block[b1:]
a1=block.index('        // [netimg]'); b1=block.index('        if (miClearTexturesFrameCount',a1);block=block[:a1]+block[b1:]
a=s.index('    // ---- RecvEvent');b=s.index('    // ---- GetID',a);s=s[:a]+block+s[b:]
p.write_bytes(s.replace('\n','\r\n').encode())
