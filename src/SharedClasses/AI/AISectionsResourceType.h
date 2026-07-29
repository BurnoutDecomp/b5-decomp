#ifndef AI_SECTIONS_RESOURCE_TYPE_H
#define AI_SECTIONS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu alias)
#include <cstddef>            // offsetof (host layout static_asserts)

namespace CgsMemory { class LinearMalloc; }

namespace BrnAI
{
// Forward decls (separate TUs); only pointer members reference them.
struct Portal;
struct BoundaryLine;
struct SectionResetPair;
// AISectionPointMap -- uniform-grid spatial index over the sections. DWARF home
// AISectionsData.h:540; the full (X360-offset-authoritative) layout lives in
// AISectionsData.cpp, which owns BuildAISectionPointMap/FindNearestAISection. Only
// pointer/return references appear here, so a forward decl suffices.
struct AISectionPointMap;

// Number of AI-section speed classes. AISection::muSpeed (+0x15) indexes the
// parallel mafSectionMinSpeeds[5] / mafSectionMaxSpeeds[5] bands below. The X360
// bounds it with "muSpeed < E_SECTION_SPEED_COUNT" (cmplwi ..,5) at
// AISectionsData.h:907 (e.g. RaceBalancingRoute::GetAISectionSpeed @0x827697A0).
// Additive grow flagged by the RaceBalancingRoute group; value attested by the
// asm literal and by the [5]-sized speed arrays.
const u32 E_SECTION_SPEED_COUNT = 5;

// Number of corners / edges of an AISection's convex footprint polygon. The X360
// IsInside @0x82677058 / PassesThrough @0x826772A8 walk mpaCorners with an 8-byte
// (Vector2) stride, seed the wrap-around edge from corner[3] (lfs 0x18/0x1C off the
// corner base) and terminate the loop at offset 0x20 == 4 * 8 -- i.e. exactly 4
// corners. The inlined GetCorner bounds-assert string is
// "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES", which names this
// constant directly. Value attested by the asm loop bound (cmpwi ..,4 / cmpwi r,0x20).
const s32 KI_AI_SECTION_EDGES = 4;

// BrnAI::AISection -- DWARF AISectionsData.h:339 (members :462-473); X360 24-byte stride.
struct AISection
{
    // -------------------------------------------------------------------------
    // AISection::Vector2 -- the PACKED 2D point type of the section footprint.
    //
    // ⚠️ NOT rw::math::vpu::Vector2, and deliberately NESTED. The DWARF declares it inside
    // this struct (`typedef Vector2Template<float> Vector2;`, AISectionsData.h; the
    // FontChar::/SmoothStep:: prefixes elsewhere in the dump are namespace-merge artifacts),
    // i.e. a plain {x, y} pair -- 8 bytes. The X360 agrees three times over: IsInside
    // @0x82677058 / PassesThrough @0x826772A8 walk mpaCorners with an 8-byte stride and
    // terminate at offset 0x20 == 4 * 8; GetMiddle @0x826771D0 reads the four X lanes at
    // +0/+8/+0x10/+0x18 and the four Z lanes at +4/+0xC/+0x14/+0x1C; and every section's
    // corner block in the shipped AI.DAT is exactly 32 bytes of eight consecutive floats.
    //
    // The unqualified `Vector2` (BrnCommonTypes.h) is the 16-byte SIMD register alias, which
    // the REST of namespace BrnAI really does use (RouteMapModuleIO's ExtrapolatedRouteRequest
    // pins mCarPosition/mCarDirection at +0x10/+0x20 on 16-byte lanes). Keeping this one
    // nested is what lets both be right -- hoisting it to namespace scope would silently
    // re-stride those request records.
    // -------------------------------------------------------------------------
    struct Vector2
    {
        f32 x;
        f32 y;
    };
    static_assert(sizeof(Vector2) == 8, "AISection::Vector2 is the packed 2-float corner type");

    // X360 (4-byte pointer) offsets in the comments; the host offsets are pinned by the
    // static_asserts below, and every member is reached BY NAME.
    Portal*       mpaPortals;       // X360 +0   :462
    BoundaryLine* mpaNoGoLines;     // X360 +4   :463
    Vector2*      mpaCorners;       // X360 +8   :464 (KI_AI_SECTION_EDGES packed 2-float corners)
    u32           mId;              // X360 +12  :465 (AISection::AISectionId == u32)
    s16           miSpanIndex;      // X360 +16  :467 (Road::SpanIndex == int16_t)
    u16           muNumNoGoLines;   // X360 +18  :468
    u8            mu8NumPortals;    // X360 +20  :470
    u8            muSpeed;          // X360 +21  :471
    u8            mu8eDistrict;     // X360 +22  :472
    u8            mx8Flags;         // X360 +23  :473
    Vector3 GetMiddle() const;      // :343 (defined in the AISection TU)

    // Load-time pointer relocation. X360 @0x8267D8C8 / @0x8267D978: rebases mpaPortals
    // (then each Portal's mpaBoundaryLines) and mpaNoGoLines UNDER A NULL GUARD, and
    // mpaCorners UNCONDITIONALLY. Bodied in AISectionsData.cpp.
    void FixUp(const void* lpBaseData);
    void FixDown(const void* lpBaseData);

