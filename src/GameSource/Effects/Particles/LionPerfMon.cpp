#include "types.hpp"

namespace CgsDev
{
class PerfMonCpu
{
public:
    static int AddMonitor(const char* lpcName, int liColour, int liMin, double lfMax, int liParent, int liFlags);
};
}

class LionPerfMon
{
public:
    int Construct(int liParent, int liUnused0, int liUnused1);

private:
    int miRender;
    int miEmitterBlend;
    int miEmitterUpdate;
    int miEmitterGenerate;
    int miEmitterParticleBuild;
    int miSimulateMatrixParticles;
    int miSimulateVectorParticles;
    int miParticleBuild;
    int miEmitterRender;
    int miEmitterCubeRender;
    int miEmitterSortedRender;
    int miEmitterSortedBuild;
    int miEmitterBucketSort;
    int miEmitterBucketRender;
    int miEmitterSub;
    int miEmitterSubRender;
    int miEmitterSubUpdate;
    int miEmitterSubGenerate;
    int miEmitterSubParticleBuild;
    int miParticleManagerUpdate;
    int miParticleManagerRender;
    int miParticleManagerWait;
};

int LionPerfMon::Construct(int liParent, int, int)
{
    miRender = CgsDev::PerfMonCpu::AddMonitor("Render", 21, 0, 100.0, liParent, 0);
    miEmitterRender = CgsDev::PerfMonCpu::AddMonitor("EmitterRender", 21, 0, 100.0, liParent, 0);
    miEmitterCubeRender = CgsDev::PerfMonCpu::AddMonitor("EmitterCubeRender", 21, 0, 100.0, liParent, 0);
    miEmitterBlend = CgsDev::PerfMonCpu::AddMonitor("EmitterBlend", 21, 0, 100.0, liParent, 0);
    miEmitterUpdate = CgsDev::PerfMonCpu::AddMonitor("EmitterUpdate", 21, 0, 100.0, liParent, 0);
    miEmitterGenerate = CgsDev::PerfMonCpu::AddMonitor("EmitterGenerate", 21, 0, 100.0, liParent, 0);
    miEmitterParticleBuild = CgsDev::PerfMonCpu::AddMonitor("EmitterParticleBuild", 21, 0, 100.0, liParent, 0);
    miSimulateMatrixParticles = CgsDev::PerfMonCpu::AddMonitor("SimulateMatrixParticles", 21, 0, 100.0, liParent, 0);
    miSimulateVectorParticles = CgsDev::PerfMonCpu::AddMonitor("SimulateVectorParticles", 21, 0, 100.0, liParent, 0);
    miParticleBuild = CgsDev::PerfMonCpu::AddMonitor("ParticleBuild", 21, 0, 100.0, liParent, 0);
    miEmitterSortedRender = CgsDev::PerfMonCpu::AddMonitor("EmitterSortedRender", 21, 0, 100.0, liParent, 0);
    miEmitterSortedBuild = CgsDev::PerfMonCpu::AddMonitor("EmitterSortedBuild", 21, 0, 100.0, liParent, 0);
    miEmitterBucketSort = CgsDev::PerfMonCpu::AddMonitor("EmitterBucketSort", 21, 0, 100.0, liParent, 0);
    miEmitterBucketRender = CgsDev::PerfMonCpu::AddMonitor("EmitterBucketRender", 21, 0, 100.0, liParent, 0);
    miEmitterSub = CgsDev::PerfMonCpu::AddMonitor("EmitterSub", 21, 0, 100.0, liParent, 0);
    miEmitterSubRender = CgsDev::PerfMonCpu::AddMonitor("EmitterSubRender", 21, 0, 100.0, liParent, 0);
    miEmitterSubUpdate = CgsDev::PerfMonCpu::AddMonitor("EmitterSubUpdate", 21, 0, 100.0, liParent, 0);
    miEmitterSubGenerate = CgsDev::PerfMonCpu::AddMonitor("EmitterSubGenerate", 21, 0, 100.0, liParent, 0);
    miEmitterSubParticleBuild = CgsDev::PerfMonCpu::AddMonitor("EmitterSubParticleBuild", 21, 0, 100.0, liParent, 0);
    miParticleManagerUpdate = CgsDev::PerfMonCpu::AddMonitor("ParticleManagerUpdate", 21, 0, 100.0, liParent, 0);
    miParticleManagerRender = CgsDev::PerfMonCpu::AddMonitor("ParticleManagerRender", 21, 0, 100.0, liParent, 0);
    miParticleManagerWait = CgsDev::PerfMonCpu::AddMonitor("ParticleManagerWait", 21, 0, 100.0, liParent, 0);
    return miParticleManagerWait;
}
