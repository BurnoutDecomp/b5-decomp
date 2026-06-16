#ifndef BRN_TRIGGER_BASE_H
#define BRN_TRIGGER_BASE_H

#include "types.hpp"          // fixed-width ints
#include "BrnCommonTypes.h"   // CgsID
#include "SharedClasses/Trigger/BrnRegion.h"  // BoxRegion (base subobject + accessor)

// SharedClasses/Trigger/BrnTriggerBase.h
//
// Owning header for `BrnTrigger::TriggerRegion`, the common base of Landmark and
// the other trigger-region kinds. SHAPE is gated on the X360 DWARF
// (references/DecFIGS/dwarfdump/SharedClasses/Trigger/BrnTriggerBase.h +
// BrnRegion.h), which is authoritative.
//
// X360 layout (DWARF): BoxRegion mBoxRegion; int32_t mId; int16_t miRegionIndex;
// uint8_t meType; uint8_t muPad[1].  (The Feb-2007 leaked header had `int32_t
// miRegionIndex` + `uint8_t muPad[3]`; the shipped build narrowed the region
// index to 16 bits, hence a single pad byte. We follow the DWARF.)
//
// mId is semantically a CgsID (GetId() returns CgsID). The DWARF prints the
// member as int32_t because that build's CgsID storage was 32-bit there; we keep
// the storage as int32_t to match the authoritative size while exposing GetId()
// as CgsID, exactly as the type's interface does. INTEGRATOR NOTE: BrnCommonTypes
// currently typedefs CgsID to u64. If the trigger subsystem's CgsID must be the
// 32-bit flavor, reconcile the typedef vs. this 32-bit storage member when wiring
// this into the real tree (see notes).
//
// All accessors used by OfflineGameMode (GetBoxRegion, GetRegionIndex, GetId) are
// INLINED in the X360 build — the ledger has NO standalone TUs for them — so
// inline bodies here are correct and sufficient for the cl /c gate.

namespace BrnTrigger
{

struct TriggerRegion
{
public:
    enum Type
    {
        E_TYPE_LANDMARK = 0,
        E_TYPE_BLACKSPOT = 1,
        E_TYPE_GENERIC_REGION = 2,
        E_TYPE_VFXBOX_REGION = 3,

        E_TYPE_COUNT = 4
    };

    static const char* KPAC_TRIGGER_TYPE_STRINGS[E_TYPE_COUNT];

    void Construct( CgsID lId, Type leType, const BoxRegion* lpBoxRegion );

    inline CgsID GetId() const;
    inline int32_t GetRegionIndex() const;
    inline Type GetType() const;
    inline void SetRegionIndex( int32_t liRegionIndex );

    void FixDown();
    void FixUp();

    inline const BoxRegion* GetBoxRegion() const;

private:
    BoxRegion mBoxRegion;   // base box (position queried by SelectRandomDestinations)
    int32_t   mId;          // CgsID storage (see header note on width)
    int16_t   miRegionIndex;
    uint8_t   meType;
    uint8_t   muPad[1];
};

inline CgsID
TriggerRegion::GetId() const
{
    return static_cast<CgsID>( mId );
}

inline int32_t
TriggerRegion::GetRegionIndex() const
{
    return miRegionIndex;
}

inline TriggerRegion::Type
TriggerRegion::GetType() const
{
    return static_cast<Type>( meType );
}

inline void
TriggerRegion::SetRegionIndex( int32_t liRegionIndex )
{
    miRegionIndex = static_cast<int16_t>( liRegionIndex );
}

inline const BoxRegion*
TriggerRegion::GetBoxRegion() const
{
    return &mBoxRegion;
}

} // namespace BrnTrigger

#endif // BRN_TRIGGER_BASE_H
