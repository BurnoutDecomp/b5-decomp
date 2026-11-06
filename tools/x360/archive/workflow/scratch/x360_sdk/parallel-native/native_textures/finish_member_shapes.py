from pathlib import Path
import re
r=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_textures')
p=r/'src/GameSource/Game/BrnLoadingScreenRenderer.cpp'
s=p.read_text(encoding='utf-8')
try:s=s.encode('cp1252').decode('utf-8')
except (UnicodeEncodeError,UnicodeDecodeError):pass
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
p=r/'src/GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.cpp';s=p.read_text(encoding='utf-8')
a=s.index('            rw::ResourceDescriptor allocation;');b=s.index('\n        }',a)
s=s[:a]+'''            const rw::Resource resource = allocator->DoAllocate(descriptor, 0);
            return renderengine::Texture2D::Initialize(resource, parameters);'''+s[b:]
for name in ['Normal','YUY2','Compressed']:
    s=s.replace(f'renderengine::Texture2D::GetResourceDescriptor(&l{name}Desc, &l{name}Params);', f'l{name}Desc = renderengine::Texture2D::GetResourceDescriptor(l{name}Params);')
s=re.sub(r'renderengine::Texture::Lock\(([^,\n]+), 0, 0, 0,\s*&([^\)]+)\);',r'\1->Lock(0, 0, 0, \2);',s)
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
