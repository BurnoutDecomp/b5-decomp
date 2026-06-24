#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSound::Logic::StateManager::IsStateAlias @ 0x82680D48 is homed inline in
// CgsStateManager.h (member access by name -- meMapState; no raw-offset cast). This
// TU exists so the header is exercised by a real compilation unit. The old opaque
// mPad[20] + miAliasState stub is superseded by the DWARF-named layout in the
// header.

namespace CgsSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// StateManager::AddToClassTypeInfoArray  @ 0x8268DFE8  (static array dword_82FFBC58)
//
// Identical RTTI-registration routine to State::AddToClassTypeInfoArray
// (@ 0x8268DF08), here templated on CgsSound::Logic::StateManager and over a
// separate per-class static array. Scans the array for the first NULL slot, capped
// at the class-array size (loop bound 0x10 == KU_SIZEOF_CLASS_ARRAY); stores the
// descriptor on finding a free slot, otherwise falls through without storing. The
// "Too Many Class registations" assert (CgsStateManager.h:363) only fires once the
// (16-bit) slot counter reaches 4*KU (0x40), which the bounded loop never reaches.
// Reproduced generically by array NAME; no raw-offset cast.
// ---------------------------------------------------------------------------
ClassTypeInfo<StateManager>* StateManager::AddToClassTypeInfoArray(ClassTypeInfo<StateManager>* apTypeInfo)
{
    static ClassTypeInfo<StateManager>* saClassTypeInfoArray[KU_SIZEOF_CLASS_ARRAY] = { 0 };

    // Scan for the first empty slot, capped at the class-array size (0x10).
    u32 lu32Index = 0;
    for (lu32Index = 0; lu32Index < KU_SIZEOF_CLASS_ARRAY; ++lu32Index)
    {
        if (saClassTypeInfoArray[lu32Index] == 0)
        {
            saClassTypeInfoArray[lu32Index] = apTypeInfo;
            return apTypeInfo;
        }
    }

    // No empty slot within the cap; the X360 only asserts once the (16-bit) counter
    // reaches 4*KU (0x40). The bounded loop stops at KU, so the predicate holds and
    // no assert fires.
    CGS_ASSERT(lu32Index < (4u * KU_SIZEOF_CLASS_ARRAY),
               "Too Many Class registations. Increase KU_SIZEOF_CLASS_ARRAY");
    return apTypeInfo;
}

// ---------------------------------------------------------------------------
// StateManager::GetStaticTypeInfo()  @ 0x8268D798  -> &unk_82F2FAA0 ("StateManager")
//   The X360 body returns the address of StateManager's static ClassTypeInfo
//   descriptor (lis/addi unk_82F2FAA0; blr). The sibling StateManager::GetTypeName
//   @0x8268D7A8 reads the descriptor's typeName ("StateManager") from +4. Mirrors the
//   EffectControl/EffectObject GetStaticTypeInfo accessors: a function-local static
//   descriptor seeded with the recovered type name, so the observable return (the
//   descriptor carrying typeName "StateManager") matches byte-for-byte.
// ---------------------------------------------------------------------------
ClassTypeInfo<StateManager>* StateManager::GetStaticTypeInfo()
{
    static ClassTypeInfo<StateManager> sTypeInfo(0, "StateManager", nullptr, nullptr);
    return &sTypeInfo;
}

} // namespace Logic
} // namespace CgsSound
