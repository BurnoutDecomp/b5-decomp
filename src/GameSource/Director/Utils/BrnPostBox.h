#pragma once

// ============================================================================
// GameSource/Director/Utils/BrnPostBox.h
//
// Canonical generic home for BrnDirector::PostBox<T> (DWARF BrnPostBox.h). A PostBox
// is a single-slot mailbox the director-camera behaviours use to hand a scene-query
// RESULT (a line-test / volume-test output event, or a pointer to one) from the
// producer (the scene-query pass) to the consumer (a camera behaviour / collision policy).
//
// LAYOUT (X360-authoritative, from the three GetPackage() const instantiations):
//   +0x00  EState meState        (E_STATE_EMPTY / E_STATE_WAITING_FOR_PACKAGE / E_STATE_GOT_PACKAGE)
//   +----  T      mPackage       (placed at the natural alignment of T)
//
// The two attested package offsets both fall out of ordinary C++ member placement:
//   * PostBox<const OutEventLineTestFineResult*> : T is a 4-byte pointer -> mPackage @+4
//     (X360 0x821FC260 / 0x821FBFA0: `addi r3,r31,4`).
//   * PostBox<OutEventLineTestNearestResult>     : T is `struct alignas(16)` -> the
//     4-byte EState is followed by 12 bytes of pad, landing mPackage @+0x10
//     (X360 0x821FBF48: `addi r3,r31,0x10`).
//
// ⭐ THE WHOLE DWARF SURFACE (2026-09-25, FX-DIRECTOR2 -- the camera scene-query closure). The
// state names are the DWARF's (BrnPostBox.h:82 -- the old E_STATE_POSTED was ours; the console's
// own assert text says E_STATE_WAITING_FOR_PACKAGE). Bodies from the PS3 (named) and the X360:
//   Construct       PS3 @0xA122C   Clear()
//   Clear           PS3 @0xA1220   meState = E_STATE_EMPTY (the X360 inlines it everywhere as a
//                                  `stw 0` to the box, e.g. VisibilityTest::ProcessSceneQueryResults)
//   WaitForPackage  PS3 @0xA137C   assert EMPTY (BrnPostBox.cpp:54), meState = WAITING
//                                  (X360: inlined in every PostOffice::AddPostBox, 0x8222D338..64)
//   TakePackage     X360 0x821FF128 (the nearest-result copy, 8 x 8-byte words) / inline in the
//                                  fine-result delivery 0x82210A08: assert WAITING
//                                  (BrnPostBox.cpp:65), mPackage = package, meState = GOT
//   HasPackage      h:99           meState == E_STATE_GOT_PACKAGE
//   GetPackage      h:108          assert GOT, return mPackage
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
        // Delivery state of the single package slot (DWARF BrnPostBox.h:82).
        enum EState
        {
            E_STATE_EMPTY               = 0,   // no package, nothing asked for
            E_STATE_WAITING_FOR_PACKAGE = 1,   // a query is out; its post office holds this box
            E_STATE_GOT_PACKAGE         = 2,   // package delivered and available to read
            E_NUM_STATES                = 3,
        };

        void Construct() { Clear(); }                                                  // h:40
        void Clear()     { meState = E_STATE_EMPTY; }                                  // h:43

        // h:46 -- the post office's AddPostBox raises this when it mints the query id.
        void WaitForPackage()
        {
            CGS_ASSERT(meState == E_STATE_EMPTY, "meState == E_STATE_EMPTY");          // BrnPostBox.cpp:54
            meState = E_STATE_WAITING_FOR_PACKAGE;
        }

        // h:56 -- the post office's Deliver hands the result over.
        void TakePackage(const Type& lrPackage)
        {
            CGS_ASSERT(meState == E_STATE_WAITING_FOR_PACKAGE,
                       "meState == E_STATE_WAITING_FOR_PACKAGE");                       // BrnPostBox.cpp:65
            mPackage = lrPackage;
            meState  = E_STATE_GOT_PACKAGE;
        }

        bool HasPackage() const { return meState == E_STATE_GOT_PACKAGE; }            // h:99
        EState GetState() const { return meState; }   // [PC accessor: the X360 reads the word inline]

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
