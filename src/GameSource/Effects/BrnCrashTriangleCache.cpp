#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnEffects::BrnCrashTriangleCache::Construct                  @ 0x8227B240
//   BrnEffects::BrnCrashTriangleCache::AddTriangles                @ 0x8228CDA8
//   BrnEffects::BrnCrashTriangleCache::CheckForDuplicateTriangles  @ 0x822847B0

namespace BrnEffects
{
    static const u32 KU_MAX_NUMBER_PACKED_TRIANGLES = 48;
    static const u32 KU_TRIANGLES_PER_PACK = 4;
    static const u16 KU_INVALID_CRASH_MATERIAL_ID = 0x11;

    struct Vector4
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

        Vector4 mVertexHash;
        Vector4 mVertex0X;
        Vector4 mVertex0Y;
        Vector4 mVertex0Z;
        Vector4 mVertex1X;
        Vector4 mVertex1Y;
        Vector4 mVertex1Z;
        Vector4 mVertex2X;
        Vector4 mVertex2Y;
        Vector4 mVertex2Z;
    };

    struct Triangle4
    {
        BrnCrashTrianglePackedFormat ExtractPackedTriangle(u32 luComponent) const;
        CollisionTag GetCollisionTag(u32 luComponent) const;

        Vector4 mVertex0X;
        Vector4 mVertex0Y;
        Vector4 mVertex0Z;
        Vector4 mVertex1X;
        Vector4 mVertex1Y;
        Vector4 mVertex1Z;
        Vector4 mVertex2X;
        Vector4 mVertex2Y;
        Vector4 mVertex2Z;
        Vector4 mSurfaceTags;
    };

    struct BrnCrashTriangleCache
    {
        void Construct();
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

    void BrnCrashTrianglePackedFormat::Clear()
    {
        mVertexHash.Clear();
        mVertex0X.Clear();
        mVertex0Y.Clear();
        mVertex0Z.Clear();
        mVertex1X.Clear();
        mVertex1Y.Clear();
        mVertex1Z.Clear();
        mVertex2X.Clear();
        mVertex2Y.Clear();
        mVertex2Z.Clear();
    }

    void BrnCrashTrianglePackedFormat::SetScalarTriangle(
        const BrnCrashTrianglePackedFormat& lTriangle,
        u32 luDestinationComponent)
    {
        mVertexHash.SetComponent(luDestinationComponent, lTriangle.mVertexHash.GetComponent(0));
        mVertex0X.SetComponent(luDestinationComponent, lTriangle.mVertex0X.GetComponent(0));
        mVertex0Y.SetComponent(luDestinationComponent, lTriangle.mVertex0Y.GetComponent(0));
        mVertex0Z.SetComponent(luDestinationComponent, lTriangle.mVertex0Z.GetComponent(0));
        mVertex1X.SetComponent(luDestinationComponent, lTriangle.mVertex1X.GetComponent(0));
        mVertex1Y.SetComponent(luDestinationComponent, lTriangle.mVertex1Y.GetComponent(0));
        mVertex1Z.SetComponent(luDestinationComponent, lTriangle.mVertex1Z.GetComponent(0));
        mVertex2X.SetComponent(luDestinationComponent, lTriangle.mVertex2X.GetComponent(0));
        mVertex2Y.SetComponent(luDestinationComponent, lTriangle.mVertex2Y.GetComponent(0));
        mVertex2Z.SetComponent(luDestinationComponent, lTriangle.mVertex2Z.GetComponent(0));
    }

    bool BrnCrashTrianglePackedFormat::HasMatchingHash(
        const BrnCrashTrianglePackedFormat& lTriangle,
        u32 luComponent) const
    {
        return mVertexHash.GetComponent(luComponent) == lTriangle.mVertexHash.GetComponent(0);
    }

    BrnCrashTrianglePackedFormat Triangle4::ExtractPackedTriangle(u32 luComponent) const
    {
        BrnCrashTrianglePackedFormat lPackedTriangle;
        lPackedTriangle.Clear();
        lPackedTriangle.mVertex0X.SetComponent(0, mVertex0X.GetComponent(luComponent));
        lPackedTriangle.mVertex0Y.SetComponent(0, mVertex0Y.GetComponent(luComponent));
        lPackedTriangle.mVertex0Z.SetComponent(0, mVertex0Z.GetComponent(luComponent));
        lPackedTriangle.mVertex1X.SetComponent(0, mVertex1X.GetComponent(luComponent));
        lPackedTriangle.mVertex1Y.SetComponent(0, mVertex1Y.GetComponent(luComponent));
        lPackedTriangle.mVertex1Z.SetComponent(0, mVertex1Z.GetComponent(luComponent));
        lPackedTriangle.mVertex2X.SetComponent(0, mVertex2X.GetComponent(luComponent));
        lPackedTriangle.mVertex2Y.SetComponent(0, mVertex2Y.GetComponent(luComponent));
        lPackedTriangle.mVertex2Z.SetComponent(0, mVertex2Z.GetComponent(luComponent));
        return lPackedTriangle;
    }

    CollisionTag Triangle4::GetCollisionTag(u32 luComponent) const
    {
        CollisionTag lTag;
        lTag.muValue = static_cast<u32>(mSurfaceTags.GetComponent(luComponent));
        return lTag;
    }

    void BrnCrashTriangleCache::Construct()
    {
        for (u32 lnCacheLoop = 0; lnCacheLoop < KU_MAX_NUMBER_PACKED_TRIANGLES; ++lnCacheLoop)
        {
            maPackedTriangles[lnCacheLoop].Clear();
        }

        mnNumberOfPackedTriangles = 0;
        mnNextPackedTriangleToFill = 0;
        mnNextComponentToFill = 0;
    }

    void BrnCrashTriangleCache::AddTriangles(const Triangle4* lpInTriangles, u32 lnNum4Triangles)
    {
        const Triangle4* lpIncomingTriangles = lpInTriangles;

        for (u32 lnTriangleLoop = 0; lnTriangleLoop < lnNum4Triangles; ++lnTriangleLoop)
        {
            BrnCrashTrianglePackedFormat laPackedTriangles[KU_TRIANGLES_PER_PACK];
            bool labSkipTriangle[KU_TRIANGLES_PER_PACK];

            for (u32 luComponent = 0; luComponent < KU_TRIANGLES_PER_PACK; ++luComponent)
            {
                laPackedTriangles[luComponent] = lpIncomingTriangles->ExtractPackedTriangle(luComponent);
                CalculateHashForPackedTriangle(&laPackedTriangles[luComponent]);

                const CollisionTag lCollisionTag = lpIncomingTriangles->GetCollisionTag(luComponent);
                labSkipTriangle[luComponent] =
                    lCollisionTag.IsEmpty() ||
                    (lCollisionTag.GetMaterialId() == KU_INVALID_CRASH_MATERIAL_ID);
            }

            CheckForDuplicateTriangles(laPackedTriangles, labSkipTriangle);
            ++lpIncomingTriangles;
        }
    }

    void BrnCrashTriangleCache::CheckForDuplicateTriangles(
        BrnCrashTrianglePackedFormat* lpaPackedTriangles,
        bool* lpbSkipTriangle)
    {
        for (u32 luComponent = 0; luComponent < KU_TRIANGLES_PER_PACK; ++luComponent)
        {
            if (!lpbSkipTriangle[luComponent] && !IsDuplicateTriangle(lpaPackedTriangles[luComponent]))
            {
                InsertTriangleIntoCache(&lpaPackedTriangles[luComponent]);
            }
        }
    }

    // @ 0x8227B2D0. Writes the incoming scalar triangle into the current
    // component lane of the current packed slot, then advances the fill cursors.
    // The asm copies each Vector4 field of *lpPackedTriangle into component
    // mnNextComponentToFill of maPackedTriangles[mnNextPackedTriangleToFill] using
    // a per-component vsel mask (the unk_8327F240 lookup) -- the lane-set semantic
    // of SetScalarTriangle. When the 4th component fills (wraps to 0), the slot
    // cursor advances modulo 48 and the populated-slot count saturates at 48.
    void BrnCrashTriangleCache::InsertTriangleIntoCache(
        BrnCrashTrianglePackedFormat* lpPackedTriangle)
    {
        maPackedTriangles[mnNextPackedTriangleToFill].SetScalarTriangle(
            *lpPackedTriangle, mnNextComponentToFill);

        mnNextComponentToFill = (mnNextComponentToFill + 1) & (KU_TRIANGLES_PER_PACK - 1);

        if (mnNextComponentToFill == 0)
        {
            const u32 luNumberOfPackedTriangles = mnNumberOfPackedTriangles;
            mnNextPackedTriangleToFill =
                (mnNextPackedTriangleToFill + 1) % KU_MAX_NUMBER_PACKED_TRIANGLES;

            mnNumberOfPackedTriangles =
                (luNumberOfPackedTriangles == KU_MAX_NUMBER_PACKED_TRIANGLES)
                    ? KU_MAX_NUMBER_PACKED_TRIANGLES
                    : luNumberOfPackedTriangles + 1;
        }
    }

    void BrnCrashTriangleCache::CalculateHashForPackedTriangle(
        BrnCrashTrianglePackedFormat* lpPackedTriangle) const
    {
        const f32 lfHash =
            lpPackedTriangle->mVertex0X.GetComponent(0) +
            lpPackedTriangle->mVertex0Y.GetComponent(0) +
            lpPackedTriangle->mVertex0Z.GetComponent(0) +
            lpPackedTriangle->mVertex1X.GetComponent(0) +
            lpPackedTriangle->mVertex1Y.GetComponent(0) +
            lpPackedTriangle->mVertex1Z.GetComponent(0) +
            lpPackedTriangle->mVertex2X.GetComponent(0) +
            lpPackedTriangle->mVertex2Y.GetComponent(0) +
            lpPackedTriangle->mVertex2Z.GetComponent(0);

        lpPackedTriangle->mVertexHash.SetComponent(0, lfHash);
    }

    bool BrnCrashTriangleCache::IsDuplicateTriangle(const BrnCrashTrianglePackedFormat& lTriangle) const
    {
        const u32 luPackedTriangleCount = GetPackedTriangleCountForSearch();

        for (u32 luPackedTriangle = 0; luPackedTriangle < luPackedTriangleCount; ++luPackedTriangle)
        {
            const u32 luComponentCount =
                (luPackedTriangle == mnNextPackedTriangleToFill && mnNextComponentToFill != 0)
                    ? mnNextComponentToFill
                    : KU_TRIANGLES_PER_PACK;

            for (u32 luComponent = 0; luComponent < luComponentCount; ++luComponent)
            {
                if (maPackedTriangles[luPackedTriangle].HasMatchingHash(lTriangle, luComponent))
                {
                    return true;
                }
            }
        }

        return false;
    }

    u32 BrnCrashTriangleCache::GetPackedTriangleCountForSearch() const
    {
        u32 luPackedTriangleCount = mnNumberOfPackedTriangles;

        if (mnNextComponentToFill != 0 && luPackedTriangleCount < KU_MAX_NUMBER_PACKED_TRIANGLES)
        {
            ++luPackedTriangleCount;
        }

        return luPackedTriangleCount;
    }
}
