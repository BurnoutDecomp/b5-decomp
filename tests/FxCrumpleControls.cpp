#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Sound/Playback/CgsSoundPcmTrace.h"
#include "GameSource/AttribSys/Enums/eImpactTime.h"
#undef CGS_ASSERT
static int assertions = 0;
#define CGS_ASSERT(c,m) do { if (!(c)) ++assertions; } while (0)
namespace CgsSound { namespace Utils {
// The effect uses only the linear curve. InterpolateLine bodies are production.
f32 Curve::GetOutput(f32 v, ECurveType curve) { CGS_ASSERT(curve == E_LINEAR, "linear"); return v; }
#include "fx_crumple_interpolate.inc"
}}
namespace CgsSound { namespace Playback {
struct Name { static u32 MakeHash(const char*) { return 1; } };
}}
namespace BrnPhysics { namespace Deformation {
struct CarSensorState { Vector3 mDisplacement = {}; };
struct CarState {
    u8 mu8NumSensors = 2;
    CarSensorState maSensors[2];
    CarSensorState& GetSensor(u8 i) { CGS_ASSERT(i < mu8NumSensors, "sensor"); return maSensors[i]; }
};
struct DeformationState {
    mutable u32 requested = 0;
    CarState* state = nullptr;
    const CarState* GetCarStateF(u32 id) const { requested = id; return state; }
};
}}
namespace BrnSound {
namespace Logic {
enum EFatalityFlag { E_FATAL_OFF, E_FATAL_ON, E_FATAL_START };
struct FrameInformation {
    CgsSound::Utils::DataPoint<EFatalityFlag> meFatality;
    CgsSound::Utils::DataPoint<AttribSys::Enums::eImpactTime::eImpactTime> meImpactTime;
};
}
namespace Module {
namespace Io {
struct LogicInputBuffer {
    struct Deform { const BrnPhysics::Deformation::DeformationState* mpDeformationState = nullptr; } deform;
    const Deform& GetDeformationInterface() const { return deform; }
};
}
struct SoundLogicModule {
    Logic::FrameInformation frame;
    Io::LogicInputBuffer input;
    Logic::FrameInformation& GetFrameInformation() { return frame; }
    Io::LogicInputBuffer* GetBrnInputStructure() { return &input; }
};
}
namespace Vehicles {
struct VehicleData { struct Id { u32 muValue = 0x01000000; } mEntityId; };
namespace Engines {
struct PhysicsControl {
    VehicleData data;
    const VehicleData* GetRawPhysicsData() const { return &data; }
};
}
namespace Deformation {
struct DeformationEffect {
    CgsSound::Utils::DataPoint<bool> mbDeforming;
    CgsSound::Utils::DataPoint<f32> mDeformAmount, mDeformIntensityLagged;
    CgsSound::Utils::Average<4, f32> mDeformDeltaAverage;
    CgsSound::Utils::InterpolateLine mFadeOut;
    f32 mfAemsIntensity = 0, mfTimeDeforming = 0;
    Engines::PhysicsControl* mpPhysicsControl;
    Module::SoundLogicModule* module;
    struct Voice {
        f32 params[4] = {}, gains[2] = {};
        void SetParameter(int i, f32 value, const u32*) { params[i] = value; }
        void SetGain(int i, f32 value, const u32*) { gains[i] = value; }
    } mPatchVoice;
    Module::SoundLogicModule* GetLogicModule() { return module; }
    f32 GetMixerOutputValue(int i, int) { return i == 0 ? 12345.0f : i == 1 ? -1234.0f : 4096.0f; }
    void UpdateParams(f32);
};
#include "fx_crumple_controls.inc"
}}}
int main() {
    using namespace BrnSound;
    using namespace Vehicles::Deformation;
    using namespace AttribSys::Enums::eImpactTime;
    Module::SoundLogicModule module;
    Vehicles::Engines::PhysicsControl physics;
    BrnPhysics::Deformation::CarState car;
    BrnPhysics::Deformation::DeformationState deform;
    deform.state = &car;
    module.input.deform.mpDeformationState = &deform;
    DeformationEffect e;
    e.module = &module; e.mpPhysicsControl = &physics;
    e.mFadeOut.Initialize(1, 1, 10, CgsSound::Utils::Curve::E_LINEAR);
    car.maSensors[0].mDisplacement = { .125f, 0, 0, 0 };
    car.maSensors[1].mDisplacement = { .25f, 0, 0, 100 };
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* label) {
        ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
    };
    auto close = [](f32 a, f32 b) { return std::fabs(a - b) < .002f; };
    e.UpdateParams(.1f);
    check(e.mPatchVoice.params[2] == 0, "ordinary driving is silent");
    check(e.mPatchVoice.params[0] == 12345 && e.mPatchVoice.params[1] == 4096 &&
          e.mPatchVoice.params[3] == -1234, "mixer controls retain original slots");
    module.frame.meFatality.Update(Logic::E_FATAL_START);
    module.frame.meImpactTime.Update(VSlow);
    e.UpdateParams(.1f);
    check(e.mbDeforming.GetCurrent(), "fatal START plus VSlow arms deformation");
    check(deform.requested == 0x01000000, "lookup uses raw physics entity id");
    check(e.mDeformAmount.GetCurrent() == .0625f, "largest xyz displacement squared; ignore w");
    check(e.mDeformDeltaAverage.GetAverage() == 0 && e.mPatchVoice.params[2] == 0,
          "first crash frame does not count old deformation");
    check(close(e.mfTimeDeforming, .1f), "deformation timer advances");
    car.maSensors[1].mDisplacement.x = .5f;
    e.UpdateParams(.1f);
    const f32 peak = .046875f * 655340.0f;
    check(e.mDeformAmount.GetPrevious() == .0625f && e.mDeformAmount.GetCurrent() == .25f,
          "deformation history updates");
    check(e.mDeformDeltaAverage.GetAverage() == .046875f, "four frame delta average");
    check(e.mDeformIntensityLagged.GetCurrent() == .046875f, "new peak held");
    check(e.mFadeOut.mfLength == 1 && e.mFadeOut.GetValueFloat() == 1, "original 1000 ms fade");
    check(close(e.mPatchVoice.params[2], peak), "sensor growth produces crumple intensity");
    e.UpdateParams(.1f);
    check(close(e.mFadeOut.GetValueFloat(), .9f) && close(e.mPatchVoice.params[2], peak * .9f),
          "equal average advances fade rather than retriggering");
    module.frame.meImpactTime.Update(True);
    e.UpdateParams(.25f);
    check(!e.mbDeforming.GetCurrent(), "ordinary impact time is not VSlow");
    check(e.mDeformAmount.GetCurrent() == 0 && e.mDeformDeltaAverage.GetAverage() == 0,
          "leaving VSlow clears deformation samples");
    check(e.mfTimeDeforming == 0 && close(e.mFadeOut.GetValueFloat(), .675f), "exit fades from current level");
    e.UpdateParams(1);
    check(e.mPatchVoice.params[2] == 0, "completed fade silences crumple");
    module.frame.meImpactTime.Update(VSlow); module.frame.meFatality.Update(Logic::E_FATAL_ON);
    module.input.deform.mpDeformationState = nullptr;
    e.mFadeOut.Initialize(1, 0, 1000, CgsSound::Utils::Curve::E_LINEAR);
    e.mDeformIntensityLagged.Flush(.0005f); e.mfAemsIntensity = 500;
    e.UpdateParams(.1f);
    check(e.mFadeOut.mfElapsedTime == 0, "absent deformation state skips envelope update");
    check(e.mfAemsIntensity == 450, "subthreshold cached intensity falls at 500 per second");
    check(close(e.mPatchVoice.params[2], .0005f * 655340), "voice receives target, not smoothed cache");
    e.mDeformIntensityLagged.Flush(-1); e.UpdateParams(.1f);
    check(e.mPatchVoice.params[2] == 0, "negative target clamps to zero");
    e.mDeformIntensityLagged.Flush(1); e.UpdateParams(.1f);
    check(e.mPatchVoice.params[2] == 32767, "large target clamps to 32767");
    e.mDeformIntensityLagged.Flush(std::numeric_limits<f32>::quiet_NaN()); e.UpdateParams(.1f);
    check(e.mPatchVoice.params[2] == 32767, "unordered fsel target selects high bound");
    module.input.deform.mpDeformationState = &deform;
    car.maSensors[0].mDisplacement = {};
    car.maSensors[1].mDisplacement = { .125f, .25f, .5f, 100 };
    e.UpdateParams(.1f);
    check(e.mDeformAmount.GetCurrent() == .328125f, "all three sensor axes contribute, w does not");
    car.maSensors[1].mDisplacement = { 1.e-20f, 0, 0, 0 };
    e.UpdateParams(.1f);
    check(e.mDeformAmount.GetCurrent() == 0, "VMX flushes denormal displacement square");
    car.maSensors[1].mDisplacement = { 1.e20f, 0, 0, 0 };
    e.UpdateParams(.1f);
    check(std::isnan(e.mDeformAmount.GetCurrent()), "finite VMX dot overflow produces QNaN");
    check(e.mPatchVoice.gains[0] == 1 && e.mPatchVoice.gains[1] == 0, "dry and reverb routing unchanged");
    check(assertions == 0, "valid fixtures have no assertions");
    std::printf("FxCrumpleControls: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
