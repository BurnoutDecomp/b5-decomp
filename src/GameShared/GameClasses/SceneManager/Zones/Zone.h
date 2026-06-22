#pragma once

// Zone.h — canonical home for CgsSceneManager::Zone (a convex zone polygon used by the
// scene manager's ZoneList for point-in-zone queries) and its Neighbour edge record.
//
// SOURCES: the DecFIGS DWARF Zone.h is GROUND TRUTH for the inline accessor + query
// bodies; the X360 ARTIST DWARF/pseudocode are authority where they disagree.
//   - Zone::IsPointInZone @ 0x822BAB60 (the boot-trace ledger function for this TU):
//     the X360 build hand-vectorises the half-plane test over VMX registers (the test
//     point arrives in a vector register). This reconstruction is SEMANTIC-PARITY to the
//     ground-truth scalar algorithm (and to the asm's per-edge logic): for every polygon
//     edge it forms the edge orthogonal, dots it with (point - corner0), and counts how
//     many edges put the point on the left vs the right; the point is inside iff it is on
//     one consistent side of every edge (countLeft==0 || countRight==0). The math runs on
//     the committed rw::math::vpu::Vector2 lanes BY NAME (.x/.y) rather than reproducing
//     the VMX permute sequence. (This is the platform-faithful scalar form of the asm; the
//     dossier flagged the VMX vector-ABI form as out of reach — this delivers the function
//     by behaviour, not by VMX byte-shape.)
//
// FLAGGED SUBSTITUTION: the DWARF setters/getters guard their preconditions with the
// rich CgsDev::StrStream + CgsDev::Assert::PrintStringed path. Per the project rule that
// substitutes that machinery, those are reconstructed with the committed CGS_ASSERT macro.

#include "types.hpp"
#include "rw/math/vpu/types.h"                       // committed rw::math::vpu::Vector2
#include "GameShared/GameClasses/Core/CgsAssert.h"   // committed CGS_ASSERT (flagged substitute)

namespace CgsSceneManager
{
    class Zone;

    // ---- DWARF Zone.h:36 / the DWARF ----
    // A neighbouring zone reference plus render/immediate flags. 16-byte aligned to match
    // the console layout (the SceneManager fixes these up as a contiguous array).
    class alignas(16) Neighbour
    {
    public:
        enum eNeighbourFlags
        {
            E_RENDERFLAG_NONE         = 0,
            E_NEIGHBOURFLAG_RENDER    = 0x01,
            E_NEIGHBOURFLAG_IMMEDIATE = 0x02
        };

        // DECLARE-ONLY: Set/FixUp/FixDown bodies live in Zone.cpp (separate TUs).
        void Set(Zone* lpZone, u32 luFlags);
        void FixUp(void* lpBaseAddress);
        void FixDown(void* lpBaseAddress);

        const Zone* GetZone() const { return mpZone; }
        u8 IsFlagSet(eNeighbourFlags lxNeighbourFlag) const
        {
            return (muFlags & static_cast<u32>(lxNeighbourFlag)) != 0;
        }
        u32 GetFlags() const { return muFlags; }

    private:
        Zone* mpZone;   // Zone.h:63
        u32   muFlags;  // Zone.h:64
    };

    // ---- DWARF Zone.h:69 / the DWARF ----
    // A convex zone polygon: an array of 2D corner points plus safe/unsafe neighbour
    // lists, an id, a type and flags. 16-byte aligned (points must be 16-byte aligned).
    class alignas(16) Zone
    {
    public:
        // DECLARE-ONLY: Construct/FixUp/FixDown bodies live in Zone.cpp (separate TUs).
        void Construct();
        void FixUp(void* lpBaseAddress);
        void FixDown(void* lpBaseAddress);

        // @0x822BAB60. Convex point-in-polygon: inside iff the point is on one consistent
        // side of every edge. Semantic-parity scalar reconstruction (see file header).
        inline bool IsPointInZone(const rw::math::vpu::Vector2& lPoint) const
        {
            s32 liCountLeft  = 0;
            s32 liCountRight = 0;

            for (s16 liIndex = 0; liIndex < miNumPoints; ++liIndex)
            {
                const rw::math::vpu::Vector2& lCorner0 = mpPoints[liIndex % miNumPoints];
                const rw::math::vpu::Vector2& lCorner1 = mpPoints[(liIndex + 1) % miNumPoints];

                // Edge vector and offset of the test point from corner0.
                const f32 lfEdgeX   = lCorner1.x - lCorner0.x;
                const f32 lfEdgeY   = lCorner1.y - lCorner0.y;
                const f32 lfOffsetX = lPoint.x - lCorner0.x;
                const f32 lfOffsetY = lPoint.y - lCorner0.y;

                // Dot of the offset with the edge orthogonal (-edgeY, edgeX) == the 2D
                // cross product; its sign tells which side of the edge the point is on.
                const f32 lfDotProduct = lfOffsetX * (-lfEdgeY) + lfOffsetY * lfEdgeX;

                if (lfDotProduct < 0.0f)
                {
                    ++liCountLeft;
                }
                else if (lfDotProduct > 0.0f)
                {
                    ++liCountRight;
                }
            }

            return (liCountLeft == 0) || (liCountRight == 0);
        }

