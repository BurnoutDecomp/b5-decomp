#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::memset

// =============================================================================
// BrnSound::Vehicles::Engines::PhysicsControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPhysicsControl.h for the base
// rationale + opaque-span layout FLAG.
//
// Recon'd function set:
//   PhysicsControl::GetEngineComponentName        @ 0x82682CA8
//   PhysicsControl::GetEngineComponentKey         @ 0x82682D10
//   PhysicsControl::GetRawPhysicsData             @ 0x82682DA0
//   PhysicsControl::PhysicsControl                @ 0x826C8890
//   PhysicsControl::`vector deleting destructor'  @ 0x826AF8B0  (-> ~PhysicsControl)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// PhysicsControl::PhysicsControl  @ 0x826C8890  (default ctor)
//
// Derived ctor: the X360 INLINES the base zero-init (no `bl` to a base ctor) and
// installs the dual leaf vptrs directly, zeroes the leading scalar/DataPoint members,
// constructs the PhysicsData (@+0x40) and vehicleengine (@+0x228) sub-objects, then
// clears the intro-reving block (+0x248..+0x280) and sets the trailing flag @+0x284 = 1.
// The inlined dual-base install + base scalar zero-init is represented by the
// `: BrnEffectControl()` base-init. PhysicsData / vehicleengine are un-homed opaque
// spans, so their construction + the intro block are reproduced by zeroing the spans +
// setting the attested flag byte rather than by ctor calls (see header FLAG).
// ---------------------------------------------------------------------------
PhysicsControl::PhysicsControl()
    : BrnSound::Logic::BrnEffectControl()   // inlined dual-vptr install + base zero-init
    , mpVehicleState(nullptr)               // +0x0C stw 0
    , mfOscillator(0.0f)                    // +0x1C stfs 0.0
    , mfAngularVelocityAccumulator(0.0f)    // +0x20 stfs 0.0
    , mpVehiclePhysicsData(nullptr)         // +0x38 stw 0
{
    // Opaque base-region / gap scratch cleared by the inlined base zero-init.
    std::memset(mau8BaseGap, 0, sizeof(mau8BaseGap));                          // +0x08..+0x0B
    std::memset(mau8Gap0x10, 0, sizeof(mau8Gap0x10));                          // +0x10..+0x1B
    std::memset(mau8Gap0x24, 0, sizeof(mau8Gap0x24));                          // +0x24..+0x37

    // PhysicsData::PhysicsData(this+0x40) + vehicleengine(this+0x228,0,0):
    // deferred sub-object types -> zero their spans.
    std::memset(mau8ProcessedPhysicsData, 0, sizeof(mau8ProcessedPhysicsData));
    std::memset(mau8VehicleEngineAttributes, 0, sizeof(mau8VehicleEngineAttributes));

    // Intro-reving / start-line block: +0x248..+0x280 cleared, trailing flag @+0x284 = 1.
    std::memset(maIntroRevingBlock, 0, sizeof(maIntroRevingBlock));
    maIntroRevingBlock[KU_INTRO_FLAG_REL] = 1;   // +0x284 stb 1
}

