from pathlib import Path
p=Path('src/GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp');s=p.read_text(encoding='utf-8'); marker='    // The two render EA::Jobs::Job objects and the five debris-update jobs'
a=s.index(marker); b=s.index('    for (s32 liJob',a)
n='''    // ARTIST 82294220: bind the two render records to real scheduler entries.
    mParticleRenderJobSparks.Clear();
    mParticleRenderJobSparks.mEntryPoint.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
        reinterpret_cast<const void*>(&ParticleRenderEntry), 0);
    mParticleRenderJobSparks.mEntryPoint.SetName("ParticleRender_Sparks");
    std::memset(&mParticleRenderJobDataSparks, 0, sizeof(mParticleRenderJobDataSparks));
    mParticleRenderJobSparks.SetData(&mParticleRenderJobDataSparks, sizeof(mParticleRenderJobDataSparks));
    mParticleRenderJobParticles.Clear();
    mParticleRenderJobParticles.mEntryPoint.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
        reinterpret_cast<const void*>(&ParticleRenderEntry), 0);
    mParticleRenderJobParticles.mEntryPoint.SetName("ParticleRender_Particles");
    std::memset(&mParticleRenderJobDataParticles, 0, sizeof(mParticleRenderJobDataParticles));
    mParticleRenderJobParticles.SetData(&mParticleRenderJobDataParticles, sizeof(mParticleRenderJobDataParticles));

'''
s=s[:a]+n+s[b:];p.write_text(s,encoding='utf-8',newline='')
