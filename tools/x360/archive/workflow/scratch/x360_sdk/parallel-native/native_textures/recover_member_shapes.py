from pathlib import Path
r=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_textures')
p=r/'src/SDKs/RenderEngineClub/MAIN/components/src/texture2d.cpp'
s=p.read_text().replace('#include <xgraphics.h>','#include <xgraphics.h>\n#include "rw/rwcore_structs.h"')
s=s.replace('Texture2D::ResourceDescriptor* Texture2D::GetResourceDescriptor(\n    ResourceDescriptor* lpDescriptor, const Parameters* lpParameters)\n{','Texture2D::ResourceDescriptor Texture2D::GetResourceDescriptor(const Parameters& parameters)\n{\n    ResourceDescriptor descriptor;\n    ResourceDescriptor* lpDescriptor = &descriptor;\n    const Parameters* lpParameters = &parameters;')
s=s.replace('maEntries','m_baseResourceDescriptors').replace('.muSize','.m_size').replace('.muAlignment','.m_alignment')
s=s.replace('Xbox2GetPhysicalMemorySize(lpParameters)','Xbox2GetPhysicalMemorySize(parameters)').replace('return lpDescriptor;', 'return descriptor;')
s=s.replace('u32 Texture2D::Xbox2GetPhysicalMemorySize(const Parameters* lpParameters)\n{','u32 Texture2D::Xbox2GetPhysicalMemorySize(const Parameters& parameters)\n{\n    const Parameters* lpParameters = &parameters;')
s=s.replace('Texture2D* Texture2D::Initialize(const rw::Resource* lpResource, const Parameters* lpParameters)\n{','Texture2D* Texture2D::Initialize(const rw::Resource& resource, const Parameters& parameters)\n{\n    const Parameters* lpParameters = &parameters;')
s=s.replace('Texture::Initialize(lpResource, &lParameters)', 'Texture::Initialize(resource, lParameters)')
p.write_bytes(s.replace('\n','\r\n').encode())
p=r/'src/SDKs/RenderEngineClub/MAIN/components/src/texture.cpp';s=p.read_text()
s=s.replace('Texture* Texture::Initialize(const rw::Resource* resource, const Parameters* parameters)\n{','Texture* Texture::Initialize(const rw::Resource& resourceRef, const Parameters& parametersRef)\n{\n    const rw::Resource* resource = &resourceRef;\n    const Parameters* parameters = &parametersRef;')
s=s.replace('Xbox2CheckPhysicalMemoryFlags(texture);','texture->Xbox2CheckPhysicalMemoryFlags();')
for result,name in [('void','Xbox2CheckPhysicalMemoryFlags'),('void','Release')]:
    s=s.replace(f'{result} Texture::{name}(Texture* texture)\n{{',f'{result} Texture::{name}()\n{{\n    Texture* texture = this;')
for result,name in [('Texture::Type','GetType'),('u32','GetWidth'),('u32','GetHeight'),('u32','GetDepth'),('PixelFormat','GetFormat')]:
    s=s.replace(f'{result} Texture::{name}(const Texture* texture)\n{{',f'{result} Texture::{name}() const\n{{\n    const Texture* texture = this;')
s=s.replace('void Texture::GetParameters(const Texture* texture, Parameters* parameters)\n{','void Texture::GetParameters(Parameters& parametersRef) const\n{\n    const Texture* texture = this;\n    Parameters* parameters = &parametersRef;')
s=s.replace('ResourceDescriptor5* Texture::GetResourceDescriptor(ResourceDescriptor5* descriptor, const Parameters* parameters)\n{','rw::ResourceDescriptor Texture::GetResourceDescriptor(const Parameters& parametersRef)\n{\n    rw::ResourceDescriptor descriptorRef;\n    rw::ResourceDescriptor* descriptor = &descriptorRef;\n    const Parameters* parameters = &parametersRef;')
s=s.replace('maEntries','m_baseResourceDescriptors').replace('.muSize','.m_size').replace('.muAlignment','.m_alignment').replace('return descriptor;', 'return descriptorRef;')
s=s.replace('bool Texture::Lock(Texture* texture, u32 flags, u32 level, u32 index, Locked* locked)\n{','bool Texture::Lock(u32 flags, u32 level, u32 index, Locked& lockedRef)\n{\n    Texture* texture = this;\n    Locked* locked = &lockedRef;')
s=s.replace('void Texture::Unlock(Texture* texture, const Locked* locked)\n{','void Texture::Unlock(const Locked& lockedRef)\n{\n    Texture* texture = this;\n    const Locked* locked = &lockedRef;')
for name in ['GetType','GetWidth','GetHeight','GetDepth']:
    s=s.replace(name+'(texture)', 'texture->'+name+'()')
s=s.replace('GetParameters(texture, &parameters);','texture->GetParameters(parameters);')
p.write_bytes(s.replace('\n','\r\n').encode())
p=r/'src/GameSource/Game/BrnLoadingScreenRenderer.cpp';s=p.read_text()
a=s.index('        renderengine::Texture2D::ResourceDescriptor lDesc;');b=s.index('        XMemCpy',a)
s=s[:a]+'''        const rw::ResourceDescriptor lDesc = renderengine::Texture2D::GetResourceDescriptor(lParams);
        rw::IResourceAllocator* lpAllocator = BrnResource::GetEmbeddedTextureAllocator();
        const rw::Resource lResource = lpAllocator->DoAllocate(lDesc, 0);
        renderengine::Texture2D* lpTexture = renderengine::Texture2D::Initialize(lResource, lParams);
        renderengine::Texture::Locked lLocked;
        lpTexture->Lock(0, 0, 0, lLocked);
'''+s[b:]
s=s.replace('renderengine::Texture::Unlock(lpTexture, &lLocked);', 'lpTexture->Unlock(lLocked);')
p.write_bytes(s.replace('\n','\r\n').encode())
p=r/'src/GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.cpp';s=p.read_text()
a=s.index('            rw::ResourceDescriptor allocation;');b=s.index('\n        }',a)
s=s[:a]+'''            const rw::Resource resource = allocator->DoAllocate(descriptor, 0);
            return renderengine::Texture2D::Initialize(resource, parameters);'''+s[b:]
for name in ['Normal','YUY2','Compressed']:
    s=s.replace(f'renderengine::Texture2D::GetResourceDescriptor(&l{name}Desc, &l{name}Params);', f'l{name}Desc = renderengine::Texture2D::GetResourceDescriptor(l{name}Params);')
import re
s=re.sub(r'renderengine::Texture::Lock\(([^,\n]+), 0, 0, 0,\s*&([^\)]+)\);',r'\1->Lock(0, 0, 0, \2);',s)
p.write_bytes(s.replace('\n','\r\n').encode())