        inline void SetPoints(rw::math::vpu::Vector2* lpPoints, s16 liNumPoints)
        {
            // FLAGGED: rich StrStream assert path -> committed CGS_ASSERT.
            CGS_ASSERT((reinterpret_cast<usize>(lpPoints) & 0xF) == 0,
                       "Points MUST be aligned on a 16 byte boundary\n");
            mpPoints    = lpPoints;
            miNumPoints = liNumPoints;
        }

        inline void SetSafeNeighbours(Neighbour* lpSafeNeighbours, s16 liSafeNeighbourCount)
        {
            mpSafeNeighbours     = lpSafeNeighbours;
            miNumSafeNeighbours  = liSafeNeighbourCount;
        }

        inline void SetUnsafeNeighbours(Neighbour* lpUnsafeNeighbours, s16 liUnsafeNeighbourCount)
        {
            mpUnsafeNeighbours    = lpUnsafeNeighbours;
            miNumUnsafeNeighbours = liUnsafeNeighbourCount;
        }

        inline void SetPoint(s16 liPointIndex, const rw::math::vpu::Vector2& lPointLocation)
        {
            // FLAGGED: rich StrStream assert path -> committed CGS_ASSERT.
            CGS_ASSERT(mpPoints && (liPointIndex >= 0) && (liPointIndex < miNumPoints),
                       "Points not initialized or invalid point index\n");
            mpPoints[liPointIndex] = lPointLocation;
        }

        inline void SetId(u64 luId)        { muZoneId   = luId; }
        inline void SetType(s16 liTypeId)  { miZoneType = liTypeId; }
        inline void SetFlags(u32 luFlags)  { muFlags    = luFlags; }

        inline rw::math::vpu::Vector2* GetPoints()           { return mpPoints; }
        inline Neighbour*              GetSafeNeighbours()   { return mpSafeNeighbours; }
        inline Neighbour*              GetUnsafeNeighbours() { return mpUnsafeNeighbours; }

        inline const rw::math::vpu::Vector2* GetPoints()           const { return mpPoints; }
        inline const Neighbour*              GetSafeNeighbours()   const { return mpSafeNeighbours; }
        inline const Neighbour*              GetUnsafeNeighbours() const { return mpUnsafeNeighbours; }

        inline s32 GetNumPoints()           const { return miNumPoints; }
        inline s32 GetNumSafeNeighbours()   const { return miNumSafeNeighbours; }
        inline s32 GetNumUnsafeNeighbours() const { return miNumUnsafeNeighbours; }

        inline rw::math::vpu::Vector2 GetPoint(s16 liIndex) const
        {
            // FLAGGED: rich StrStream assert path -> committed CGS_ASSERT.
            CGS_ASSERT(mpPoints && (liIndex >= 0) && (liIndex < miNumPoints),
                       "Points not initialized or invalid point index\n");
            return mpPoints[liIndex];
        }

        inline const Neighbour* GetSafeNeighbour(s16 liIndex) const
        {
            // FLAGGED: rich StrStream assert path -> committed CGS_ASSERT.
            CGS_ASSERT(mpSafeNeighbours && (liIndex >= 0) && (liIndex < miNumSafeNeighbours),
                       "Neighbours not initialized or invalid neighbour index\n");
            return &mpSafeNeighbours[liIndex];
        }

        inline const Neighbour* GetUnsafeNeighbour(s16 liIndex) const
        {
            // FLAGGED: rich StrStream assert path -> committed CGS_ASSERT.
            CGS_ASSERT(mpUnsafeNeighbours && (liIndex >= 0) && (liIndex < miNumUnsafeNeighbours),
                       "Neighbours not initialized or invalid neighbour index\n");
            return &mpUnsafeNeighbours[liIndex];
        }

        inline u64 GetId()   const { return muZoneId; }
        inline s16 GetType() const { return miZoneType; }
        inline u32 GetFlags() const { return muFlags; }

    private:
        rw::math::vpu::Vector2* mpPoints;            // Zone.h:164
        Neighbour*              mpSafeNeighbours;    // Zone.h:167
        Neighbour*              mpUnsafeNeighbours;  // Zone.h:170
        u64                     muZoneId;            // Zone.h:173
        s16                     miZoneType;          // Zone.h:174
        s16                     miNumPoints;         // Zone.h:177
        s16                     miNumSafeNeighbours; // Zone.h:178
        s16                     miNumUnsafeNeighbours; // Zone.h:179
        u32                     muFlags;             // Zone.h:182
    };

} // namespace CgsSceneManager
