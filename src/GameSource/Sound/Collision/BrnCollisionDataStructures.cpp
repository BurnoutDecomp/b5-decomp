#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (validity / same-scrape tripwires)

// =============================================================================
// BrnSound::Logic::Collision::ScrapeInfo -- out-of-line bodies for the two
// ledger functions owned by this TU:
//   operator==     @ 0x826821F0
//   UpdateHistory  @ 0x826822C8
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// See BrnCollisionDataStructures.h for the layout map and the un-DWARF'd-member
// FLAG. All members reached BY NAME.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// ScrapeInfo::operator==  @ 0x826821F0
//
//   assert(mbValid)            ; lbz 0x29(this)  -> "mbValid"        (h:111)
//   assert(lInfo.mbValid)      ; lbz 0x29(lInfo) -> "lInfo.mbValid"  (h:112)
//   result = false
//   if (lInfo.mu32Kind == mu32Kind)          ; lwz 0x20 / cmpw
//   {
//       // unordered object-pair match
//       if ( (lInfo.maObjectId[0] == maObjectId[0] && lInfo.maObjectId[1] == maObjectId[1])
//         || (lInfo.maObjectId[0] == maObjectId[1] && lInfo.maObjectId[1] == maObjectId[0]) )
//           result = true;
//   }
//   return result
//
// Store-for-store with the X360: object ids compared with cmplw (unsigned u32),
// the kind compared with cmpw at +0x20, the validity bytes read at +0x29. The
// X360 reads lInfo's fields via r29 and *this via r31; the two asserts are
// non-gating tripwires.
// ---------------------------------------------------------------------------
bool ScrapeInfo::operator==( const ScrapeInfo& lInfo ) const
{
    CGS_ASSERT( mbValid,       "mbValid" );
    CGS_ASSERT( lInfo.mbValid, "lInfo.mbValid" );

    bool lbEqual = false;

    if ( lInfo.mu32Kind == mu32Kind )
    {
        if ( ( lInfo.maObjectId[0] == maObjectId[0] && lInfo.maObjectId[1] == maObjectId[1] )
          || ( lInfo.maObjectId[0] == maObjectId[1] && lInfo.maObjectId[1] == maObjectId[0] ) )
        {
            lbEqual = true;
        }
    }

    return lbEqual;
}

// ---------------------------------------------------------------------------
// ScrapeInfo::UpdateHistory  @ 0x826822C8
//
//   assert(*this == lInfo)     ; bl operator== ; "*this == lInfo"     (h:146)
//   mfParam0x24 = lInfo.mfParam0x24    ; lfs 0x24(lInfo) -> stfs 0x24(this)
//   mfParam0x18 = lInfo.mfParam0x18    ; lfs 0x18(lInfo) -> stfs 0x18(this)
//
// Store-for-store with the X360: both copies are f32 (lfs/stfs), in the asm order
// (+0x24 then +0x18). The leading assert calls operator== (non-gating tripwire).
// ---------------------------------------------------------------------------
void ScrapeInfo::UpdateHistory( const ScrapeInfo& lInfo )
{
    CGS_ASSERT( *this == lInfo, "*this == lInfo" );

    mfParam0x24 = lInfo.mfParam0x24;
    mfParam0x18 = lInfo.mfParam0x18;
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
