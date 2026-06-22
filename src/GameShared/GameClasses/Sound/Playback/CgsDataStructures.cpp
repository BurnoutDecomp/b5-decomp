// ============================================================================
// CgsDataStructures.cpp -- CgsSound::Playback::IEntityFixer runtime bodies.
//
// Bodied store-for-store from the X360 asm. The single function recon'd in this
// TU is IEntityFixer::GetFixer @ 0x826809B0.
//
// dep_flags: none un-homed -- Name comes from CgsCommon.h, Entity/Registry from
// CgsRegistry.h (both committed sibling homes). The concrete EntityFixer<T>
// subclasses and the ctor/dtor that maintain spHead are DEFERRED to their own
// TU(s); spHead itself is defined here (its DWARF home is CgsDataStructures.cpp:38).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

namespace CgsSound
{
namespace Playback
{

// CgsDataStructures.cpp:38. The static head of the fixer registration list (the
// X360 global dword_82FFB9DC). Concrete fixers link themselves on via their ctor
// (DEFERRED). Starts empty.
const IEntityFixer* IEntityFixer::spHead = 0;

// IEntityFixer::GetFixer @ 0x826809B0
//
// X360 control flow reproduced exactly:
//   v2 = spHead (dword_82FFB9DC); if (!v2) return 0;
//   loop:
//     v2->DoGetTypeName() -> v4   ((**v2)(&v4, v2): vtable slot 0, Name by value)
//     if (aName == v4) return v2;
//     v2 = v2->mpNext (*(v2 + 4));
//     if (!v2) return 0;
//
// `aName` is passed by value (4-byte Name); the X360 ABI hands the small struct
// in via a hidden pointer, which is why the asm dereferences r3 (`*a1`). The
// per-fixer type Name is fetched through the vtable into a stack local and the
// match is on the interned hash word (Name::GetValue).
const IEntityFixer* IEntityFixer::GetFixer(Name aName)
{
    const IEntityFixer* lpFixer = spHead;                   // r31 <- dword_82FFB9DC
    if (lpFixer == 0)                                       // cmplwi r31,0 ; beq
        return 0;                                           // li r3,0

    while (true)
    {
        const Name lTypeName = lpFixer->DoGetTypeName();    // (**v2)(&v4, v2)
        if (aName.GetValue() == lTypeName.GetValue())       // cmplw *a1, v4 ; beq
            return lpFixer;                                 // mr r3, r31

        lpFixer = lpFixer->mpNext;                          // lwz r31, 4(r31)
        if (lpFixer == 0)                                   // cmplwi r31,0 ; bne loop
            return 0;                                       // li r3,0
    }
}

}
}
