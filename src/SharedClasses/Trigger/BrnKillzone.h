#ifndef BRN_KILLZONE_H
#define BRN_KILLZONE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

// SharedClasses/Trigger/BrnKillzone.h
//
// Minimal owning slice for BrnTrigger::Killzone, modeled from the X360 DecFIGS
// DWARF (references/DecFIGS/dwarfdump/SharedClasses/Trigger/BrnKillzone.h), which
// is authoritative for shape. The only reason this header exists right now is that
// TriggerData::GetKillzone (X360 0x82354820) returns &mpKillzones[i] and the X360
// asm uses a 16-byte stride (addi r3, r11, 16*idx) -- exactly the 4-word layout
// below (mppTriggers ptr + miTriggerCount + mpRegionIds ptr + miRegionIdCount =
// 16B). The accessors are all INLINED in the X360 build so inline bodies are
// sufficient for the cl /c gate; Construct/FixUp/FixDown have their own TUs and are
// declaration-only.
// INTEGRATOR: GenericRegion is forward-declared (GetTrigger returns it); fold in
// its real owning header when that type lands.

namespace BrnTrigger
{
struct GenericRegion;   // forward decl: GetTrigger returns a GenericRegion*

struct Killzone
{
public:
    void Construct( int32_t liTriggerCount, int32_t liRegionIdCount, /*LinearMalloc*/ void* lpLinearMalloc );
    void FixDown( /*MemoryResource*/ );
    void FixUp( /*MemoryResource*/ );

    inline int32_t GetTriggerCount() const  { return miTriggerCount; }
    inline int32_t GetRegionIdCount() const { return miRegionIdCount; }

    // [gateui] 2026-08-20: BODIED in the sibling BrnKillzone.cpp (it needs the complete
    // GenericRegion type, which this header deliberately keeps forward-declared). It was a
    // measured UNDEF external in BrnTriggerQueryManager.obj and is one of the thirteen
    // unresolved externals build_game_exe.bat:2369-2375 blames for that TU being unmounted.
    const GenericRegion* GetTrigger( int32_t liIndex ) const;
    inline CgsID GetRegionId( int32_t liIndex ) const { return mpRegionIds[liIndex]; }

    void SetTrigger( int32_t liIndex, const GenericRegion* lpTrigger ) const;
    void SetRegionId( int32_t liIndex, CgsID lId ) const;

private:
    const GenericRegion** mppTriggers;     // 0x00
    int32_t               miTriggerCount;  // 0x04
    CgsID*                mpRegionIds;      // 0x08
    int32_t               miRegionIdCount;  // 0x0C  (total 16B == X360 stride)
};
}

#endif // BRN_KILLZONE_H
