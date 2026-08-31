#ifndef BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"   // HybridExhaustControl base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::HybridEngineControl
//   GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). DWARF
// (BrnHybridEngineControl.h:187): HybridEngineControl : public HybridExhaustControl.
// The dual-source (loop + Ginsu) engine EffectControl leaf. Its ONE own data member
// is the paired-exhaust back-pointer (DWARF h:208, see below): sizeof(base) == 0x130
// (HybridExhaustControl::Create @0x826B34E0 allocates 0x130) and CreateObject
// @0x826CC738 allocates 0x140, so the +0x130 slot UpdateGinsuRPM reads is THIS
// class's member -- an earlier revision misbound it to the base's mpEngineControl.
// The two leaf vptr installs (primary/EffectControl @+0, IResourceRequester
// sub-object @+4) are produced structurally by the base spine + the virtual dtor.
//
// UPGRADE NOTE: this class was previously a standalone minimal struct (for
// UpdateGinsuRPM only). It is now the real X360-attested leaf deriving from
// HybridExhaustControl, so CreateObject / the deleting-destructor anchor can be homed
// and UpdateGinsuRPM operates on the base's committed DataPoint<f32> mGinsuRpm.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct HybridEngineControl : public HybridExhaustControl
{
    HybridEngineControl() : mpHybridExhaustControl(nullptr) {}
    virtual ~HybridEngineControl();     // anchor for the scalar deleting destructor @ 0x826CC7F0

    // @ 0x826CC738 -- RTTI factory hook.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );

    // @ 0x82699B98 (DWARF :193, virtual) -- shift the Ginsu RPM DataPoint forward one
    // frame, sampling the paired exhaust control's current Ginsu RPM
    // (mpHybridExhaustControl->GetGinsuRPM(), the inlined `lfs 0x8C(control)`).
    virtual void UpdateGinsuRPM();
    virtual bool Attach(); // @ 0x82699BB0

    virtual s32 GetController(s32 aiSlot); // @ 0x82684AB0
    virtual void AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82684B20

    // DWARF BrnHybridEngineControl.h:208 -- the ONLY own data member (console +0x130,
    // the first slot past the 0x130-byte base): the paired HybridExhaustControl this
    // engine control mirrors its Ginsu RPM from. (2026-08-25 wave 6: was misread as
    // the BASE's mpEngineControl -- the base ends at +0x130, so it cannot be.)
    HybridExhaustControl* mpHybridExhaustControl;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H
