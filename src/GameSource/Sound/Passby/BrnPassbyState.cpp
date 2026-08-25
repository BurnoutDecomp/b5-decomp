#include "GameSource/Sound/Passby/BrnPassbyState.h"

// =============================================================================
// BrnSound::Logic::Passby::PassbyState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPassbyState.h for the
// inheritance rationale and the un-recoverable-descriptor-fields FLAG.
//
// This TU's recon'd function set:
//   GetStaticTypeInfo()  @ 0x82688FC8
//   PassbyState()        @ 0x826BF5E0
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// ---------------------------------------------------------------------------
// PassbyState::PassbyState  @ 0x826BF5E0  (moved from the GameShared CgsState.cpp
// rival, 2026-08-25 audio-faithfulness wave 5)
//
// Zeroes/seeds the embedded passby record field-for-field (the rival's numeric
// tail decoded onto the DWARF Passby members):
//   stvx128 vZero, +96   -> mPassbyData.mStaticPos            (16-byte vector zero)
//   stw 0,  +112         -> mPassbyData.mp3dControl
//   stfs 0, +116         -> mPassbyData.mfRelativeVelocityMagnitude
//   stw 3,  +120         -> mPassbyData.meType = 3
//   stfs 1.0, +124       -> mPassbyData.mfVolumeModifier      (flt_82001C98)
//   stb 0,  +128         -> mPassbyData.mbSuppressBoostBys
// mfTimeOutTimer (+144) is NOT stored by the X360 ctor; value-defined here on the
// host (0.0f) so the member is never read uninitialised -- no behavioural
// reliance on the pre-seed value is attested.
// ---------------------------------------------------------------------------
PassbyState::PassbyState()
    : BrnSound::Logic::BrnState()
    , mfTimeOutTimer(0.0f)
{
    mPassbyData.mStaticPos.SetZero();                       // stvx128 zero, +96
    mPassbyData.mp3dControl                 = 0;            // stw 0, +112
    mPassbyData.mfRelativeVelocityMagnitude = 0.0f;         // stfs 0, +116
    mPassbyData.meType = AttribSys::Enums::ePassbyTypes::TrafficSmall; // stw 3, +120
    mPassbyData.mfVolumeModifier            = 1.0f;         // stfs flt_82001C98, +124
    mPassbyData.mbSuppressBoostBys          = false;        // stb 0, +128
}

// ---------------------------------------------------------------------------
// GetStaticTypeInfo  @ 0x82688FC8
//
//   lis   r11, unk_82F2F95C@ha
//   addi  r3,  r11, unk_82F2F95C@l   ; r3 = &sTypeInfo
//   blr
//
// Returns the address of PassbyState's per-class static RTTI descriptor. The
// X360 builds this descriptor in rodata (at 0x82F2F95C). Here it is a
// function-local static, seeded with the canonical type name "PassbyState" and
// the real registration id recovered from the PS3 DecFIGS build:
//   ObjectID        = 0x40000  (PS3 __static_initialization_and_destruction_0
//                               @ 0x85FA1C: PassbyState::sTypeInfo.ObjectID = 0x40000)
//   mpcTypeName     = "PassbyState"
//   mpBaseTypeInfo  = nullptr  (FLAG: BrnState's descriptor chain is DEFERRED;
//                               PS3 sets baseTypeInfo = &BrnState::sTypeInfo, but
//                               BrnState's descriptor is not homed in this TU)
//   mpfnCreateObject= nullptr  (CreateObject @ 0x826D4978 is its own DEFERRED slice)
// This mirrors the committed CgsEffectBase.cpp GetStaticTypeInfo precedent: the
// observable return — a stable &sTypeInfo whose typeName is the recovered class
// name — matches, without fabricating the descriptor's id/base wiring.
//
// NOTE: BrnState.h's minimal CgsSound::Logic::ClassTypeInfo<T> has no constructor
// (aggregate members ObjectID / mpcTypeName / mpBaseTypeInfo / mpfnCreateObject),
// so the descriptor is aggregate-initialised here.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* PassbyState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo =
    {
        0x40000,       // ObjectID        (PS3 0x85FA1C: PassbyState id = 0x40000)
        "PassbyState", // mpcTypeName
        nullptr,       // mpBaseTypeInfo  (DEFERRED — BrnState descriptor chain)
        nullptr,       // mpfnCreateObject(DEFERRED — CreateObject @ 0x826D4978)
    };
    return &sTypeInfo;
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
