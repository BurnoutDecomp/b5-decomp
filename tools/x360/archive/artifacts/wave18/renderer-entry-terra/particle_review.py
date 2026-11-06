from pathlib import Path
p=Path('src/GameSource/Effects/Particles/ParticleModule.cpp');s=p.read_text(encoding='utf-8');a=s.index('    // =========================================================================\n    // RenderQuarterResParticles'); b=s.index('    // ARTIST 82294A20:',a);s=s[:a]+s[b:];s=s.replace('        const u32 count = mParticleRenderJobDataParticles.mSimpleParticleBatchArray.GetLength();\n        const f32 nearPlane', '''        if (mParticleRenderJobDataParticles.mSimpleParticleBatchArray.GetCount() == -1)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Array used before Construct/Clear was called",
                "..\\\\..\\\\..\\\\GameShared\\\\GameClasses\\\\Containers/CgsArray.h", 336);
            CgsDev::Assert::EndAssert();
        }
        const u32 count = static_cast<u32>(mParticleRenderJobDataParticles.mSimpleParticleBatchArray.GetCount());
        const f32 whiteLevel = data.mfWhiteLevel;
        const f32 nearPlane''');s=s.replace('                data.mfWhiteLevel, false, nearPlane, farPlane, KF_LION_DEPTH_FADE_DISTANCE, 0.0f, 0.0f, 0);','                whiteLevel, false, nearPlane, farPlane, KF_LION_DEPTH_FADE_DISTANCE, 0.0f, 0.0f, 0);');p.write_text(s,encoding='utf-8',newline='')
p=Path('src/GameSource/Effects/Particles/ParticleModule.h');s=p.read_text(encoding='utf-8');s=s.replace('        // locks the two vertex buffers and starts the two particle render jobs. The jobs are\n        // run inline on this single-threaded build -- see the .cpp.','        // locks both vertex buffers and submits the two native particle render jobs.');a=s.index('        // X360 0x82294A20 -- render thread:');b=s.index('        void RenderQuarterResParticles(',a);s=s[:a]+'''        // ARTIST 0x82294A20: render the two simple batches around the Lion draw,
        // forwarding the original depth target to each simple-particle dispatch.
'''+s[b:];p.write_text(s,encoding='utf-8',newline='')
