#ifndef GAMESOURCE_EFFECTS_BRNCRASHTRIANGLECACHE_H
#define GAMESOURCE_EFFECTS_BRNCRASHTRIANGLECACHE_H

#include "types.hpp"

// ============================================================================
// GameSource/Effects/BrnCrashTriangleCache.h
//
// BrnEffects::BrnCrashTriangleCache -- the effects module's cache of the world
// triangles the crashing player car is grinding over (EffectsModule::
// HandlePlayerTriangleCache @0x82296EA0 feeds it from the scene manager's
// triangle-cache interface; the crash spark / debris handlers read it).
//
// 2026-09-02 (tyre-mark wave): the type used to live at the top of
// BrnCrashTriangleCache.cpp. EffectsModule embeds it BY VALUE
// (DWARF EffectsModule.h:621 `BrnCrashTriangleCache mCrashTriangleCache`, X360
// +0x2D400, constructed by EffectsModule::Construct @0x8228FE98), so the
// declarations moved here -- same names, same layout, nothing forked. Bodies stay
// in the .cpp (Construct @0x8227B240, AddTriangles @0x8228CDA8,
// CheckForDuplicateTriangles @0x822847B0).
// ============================================================================

namespace BrnEffects
{
    static const u32 KU_MAX_NUMBER_PACKED_TRIANGLES = 48;
    static const u32 KU_TRIANGLES_PER_PACK = 4;
    static const u16 KU_INVALID_CRASH_MATERIAL_ID = 0x11;

    // The SoA lane of the packed crash-triangle format: four scalars, one per triangle
    // of the Triangle4 batch. It is NOT the math Vector4 -- it carries no vpu semantics
    // and is only ever indexed by triangle component. It used to be a file-local
    // `Vector4` at the top of BrnCrashTriangleCache.cpp; promoting it verbatim into this
    // header (2026-09-02) put a `BrnEffects::Vector4` in scope for the WHOLE namespace,
    // which silently shadowed the global `::Vector4` (= rw::math::vpu::Vector4) in every
    // other BrnEffects header -- measured: it shrank BrnEffects::BloomData from 32 to 24
    // bytes and tripped the `BloomData layout drift` static_assert in
    // SharedClasses/Graphics/BrnEffectsData.h the first time a TU included both. Named
    // apart so it cannot shadow again.
    struct Vector4Lane
    {
        f32 mafValues[4];

        void Clear()
        {
            for (u32 luComponent = 0; luComponent < 4; ++luComponent)
            {
                mafValues[luComponent] = 0.0f;
            }
        }

        f32 GetComponent(u32 luComponent) const
        {
            return mafValues[luComponent];
        }

        void SetComponent(u32 luComponent, f32 lfValue)
        {
            mafValues[luComponent] = lfValue;
        }
    };

    struct CollisionTag
    {
        u32 muValue;

        bool IsEmpty() const
        {
            return muValue == 0;
        }

        u16 GetMaterialId() const
        {
            return static_cast<u16>((muValue >> 20) & 0x3F);
        }
    };

    struct BrnCrashTrianglePackedFormat
    {
        void Clear();
        void SetScalarTriangle(const BrnCrashTrianglePackedFormat& lTriangle, u32 luDestinationComponent);
        bool HasMatchingHash(const BrnCrashTrianglePackedFormat& lTriangle, u32 luComponent) const;

        Vector4Lane mVertexHash;
        Vector4Lane mVertex0X;
        Vector4Lane mVertex0Y;
        Vector4Lane mVertex0Z;
        Vector4Lane mVertex1X;
        Vector4Lane mVertex1Y;
        Vector4Lane mVertex1Z;
        Vector4Lane mVertex2X;
        Vector4Lane mVertex2Y;
        Vector4Lane mVertex2Z;
    };

    struct Triangle4
    {
        BrnCrashTrianglePackedFormat ExtractPackedTriangle(u32 luComponent) const;
        CollisionTag GetCollisionTag(u32 luComponent) const;

        Vector4Lane mVertex0X;
        Vector4Lane mVertex0Y;
        Vector4Lane mVertex0Z;
        Vector4Lane mVertex1X;
        Vector4Lane mVertex1Y;
        Vector4Lane mVertex1Z;
        Vector4Lane mVertex2X;
        Vector4Lane mVertex2Y;
        Vector4Lane mVertex2Z;
        Vector4Lane mSurfaceTags;
    };

    struct BrnCrashTriangleCache
    {
        void Construct();
        // The three-word counter reset EffectsModule stores inline (HandleGameActions
        // @0x82296FD8 case 39 and HandlePlayerTriangleCache @0x82296EA0: X360 +0x2F200 /
        // +0x2F204 / +0x2F208 = 0 -- the packed triangles themselves are NOT cleared, which
        // is what distinguishes it from Construct). Additive (2026-09-02).
        void ResetCounters()
        {
            mnNumberOfPackedTriangles  = 0;
            mnNextPackedTriangleToFill = 0;
            mnNextComponentToFill      = 0;
        }
        void AddTriangles(const Triangle4* lpInTriangles, u32 lnNum4Triangles);
        void CheckForDuplicateTriangles(BrnCrashTrianglePackedFormat* lpaPackedTriangles, bool* lpbSkipTriangle);
        void InsertTriangleIntoCache(BrnCrashTrianglePackedFormat* lpPackedTriangle);

    private:
        void CalculateHashForPackedTriangle(BrnCrashTrianglePackedFormat* lpPackedTriangle) const;
        bool IsDuplicateTriangle(const BrnCrashTrianglePackedFormat& lTriangle) const;
        u32 GetPackedTriangleCountForSearch() const;

        BrnCrashTrianglePackedFormat maPackedTriangles[KU_MAX_NUMBER_PACKED_TRIANGLES];
        u32 mnNumberOfPackedTriangles;
        u32 mnNextPackedTriangleToFill;
        u32 mnNextComponentToFill;
    };
}

#endif // GAMESOURCE_EFFECTS_BRNCRASHTRIANGLECACHE_H
