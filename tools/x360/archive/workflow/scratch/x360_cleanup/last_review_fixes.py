import re
import strip_host as s

for name in [
    'src/GameShared/GameClasses/Graphics/CgsRenderTarget.cpp',
    'src/GameSource/Graphics/BrnRendererModule.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxrendertarget.cpp',
]:
    path=s.ROOT/name;text=path.read_text(encoding='utf-8-sig')
    text=re.sub(r'^\s*renderengine::Device::SetState\(lpState\);\s*\n','\n',text,flags=re.M)
    text=re.sub(r'(?m)^(\s*)((?:renderengine::)?gpLastRenderTargetState = lpState;)',r'\1renderengine::Device::SetState(lpState);\n\1\2',text)
    path.write_bytes(text.replace('\n','\r\n').encode('utf-8'))

name='src/GameSource/Graphics/BrnRendererModule_wBT_01.cpp'
path=s.ROOT/name;data=path.read_bytes();spans=[]
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type=='function_definition' and 'BrnRendererModule::TakeScreenshot' in s.declarator_names(node,data):
        spans.append(s.erase_span(node,data,True))
text=s.apply_spans(data,spans).decode('utf-8').replace('\r\n','\n')
text=text.replace('const u64 lu64MainStart', 'const u32 lu64MainStart')
text=text.replace('const u64 lu64PhysicalStart', 'const u32 lu64PhysicalStart')
text=text.replace('const u64 lu64PhysicalEnd', 'const u32 lu64PhysicalEnd')
text=text.replace('static_cast<u64>(reinterpret_cast<uintptr_t>', 'static_cast<u32>(reinterpret_cast<uintptr_t>')
text=text.replace('    const u32 lu64PhysicalEnd', '    const u32 lu64MainEnd = lu64MainStart + luMainCapacity;\n    const u32 lu64PhysicalEnd',1)
text=text.replace('lu64MainStart','luMainStart').replace('lu64MainEnd','luMainEnd').replace('lu64PhysicalStart','luPhysicalStart').replace('lu64PhysicalEnd','luPhysicalEnd')
path.write_bytes(text.replace('\n','\r\n').encode('utf-8'))

path=s.ROOT/'src/SDKs/EATech/include/Apt/AptLoader.cpp'
text=path.read_text(encoding='utf-8-sig').replace('static_cast<char*>(pData) + 0x20)->Link(', 'static_cast<char*>(pData) + 0x10)->Link(')
path.write_bytes(text.replace('\n','\r\n').encode('utf-8'))
print('Fixed native state-call ordering, removed screenshot substitute, and restored 32-bit link/report widths')
