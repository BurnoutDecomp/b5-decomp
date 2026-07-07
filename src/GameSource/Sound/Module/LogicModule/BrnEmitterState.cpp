#include "GameSource/Sound/Module/LogicModule/BrnEmitterState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::memcpy (the 16-byte lvx/stvx entity copy)

// =============================================================================
// BrnSound::Logic::World::EmitterState -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEmitterState.h for the
// inheritance rationale. Mirrors the committed sibling
// BrnSound::Logic::GlobalState::~GlobalState (@ 0x826D2250,
// GameSource/Sound/Global/BrnGlobalState.cpp) 1:1.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

namespace
{
// Position-match tolerance splatted into IsAttachedToThis's per-lane compare. The
// X360 loads it as a single float from rodata @0x820AA0E0 (lvlx + vspltw128 lane 0).
// That rodata is NOT present in any dump available to this reconstruction, so its
// exact value is UNRECOVERED; seeded with a small placeholder so the compare's
// structure is faithful and compiles. FLAG: do NOT trust this magnitude.
const f32 KfAttachPositionEpsilon_UNRECOVERED = 0.01f;   // @0x820AA0E0 (un-dumped)

// The entity type lives in the HIGH 16 bits of the mPosPlus w component (a float
// slot reinterpreted as bits, then >>16 -- exactly the X360 `srwi rN, rN, 16`).
inline u16 EntityTypeHighWord(f32 lfPackedW)
{
    u32 luBits;
    std::memcpy(&luBits, &lfPackedW, sizeof(luBits));
    return static_cast<u16>(luBits >> 16);
}
} // namespace

// ---------------------------------------------------------------------------
// ~EmitterState  @ 0x826D1F70  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install EmitterState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects   ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//
// The single observable source-level side effect is the DestroyEffects() call on
// the State base (reused BY NAME, un-homed body). The two vtable installs and the
// conditional allocator-routed free are the compiler-synthesised parts of MSVC's
// deleting-destructor thunk, re-emitted here from this virtual destructor + the
// class's operator delete; off_82FFB954 (the sound allocator) is not homed in
// this group, so the host toolchain's `delete` stands in for the custom-allocator
// dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
EmitterState::~EmitterState()
{
    DestroyEffects();
}

// ---------------------------------------------------------------------------
// EmitterState::GetTypeName() const  @ 0x82686798
//
//   lis/addi r11, sTypeInfo(82F2F83C) ; lwz r3, 4(r11) ; blr
//
// Loads sTypeInfo.mpcTypeName (the +0x4 field of this class's static
// ClassTypeInfo descriptor at 82F2F83C), the interned "EmitterState" type-name
// literal. Per the committed leaf-GetTypeName convention (GlobalState::GetTypeName
// @0x82686808 -> `return "GlobalState";`, CollisionStateManager::GetTypeName ->
// `return "CollisionStateManager";`) the leaf returns that interned literal directly.
// ---------------------------------------------------------------------------
const char* EmitterState::GetTypeName() const
{
    return "EmitterState";
}

// ---------------------------------------------------------------------------
// Attach  @ 0x826D2010
//
//   cmplwi cr6, r4, 0                       ; if (lpvAttachment == 0)
//   bne    cr6, <store>
//   bl     BeginAssert/FireAssert("lpvAttachment",...)/EndAssert
//   li     r11, 0x60
//   lvx128 v0, r0, r30                      ; v0 = *(StaticSoundEntity*)lpvAttachment (16B)
//   stvx128 v0, r31, r11                    ; mEntity (this+0x60) = v0        (inlined SetSoundEntity)
//   mr     r3, r31 ; li r4, 0
//   bl     CgsSound::Logic::State::Attach   ; State::Attach(this, NULL)
//
// Null-asserts the attachment, copies the incoming 16-byte StaticSoundEntity into
// mEntity (the DWARF-attested SetSoundEntity, inlined here as one 128-bit move),
// then chains to the base State::Attach -- note the X360 passes r4 = 0, so the base
// is called with a NULL attachment (the entity is recorded in mEntity, not handed
// to the base). State::Attach is reused BY NAME (declaration-only base member, same
// idiom as the committed sibling TrafficState::Attach @0x826CB270). CGS_ASSERT folds
// the Begin/Fire/End sequence; the baked d:\p4 file/line is dropped.
// ---------------------------------------------------------------------------
void EmitterState::Attach(void* lpvAttachment)
{
    CGS_ASSERT(lpvAttachment != 0, "lpvAttachment");
    std::memcpy(mEntity, lpvAttachment, sizeof(mEntity));   // 16-byte lvx128/stvx128 copy
    State::Attach(nullptr);
}

