from pathlib import Path
r=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_lion/src/SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime')
p=r/'include/ParticleEmitter.h';s=p.read_bytes().decode();a=s.index('// The three specialisations the console emitted');s=s[:a]+'''// ParticleEmitter.cpp explicitly instantiates the three native helper variants.
// Callers see the template declaration above; its body stays in that owning TU.
'''.replace('\n','\r\n');p.write_bytes(s.encode())
p=r/'include/ParticleEmitter.cpp';s=p.read_bytes().decode();a=s.index('// The three the console emitted');b=s.index('template u32',a);s=s[:a]+'''// Emit the three native helper variants here (ARTIST 0x829120C0 / 0x829123E0 / 0x82912610).
'''.replace('\n','\r\n')+s[b:];p.write_bytes(s.encode())
p=r/'source/ParticleRender/ParticleRender.cpp';s=p.read_bytes().decode();s=s.replace('if (lbDrawThisWorld)','if (apEmitter->GetBindings().GetWorldIndex() == 0)');p.write_bytes(s.encode())
