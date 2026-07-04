#include "GameSource/Replays/BrnReplayQuantisedVector4.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsBitStream.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"

#include <cstddef>   // offsetof

// BrnReplays::QuantisedVector4Compression, reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   ctor @ 0x8264C7E8: stores the four per-lane bit counts at +0..+3, asserts
//   lMin[i] < lMax[i] for each lane i (the X360 body computes each compare with a
//   vperm-extract of lane i from both vectors followed by vcmpgtfp comparing
//   lMax[i] > lMin[i]; the scalar reconstruction reads the named Vector4 lane), then
//   stores mMin at +16 and mMax at +32 (16-byte vector stores).
//
// The per-lane asserts cite GameSource/Replays/BrnReplayQuantisedVector4.h lines
// 132-135 in the X360 build; the CGS_ASSERT substitution carries __FILE__/__LINE__
// of this reconstruction instead (semantically equivalent -- the boot-trace milestone
// runs this ctor with valid ranges, so the asserts never fire).

namespace BrnReplays
{
    void QuantisedVector4Compression::_AssertLayout()
    {
        static_assert(offsetof(QuantisedVector4Compression, maBitCounts) == 0x00,
                      "maBitCounts @0x00");
        static_assert(offsetof(QuantisedVector4Compression, mMin) == 0x10, "mMin @0x10");
        static_assert(offsetof(QuantisedVector4Compression, mMax) == 0x20, "mMax @0x20");
        static_assert(sizeof(QuantisedVector4Compression) == 0x30,
                      "sizeof QuantisedVector4Compression == 0x30");
    }

    QuantisedVector4Compression::QuantisedVector4Compression(
        const Vector4& lrMin, const Vector4& lrMax,
        u8 luBitsX, u8 luBitsY, u8 luBitsZ, u8 luBitsW)
    {
        // +0..+3: stb r4..r7, 0..3(r31)
        maBitCounts[0] = luBitsX;
        maBitCounts[1] = luBitsY;
        maBitCounts[2] = luBitsZ;
        maBitCounts[3] = luBitsW;

        // Per-lane range sanity: lMin[i] < lMax[i] (X360: vcmpgtfp lMax[i] > lMin[i]).
        CGS_ASSERT(lrMin.x < lrMax.x, "lMin[0] < lMax[0]");
        CGS_ASSERT(lrMin.y < lrMax.y, "lMin[1] < lMax[1]");
        CGS_ASSERT(lrMin.z < lrMax.z, "lMin[2] < lMax[2]");
        CGS_ASSERT(lrMin.w < lrMax.w, "lMin[3] < lMax[3]");

        // +16 / +32: stvx128 v127(lMin)/v126(lMax)
        mMin = lrMin;
        mMax = lrMax;
    }

    // ---------------------------------------------------------------------------------------------
    // The vec4 quantiser free functions (Pack @0x8265AB58, UnPack @0x8265ACC8,
    // PrintVector4 @0x82653340). Reconstructed from BURNOUT_X360_ARTIST.XEX.
    // ---------------------------------------------------------------------------------------------

