from pathlib import Path
h=Path('src/GameSource/Effects/Particles/ParticleModule.h'); s=h.read_text(encoding='utf-8'); s=s.replace('void BuildLionVertexBuffers(const ParticleRenderData* lpRenderData);','void BuildLionVertexBuffers(const ParticleRenderData& lrRenderData);').replace('void RenderFullResParticles(const ParticleRenderData* lpRenderData);','void RenderFullResParticles(const ParticleRenderData& lrRenderData);'); h.write_text(s,encoding='utf-8',newline='')
p=Path('src/GameSource/Effects/Particles/ParticleModule.cpp'); s=p.read_text(encoding='utf-8');
for method in ['BuildLionVertexBuffers','RenderFullResParticles']:
 old='void ParticleModule::'+method+'(const ParticleRenderData* lpRenderData)\n    {'
 new='void ParticleModule::'+method+'(const ParticleRenderData& lrRenderData)\n    {\n        const ParticleRenderData* const lpRenderData = &lrRenderData;'
 assert old in s,method
 s=s.replace(old,new,1)
p.write_text(s,encoding='utf-8',newline='')
