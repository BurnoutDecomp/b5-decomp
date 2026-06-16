#ifndef BRN_LANDMARK_H
#define BRN_LANDMARK_H

#include "types.hpp"          // fixed-width ints
#include "BrnCommonTypes.h"   // CgsID, Vector3
#include "SharedClasses/Trigger/BrnTriggerBase.h"  // TriggerRegion (base) + BoxRegion

// Forward decl for the out-of-line FixDown/FixUp(MemoryResource) signatures only.
// rw::MemoryResource is not yet defined in the reconstructed tree; the consumer in
// scope (SelectRandomDestinations) never calls these, and they have their own TUs,
// so a forward decl keeps this header self-compiling. INTEGRATOR: replace with the
// real rw::MemoryResource definition/include once it lands in the tree.
namespace rw { class MemoryResource; }

// SharedClasses/Trigger/BrnLandmark.h
//
// Owning header for `BrnTrigger::Landmark` (and its small companion StartingGrid).
//
// SHAPE is gated on the X360 DWARF (references/DecFIGS/dwarfdump/SharedClasses/
// Trigger/BrnLandmark.h), which is authoritative and DIFFERS from the older
// Feb-2007 leaked header:
//   * X360 Landmark has NO mpaFinishLines / miFinishLineCount members (the
//     finish-line array was removed before ship).
//   * miStartingGridCount is int8_t (not int32_t).
//   * An extra uint8_t mu8Flags exists, with KI_FLAG_ONLINE = 1 / IsOnline().
//   * GetDesignIndex() returns uint8_t.
// We follow the DWARF.
//
// MINIMAL SCOPE: BrnGameState::OfflineGameMode::SelectRandomDestinations reaches
// a Landmark via TriggerData::GetLandmark(), then calls
// GetBoxRegion()->GetPosition() (inherited from TriggerRegion) and the per-type
// accessors GetDesignIndex() / GetId() / GetRegionIndex(). All of those are
// INLINED in the X360 build (the ledger has zero standalone TUs for GetBoxRegion,
// GetRegionIndex, GetDesignIndex), so inline bodies here are sufficient for the
// cl /c gate. Construct()/FixDown()/FixUp() are out-of-line (own TUs) — declared
// only. The StartingGrid array accessors are NOT used by the consumer but are
// kept as inline members of the owning type.

namespace BrnTrigger
{

// Per-grid starting layout. DWARF shape: two arrays of 8 Vector3 each. Kept
// complete (and small) so Landmark's pointer-based inline accessors stay valid.
struct StartingGrid
{
public:
    static const int32_t KI_STARTING_POINT_COUNT = 8;

    void Construct();
    void SetPoint( int32_t liIndex, Vector3 lPosition, Vector3 lDirection );

    inline Vector3 GetPosition( int32_t liIndex ) const;
    inline Vector3 GetDirection( int32_t liIndex ) const;

    void FixDown();
    void FixUp();

private:
    Vector3 maStartingPositions[KI_STARTING_POINT_COUNT];
    Vector3 maStartingDirections[KI_STARTING_POINT_COUNT];
};

inline Vector3
StartingGrid::GetPosition( int32_t liIndex ) const
{
    return maStartingPositions[liIndex];
}

inline Vector3
StartingGrid::GetDirection( int32_t liIndex ) const
{
    return maStartingDirections[liIndex];
}

// The landmark trigger. Inherits its BoxRegion + id + region-index from
// TriggerRegion; adds the starting-grid array, design index, district, and flags.
class Landmark : public TriggerRegion
{
public:
    static const int32_t KI_FLAG_ONLINE = 1;

    // Out-of-line (own TUs) — declaration only:
    void Construct( CgsID lId, const BoxRegion* lpRegion, int32_t liStartingGridCount,
                    /*LinearMalloc*/ void* lpLinearMalloc, uint8_t luDesignIndex,
                    uint8_t luDistrict, bool lbOnline );
    void FixDown( rw::MemoryResource lpBaseData );
    void FixUp( rw::MemoryResource lpBaseData );

    // Inlined accessors (no standalone TUs in the X360 ledger):
    inline void SetStartingGrid( int32_t liIndex, const StartingGrid* lpStartingGrid );
    inline StartingGrid* GetStartingGrid( int32_t liIndex );
    inline const StartingGrid* GetStartingGrid( int32_t liIndex ) const;
    inline int32_t GetStartingGridCount() const;

    // Used by SelectRandomDestinations:
    inline uint8_t GetDesignIndex() const;

    inline uint8_t GetDistrict() const;
    inline bool    IsOnline() const;

private:
    StartingGrid* mpaStartingGrids;
    int8_t        miStartingGridCount;
    uint8_t       muDesignIndex;
    uint8_t       muDistrict;
    uint8_t       mu8Flags;
};

inline void
Landmark::SetStartingGrid( int32_t liIndex, const StartingGrid* lpStartingGrid )
{
    mpaStartingGrids[liIndex] = *lpStartingGrid;
}

inline StartingGrid*
Landmark::GetStartingGrid( int32_t liIndex )
{
    return &mpaStartingGrids[liIndex];
}

inline const StartingGrid*
Landmark::GetStartingGrid( int32_t liIndex ) const
{
    return &mpaStartingGrids[liIndex];
}

inline int32_t
Landmark::GetStartingGridCount() const
{
    return miStartingGridCount;
}

inline uint8_t
Landmark::GetDesignIndex() const
{
    return muDesignIndex;
}

inline uint8_t
Landmark::GetDistrict() const
{
    return muDistrict;
}

inline bool
Landmark::IsOnline() const
{
    return ( mu8Flags & KI_FLAG_ONLINE ) != 0;
}

} // namespace BrnTrigger

#endif // BRN_LANDMARK_H