// ---------------------------------------------------------------------------
// IsAttachedToThis  @ 0x826BADA0
//
//   cmplwi cr6, r4, 0                       ; if (lpvTestAttachment == 0)
//   ... BeginAssert/FireAssert("lpvTestAttachment",...)/EndAssert
//   lvlx   v0, unk_820AA0E0 ; vspltw128 v127, v0, 0   ; v127 = eps splat (rodata, UNRECOVERED)
//   lbz    r10, 0x48(this)  ; if (!mbIsAttached) assert("IsAttached()")  ; GetSoundEntity()
//   lvx128 v13, r0, r28      ; v13 = *(StaticSoundEntity*)lpvTestAttachment
//   lvx128 v12, this, 0x60   ; v12 = mEntity
//   vsubfp v13, v13, v12                    ; diff = test - mine
//   vspltisw v0,-1 ; vslw v0,v0,v0 ; vandc v0,v13,v0 ; vmr v13,v0   ; v13 = abs(diff)
//   vrlimi128 v13, v0, 1, 1                 ; neutralise the w lane (packed type)
//   vcmpgtfp128. v0, v13, v127              ; per-lane abs(diff) > eps ; test CR6 "none-true"
//   beq    <return 0>                       ; some xyz lane exceeds eps -> not attached
//   lfs    f0, 0xC(r28)                     ; test.w
//   lbz    r11, 0x48(this)  ; if (!mbIsAttached) assert("IsAttached()")  ; GetSoundEntity()
//   lfs    f0, 0x6C(this)                   ; mine.w  (mEntity + 0xC)
//   srwi ; srwi ; cmplw    ; li r3,1 ; beq <keep> ; li r3,0   ; return HIWORD(test.w)==HIWORD(mine.w)
//
// "Is the (position, type) of the passed StaticSoundEntity the same one I'm attached
// to?" Two inlined GetSoundEntity() reads (each asserting IsAttached()) yield mEntity:
// first for the XYZ position compare against the splatted tolerance (the packed w
// lane is masked out of that compare by vrlimi128), then for the packed type in the
// high 16 bits of the w component, which must match exactly. CGS_ASSERT folds each
// Begin/Fire/End; baked file/line dropped. FLAG: the position tolerance is un-dumped
// rodata (see KfAttachPositionEpsilon_UNRECOVERED) -- the compare STRUCTURE is
// faithful but its threshold magnitude is not proven.
// ---------------------------------------------------------------------------
bool EmitterState::IsAttachedToThis(void* lpvTestAttachment)
{
    CGS_ASSERT(lpvTestAttachment != 0, "lpvTestAttachment");

    const f32* lpTest = static_cast<const f32*>(lpvTestAttachment);      // test entity mPosPlus xyzw
    const f32* lpMine = reinterpret_cast<const f32*>(mEntity);           // GetSoundEntity().mPosPlus

    // GetSoundEntity() asserts IsAttached() before yielding mEntity (position read).
    CGS_ASSERT(IsAttached(), "IsAttached()");

    const f32 lfDx = lpTest[0] - lpMine[0];
    const f32 lfDy = lpTest[1] - lpMine[1];
    const f32 lfDz = lpTest[2] - lpMine[2];
    const f32 lfAbsDx = (lfDx < 0.0f) ? -lfDx : lfDx;   // vandc sign-bit clear == fabs
    const f32 lfAbsDy = (lfDy < 0.0f) ? -lfDy : lfDy;
    const f32 lfAbsDz = (lfDz < 0.0f) ? -lfDz : lfDz;
    if (lfAbsDx > KfAttachPositionEpsilon_UNRECOVERED ||
        lfAbsDy > KfAttachPositionEpsilon_UNRECOVERED ||
        lfAbsDz > KfAttachPositionEpsilon_UNRECOVERED)
    {
        return false;
    }

    // Second GetSoundEntity() (again asserts IsAttached()) for the packed type match.
    CGS_ASSERT(IsAttached(), "IsAttached()");
    return EntityTypeHighWord(lpTest[3]) == EntityTypeHighWord(lpMine[3]);
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
