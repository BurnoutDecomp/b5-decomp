#ifndef CGS_SOUND_LOGIC_CGS3DEFFECTCONTROL_H
#define CGS_SOUND_LOGIC_CGS3DEFFECTCONTROL_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"      // CgsSound::Logic::EffectControl (base) + EffectBase members (mpLogicModule +0x28 / mpDynamicMixIo +0x30)
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"      // CgsSound::Logic::MicrophoneSystem + Utils::DataPoint
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"                 // Nicotine::DMixIO (mpDynamicMixIo == "lpNicotineData")
#include "rw/math/vpu/types.h"                                     // rw::math::vpu::Vector3 / Vector2
#include "rw/math/vpu/vector3_operation.h"                         // rw::math::vpu::operator- / Length

// =============================================================================
// CgsSound::Logic::Cgs3dEffectControl
//   GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF is AUTHORITATIVE for shape
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.h):
//   struct CgsSound::Logic::Cgs3dEffectControl : public CgsSound::Logic::EffectControl
//
// The engine 3D-positional effect control: latches an emitter position/direction,
// samples the sound-logic MicrophoneSystem each frame and drives the dynamic-mixer
// (Nicotine::DMixIO) panning/distance input slots.
//
// ODR NOTE: the minimal stub `struct Cgs3dEffectControl : public EffectControl {}`
// in GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h is the DEFERRED slice
// for the Passby leaf teardown ONLY; per corpus convention (see CgsEffectBase.h) it
// is never co-included in the same TU as this canonical home. When this home lands,
// the stub in Brn3DEffectControl.h should be folded out (its declaration swapped for
// an #include of this header) to remove the duplicate definition.
//
// DEFERRED-VIRTUAL NOTE: the RTTI/control-surface virtuals below are declared but
// bodied elsewhere (outside this batch's 3 functions). This mirrors the committed
// BrnEffectControl.h precedent (declared-undefined virtuals for correct vtable
// shape; only the destructor + the 2 batch methods are bodied in this TU). The
// virtual DECLARATION ORDER mirrors the DWARF dump exactly (RTTI pair, UpdateParams,
// AttachEmitter*, Notify, Detach, then the destructor LAST).
//
// X360 offset map proven by this TU's functions (over the 32-bit layout):
//   inherited EffectBase (see CgsEffectBase.h):
//     +0x28 -> mpLogicModule   (Generate3DParams: lwz 0x28(this); +0x29A0 -> mMicrophoneSystem)
//     +0x30 -> mpDynamicMixIo  (SetMixerInputValueUnbound: lwz 0x30(this); the "lpNicotineData" DMixIO*)
//   own members (DWARF order):
//     +0x54 -> mpEmitterPosition (Generate3DParams: lwz 0x54(this); asserted "mpEmitterPosition")
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the base carries vptrs and pointer
// members, so absolute member offsets are NOT static_asserted across the 32/64
// boundary. Members are pinned BY NAME + SEQUENCE (DWARF order) only.
// =============================================================================

namespace CgsSound
{
namespace Io { class MessageHeader; }   // Cgs3dEffectControl.h DWARF: Notify(const CgsSound::Io::MessageHeader*)

namespace Logic
{

// Cgs3dEffectControl.h:36 (DWARF): Cgs3dEffectControl : public EffectControl.
struct Cgs3dEffectControl : public CgsSound::Logic::EffectControl
{
    // Cgs3dEffectControl.h:157 (DWARF). Debug-render config sub-struct.
    struct DebugRendererMessage
    {
        DebugRendererMessage()               // Cgs3dEffectControl.h:158
            : mfYOffset(0.0f), mfRadius(0.0f), mbEnable(false) {}
        f32  mfYOffset;                      // Cgs3dEffectControl.h:164
        f32  mfRadius;                       // Cgs3dEffectControl.h:165
        bool mbEnable;                       // Cgs3dEffectControl.h:166
    };

    Cgs3dEffectControl() {}                   // Cgs3dEffectControl.cpp:51

    // --- RTTI / control surface (DEFERRED bodies; declared for vtable shape) ----
    // DWARF virtual order: GetTypeInfo, GetTypeName, UpdateParams, AttachEmitter*,
    // Notify, Detach, ~Cgs3dEffectControl (last).
    virtual ClassTypeInfo<EffectControl>* GetTypeInfo() const;   // Cgs3dEffectControl.cpp:36
    virtual const char*                   GetTypeName() const;   // Cgs3dEffectControl.cpp:36
    virtual void UpdateParams(f32 afDeltaTime);                  // Cgs3dEffectControl.cpp:63
    virtual void AttachEmitterPosition(const rw::math::vpu::Vector3* apPosition);   // Cgs3dEffectControl.cpp:305
    virtual void AttachEmitterDirection(const rw::math::vpu::Vector3* apDirection); // Cgs3dEffectControl.cpp:313
    virtual void Notify(const CgsSound::Io::MessageHeader* apMessageHeader);        // Cgs3dEffectControl.cpp:289
    virtual bool Detach();                                       // Cgs3dEffectControl.cpp:331

    // Out-of-line so the deleting-destructor thunk @ 0x826C3E70 is emitted in this
    // class's own TU (Cgs3dEffectControl.cpp). DWARF declares the dtor LAST.
    virtual ~Cgs3dEffectControl();           // Cgs3dEffectControl.h:36 (DWARF)

protected:
    // Cgs3dEffectControl.h:221 @ 0x82682570. Push a raw value into one of the
    // dynamic-mixer input slots (via mpDynamicMixIo->GetDMixInputPtr()).
    void SetMixerInputValueUnbound(s32 aiSlot, s32 aiValue);

    // Cgs3dEffectControl.cpp:123 @ 0x826EA7D8. Recompute the mixer pan/distance
    // input slots for the current player count from the microphone system.
    void Generate3DParams(s32 aiUnused);

    // Cgs3dEffectControl.cpp:166 @ (deferred). Panning angle emitter->mic. DEFERRED body.
    s32 GetPanningAngle(MicrophoneSystem& arMicSystem,
                        MicrophoneSystem::EMicPositions aePosition);

    // --- members (DWARF order; X360 offsets, not asserted on host) -------------
    CgsSound::Utils::DataPoint<f32> mfDistanceToMic[2];   // Cgs3dEffectControl.h:141
    CgsSound::Utils::DataPoint<f32> mfVelocityToMic[2];   // Cgs3dEffectControl.h:142
    const rw::math::vpu::Vector3* mpEmitterPosition;  // Cgs3dEffectControl.h:144  (+0x54 X360)
    const rw::math::vpu::Vector3* mpEmitterDirection; // Cgs3dEffectControl.h:145
    CgsSound::Utils::DataPoint<rw::math::vpu::Vector3> mEmitterPosition;   // Cgs3dEffectControl.h:147
    CgsSound::Utils::DataPoint<rw::math::vpu::Vector3> mEmitterDirection;  // Cgs3dEffectControl.h:148
    DebugRendererMessage mDebugRenderingMessageData;            // Cgs3dEffectControl.h:170
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGS3DEFFECTCONTROL_H