    // True when this section must not be used as a reset-on-track link target. X360
    // @0x8276AC18 (BrnAI::ResetOnTrackManager::UpdateResetOnTrackSectionUsingCurrentSection):
    // reads mx8Flags @+23 and returns true if either bit 0x01 or bit 0x40 is set, false otherwise.
    bool IsUnsuitableForResetOnTrackLink() const;

    // Return the luPortalIndex'th portal of this section. X360 @0x8230F5D0: bounds-checks
    // luPortalIndex against mu8NumPortals (+20) firing a streamed dev-assert on overrun,
    // then returns &mpaPortals[luPortalIndex] (Portal is a 20-byte struct).
    const Portal* GetPortal(u8 luPortalIndex) const;

    // Convex point-in-polygon test of (lfX, lfY) against the section's KI_AI_SECTION_EDGES
    // corner footprint (the world (x, z) ground plane). X360 @0x82677058.
    bool IsInside(f32 lfX, f32 lfY) const;

    // True if the 2D segment lStart->lEnd enters this section (either endpoint inside, or it
    // crosses a polygon edge with the query parameter in [0,1] and the edge parameter >= 0).
    // X360 @0x826772A8.
    bool PassesThrough(Vector2 lStart, Vector2 lEnd) const;
};

// KU_AI_SECTIONS_DATA_VERSION (DWARF AISectionsData.h). NOTE the console's
// AISectionsData::FixUp does NOT check it (unlike TrafficData::FixUp) -- kept for the
// consumers that do.
const u32 KU_AI_SECTIONS_DATA_VERSION = 12;

// BrnAI::AISectionsData -- DWARF AISectionsData.h:568 (members :637-650); X360 64 bytes.
struct AISectionsData
{
    AISection*        mpaSections;            // X360 +0   :637
    SectionResetPair* mpaSectionResetPairs;   // X360 +4   :638
    f32               mafSectionMinSpeeds[5]; // X360 +8   :640
    f32               mafSectionMaxSpeeds[5]; // X360 +28  :641
    u32               muNumSections;          // X360 +48  :643
    u32               muNumSectionResetPairs; // X360 +52  :644
    u32               muVersion;              // X360 +56  :646
private:
    u32               muSizeInBytes;          // X360 +60  :650 (private)
public:
    // Load-time pointer relocation. X360 @0x8267DA28 / @0x8267DAA0, reached through
    // AISectionsResourceType::FixUp @0x8267DB28 (`mr r3,r4; mr r4,r5; b ...`), which forwards
    // the rw::Resource so the body can read the block base out of it. The DWARF signature is
    // MemoryResource-shaped; modelled here as the base pointer, matching the Traffic path.
    // ⚠️ WIDENED (was `int FixUp(int liDelta)`): an int delta truncates a 64-bit host base to
    // its low 32 bits -- bug class (a) -- and the widened lane data now carries 64-bit slots.
    void FixUp(const void* lpBaseData);
    void FixDown(const void* lpBaseData);

    u32              GetSizeInBytes() const { return muSizeInBytes; }   // :590 -- replaces the raw +60 read
    const AISection* GetAISection(u32 luSectionIndex) const;           // :594 (DWARF const drift)
    Vector3          GetMiddle(u16 luSectionIndex) const;              // :613

    // Spatial-lookup pair (bodied in AISectionsData.cpp). BuildAISectionPointMap @0x8267A688
    // allocates the grid index out of the supplied LinearMalloc; FindNearestAISection
    // @0x82676CC0 (the point-map overload, :623) returns the original index of the section
    // whose middle is closest to the query position.
    AISectionPointMap* BuildAISectionPointMap(CgsMemory::LinearMalloc* lpMalloc) const;  // :626
    u16                FindNearestAISection(Vector3 lPosition, AISectionPointMap* lpMap) const; // :623
};

// BrnAI::SectionResetPair -- a start section plus the section a reset sends you to
// (DWARF AISectionsData.h). No pointers; stride 8 on both target and host. This header
// is its only home.
struct SectionResetPair
{
    u32 meResetSpeed;          // BrnAI::EResetSpeedType
    u16 muStartSectionIndex;
    u16 muResetSectionIndex;
};

// ---- host layout contract with tools/assets/bundles/lane_transcode.py ---------------
// (Portal / BoundaryLine are pinned in their own owning headers,
//  GameSource/World/AI/BrnAIPortal.h and BrnAIBoundaryLine.h.)
static_assert(offsetof(AISection, mpaCorners)  == 0x10, "AISection::mpaCorners");
static_assert(offsetof(AISection, mId)         == 0x18, "AISection::mId");
static_assert(offsetof(AISection, mx8Flags)    == 0x23, "AISection::mx8Flags");
static_assert(sizeof(AISection) == 0x28, "AISection host sizeof");
static_assert(sizeof(SectionResetPair) == 8, "SectionResetPair stride");
static_assert(offsetof(AISectionsData, mafSectionMinSpeeds) == 0x10, "AISectionsData::mafSectionMinSpeeds");
static_assert(offsetof(AISectionsData, muNumSections)       == 0x38, "AISectionsData::muNumSections");
static_assert(sizeof(AISectionsData) == 0x48, "AISectionsData host sizeof");

class AISectionsResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
};
}

#endif
