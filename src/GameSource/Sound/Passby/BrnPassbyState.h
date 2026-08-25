#ifndef BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_STATE_H
#define BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"
#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"   // PassbyStateManager::Passby (mPassbyData)

// =============================================================================
// BrnSound::Logic::Passby::PassbyState
//   GameSource/Sound/Passby/BrnPassbyState.h (DWARF home) +
//   GameSource/Sound/Passby/BrnPassbyState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PassbyState is the sound-logic state node for vehicle pass-bys. DWARF
// (BrnPassbyState.h:35):
//   struct BrnSound::Logic::Passby::PassbyState : public BrnSound::Logic::BrnState
// so it inherits the COMMITTED BrnState base (CgsSound::Logic::State sound-logic
// state base) reused BY NAME from
// GameSource/Sound/Module/LogicModule/BrnState.h, and overrides the per-class RTTI
// hooks (GetTypeInfo / GetTypeName / GetStaticTypeInfo / CreateObject).
//
// This TU's recon'd function set:
//   GetStaticTypeInfo()  @ 0x82688FC8
//   PassbyState()        @ 0x826BF5E0  (moved from the GameShared CgsState.cpp
//                        rival, 2026-08-25 audio-faithfulness wave 5 -- with the
//                        DWARF members mPassbyData/mfTimeOutTimer, onto which the
//                        rival's ctor-derived numeric tail decodes exactly)
// whose X360 body is a single static-rodata load:
//   lis  r11, unk_82F2F95C@ha
//   addi r3,  r11, unk_82F2F95C@l   ; r3 = &sTypeInfo  (the static descriptor)
//   blr
// i.e. it returns the address of PassbyState's per-class static ClassTypeInfo
// descriptor (sTypeInfo). The static descriptor lives in rodata at 0x82F2F95C,
// immediately preceding off_82F2F960 (PassbyState's CgsSound::MemBase allocator
// pool tag used by PassbyState::CreateObject @ 0x826D4978:
//   CgsSound::MemBase::operator new(160, off_82F2F960)).
//
// GetStaticTypeInfo() is a STATIC method (the X360 body takes no `this` and the
// return is constant), matching the DWARF `ClassTypeInfo<...>* GetStaticTypeInfo()`
// (BrnPassbyState.h:37). The remaining RTTI hooks (GetTypeInfo / GetTypeName /
// CreateObject) and the rest of the PassbyState surface are DEFERRED to their own
// recon slices (the boot trace executed only GetStaticTypeInfo).
//
// FLAG (un-recoverable static-descriptor fields): the rodata contents of sTypeInfo
// (its ObjectID and base-type pointer) are NOT recoverable from this single
// address-returning function. Following the committed CgsEffectBase.cpp precedent
// (EffectControl/EffectObject GetStaticTypeInfo), the descriptor is seeded as a
// function-local static with the canonical type name "PassbyState" and HONEST
// placeholder ObjectID(0)/base(nullptr)/factory(nullptr); the observable return
// (a stable &sTypeInfo whose typeName is the recovered class name) matches. The
// real ObjectID and the chained base descriptor are owned by the per-class
// RTTI-registration TU and are DEFERRED there (do not fabricate the id/base).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): GetStaticTypeInfo() returns the address
// of a static descriptor and ignores `this`, so no member offsets are touched and
// none are asserted here. The committed BrnState base (BrnState.h) is reused BY
// NAME; its minimal CgsSound::Logic::ClassTypeInfo<T> shape (no ctor; aggregate
// members ObjectID/mpcTypeName/mpBaseTypeInfo/mpfnCreateObject) is reused as-is.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// BrnPassbyState.h:35 (DWARF). Derives the committed BrnState sound-logic state
// base and overrides the per-class RTTI virtuals. GetStaticTypeInfo() is bodied in
// this TU's .cpp; the remaining RTTI hooks are declared for the vtable shape but
// DEFERRED (outside this TU's recon'd function set).
struct PassbyState : public BrnSound::Logic::BrnState
{
    // @ 0x826BF5E0 (was homed in the GameShared CgsState.cpp rival; moved here
    // 2026-08-25, audio-faithfulness wave 5). Zeroes/seeds the embedded passby
    // record + the timeout timer. Bodied in BrnPassbyState.cpp.
    PassbyState();
    virtual ~PassbyState() {}

    // — per-class RTTI. DEFERRED bodies (declared for the state vtable shape;
    // the descriptor registration / id chaining lives in its own recon slice).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char*                                            GetTypeName() const;

    // @ 0x82688FC8 — returns &sTypeInfo (the static per-class RTTI descriptor).
    // Bodied here. STATIC (the X360 body takes no `this`).
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();

private:
    // DWARF BrnPassbyState.h:73 `PassbyStateManager::Passby mPassbyData`. The
    // passby record this state is servicing. Console offset +96 (16-aligned); the
    // ctor's zero/seed stores decode onto its fields exactly (the old CgsState.h
    // rival's numeric tail): mStaticPos@96 (stvx128 zero), mp3dControl@112=0,
    // mfRelativeVelocityMagnitude@116=0, meType@120=3, mfVolumeModifier@124=1.0,
    // mbSuppressBoostBys@128=0.
    PassbyStateManager::Passby mPassbyData;              // +96

    // DWARF BrnPassbyState.h:74. Passby time-out countdown (console +144, after
    // the record's align-16 pad; not stored by the ctor @0x826BF5E0).
    f32 mfTimeOutTimer;                                  // +144
};

} // namespace Passby
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_PASSBY_BRN_PASSBY_STATE_H
