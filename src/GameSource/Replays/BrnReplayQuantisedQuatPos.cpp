// BrnReplays::QuantisedQuatPos -- packs/unpacks a quaternion-orientation + position pair into a
// compact 12-byte replay record.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (QuantisedQuatPos TU):
//   Pack           @0x82657C50
//   UnPack         @0x82657E50
//   ValidateQuatPos@0x82657AB0
//   PrintQuatPos   @0x82653028
// No DWARF or Feb-2007 source recovered; the surface is taken from the X360 asm and the
// TrafficEntitySerialiser / PropSerialiserFrame call sites.
//
// Working-buffer layout (8 floats): [0..3] quaternion (x,y,z,w), [4..6] position (x,y,z), [7] pad.
// Packed 12-byte record (MSB-first via CgsNetwork::BitStream):
//   pos.x 23 bits [-10000,10000], pos.y 19 bits [-1000,1000], pos.z 24 bits [-10000,10000],
//   quat.x/y/z 10 bits each [-1,1]; quat.w dropped, rederived on unpack as
//   +sqrt(1 - x^2 - y^2 - z^2) after Pack folds the w-sign so w is non-negative. 96 bits = 12 bytes.

#include "GameSource/Replays/BrnReplayQuantisedQuatPos.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsBitStream.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"

#include <cmath>

namespace BrnReplays
{
namespace
{
    // fsel-based clamp matching the X360 codegen: lo = (min-v>=0)?min:v; then
    // r = (max-lo>=0)?lo:max. Order-preserving so NaN/-0.0 behaviour matches the asm.
    static inline float FselClamp(float lfValue, float lfMin, float lfMax)
    {
        const float lfLo = ((lfMin - lfValue) >= 0.0f) ? lfMin : lfValue;
        return ((lfMax - lfLo) >= 0.0f) ? lfLo : lfMax;
    }
}

namespace QuantisedQuatPos
{
    // @ 0x82653028
    // Debug dump of a quat+pos working buffer to the engine log, gated by the message
    // filter. lpQuatPos is the 8-float buffer (quat[0..3], pos[4..6]). The X360 streams
    // the position and quaternion through the StrStreamBase vector operator<< overloads
    // (VMX lvx128 of a padded 4-lane copy); reconstructed here as the equivalent per-
    // component stream chain (mirrors BrnBehaviourRenderMetrics). liUnused is dead.
    void PrintQuatPos(void* /*lpContext*/, const float* lpQuatPos, int /*liUnused*/)
    {
        const float lfPosX = lpQuatPos[4];
        const float lfPosY = lpQuatPos[5];
        const float lfPosZ = lpQuatPos[6];

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            CgsDev::StrStreamBase& lrPrint = *CgsDev::Log::gpDebugPrint;
            lrPrint << "Pos: ";
            lrPrint << lfPosX;
            lrPrint << lfPosY;
            lrPrint << lfPosZ;
            lrPrint << " Quaternion: ";
            lrPrint << lpQuatPos[0];
            lrPrint << lpQuatPos[1];
            lrPrint << lpQuatPos[2];
            lrPrint << lpQuatPos[3];
            lrPrint << "\n";
        }
    }

    // @ 0x82657AB0
    // Range-check a quat+pos working buffer (quat[0..3], pos[4..6]): every field must
    // already equal its clamp to the packed range (pos.x/z +/-10000, pos.y +/-1000,
    // quat +/-1). A mismatch means the value would not survive quantisation -> dump it
    // and assert. lpContext is forwarded to the debug dump. Debug-only validation.
    void ValidateQuatPos(void* lpContext, const float* lpQuatPos)
    {
        const float lfPosX = lpQuatPos[4];
        const float lfPosY = lpQuatPos[5];
        const float lfPosZ = lpQuatPos[6];

        const float lfClampPosX = FselClamp(lfPosX, -10000.0f, 10000.0f);
        const float lfClampPosY = FselClamp(lfPosY, -1000.0f, 1000.0f);
        const float lfClampPosZ = FselClamp(lfPosZ, -10000.0f, 10000.0f);

        if (lfPosX != lfClampPosX || lfPosY != lfClampPosY || lfPosZ != lfClampPosZ)
        {
            PrintQuatPos(lpContext, lpQuatPos);
            CGS_ASSERT(false, "Bad QuatPos\n");
        }

        const float lfQx = lpQuatPos[0];
        const float lfQy = lpQuatPos[1];
        const float lfQz = lpQuatPos[2];
        const float lfQw = lpQuatPos[3];

        const float lfClampQx = FselClamp(lfQx, -1.0f, 1.0f);
        const float lfClampQy = FselClamp(lfQy, -1.0f, 1.0f);
        const float lfClampQz = FselClamp(lfQz, -1.0f, 1.0f);
        const float lfClampQw = FselClamp(lfQw, -1.0f, 1.0f);

        if (lfQx != lfClampQx || lfQy != lfClampQy || lfQz != lfClampQz || lfQw != lfClampQw)
        {
            PrintQuatPos(lpContext, lpQuatPos);
            CGS_ASSERT(false, "Bad QuatPos\n");
        }
    }

