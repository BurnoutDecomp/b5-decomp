#include "BrnCommonTypes.h"
#include "pc/math/VmxFloat.h"
#include "pc/debug/IniComponentLifecycle.h"
#include <cmath>
#include <cstdio>

namespace CgsDev { struct DebugComponent {}; }
namespace BrnWorld
{
    enum EActiveRaceCarIndex { E_ACTIVE_RACE_CAR_INDEX_0=0, E_ACTIVE_RACE_CAR_INDEX_COUNT=4 };
    struct ActiveRaceCar
    {
        bool attached=false;
        mutable int velocityReads=0;
        Vector3 velocity={3,4,0,0};
        bool IsAttached() const { return attached; }
        Vector3 GetVelocity() const { ++velocityReads; return velocity; }
    };
    struct RaceCarEntityModule
    {
        EActiveRaceCarIndex index=static_cast<EActiveRaceCarIndex>(-1);
        mutable int lookups=0, invalidLookups=0;
        mutable ActiveRaceCar car;
        EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return index; }
        ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex i) const
        {
            ++lookups;
            if (i<0 || i>=E_ACTIVE_RACE_CAR_INDEX_COUNT) ++invalidLookups;
            return &car; // observer: count invalid reads without crashing this fixture
        }
    };
    struct RaceCarEntityModuleDebugComponent : CgsDev::DebugComponent
    {
        RaceCarEntityModule* mpRaceCarEntityModule=nullptr;
        s32 miAverageSpeedFrameCount=0;
        f32 mfAverageSpeed=0;
        void Update();
    };
}
#include "IniStartup.inc"
namespace
{
    int checks=0, failures=0;
    void Check(bool pass, const char* text)
    { ++checks; if (!pass) { ++failures; std::printf("FAIL %s\n",text); } }
}
int main()
{
    using namespace BrnWorld;
    CgsPC::Debug::ResetIniComponents();
    RaceCarEntityModule module;
    RaceCarEntityModuleDebugComponent component, other;
    component.mpRaceCarEntityModule=&module;
    Check(!CgsPC::Debug::IsIniComponent(&component),"ordinary component has no startup-origin mark");
    CgsPC::Debug::SetIniComponent(&component,true);
    CgsPC::Debug::SetIniComponent(&component,true);
    Check(CgsPC::Debug::IsIniComponent(&component) && !CgsPC::Debug::IsIniComponent(&other),"INI origin is isolated by component identity");
    for (int index : {-1,4,7})
    {
        module.index=static_cast<EActiveRaceCarIndex>(index);
        module.lookups=module.invalidLookups=module.car.velocityReads=0;
        component.miAverageSpeedFrameCount=0;
        component.mfAverageSpeed=3.25f;
        component.Update();
        Check(module.lookups==0 && module.invalidLookups==0,"loading player index is checked before array access");
        Check(module.car.velocityReads==0,"loading never reads unattached player velocity");
        Check(component.miAverageSpeedFrameCount==0 && component.mfAverageSpeed==3.25f,"loading leaves average-speed history untouched");
    }
    module.index=E_ACTIVE_RACE_CAR_INDEX_0;
    module.lookups=module.invalidLookups=module.car.velocityReads=0;
    module.car.attached=false;
    component.Update();
    Check(module.lookups==1 && module.invalidLookups==0,"valid slot can be checked for attachment");
    Check(module.car.velocityReads==0 && component.miAverageSpeedFrameCount==0,"unattached valid slot cannot enter original update");
    component.mpRaceCarEntityModule=nullptr;
    Check(IniPlayerUnavailablePC(&component,nullptr),"INI startup tolerates absent owner before initialization");
    component.mpRaceCarEntityModule=&module;
    component.miAverageSpeedFrameCount=0; component.mfAverageSpeed=0;
    module.car.attached=true;
    component.Update();
    Check(module.car.velocityReads==1 && component.miAverageSpeedFrameCount==1,"attached car resumes original sample update");
    Check(std::fabs(component.mfAverageSpeed-5.0f)<0.00001f,"first velocity contributes original magnitude");
    module.car.velocity={10,0,0,0}; component.Update();
    Check(component.miAverageSpeedFrameCount==2 && std::fabs(component.mfAverageSpeed-7.5f)<0.00001f,"original running mean continues across attached frames");
    module.index=static_cast<EActiveRaceCarIndex>(-1);
    const int reads=module.car.velocityReads;
    component.Update();
    Check(module.car.velocityReads==reads && component.miAverageSpeedFrameCount==2,"later unloading waits again without resetting history");
    CgsPC::Debug::SetIniComponent(&component,false);
    Check(!CgsPC::Debug::IsIniComponent(&component) && !IniPlayerUnavailablePC(&component,nullptr),"explicit activation removes the startup-only policy");
    module.index=E_ACTIVE_RACE_CAR_INDEX_0; module.car.attached=true;
    module.lookups=0; component.Update();
    Check(module.lookups==1 && component.miAverageSpeedFrameCount==3,"ordinary activation executes the original method directly");
    CgsPC::Debug::SetIniComponent(&component,true);
    CgsPC::Debug::SetIniComponent(&other,true);
    CgsPC::Debug::ResetIniComponents();
    Check(!CgsPC::Debug::IsIniComponent(&component) && !CgsPC::Debug::IsIniComponent(&other),"manager reconstruction clears all startup origins");
    std::printf("PCDebugIniStartup: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
