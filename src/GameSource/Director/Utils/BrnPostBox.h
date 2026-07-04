#pragma once

// ============================================================================
// GameSource/Director/Utils/BrnPostBox.h
//
// Canonical generic home for BrnDirector::PostBox<T> (DWARF BrnPostBox.h). A PostBox
// is a single-slot mailbox the director-camera behaviours use to hand a scene-query
// RESULT (a line-test / volume-test output event, or a pointer to one) from the
// producer (the async scene-query pass) to the consumer (a camera behaviour's Update).
//
// LAYOUT (X360-authoritative, from the three GetPackage() const instantiations this
// batch homes):
//   +0x00  EState meState        (E_STATE_EMPTY / E_STATE_POSTED / E_STATE_GOT_PACKAGE)
//   +----  T      mPackage       (placed at the natural alignment of T)
//
// The two attested package offsets both fall out of ordinary C++ member placement:
//   * PostBox<const OutEventLineTestFineResult*> : T is a 4-byte pointer -> mPackage @+4
//     (X360 0x821FC260 / 0x821FBFA0: `addi r3,r31,4`).
//   * PostBox<OutEventLineTestNearestResult>     : T is `struct alignas(16)` -> the
//     4-byte EState is followed by 12 bytes of pad, landing mPackage @+0x10
//     (X360 0x821FBF48: `addi r3,r31,0x10`).
//
// GetPackage() const asserts the box holds a delivered package (meState ==
// E_STATE_GOT_PACKAGE == 2), then returns a reference to mPackage (DWARF BrnPostBox.h:108
// / return-type decl :100). Only the accessors this batch instantiates are bodied; the
// producer side (Post/Clear) lands with its own ledger claim.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnDirector
{
    // Single-slot mailbox for one scene-query result package.
    template <class Type>
    class PostBox
    {
    public:
        // Delivery state of the single package slot.
        enum EState
        {
            E_STATE_EMPTY       = 0,   // no package
            E_STATE_POSTED      = 1,   // producer posted, consumer has not collected
            E_STATE_GOT_PACKAGE = 2,   // package delivered and available to read
        };

        // Return the delivered package. Asserts the box actually holds one
        // (meState == E_STATE_GOT_PACKAGE). X360-attested const accessor
        // (BrnPostBox.h:108); returns &mPackage.
        const Type& GetPackage() const
        {
            CGS_ASSERT(meState == E_STATE_GOT_PACKAGE, "meState == E_STATE_GOT_PACKAGE");
            return mPackage;
        }

    private:
        EState meState;    // +0x00
        Type   mPackage;   // +sizeof/align(EState) rounded to alignof(Type)
    };
}