// ---------------------------------------------------------------------------
// ~PhysicsControl  @ 0x826AF8B0  (anchor for the X360 `vector deleting destructor').
// The observable member teardown is the base BrnEffectControl dtor chain plus the
// PhysicsData / vehicleengine sub-object destructors (compiler-synthesised); this leaf
// adds nothing. The (a2 & 1) allocator-free tail is left to the host toolchain
// (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
PhysicsControl::~PhysicsControl()
{
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetEngineComponentName  @ 0x82682CA8
//   const char* GetEngineComponentName(VehicleState::EEngineComponentType)
//
//   assert mpVehicleState != 0
//   return VehicleState::GetEngineComponentName(mpVehicleState, type)
// ---------------------------------------------------------------------------
const char* PhysicsControl::GetEngineComponentName(
    BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType )
{
    CGS_ASSERT(mpVehicleState != nullptr, "lpVehicleState");

    return BrnSound::Vehicles::VehicleState::GetEngineComponentName(
        mpVehicleState, aeComponentType );
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetEngineComponentKey  @ 0x82682D10
//   Attribute::Key GetEngineComponentKey(VehicleState::EEngineComponentType)
//
// mEngineComponentKey is VehicleState's key array at byte +0x510 with an 8-byte
// element STRIDE (0xA2*8 = 0x510). Attribute::Key itself is a 4-byte u32; the 8-byte
// stride is VehicleState's internal layout, so we index by explicit byte stride and
// return the low word as the key. FLAG: +0x510 is an X360 byte offset into the opaque
// (un-homed) VehicleState; reproduced by value, not static_asserted.
// ---------------------------------------------------------------------------
Attribute::Key PhysicsControl::GetEngineComponentKey(
    BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType )
{
    const u8* lpVehicleState = static_cast<const u8*>(mpVehicleState);
    CGS_ASSERT(lpVehicleState != nullptr, "lpVehicleState");

    // VehicleState->mEngineComponentKey[type] : element at +0x510, 8-byte stride.
    const u8* lpKeyElem = lpVehicleState + (static_cast<u32>(aeComponentType) + 0xA2) * 8u;
    const Attribute::Key lKey = *reinterpret_cast<const Attribute::Key*>(lpKeyElem);

    // The console asserts the LOOKED-UP KEY is non-zero (the assert string names the
    // member value). An earlier revision mistranslated this into `pointer + 0x510 !=
    // nullptr` -- a tautology the compiler deletes -- losing the real guard
    // (fixed 2026-08-25, audio-faithfulness wave 5).
    CGS_ASSERT(lKey != 0, "mEngineComponentKey != 0");
    return lKey;
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetRawPhysicsData  @ 0x82682DA0
//   const VehicleData* GetRawPhysicsData() const
//     assert mpVehiclePhysicsData != 0 ; return mpVehiclePhysicsData
// ---------------------------------------------------------------------------
const BrnSound::Vehicles::VehicleData* PhysicsControl::GetRawPhysicsData() const
{
    CGS_ASSERT(mpVehiclePhysicsData != nullptr, "mpVehiclePhysicsData");
    return mpVehiclePhysicsData;
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetStaticTypeInfo()  @ 0x82684368  (IDA-truncated "BrnSound::Vehicle")
//
//   lis   r11, unk_82F2F578@ha
//   addi  r3,  r11, unk_82F2F578@l   ; r3 = &sTypeInfo (the rodata descriptor)
//   blr
//
// Returns PhysicsControl's per-class static RTTI descriptor. The 2-instruction
// &unk_X; blr shape is the committed per-class GetStaticTypeInfo() accessor form
// (ExplosionState::GetStaticTypeInfo @ 0x82689198) -- a function-local static,
// aggregate-initialised ClassTypeInfo<EffectControl>.
//
// FLAG (confidence medium): ObjectID is unrecovered (the per-leaf registration
// static-init that would seed it was not exported) -> seeded 0 per the in-tree
// placeholder convention; baseTypeInfo (EffectControl RTTI chain) and createObject
// are DEFERRED (un-homed) -> nullptr. typeName "PhysicsControl" is inferred from the
// class identity / the adjacent GetTypeName @ 0x82684378 tag (not proven in-scope,
// mirrors the ExplosionState precedent's inferred-typeName flag).
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* PhysicsControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo =
    {
        0,                // ObjectID         (FLAG: EffectControl-side id unrecovered)
        "PhysicsControl", // mpcTypeName      (FLAG: inferred from class / adjacent GetTypeName tag)
        nullptr,          // mpBaseTypeInfo   (DEFERRED -- EffectControl RTTI chain un-homed)
        nullptr,          // mpfnCreateObject (DEFERRED -- CreateObject not homed in this slice)
    };
    return &sTypeInfo;
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
