// The overlay must read a completed frame while current-frame counters run.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

namespace CgsDev { namespace Assert {
std::atomic<unsigned> assertions{0};
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
static int checks, failures;
static void Check(bool good,const char* label) {
    ++checks; if (!good) { ++failures; std::printf("FAIL %s\n",label); }
}
int main() {
    using namespace CgsDev;
    using namespace CgsDev::PerfMonCpu;
    Check(Construct(64,nullptr),"registry constructed");
    const int timer = AddMonitor("Dispatch work",E_PMP_GENERAL,false,1.0f,false);
    const int scaled = AddMonitor("Per-step work",E_PMP_GENERAL,false,1.0f,true);
    StartProfiling();
    std::atomic<bool> running{true};
    std::thread writer([&] {
        for (int i=0;i<10000;++i) { StartMonitor(timer); StopMonitor(timer); }
        running = false;
    });
    bool stable = true;
    do {
        PerfMonCpuMonitorData data = {};
        GetMonitorData(timer,&data);
        stable &= data.miNumCalls == 0 && data.mfCurrentValue == 0.0f;
    } while (running);
    writer.join();
    Check(stable,"overlay sees completed data while dispatch updates live counters");
    StopProfiling();
    PerfMonCpuMonitorData data = {}; GetMonitorData(timer,&data);
    Check(data.miNumCalls == 10000 && data.mfCurrentValue > 0,
          "joined frame publishes matching time and call count");
    StartProfiling();
    GetMonitorData(timer,&data);
    Check(data.miNumCalls == 10000 && data.mfCurrentValue > 0,
          "starting another frame does not erase the displayed snapshot");
    SetNumIterationsTaken(4);
    for (int i=0;i<8;++i) { StartMonitor(scaled); StopMonitor(scaled); }
    StopProfiling(); GetMonitorData(scaled,&data);
    Check(data.miNumCalls == 2,"published count uses the same simulation scaling as time");

    std::atomic<int> registrars{2};
    auto add = [&] {
        for (int i=0;i<16;++i) AddMonitor("Complete registered slot",E_PMP_GENERAL,false,1,false);
        --registrars;
    };
    std::thread first(add),second(add);
    bool published = true;
    do {
        const int count = GetMonitorCount();
        for (int i=2;i<count;++i) {
            GetMonitorData(i,&data);
            published &= std::strcmp(data.mpcName,"Complete registered slot") == 0;
        }
    } while (registrars);
    first.join(); second.join();
    Check(published && GetMonitorCount() == 34,"parallel registration publishes complete unique slots");
    ResetValuesInActiveMonitors(); StartProfiling(); GetMonitorData(timer,&data);
    Check(data.miNumCalls == 0 && data.mfCurrentValue == 0,"reset clears the completed snapshot");
    StopProfiling(); Destruct();
    Check(CgsDev::Assert::assertions == 0,"independent frame owners raise no monitor assertions");
    std::printf("PCPerfMonSnapshot: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
