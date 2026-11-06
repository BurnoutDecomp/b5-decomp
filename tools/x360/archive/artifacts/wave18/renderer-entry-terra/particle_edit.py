from pathlib import Path
p=Path('src/GameSource/Effects/Particles/ParticleModule.cpp')
s=p.read_text(encoding='utf-8')
s=s.replace('#include <cstddef>', '#include "GameSource/Effects/Particles/ParticleCpuMonitors.h"\n#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"\n#include "GameShared/GameClasses/System/CgsHardwareInit.h"\n#include "SDKs/EATech/eajobs/job_scheduler.h"\n#include <cstddef>',1)
a=s.index('    void ParticleModule::BeginParticleRenderJob(')
b=s.index('    void ParticleModule::BuildLionVertexBuffers(',a)
# Retire old PC-only explanatory banner preceding the next function.
c=s.rfind('    //', a,b)
new='''    // ARTIST 8228A7C0: prepare both jobs while their vertex buffers remain locked.
    void ParticleModule::BeginParticleRenderJob(const ParticleRenderData& data)
    {
        ParticleCpuMonitors& monitors = (data.muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate)
            ? gCrashCpuMonitors : gRaceCpuMonitors;
        CgsDev::PerfMonCpu::StartMonitor(monitors.miBeginParticleRenderJob);
        static u32 lastFrame = 0;
        const bool changed = data.muCurrentFrame != lastFrame;
        lastFrame = data.muCurrentFrame;
        if (changed)
        {
            const Matrix44& source = data.mCgsCamera.mView;
            rw::math::vpu::Matrix44Affine view;
            view.xAxis = Vector3(source.xAxis.mV);
            view.yAxis = Vector3(source.yAxis.mV);
            view.zAxis = Vector3(source.zAxis.mV);
            view.wAxis = Vector3(source.wAxis.mV);
            const Matrix44& projection = data.mCgsCamera.mProjection;
            mParticleRenderJobDataSparks.mSparkFrameDataSet.Update(view, projection, data.mfCurrentTimeStep);
            f32 time = mParticleRenderJobDataSparks.mSparkFrameDataSet.GetFrame(0).mfTimeStamp;
            bool anyBuckets = false;
            for (u32 i = 0; i < KU_NUM_SPARK_ARRAYS; ++i)
            {
                maSparks[i].FreeUnusedBuckets(time);
                if (maSparks[i].mRegularBank.mpBuckets || maSparks[i].mCrashBank.mpBuckets)
                    anyBuckets = true;
            }
            if (!anyBuckets)
                mParticleRenderJobDataSparks.mSparkFrameDataSet.Reset(view, projection, 0.0f);
            else if (data.muFlags & ParticleRenderData::eRenderDataFlagCameraSwitched)
                mParticleRenderJobDataSparks.mSparkFrameDataSet.Reset(view, projection, time);
        }
        mParticleRenderJobDataParticles.mSimpleParticleBatchArray.Clear();
        mParticleRenderJobDataSparks.mSparkBatchArray.Clear();
        mVertexBufferManagerSparks.FlipBuffer();
        mVertexBufferManagerParticles.FlipBuffer();
        mParticleRenderJobDataSparks.mDestinationVertexBuffer = mVertexBufferManagerSparks.Lock();
        mParticleRenderJobDataParticles.mDestinationVertexBuffer = mVertexBufferManagerParticles.Lock();
        ParticleRenderJobData& sparks = mParticleRenderJobDataSparks;
        sparks.mCamera = data.mCgsCamera;
        sparks.mpaSimpleParticleArrays = 0;
        sparks.mpaSparkArrays = maSparks;
        sparks.mfCurrentTime = data.mfCurrentTime;
        sparks.mfTimeMultiplier = data.mfTimeStepMultiplier;
        sparks.mfWhiteLevel = data.mfWhiteLevel;
        sparks.mbIsCrash = (data.muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0;
        sparks.mbRenderParticles = false;
        sparks.mbRenderSparks = (data.muFlags & ParticleRenderData::eRenderDataFlagRenderSparks) != 0;
        ParticleRenderJobData& particles = mParticleRenderJobDataParticles;
        particles.mCamera = data.mCgsCamera;
        particles.mpaSimpleParticleArrays = maSimpleParticles;
        particles.mpaSparkArrays = 0;
        particles.mfCurrentTime = data.mfCurrentTime;
        particles.mfTimeMultiplier = data.mfTimeStepMultiplier;
        particles.mfWhiteLevel = data.mfWhiteLevel;
        particles.mbIsCrash = sparks.mbIsCrash;
        particles.mbRenderParticles = (data.muFlags & ParticleRenderData::eRenderDataFlagRenderSimple) != 0;
        particles.mbRenderSparks = false;
        CgsSystem::JobManager()->AddJobs(&mParticleRenderJobSparks, 1);
        CgsSystem::JobManager()->AddJobs(&mParticleRenderJobParticles, 1);
        CgsDev::PerfMonCpu::StopMonitor(monitors.miBeginParticleRenderJob);
    }

    // ARTIST 8227A320: finish both jobs before unlocking either destination.
    void ParticleModule::WaitOnParticleRenderJob(const ParticleRenderData& data)
    {
        ParticleCpuMonitors& monitors = (data.muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate)
            ? gCrashCpuMonitors : gRaceCpuMonitors;
        CgsDev::PerfMonCpu::StartMonitor(monitors.miWaitOnParticleRenderJob);
        mParticleRenderJobSparks.WaitOn(0, 0, -1);
        mParticleRenderJobParticles.WaitOn(0, 0, -1);
        mVertexBufferManagerSparks.UnLock();
        mVertexBufferManagerParticles.UnLock();
        CgsDev::PerfMonCpu::StopMonitor(monitors.miWaitOnParticleRenderJob);
    }

'''
s=s[:a]+new+s[b:]
a=s.index('    void ParticleModule::RenderQuarterResParticles(')
b=s.index('    // Native absolute layout pin.',a)
new='''    // ARTIST 82294A20: r5 is the z-fade target consumed by both simple passes.
    void ParticleModule::RenderQuarterResParticles(const ParticleRenderData& data, CgsRenderTarget* target)
    {
        ParticleCpuMonitors& monitors = (data.muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate)
            ? gCrashCpuMonitors : gRaceCpuMonitors;
        CgsDev::PerfMonCpu::StartMonitor(monitors.miRenderQuarterResParticles);
        renderengine::VertexBuffer* const vertexBuffer = mVertexBufferManagerParticles.GetVertexBuffer();
        const u32 preLion = mParticleRenderJobDataParticles.mSimpleParticleBatchArray.GetPreLionCount();
        const u32 count = mParticleRenderJobDataParticles.mSimpleParticleBatchArray.GetLength();
        const f32 nearPlane = data.mCgsCamera.maProjectionScalars[7];
        const f32 farPlane = data.mCgsCamera.maProjectionScalars[8];
        const bool zFade = (data.muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0 || mbZFadeEnabled;
        CgsDev::PerfMonCpu::StartMonitor(monitors.miSimpleParticlesDispatch);
        mSimpleParticleRenderer.Dispatch(vertexBuffer, mParticleRenderJobDataParticles.mSimpleParticleBatchArray,
                                         0, preLion, target, nearPlane, farPlane, zFade);
        CgsDev::PerfMonCpu::StopMonitor(monitors.miSimpleParticlesDispatch);
        if (!mbIsInJunkyard)
        {
            CgsDev::PerfMonCpu::StartMonitor(monitors.miLionDispatch);
            cLionFX::Dispatch(mVertexBufferManagerLion.GetVertexBuffer(), mLionBatchArray,
                data.mfWhiteLevel, false, nearPlane, farPlane, KF_LION_DEPTH_FADE_DISTANCE, 0.0f, 0.0f, 0);
            CgsDev::PerfMonCpu::StopMonitor(monitors.miLionDispatch);
        }
        CgsDev::PerfMonCpu::StartMonitor(monitors.miSimpleParticlesDispatch);
        mSimpleParticleRenderer.Dispatch(vertexBuffer, mParticleRenderJobDataParticles.mSimpleParticleBatchArray,
                                         preLion, count, target, nearPlane, farPlane, zFade);
        CgsDev::PerfMonCpu::StopMonitor(monitors.miSimpleParticlesDispatch);
        CgsDev::PerfMonCpu::StopMonitor(monitors.miRenderQuarterResParticles);
    }

'''
s=s[:a]+new+s[b:]
p.write_text(s,encoding='utf-8',newline='')
