// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wR_11.cpp
//   (road-rules wave partfile -- the buffered new-high-score list)
//
//   NewHighScoreBuffer::EraseMatchingEntries
//
// Kept in its own partfile so it can be dropped without touching _wR_10 if
// another lane lands the same body.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>::GetLength / operator[] / Erase

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Drop every buffered entry for (lRoadID, leScoreType).
//
// The index still advances after an Erase, so the entry that slides into the
// erased slot is not re-tested: two adjacent matches leave the second one in
// the buffer. That is the console's loop, kept as is.
// ----------------------------------------------------------------------------
void NewHighScoreBuffer::EraseMatchingEntries( ::CgsID lRoadID, BrnStreetData::ScoreType leScoreType )
{
    for ( u32 luIndex = 0; luIndex < GetLength(); ++luIndex )
    {
        if ( lRoadID == (*this)[luIndex].mRoadID &&
             leScoreType == (*this)[luIndex].meScoreType )
        {
            Erase( luIndex );
        }
    }
}

} // namespace BrnGameState
