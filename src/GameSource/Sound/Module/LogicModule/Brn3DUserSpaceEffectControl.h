#ifndef BRN_SOUND_LOGIC_BRN_3D_USER_SPACE_EFFECT_CONTROL_H
#define BRN_SOUND_LOGIC_BRN_3D_USER_SPACE_EFFECT_CONTROL_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3, Matrix44Affine
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"  // committed Brn3DEffectControl base (BY NAME)

// =============================================================================
// BrnSound::Logic::Brn3DUserSpaceEffectControl
//   GameSource/Sound/Module/LogicModule/Brn3DUserSpaceEffectControl.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/Brn3DUserSpaceEffectControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// DWARF (BrnEffectControl.h:158):
//   struct Brn3DUserSpaceEffectControl : public BrnSound::Logic::Brn3DEffectControl
// It is the intermediate 3D-positional effect control that adds the user-space
// transform + emitter state (mpTransform + four Vector3s) on top of the committed
// Brn3DEffectControl base (which owns mEngineDataAtrib). The X360 Collision3DControl
// ctor (@ 0x826E88D0) INLINES this class's construction, so this home owns the four
// zero-inited Vector3s + the null mpTransform so leaves can chain it BY NAME.
//
// This TU's recon'd function set is exactly two entries:
//   CreateObject(u32)              @ 0x826E0E10  (the RTTI factory hook)
//   `vector deleting destructor'   @ 0x826E0D60  (empty leaf -- all teardown comes
//        from the inherited ~Brn3DEffectControl chain; same as the committed sibling
//        Passby3DControl @ 0x826E8ED0)
//
// FLAG (shape vs full surface): the full DWARF surface (the RTTI GetTypeInfo/
// GetTypeName/GetStaticTypeInfo hooks, and the AttachTransform/AttachEmitterPosition/
// AttachEmitterDirection/UpdateParams virtual surface) is DEFERRED to its own recon
// slices; only the inheritance spine, the four user-space members, the CreateObject
// factory and the virtual dtor are materialised.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 offsets (mpTransform @ +0xD0,
// the four +0x10-strided Vector3s @ +0xE0/+0xF0/+0x100/+0x110) assume 4-byte
// pointers/vptr; members are pinned BY NAME + SEQUENCE and absolute offsets are NOT
// static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnEffectControl.h:158 (DWARF). Intermediate 3D user-space effect control.
struct Brn3DUserSpaceEffectControl : public BrnSound::Logic::Brn3DEffectControl
{
    // Ctor @ BrnEffectControl.cpp:215 (X360). Chains Brn3DEffectControl(), nulls
    // mpTransform, and zero-inits the four Vector3s (matching the 4x GetVector3_Zero
    // / stvx128 v0 stores the Collision3DControl ctor inlines). rw::math::vpu::Vector3's
    // default ctor is a VMX swizzle (NOT guaranteed zero), so SetZero() is called
    // explicitly to reproduce the zero-stores.
    Brn3DUserSpaceEffectControl()
        : mpTransform(nullptr)              // stw 0, +0xD0
    {
        mPositionInUserSpace.SetZero();     // stvx128 v0, +0xE0
        mDirectionInUserSpace.SetZero();    // stvx128 v0, +0xF0
        mGeneratedPosition.SetZero();       // stvx128 v0, +0x100
        mGeneratedDirection.SetZero();      // stvx128 v0, +0x110
    }

    // Vector deleting destructor @ 0x826E0D60 -- out-of-line; see .cpp (empty body).
    virtual ~Brn3DUserSpaceEffectControl();

    virtual void UpdateParams(f32 afDeltaTime) override;
    virtual void AttachEmitterPosition(const Vector3* apPosition) override;
    virtual void AttachEmitterDirection(const Vector3* apDirection) override;
    virtual void AttachTransform(const Matrix44Affine* apTransform);

    // BrnEffectControl.cpp:46 -- the RTTI factory hook (@ 0x826E0E10). Non-virtual;
    // it is the createObject function pointer seeded into the ClassTypeInfo descriptor.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );

    // DWARF (BrnEffectControl.h:188-194). User-space transform + emitter state.
    const rw::math::vpu::Matrix44Affine* mpTransform;   // +0xD0
    Vector3                              mPositionInUserSpace;   // +0xE0
    Vector3                              mDirectionInUserSpace;  // +0xF0
    Vector3                              mGeneratedPosition;     // +0x100
    Vector3                              mGeneratedDirection;    // +0x110
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_3D_USER_SPACE_EFFECT_CONTROL_H