    // @0x82653340 -- log "Vec: <x> <y> <z> <w>\n" to gpDebugPrint (filtered). The vector arrives in
    // VMX v1 (vmr128 v127,v1); the Hex-Rays integer args are dead register leftovers.
    void QuantisedVector4::PrintVector4(const Vector4& lrValue)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            CgsDev::StrStreamBase& lrStream = *CgsDev::Log::gpDebugPrint;
            lrStream << "Vec: "
                     << lrValue.x << lrValue.y << lrValue.z << lrValue.w
                     << "\n";
        }
    }

    // @0x8265AB58 -- quantise lrValue through lpCompression into lpDest.
    void QuantisedVector4::Pack(void* lpDest,
                               const QuantisedVector4Compression* lpCompression,
                               const Vector4& lrValue)
    {
        const QuantisedVector4Compression& lrC = *lpCompression;

        // Range sanity: every lane of the value must lie within [mMin, mMax].
        // (X360: vmaxfp(value,mMin) / vminfp(.,mMax) / vcmpeqfp.(value,clamped) --
        // if any lane was clamped the value was out of range.)
        const bool lbInRange =
            lrValue.x >= lrC.mMin.x && lrValue.x <= lrC.mMax.x &&
            lrValue.y >= lrC.mMin.y && lrValue.y <= lrC.mMax.y &&
            lrValue.z >= lrC.mMin.z && lrValue.z <= lrC.mMax.z &&
            lrValue.w >= lrC.mMin.w && lrValue.w <= lrC.mMax.w;
        if (!lbInRange)
        {
            QuantisedVector4::PrintVector4(lrValue);
            CGS_ASSERT(lbInRange, "Unable to quantise vec4");
        }

        // Total packed length: sum the per-lane bit counts, round up to whole bytes,
        // then back to bits (8 * ((sum + 7) / 8)).
        const s32 liTotalBits = 8 * ((lrC.maBitCounts[1] + lrC.maBitCounts[3] +
                                     lrC.maBitCounts[2] + lrC.maBitCounts[0] + 7) / 8);

        CgsNetwork::BitStream lStream;
        lStream.Prepare(static_cast<u8*>(lpDest), 0, 0, liTotalBits);

        u32 luPacked;
        CgsNetwork::FloatQuantiser::Pack(lrValue.x, lrC.mMin.x, lrC.mMax.x,
                                        lrC.maBitCounts[0], &luPacked);
        lStream.AddBits(luPacked, lrC.maBitCounts[0]);

        CgsNetwork::FloatQuantiser::Pack(lrValue.y, lrC.mMin.y, lrC.mMax.y,
                                        lrC.maBitCounts[1], &luPacked);
        lStream.AddBits(luPacked, lrC.maBitCounts[1]);

        CgsNetwork::FloatQuantiser::Pack(lrValue.z, lrC.mMin.z, lrC.mMax.z,
                                        lrC.maBitCounts[2], &luPacked);
        lStream.AddBits(luPacked, lrC.maBitCounts[2]);

        CgsNetwork::FloatQuantiser::Pack(lrValue.w, lrC.mMin.w, lrC.mMax.w,
                                        lrC.maBitCounts[3], &luPacked);
        lStream.AddBits(luPacked, lrC.maBitCounts[3]);
    }

    // @0x8265ACC8 -- reconstruct the Vector4 from lpSource into lpDest; returns lpDest.
    // X360 ABI: r3=lpDest (returned), r4=lpSource (fed to BitStream::Prepare), r5=lpCompression
    // (bit counts + mMin/mMax). Argument order (dest, source, compression) is the REVERSE of Pack's.
    void* QuantisedVector4::UnPack(void* lpDest,
                                  const void* lpSource,
                                  const QuantisedVector4Compression* lpCompression)
    {
        const QuantisedVector4Compression& lrC = *lpCompression;

        const s32 liTotalBits = 8 * ((lrC.maBitCounts[1] + lrC.maBitCounts[0] +
                                     lrC.maBitCounts[3] + lrC.maBitCounts[2] + 7) / 8);

        CgsNetwork::BitStream lStream;
        lStream.Prepare(const_cast<u8*>(static_cast<const u8*>(lpSource)),
                        0, liTotalBits, liTotalBits);

        float lfX;
        float lfY;
        float lfZ;
        float lfW;

        s32 liBits = static_cast<s32>(lStream.GetBits(lrC.maBitCounts[0]));
        CgsNetwork::FloatQuantiser::UnPack(&lfX, lrC.mMin.x, lrC.mMax.x,
                                          lrC.maBitCounts[0], static_cast<u32>(liBits));

        liBits = static_cast<s32>(lStream.GetBits(lrC.maBitCounts[1]));
        CgsNetwork::FloatQuantiser::UnPack(&lfY, lrC.mMin.y, lrC.mMax.y,
                                          lrC.maBitCounts[1], static_cast<u32>(liBits));

        liBits = static_cast<s32>(lStream.GetBits(lrC.maBitCounts[2]));
        CgsNetwork::FloatQuantiser::UnPack(&lfZ, lrC.mMin.z, lrC.mMax.z,
                                          lrC.maBitCounts[2], static_cast<u32>(liBits));

        liBits = static_cast<s32>(lStream.GetBits(lrC.maBitCounts[3]));
        CgsNetwork::FloatQuantiser::UnPack(&lfW, lrC.mMin.w, lrC.mMax.w,
                                          lrC.maBitCounts[3], static_cast<u32>(liBits));

        Vector4 lValue;
        lValue.x = lfX;
        lValue.y = lfY;
        lValue.z = lfZ;
        lValue.w = lfW;

        // Range sanity on the reconstructed value (mirrors Pack's clamp check).
        const bool lbInRange =
            lValue.x >= lrC.mMin.x && lValue.x <= lrC.mMax.x &&
            lValue.y >= lrC.mMin.y && lValue.y <= lrC.mMax.y &&
            lValue.z >= lrC.mMin.z && lValue.z <= lrC.mMax.z &&
            lValue.w >= lrC.mMin.w && lValue.w <= lrC.mMax.w;
        if (!lbInRange)
        {
            QuantisedVector4::PrintVector4(lValue);
            CGS_ASSERT(lbInRange, "Unable to quantise vec4");
        }

        *static_cast<Vector4*>(lpDest) = lValue;
        return lpDest;
    }
}
