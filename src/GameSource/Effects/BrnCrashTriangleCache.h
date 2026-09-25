#ifndef GAMESOURCE_EFFECTS_BRNCRASHTRIANGLECACHE_H
#define GAMESOURCE_EFFECTS_BRNCRASHTRIANGLECACHE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Vector3Plus (BrnCrashLineTriangleCacheFormat)

// ============================================================================
// GameSource/Effects/BrnCrashTriangleCache.h
//
// BrnEffects::BrnCrashTriangleCache -- the effects module's cache of the world
// triangles the crashing player car is grinding over (EffectsModule::
// HandlePlayerTriangleCache @0x82296EA0 feeds it from the scene manager's
// triangle-cache interface; the crash-debris integrator reads it:
// BrnDebrisArrayLite::UpdateBucket @0x82C08410 -> CollideWithTriangleCache).
//
// 2026-09-02 (tyre-mark wave): the type used to live at the top of
// BrnCrashTriangleCache.cpp. EffectsModule embeds it BY VALUE
// (DWARF EffectsModule.h:621 `BrnCrashTriangleCache mCrashTriangleCache`, X360
// +0x2D400, constructed by EffectsModule::Construct @0x8228FE98), so the
// declarations moved here. Bodies stay in the .cpp (Construct @0x8227B240,
// AddTriangles @0x8228CDA8, CheckForDuplicateTriangles @0x822847B0,
// InsertTriangleIntoCache @0x8227B2D0, CollideWithTriangleCache @0x822849B8).
//
// 2026-09-23 (crash parity FX-FX, G09-D1..D6): the incoming batch type is the REAL
// CgsGeometric::Triangle4 (0xE0 bytes, DWARF h:123, PS3 mangled
// _ZN10BrnEffects21BrnCrashTriangleCache12AddTrianglesEPKN12CgsGeometric9Triangle4Ej). The
// local 0xA0-byte `BrnEffects::Triangle4` fork (9 vertex lanes + "mSurfaceTags" at +0x90,
// which is the real mValidMasks) and its `BrnEffects::CollisionTag` helper are gone; see
// the .cpp banner for what the fork did to every batch after the first.
// ============================================================================

namespace CgsGeometric
{
    // Pointer-only use here (AddTriangles' batch array). A forward declaration keeps
    // CgsTriangle4.h out of EffectsModule.h / BrnDispatchThreadInputBuffer.h, which embed
    // this cache by value; the .cpp includes the full type.
    struct Triangle4;
}

namespace BrnEffects
{
    static const u32 KU_MAX_NUMBER_PACKED_TRIANGLES = 48;   // DWARF BrnCrashTriangleCache.h:29
    static const u32 KU_TRIANGLES_PER_PACK = 4;

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

    // DWARF BrnCrashTriangleCache.h:42-61 -- one debris line segment handed to
    // CollideWithTriangleCache (X360 stride 0x30: `addi r11, r11, 0x30` @0x82284EC8).
    // BrnDebrisArrayLite::UpdateBucket seeds mLineIntersectNormalPlusLineParms with
    // {0, 0, 0, 1.0} (0x82C087AC..0x82C087C8); CollideWithTriangleCache overwrites it with the
    // unit normal (xyz) and line parameter (w) of the nearest front-facing hit whose parameter
    // is below the current w. (The DWARF also declares an inline `Initialise(Vector3, Vector3)`
    // at h:49; no X360 ledger row attests it, so it is not declared here.)
    struct BrnCrashLineTriangleCacheFormat
    {
        Vector3     mLineStartPosition;                 // h:59  +0x00
        Vector3     mLineEndPos;                        // h:60  +0x10
        Vector3Plus mLineIntersectNormalPlusLineParms;  // h:61  +0x20  xyz = normal, w = line param
    };

    struct BrnCrashTrianglePackedFormat
    {
        void Clear();
        void SetScalarTriangle(const BrnCrashTrianglePackedFormat& lTriangle, u32 luDestinationComponent);

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

    // TRANSITIONAL (FX-FX 2026-09-23). EffectsModule::HandlePlayerTriangleCache
    // (EffectsModule.cpp:1960 / :1968 -- not this job's file) still spells its two calls as
    // `AddTriangles(reinterpret_cast<const Triangle4*>(lpCache), ...)` inside namespace
    // BrnEffects. With the 0xA0-byte fork deleted, this alias makes that cast the identity on
    // the real 0xE0-byte CgsGeometric::Triangle4 (the console passes lpCache straight through).
    // Delete it together with those two casts.
    typedef CgsGeometric::Triangle4 Triangle4;

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
        // DWARF h:123. lnNum4Triangles counts Triangle4 BATCHES (four triangles each).
        void AddTriangles(const CgsGeometric::Triangle4* lpInTriangles, u32 lnNum4Triangles);
        // DWARF h:127 -- inline on the console: the debris integrator sub_82C08410 reads the count
        // word straight (`lwz r11, 0x1E00(r18) ; cmplwi r11, 0` @0x82C0886C) before it collides.
        bool IsEmpty() const { return mnNumberOfPackedTriangles == 0; }
        // DWARF h:135 (const: PS3 _ZNK...24CollideWithTriangleCache... @0xE0ADC).
        void CollideWithTriangleCache(BrnCrashLineTriangleCacheFormat* lpLinesToTest, u32 luNumberLines) const;
        // DWARF h:155. lpbAddTriangleBool[i] == true means "do NOT add triangle i" (the console
        // seeds its found-mask with all-ones for a true byte, 0x822847D8..0x82284830); the DWARF
        // name reads the other way round.
        void CheckForDuplicateTriangles(BrnCrashTrianglePackedFormat* lpaPackedTriangles, bool* lpbAddTriangleBool);
        void InsertTriangleIntoCache(BrnCrashTrianglePackedFormat* lpPackedTriangle);   // DWARF h:150

    private:
        void CalculateHashForPackedTriangle(BrnCrashTrianglePackedFormat* lpPackedTriangle);   // DWARF h:146

        BrnCrashTrianglePackedFormat maPackedTriangles[KU_MAX_NUMBER_PACKED_TRIANGLES];   // h:158  +0x0000
        u32 mnNumberOfPackedTriangles;                                                     // h:160  +0x1E00
        u32 mnNextPackedTriangleToFill;                                                    // h:161  +0x1E04
        u32 mnNextComponentToFill;                                                         // h:162  +0x1E08
    };
}

#endif // GAMESOURCE_EFFECTS_BRNCRASHTRIANGLECACHE_H
