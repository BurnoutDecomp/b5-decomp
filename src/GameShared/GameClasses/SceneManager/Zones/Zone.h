#pragma once

// Zone.h — canonical home for CgsSceneManager::Zone (a convex zone polygon used by the
// scene manager's ZoneList for point-in-zone queries) and its Neighbour edge record.
//
// ARTIST 822BAB60 supplies the by-value VMX point query. DecFIGS Zone.h
// supplies the declarations; native point and neighbour records retain their layout.
// The query uses the original signed edge test, separate products/add and CR6 result.
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
    class CGS_ALIGN(16) Neighbour
    {
    public:
        enum eNeighbourFlags
        {
            E_RENDERFLAG_NONE         = 0,
            E_NEIGHBOURFLAG_RENDER    = 0x01,
            E_NEIGHBOURFLAG_IMMEDIATE = 0x02
        };

        // DECLARE-ONLY: Set/FixUp/FixDown and the two X360-emitted out-of-line accessors
        // (GetZone @0x828AC430, IsFlagSet @0x828AC438) are homed by Zone.cpp.
        void Set(Zone* lpZone, u32 luFlags);
        void FixUp(void* lpBaseAddress);
        void FixDown(void* lpBaseAddress);

        // @0x828AC430. Returns the neighbouring zone pointer (mpZone @ +0). Out-of-line in Zone.cpp.
        const Zone* GetZone() const;

        // @0x828AC438. Returns whether ALL bits of the flag mask are set in muFlags -- the X360
        // tests `flag == (muFlags & flag)`, not a !=0 / any-bit test. Out-of-line in Zone.cpp.
        u8 IsFlagSet(eNeighbourFlags lxNeighbourFlag) const;

        u32 GetFlags() const { return muFlags; }

    private:
        // Zone::FixUp/FixDown rebase the embedded neighbour arrays' mpZone pointers inline (the
        // X360 inlines the per-element pointer fixup into Zone::FixDown rather than calling
        // Neighbour::FixDown); grant Zone access for that rebase.
        friend class Zone;

        Zone* mpZone;   // Zone.h:63
        u32   muFlags;  // Zone.h:64
    };

    // ---- DWARF Zone.h:69 / the DWARF ----
    // A convex zone polygon: an array of 2D corner points plus safe/unsafe neighbour
    // lists, an id, a type and flags. 16-byte aligned (points must be 16-byte aligned).
    class CGS_ALIGN(16) Zone
    {
    public:
        // DECLARE-ONLY: Construct/FixUp/FixDown bodies live in Zone.cpp (separate TUs).
        void Construct();
        void FixUp(void* lpBaseAddress);
        void FixDown(void* lpBaseAddress);

        // ARTIST 822BAB60: native point in v1 and full VMX edge arithmetic.
        inline bool IsPointInZone(rw::math::vpu::Vector2::InParam lPoint) const
        {
            CGS_STATIC_ASSERT(offsetof(Zone, miNumPoints) == 0x1A, "ARTIST zone point count");
            s32 liCountLeft = 0;
            s32 liCountRight = 0;
            const __vector4 lvZero = __vspltisw(0);
            const __vector4 lvOnes = __vspltisw(-1);
            const __vector4 lvSign = __vslw(lvOnes, lvOnes);
            for (s16 liIndex = 0; liIndex < miNumPoints; ++liIndex)
            {
                const __vector4 lvCorner0 = mpPoints[liIndex % miNumPoints].mV;
                const __vector4 lvCorner1 = mpPoints[(liIndex + 1) % miNumPoints].mV;
                const __vector4 lvEdge = __vsubfp(lvCorner1, lvCorner0);
                const __vector4 lvOffset = __vsubfp(lPoint.mV, lvCorner0);
                const __vector4 lvNegativeY = __vxor(__vspltw(lvEdge, 1), lvSign);
                __vector4 lvOrthogonal = __vrlimi(lvZero, lvNegativeY, 8, 0);
                lvOrthogonal = __vrlimi(lvOrthogonal, lvEdge, 4, 3);
                const __vector4 lvProduct = __vmulfp(lvOffset, lvOrthogonal);
                const __vector4 lvDot = __vaddfp(__vspltw(lvProduct, 0), __vspltw(lvProduct, 1));
                u32 luComparison;
                __vcmpgtfpR(lvZero, lvDot, &luComparison);
                if ((luComparison & 0x80u) != 0)
                    ++liCountLeft;
                else
                {
                    __vcmpgtfpR(lvDot, lvZero, &luComparison);
                    if ((luComparison & 0x80u) != 0)
                        ++liCountRight;
                }
            }
            return liCountLeft == 0 || liCountRight == 0;
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

        // @0x822BACE8. Centroid of the zone polygon: the average of all corner points
        // (tail-calls CalculateCentre4 for the exactly-4-point case). All four Vector2 lanes
        // participate (the X360 sums full 16-byte point vectors). Out-of-line body in Zone.cpp.
        rw::math::vpu::Vector2 CalculateCentre() const;

        // @0x822A3CA0. Branch-free centroid for the 4-point case: (p0+p1+p2+p3)*0.25.
        // Asserts miNumPoints == 4. Out-of-line body in Zone.cpp.
        rw::math::vpu::Vector2 CalculateCentre4() const;

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