    // @ 0x82657C50
    // Quantise the source quaternion+position (8-float working buffer: quat[0..3],
    // pos[4..6]) into the 12-byte record lpDest12. The quaternion is normalised; the
    // w-sign is folded (w forced non-negative so w can be dropped and rederived on
    // unpack), then each field is bit-quantised into its packed range: pos.x/z 23/24
    // bits over +/-10000, pos.y 19 bits over +/-1000, quat.x/y/z 10 bits over +/-1.
    void Pack(void* lpDest12, const float* lpSource)
    {
        const float lfQx = lpSource[0];
        const float lfQy = lpSource[1];
        const float lfQz = lpSource[2];
        const float lfQw = lpSource[3];

        const float lfMagnitude = std::sqrt((lfQx * lfQx) + (lfQy * lfQy)
                                          + (lfQz * lfQz) + (lfQw * lfQw));
        CGS_ASSERT(std::fabs(lfMagnitude - 1.0f) < 0.0001f,
                   "RwMathFPU::IsSimilar( RwMathFPU::Magnitude( lQuatPos.GetQuaternion() ), 1.0f, 0.0001f )");

        // Fold the w-sign: if w < 0, negate the imaginary lanes so w is non-negative
        // and can be reconstructed as +sqrt(1 - x^2 - y^2 - z^2) on unpack.
        float lfPackQx = lfQx;
        float lfPackQy = lfQy;
        float lfPackQz = lfQz;
        if (lfQw < 0.0f)
        {
            lfPackQx = -lfPackQx;
            lfPackQy = -lfPackQy;
            lfPackQz = -lfPackQz;
        }

        CgsNetwork::BitStream lStream;
        lStream.Prepare(static_cast<u8*>(lpDest12), 0, 0, 96);

        ValidateQuatPos(lpDest12, lpSource);

        u32 luPacked;
        CgsNetwork::FloatQuantiser::Pack(lpSource[4], -10000.0f, 10000.0f, 23, &luPacked);
        lStream.AddBits(luPacked, 23);
        CgsNetwork::FloatQuantiser::Pack(lpSource[5], -1000.0f, 1000.0f, 19, &luPacked);
        lStream.AddBits(luPacked, 19);
        CgsNetwork::FloatQuantiser::Pack(lpSource[6], -10000.0f, 10000.0f, 24, &luPacked);
        lStream.AddBits(luPacked, 24);
        CgsNetwork::FloatQuantiser::Pack(lfPackQx, -1.0f, 1.0f, 10, &luPacked);
        lStream.AddBits(luPacked, 10);
        CgsNetwork::FloatQuantiser::Pack(lfPackQy, -1.0f, 1.0f, 10, &luPacked);
        lStream.AddBits(luPacked, 10);
        CgsNetwork::FloatQuantiser::Pack(lfPackQz, -1.0f, 1.0f, 10, &luPacked);
        lStream.AddBits(luPacked, 10);
    }

    // @ 0x82657E50
    // Expand the 12-byte quantised record lpSource12 into the 8-float working buffer
    // lpDest32 (quat[0..3], pos[4..6]). The dropped quaternion w is rederived as
    // +sqrt(1 - x^2 - y^2 - z^2) (Pack forced w non-negative). Returns lpDest32.
    float* UnPack(void* lpDest32, const void* lpSource12)
    {
        float* lpfDest = static_cast<float*>(lpDest32);

        CgsNetwork::BitStream lStream;
        lStream.Prepare(static_cast<u8*>(const_cast<void*>(lpSource12)), 0, 96, 96);

        float lfPosX;
        float lfPosY;
        float lfPosZ;
        float lfQx;
        float lfQy;
        float lfQz;

        u32 luBits;
        luBits = static_cast<u32>(lStream.GetBits(23));
        CgsNetwork::FloatQuantiser::UnPack(&lfPosX, -10000.0f, 10000.0f, 23, luBits);
        luBits = static_cast<u32>(lStream.GetBits(19));
        CgsNetwork::FloatQuantiser::UnPack(&lfPosY, -1000.0f, 1000.0f, 19, luBits);
        luBits = static_cast<u32>(lStream.GetBits(24));
        CgsNetwork::FloatQuantiser::UnPack(&lfPosZ, -10000.0f, 10000.0f, 24, luBits);
        luBits = static_cast<u32>(lStream.GetBits(10));
        CgsNetwork::FloatQuantiser::UnPack(&lfQx, -1.0f, 1.0f, 10, luBits);
        luBits = static_cast<u32>(lStream.GetBits(10));
        CgsNetwork::FloatQuantiser::UnPack(&lfQy, -1.0f, 1.0f, 10, luBits);
        luBits = static_cast<u32>(lStream.GetBits(10));
        CgsNetwork::FloatQuantiser::UnPack(&lfQz, -1.0f, 1.0f, 10, luBits);

        lpfDest[1] = lfQy;
        lpfDest[2] = lfQz;
        lpfDest[0] = lfQx;
        lpfDest[3] = std::sqrt(1.0f - ((lfQx * lfQx) + (lfQy * lfQy) + (lfQz * lfQz)));
        lpfDest[4] = lfPosX;
        lpfDest[5] = lfPosY;
        lpfDest[6] = lfPosZ;

        ValidateQuatPos(const_cast<void*>(lpSource12), lpfDest);
        return lpfDest;
    }
}
}
