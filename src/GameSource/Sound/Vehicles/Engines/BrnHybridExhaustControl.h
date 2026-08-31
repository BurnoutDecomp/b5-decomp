#ifndef BRN_SOUND_VEHICLES_ENGINES_HYBRID_EXHAUST_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_HYBRID_EXHAUST_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::Average / DataPoint (BY NAME)
#include "GameSource/AttribSys/Generated/classes/vehicleengine.h"  // Attrib::Gen::vehicleengine member (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::HybridExhaustControl  (+ leaf HybridEngineControl)
//   GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.{h,cpp}  (DWARF home)
//   HybridEngineControl lives in BrnHybridEngineControl.{h,cpp} (its own DWARF home).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnHybridEngineControl.h:81):
//   HybridExhaustControl : public BrnSound::Logic::BrnEffectControl
// The hybrid (loop + Ginsu) exhaust engine sound CONTROL. It cross-fades a loop-model
// bank against accel/decel Ginsu grains, tracking physics/audio RPM deltas.
//
// CgsSound::Utils::Graph is the sound-utility graph from CgsSoundUtils.h. ARTIST
// constructs it over the embedded Vector2[6] table and fills those points from the
// selected vehicleengine attributes in Attach; this is distinct from the serialized
// BrnSoundLoopModelData Graph used by the loop-model resource.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// The controller siblings the back-pointers reference (pointers only; full homes in
// their own headers).
struct PhysicsControl;
struct EngineControl;
struct ShiftControl;
struct ClutchControl;

struct HybridExhaustControl : public BrnSound::Logic::BrnEffectControl
{
    // DWARF BrnHybridEngineControl.h:52. The per-source engine mix weights.
    struct EngineMix
    {
        EngineMix() : Loop(0.0f), AccelGinsu(0.0f), DecelGinsu(0.0f), Cutoff(0.0f) {}
        f32 Loop;
        f32 AccelGinsu;
        f32 DecelGinsu;
        f32 Cutoff;
    };

    HybridExhaustControl();             // @ 0x826AF938
    virtual ~HybridExhaustControl();    // anchor for the vector deleting destructor @ 0x826AFA60

    virtual s32 GetController(s32 aiSlot); // @ 0x826849B0
    virtual void AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82684A20
    virtual bool Attach(); // @ 0x826997B0
    virtual void UpdateParams(f32 afTimeStep); // @ 0x826E3DE0

    // @ 0x826B34E0 -- RTTI factory hook. Returns the +4 IResourceRequester base view.
    // NOTE: Create allocates 0x130 == sizeof(HybridExhaustControl) -- the fact that
    // pins +0x130 as the FIRST derived-class member slot (see HybridEngineControl).
    static BrnSound::Logic::IResourceRequester* Create( bool abFlavour );

    // DWARF BrnHybridEngineControl.h:177 (public accessor). The current Ginsu RPM
    // sample -- the `lfs 0x8C(control)` read the paired HybridEngineControl's
    // UpdateGinsuRPM @0x82699B98 inlines.
    f32 GetGinsuRPM() const { return mGinsuRpm.mCurrentValue; }
    f32 GetDeltaRPM() const { return mAverageDeltaRPM.GetAverage(); }
    const EngineMix& GetEngineMix() const { return mFinalEngineVolume; }
    const EngineMix& GetFinalEngineVolume() const { return mFinalEngineVolume; }
    // DecFIGS BrnHybridEngineControl.h:253; ARTIST inlines this as the +0x4C
    // attribute-instance read used by DualGinsuEffect.
    Attrib::Gen::vehicleengine& GetVehicleEngineAttributes()
    {
        return mVehicleEngineAttributes;
    }

    // ---- members in DWARF order (offsets are X360 facts, not asserted on host) ----
    // The controller back-pointers, typed per the DWARF (BrnHybridEngineControl.h:
    // 123-126; 2026-08-25 wave 6 -- were untyped void*). X360 slots +0x34/+0x38/
    // +0x3C/+0x40, wired by AttachController (cases 0/4/2/3 @0x82684B20).
    PhysicsControl*          mpPhysicsControl;
    EngineControl*           mpEngineControl;
    ShiftControl*            mpShiftControl;
    ClutchControl*           mpClutchControl;
    Attrib::Gen::vehicleengine mVehicleEngineAttributes;                 // h:99 (this+0x48,0,0)
    Attrib::Gen::vehicleengine mMasterVehicleEngineComponentAttributes;  // h:102 (this+0x58,0,0)
    CgsSound::Utils::Average<3u, f32> mAverageDeltaRPM;                  // h:105
    CgsSound::Utils::DataPoint<f32>   mPhysicsDeltaRpm;                  // h:108
    CgsSound::Utils::DataPoint<f32>   mAudioDeltaRpm;                    // h:111
    CgsSound::Utils::DataPoint<f32>   mGinsuRpm;                         // h:114
    CgsSound::Utils::Graph            mDecelCrossfadeMix;                // h:117
    Vector2                           maCrossFadesPoints[6];              // h:120
    f32                      mfPercentOfAccelThreshold;                  // h:123
    f32                      mfPercentOfDecelThreshold;                  // h:126
    EngineMix                mFinalEngineMix;                           // h:129
    EngineMix                mFinalEngineVolume;                        // h:132

protected:
    void UpdateDeltaRPM();             // @ 0x826B3548
    virtual void UpdateGinsuRPM();     // @ 0x826999F0
    void UpdateMix(f32 afTimeStep);    // @ 0x826CC878
    void UpdateIdleVolume(f32& arfVolume);     // @ 0x82699AF0
    void UpdateRpmVolume(f32& arfVolume);      // inlined in ARTIST UpdateMix
    void UpdateRotationVolume(f32& arfVolume); // @ 0x82699A78
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_HYBRID_EXHAUST_CONTROL_H
