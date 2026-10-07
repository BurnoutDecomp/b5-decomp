#include <cstdio>
#include <cstring>
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"
#include "GameSource/Effects/EffectsDebugPostFxSettingsPC.h"
#include "SharedClasses/Graphics/BrnEffectsData.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int checks, failures, asserts, assetCalls, assetDestructs;
static void Check(bool value, const char* label) {
    ++checks;
    if (!value) { if (failures++ < 12) std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++asserts; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace Attrib {
u64 StringToKey(const char* text) {
    if (!std::strcmp(text,"198102")) return 101;
    if (!std::strcmp(text,"191270")) return 102;
    if (!std::strcmp(text,"374388")) return 103;
    if (!std::strcmp(text,"218901")) return 104;
    ++failures; return 0;
}
namespace Gen {
struct b4blurasset {
    b4blurasset(u64 key, void*) { Check(key==104,"original unconditional blur asset"); ++assetCalls; }
    ~b4blurasset() { ++assetDestructs; }
};
} }
namespace BrnEffects {
// Asset constructors are dependency oracles: distinct values reveal wrong
// keys, skipped asset reads and camera modifiers replacing authored values.
void VignetteData::Construct(const u64& key) {
    Check(key==101,"vignette key"); std::memset(this,0,sizeof(*this)); mfAngle=17;
}
void BloomData::Construct(const u64& key) {
    Check(key==102,"bloom key"); std::memset(this,0,sizeof(*this));
    mfLuminance=2.25f; mfThreshold=3.5f; mv4Scale={4,5,6,7};
}
void TintData2d::Construct(const u64& key) {
    Check(key==103,"2d tint key"); mv4Colour={0.1f,0.2f,0.3f,0.4f};
}
void BlurData::Construct(const u64& key) {
    Check(key==104,"blur key"); std::memset(this,0,sizeof(*this)); mfOpacity=0.625f; mfVelocity=8;
}
static void LogNotReconstructed(bool&, const char*) {}
struct EffectsModule {
#include "effects_cache.inc"
    struct Debug {
        EffectsDebugPostFxSettingsPC settings;
        EffectsDebugPostFxSettingsPC GetPostFxSettingsPC() const { return settings; }
    } mDebugComponent;
    TempRaceCarStateCache mCarStateCache;
    void GenerateRenderRequests(const EffectsIO::DispatchInputBuffer*);
};
}
#include "original_effects.inc"

int main() {
    BrnEffects::EffectsModule module;
    module.mCarStateCache={};
    module.mCarStateCache.mvLinearVelocity={13,14,15,16};
    module.mCarStateCache.mvAngularVelocity={23,24,25,26};
    module.mCarStateCache.mfSpeedMPH=73;
    module.mCarStateCache.mfSteering=-0.75f;
    module.mCarStateCache.mCarTransform.wAxis={31,32,33,1};
    module.mCarStateCache.mCameraTransform.yAxis={41,42,43,0};
    module.mCarStateCache.mbIsGameCamera=true;
    int dispatches=0;
    for (unsigned mask=0;mask<256;++mask) for (unsigned cameraCase=0;cameraCase<4;++cameraCase) {
        auto& d=module.mDebugComponent.settings;
        d.mbBloom=mask&1; d.mbVignette=mask&2; d.mbDepthOfField=mask&4;
        d.mbTint=mask&8; d.mbTint2d=mask&16; d.mbMotionBlur=mask&32;
        d.mbMotionBlurEnableUserSettings=mask&64; d.mbMotionBlurUserHighQuality=mask&128;
        d.mfMotionBlurUserAmountCars=0.35f; d.mfMotionBlurUserAmountWorld=0.65f;
        BrnDirector::Camera::Camera camera;
        std::memset(&camera,0,sizeof(camera));
        auto& fx=camera.GetEffects(); fx.mfBloomLuminance=0.25f; fx.mfBloomThreshold=-0.5f;
        fx.mMotionBlurData.mfCarsBlurAmount=0.2f; fx.mMotionBlurData.mfWorldBlurAmount=0.8f;
        fx.mMotionBlurData.mbIsActive=(cameraCase&1)!=0; fx.mMotionBlurData.mbIsExpensiveMotionBlur=true;
        auto& dof=camera.GetDepthOfField();
        dof.mfFocusStartDistanceMeters=3; dof.mfPerfectFocusStartDistanceMeters=4;
        dof.mfPerfectFocusEndDistanceMeters=5; dof.mfFocusEndDistanceMeters=6;
        dof.mfBlurriness=(cameraCase&2)?0.75f:0.0f;
        BrnEffectsFrame frame; std::memset(&frame,0,sizeof(frame));
        BrnEffects::EffectsIO::DispatchInputBuffer input;
        input.CgsModule::IOBuffer::Construct(); input.LockForWrite();
        input.SetBaseEffectsFrame(&frame); input.SetCameraInput(&camera); input.UnlockForWrite();
        input.LockForRead(); module.GenerateRenderRequests(&input); input.UnlockForRead(); ++dispatches;
        const bool blur=fx.mMotionBlurData.mbIsActive || (d.mbMotionBlurEnableUserSettings&&d.mbMotionBlur);
        Check(frame.mbUseBloom==d.mbBloom && frame.mbUseVignette==d.mbVignette && frame.mbUseTint==d.mbTint
              && frame.mbUseTint2d==d.mbTint2d && frame.mbUseBlur==blur
              && frame.mbUseDepthOfField==(d.mbDepthOfField&&dof.mfBlurriness>0),"original six enable decisions");
        if(d.mbBloom) Check(frame.GetBloomWeight()==1 && frame.GetBloomData().mfLuminance==2.5f
            && frame.GetBloomData().mfThreshold==3 && frame.GetBloomData().mv4Scale.w==7,"authored bloom plus camera modifiers");
        if(d.mbVignette) Check(frame.GetVignetteWeight()==1 && frame.GetVignetteData().mfAngle==17,"authored vignette");
        if(d.mbTint2d) Check(frame.Get2dTintWeight()==1 && frame.GetTintData2d().mv4Colour.w==0.4f,"authored tint colour");
        if(frame.mbUseDepthOfField) Check(frame.GetDepthOfFieldWeight()==1 && frame.GetDepthOfFieldData().mfFarPlane==6
            && frame.GetDepthOfFieldData().mfDofAmount==0.75f,"camera depth of field band");
        if(blur) Check(frame.GetBlurWeight()==1 && frame.GetBlurData().mfOpacity==0.625f,"authored radial blur");
        const auto& motion=frame.GetMotionBlurData();
        Check(motion.mbIsActive==(d.mbMotionBlur&&(d.mbMotionBlurEnableUserSettings||fx.mMotionBlurData.mbIsActive))
            && motion.mbIsExpensiveMotionBlur==(d.mbMotionBlurEnableUserSettings?d.mbMotionBlurUserHighQuality:true)
            && motion.mfCarsBlurAmount==(d.mbMotionBlurEnableUserSettings?0.35f:0.2f)
            && motion.mfWorldBlurAmount==(d.mbMotionBlurEnableUserSettings?0.65f:0.8f),"original motion blur override branch");
        Check(frame.GetLinearVelocity().x==13 && frame.GetAngularVelocity().z==25
            && frame.GetSpeedMPH()==73 && frame.GetSteering()==-0.75f,"real effects player cache");
        Check(frame.GetCarTransform().wAxis.y==32 && frame.GetCameraTransform().yAxis.z==43
            && frame.GetIsRacingGameplayCamera(),"cache matrices and camera kind copied whole");
    }
    Check(assetCalls==dispatches && assetDestructs==dispatches,"unconditional attrib lifetime for every dispatch");
    Check(asserts==0,"real IO accessors retain correct lock discipline");
    std::printf("PCOriginalEffectsFrame: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
