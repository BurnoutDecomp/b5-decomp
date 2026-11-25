#include "CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsSmartBitStream.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsIntQuantiser.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDConvertToString
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (streamed assert)
#include "rw/math/vpu/vector3_operation.h"                              // Min / Max / Magnitude

#include <cmath>   // native CRT sin/cos
#include <xtl.h>
#include <xboxmath.h> // original XMVectorASin/ATan and native VMX intrinsics

// CgsNetwork::Message  -- the reconstructed functions of the
// class:CgsNetwork::Message translation unit.
//
//   GetGameID            @ 0x8286F3E8
//   Pack                 @ 0x82880160
//   PrepareAck           @ 0x8286F488
//   PrepareForSend       @ 0x8257D5F8
//   PrepareNack          @ 0x8286F508
//   SetType              @ 0x82579C30
//   UnPack               @ 0x828801F0
//   Construct            @ 0x82870B90
//   GetVectorFromAngles  @ 0x82870D78
//   PackOrUnpack         @ 0x82880F50
//   PackOrUnpackBuffer   @ 0x8288EA70
//   PackOrUnpack()       (base virtual, folded `return 0` leaf)
//   GetPackedMessageSize
//
// All offsets/stores are store-for-store faithful to the X360 pseudocode + asm.
// Members are referenced by name; asserts use the house CGS_ASSERT machinery.

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network/Packeting/Messages/CgsMessage.h".

namespace CgsNetwork
{
    // Largest packed message in bytes (file-scope in this TU).
    const s32 KI_MAX_PACKED_MESSAGE_SIZE = 1400;

    namespace
    {
        const char KAC_ARTIST_FILE[] =
            "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\Messages\\CgsMessage.cpp";

        const f32 KF_ANGLE_PI      = 3.14159274f;
        const f32 KF_ANGLE_HALF_PI = 1.57079637f;

        // A vector shorter than this has no direction worth encoding.
        const f32 KF_MIN_ANGLES_MAGNITUDE = 1.52587890625e-05f;   // 2^-16

        // Keep the native fmadds boundary out of line: /O2 otherwise reassociates
        // the rounded addend product into a different fused operation.
        __declspec(noinline) f32 MultiplyAdd(f32 lfA, f32 lfB, f32 lfAddend)
        {
            return lfA * lfB + lfAddend;
        }

        // ARTIST 82881748..828817E4 and 82881B40..82881BB0: one reciprocal
        // refinement, the original XboxMath arctangent, then VMX quadrant selects.
        // This differs from XMVectorATan2's extra refinement/special-case policy.
        XMVECTOR ATan2(XMVECTOR lvNumerator, XMVECTOR lvDenominator)
        {
            const XMVECTOR lvZero = __vspltisw(0);
            const XMVECTOR lvOne = XMVectorReplicate(1.0f);
            const XMVECTOR lvTrue = __vspltisw(-1);
            const XMVECTOR lvSignMask = __vslw(lvTrue, lvTrue);
            XMVECTOR lvReciprocal = __vrefp(lvDenominator);
            const XMVECTOR lvError = __vnmsubfp(lvReciprocal, lvDenominator, lvOne);
            lvReciprocal = __vmaddfp(lvReciprocal, lvError, lvReciprocal);
            const XMVECTOR lvRatio = __vmaddfp(lvNumerator, lvReciprocal, lvZero);
            const XMVECTOR lvAngle = XMVectorATan(lvRatio);
            const XMVECTOR lvSign = __vand(lvNumerator, lvSignMask);
            const XMVECTOR lvPi = __vor(XMVectorReplicate(KF_ANGLE_PI), lvSign);
            const XMVECTOR lvHalfPi = __vor(XMVectorReplicate(KF_ANGLE_HALF_PI), lvSign);
            const XMVECTOR lvNegative = __vcmpgtfp(lvZero, lvDenominator);
            const XMVECTOR lvIsZero = __vcmpeqfp(lvZero, lvDenominator);
            const XMVECTOR lvQuadrant = __vsel(lvAngle, __vaddfp(lvPi, lvAngle), lvNegative);
            return __vsel(lvQuadrant, lvHalfPi, lvIsZero);
        }

        // ARTIST 828819E4..82881A3C / 82881AB4..82881AE8 performs TWO Newton
        // steps. SDK6534's XMVector3Length/Normalize performs only one.
        XMVECTOR ReciprocalSqrt(XMVECTOR lvLengthSquared)
        {
            const XMVECTOR lvOne = XMVectorReplicate(1.0f);
            const XMVECTOR lvHalf = XMVectorReplicate(0.5f);
            XMVECTOR lvEstimate = __vrsqrtefp(lvLengthSquared);
            XMVECTOR lvSquared = __vmulfp(lvEstimate, lvEstimate);
            XMVECTOR lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
            XMVECTOR lvError = __vnmsubfp(lvLengthSquared, lvSquared, lvOne);
            lvEstimate = __vmaddfp(lvHalfEstimate, lvError, lvEstimate);
            lvSquared = __vmulfp(lvEstimate, lvEstimate);
            lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
            lvError = __vnmsubfp(lvLengthSquared, lvSquared, lvOne);
            return __vmaddfp(lvHalfEstimate, lvError, lvEstimate);
        }

        // Time between two frames at each rate, and the wrap of the 16-bit frame counter.
        const f32 KF_TIME_STEP_50HZ   = 0.0199999996f;
        const f32 KF_TIME_STEP_60HZ   = 0.0166666675f;
        const s32 KI_FRAMES_PER_WRAP  = 0xFFFF;
        const s64 KI_HALF_WRAP_FRAMES = 0x8000;

        // The float reference is consulted only while the frames are this close to each
        // other (in either direction).
        const u16 KU16_TIME_DIFF_CHECK_FRAMES = 0x5554;

        // The frame distance the diagnostic reports alongside the arguments.
        const s32 KI_TIME_DIFF_REPORTED_FRAME_RANGE = 0x5555;

        // The float reference drifts with the session length: the allowed error grows
        // from 33 and from 242 wraps on.
        const s32 KI_TIME_DIFF_SHORT_SESSION_WRAPS  = 33;
        const s32 KI_TIME_DIFF_MEDIUM_SESSION_WRAPS = 242;
        const f32 KF_TIME_DIFF_SHORT_SESSION_ERROR  = 0.0299999993f;
        const f32 KF_TIME_DIFF_MEDIUM_SESSION_ERROR = 0.0500000007f;
        const f32 KF_TIME_DIFF_LONG_SESSION_ERROR   = 0.0700000003f;
    }

    // DecFIGS CgsMessage.cpp:355 and ARTIST 82870B90.
    void Message::Construct()
    {
        mu8GameID = KU8_INVALID_GAME_ID;
        mi8Type = static_cast<s8>(-1);
        mx8Flags = 0;
        mu16Frame = KU16_INVALID_FRAME;
    }

    // ---- GetVectorFromAngles @ 0x82870D78 --------------------------------------
    // Builds a scaled direction vector from two angles. The X360 build evaluates
    // sinf/cosf of the two angle args (the asm calls sin/cos on doubles and frsps
    // each result back to f32), assembles the unit direction
    //   ( cos(B)*cos(A), sin(B), cos(B)*sin(A), 0 )
    // on the stack, splats the magnitude across all four lanes, multiplies
    // (vmulfp128) and stores the 16-byte result through the output pointer. The
    // final scale uses the original native VMX multiply, preserving all four lanes
    // (lane0=x .. lane3=w), including the zero fourth lane.
    void Message::GetVectorFromAngles(rw::math::vpu::Vector3* lpvOut,
                                      f32 lfAngleA, f32 lfAngleB, f32 lfMagnitude)
    {
        const f32 lfSinB = static_cast<f32>(sin(static_cast<double>(lfAngleB)));
        const f32 lfSinA = static_cast<f32>(sin(static_cast<double>(lfAngleA)));
        const f32 lfCosBForZ = static_cast<f32>(cos(static_cast<double>(lfAngleB)));
        const f32 lfZ = lfCosBForZ * lfSinA;
        const f32 lfCosA = static_cast<f32>(cos(static_cast<double>(lfAngleA)));
        const f32 lfCosBForX = static_cast<f32>(cos(static_cast<double>(lfAngleB)));
        const XMVECTOR lvDirection = { lfCosBForX * lfCosA, lfSinB, lfZ, 0.0f };
        __stvx(__vmulfp(lvDirection, XMVectorReplicate(lfMagnitude)), lpvOut, 0);
    }

    // ---- SetMatrixFromEulerAngles ---------------------------------------------------
    // Rotation rows from the negated roll / pitch / yaw (cos and sin in double, rounded to
    // single). Only the x, y and z lanes of the three rotation rows are written; the
    // translation row and the w lanes keep their values. The console fuses the four
    // multiply-adds; MultiplyAdd preserves their native scalar fmadds boundary.
    void Message::SetMatrixFromEulerAngles(rw::math::vpu::Matrix44Affine* lpMatrix,
                                           f32 lfRoll, f32 lfPitch, f32 lfYaw)
    {
        const f32 lfCosRoll  = static_cast<f32>(cos(static_cast<double>(-lfRoll)));
        const f32 lfSinRoll  = static_cast<f32>(sin(static_cast<double>(-lfRoll)));
        const f32 lfCosPitch = static_cast<f32>(cos(static_cast<double>(-lfPitch)));
        const f32 lfSinPitch = static_cast<f32>(sin(static_cast<double>(-lfPitch)));
        const f32 lfCosYaw   = static_cast<f32>(cos(static_cast<double>(-lfYaw)));
        const f32 lfSinYaw   = static_cast<f32>(sin(static_cast<double>(-lfYaw)));

        const f32 lfCosYawSinPitch = lfCosYaw * lfSinPitch;
        const f32 lfSinYawSinPitch = lfSinYaw * lfSinPitch;

        lpMatrix->xAxis.mV.x = MultiplyAdd(lfSinYawSinPitch, lfSinRoll, lfCosYaw * lfCosRoll);
        lpMatrix->xAxis.mV.y = lfCosPitch * lfSinRoll;
        lpMatrix->xAxis.mV.z = MultiplyAdd(lfCosYawSinPitch, lfSinRoll, -(lfSinYaw * lfCosRoll));

        lpMatrix->yAxis.mV.x = MultiplyAdd(lfSinYawSinPitch, lfCosRoll, -(lfCosYaw * lfSinRoll));
        lpMatrix->yAxis.mV.y = lfCosPitch * lfCosRoll;
        lpMatrix->yAxis.mV.z = MultiplyAdd(lfCosYawSinPitch, lfCosRoll, lfSinYaw * lfSinRoll);

        lpMatrix->zAxis.mV.x = lfSinYaw * lfCosPitch;
        lpMatrix->zAxis.mV.y = -lfSinPitch;
        lpMatrix->zAxis.mV.z = lfCosYaw * lfCosPitch;
    }

    // ---- quantised-integer field primitives --------------------------------------
    // Every integer PackOrUnpack overload has the same shape: the lifecycle word picks
    // the direction. Packing (0) quantises the widened field into [liMin, liMax] with
    // IntQuantiser::Pack and appends the reported number of bits (a full stream reports
    // KX_PACK_FAILED_NO_SPACE); unpacking (1) reads the value back through the
    // SmartBitStream quantised-int reader and narrows it into the field; any other state fires
    // the "called without telling it which it is doing" assert and reports success.
    // The overloads differ only in the field width and the widening (sign- or
    // zero-extension) of the value handed to the quantiser.

    // int8_t field.
    PackOrUnpackResult Message::PackOrUnpack(s8* lpi8Field, s32 liMin, s32 liMax)
    {
        switch (mePackOrUnpack)
        {
        case E_PACK_INTO_BITSTREAM:
        {
            u32 luPacked;
            s32 liNumBits;
            IntQuantiser::Pack(*lpi8Field, liMin, liMax, &luPacked, &liNumBits);
            if (!mBitstream.AddBits(luPacked, liNumBits))
                return KX_PACK_FAILED_NO_SPACE;
            break;
        }
        case E_UNPACK_FROM_BITSTREAM:
            *lpi8Field = static_cast<s8>(mBitstream.GetQuantisedInt(liMin, liMax));
            break;
        default:
            CGS_ASSERT_STREAM_AT(false, "CgsNetwork::Message::PackOrUnpack"
                                         << " called without telling it which it is doing\n",
                                 KAC_ARTIST_FILE, 518);
            break;
        }
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // uint8_t field.
    PackOrUnpackResult Message::PackOrUnpack(u8* lpu8Field, s32 liMin, s32 liMax)
    {
        switch (mePackOrUnpack)
        {
        case E_PACK_INTO_BITSTREAM:
        {
            u32 luPacked;
            s32 liNumBits;
            IntQuantiser::Pack(*lpu8Field, liMin, liMax, &luPacked, &liNumBits);
            if (!mBitstream.AddBits(luPacked, liNumBits))
                return KX_PACK_FAILED_NO_SPACE;
            break;
        }
        case E_UNPACK_FROM_BITSTREAM:
            *lpu8Field = static_cast<u8>(mBitstream.GetQuantisedInt(liMin, liMax));
            break;
        default:
            CGS_ASSERT_STREAM_AT(false, "CgsNetwork::Message::PackOrUnpack"
                                         << " called without telling it which it is doing\n",
                                 KAC_ARTIST_FILE, 556);
            break;
        }
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // DecFIGS CgsMessage.cpp:576, :617. ARTIST 82881198 and 82881250.
    PackOrUnpackResult Message::PackOrUnpack(s16* lps16Field, s32 liMin, s32 liMax)
    {
        switch (mePackOrUnpack)
        {
        case E_PACK_INTO_BITSTREAM:
        {
            u32 luPacked;
            s32 liNumBits;
            IntQuantiser::Pack(*lps16Field, liMin, liMax, &luPacked, &liNumBits);
            if (!mBitstream.AddBits(luPacked, liNumBits))
                return KX_PACK_FAILED_NO_SPACE;
            break;
        }
        case E_UNPACK_FROM_BITSTREAM:
            *lps16Field = static_cast<s16>(mBitstream.GetQuantisedInt(liMin, liMax));
            break;
        default:
            CGS_ASSERT_AT(false, "PackOrUnpack called without telling it which operation to do\n",
                          KAC_ARTIST_FILE, 595);
            break;
        }
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    PackOrUnpackResult Message::PackOrUnpack(u16* lpu16Field, s32 liMin, s32 liMax)
    {
        switch (mePackOrUnpack)
        {
        case E_PACK_INTO_BITSTREAM:
        {
            u32 luPacked;
            s32 liNumBits;
            IntQuantiser::Pack(*lpu16Field, liMin, liMax, &luPacked, &liNumBits);
            if (!mBitstream.AddBits(luPacked, liNumBits))
                return KX_PACK_FAILED_NO_SPACE;
            break;
        }
        case E_UNPACK_FROM_BITSTREAM:
            *lpu16Field = static_cast<u16>(mBitstream.GetQuantisedInt(liMin, liMax));
            break;
        default:
            CGS_ASSERT_STREAM_AT(false, "CgsNetwork::Message::PackOrUnpack"
                                         << " called without telling it which it is doing\n",
                                 KAC_ARTIST_FILE, 635);
            break;
        }
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // int32_t field.
    PackOrUnpackResult Message::PackOrUnpack(s32* lpiField, s32 liMin, s32 liMax)
    {
        switch (mePackOrUnpack)
        {
        case E_PACK_INTO_BITSTREAM:
        {
            u32 luPacked;
            s32 liNumBits;
            IntQuantiser::Pack(*lpiField, liMin, liMax, &luPacked, &liNumBits);
            if (!mBitstream.AddBits(luPacked, liNumBits))
                return KX_PACK_FAILED_NO_SPACE;
            break;
        }
        case E_UNPACK_FROM_BITSTREAM:
            *lpiField = static_cast<s32>(mBitstream.GetQuantisedInt(liMin, liMax));
            break;
        default:
            CGS_ASSERT_STREAM_AT(false, "CgsNetwork::Message::PackOrUnpack"
                                         << " called without telling it which it is doing\n",
                                 KAC_ARTIST_FILE, 674);
            break;
        }
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ARTIST 0x82881BF0. This overload also admits the invalid ID sentinel.
    PackOrUnpackResult Message::PackOrUnpack(NetworkPlayerID* lpNetworkPlayerID)
    {
        return PackOrUnpack(lpNetworkPlayerID, K_INVALID_PLAYER_ID, 0x7FFFFFFF);
    }

    // uint32_t field (the word goes to the quantiser unchanged).
    PackOrUnpackResult PackOrUnpackUInt(Message* lpMessage, u32* lpu32Field, s32 liMin, s32 liMax)
    {
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(static_cast<s32>(*lpu32Field), liMin, liMax, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            *lpu32Field = static_cast<u32>(lpMessage->mBitstream.GetQuantisedInt(liMin, liMax));
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // bool field: travels as a u8 in [0, 1]. When packing, the flag is normalised into
    // the temporary first; when unpacking, the field becomes (value == 1). The
    // lifecycle word is re-read after the u8 call.
    PackOrUnpackResult Message::PackOrUnpack(bool* lpbField)
    {
        u8 lu8Value = 0;   // left unset by the console when the lifecycle word is invalid
        if (mePackOrUnpack == E_PACK_INTO_BITSTREAM)
        {
            lu8Value = (*lpbField != false) ? 1 : 0;
        }

        const PackOrUnpackResult lxResult = PackOrUnpack(&lu8Value, 0, 1);

        if (mePackOrUnpack != E_PACK_INTO_BITSTREAM)
        {
            *lpbField = (lu8Value == 1);
        }
        return lxResult;
    }

    // ---- CgsID field --------------------------------------------------------------
    // A CgsID travels as its low then its high 32-bit half, each a full-range int. Both
    // directions also render the id to text (the result is unused); packing first checks
    // the halves recombine to the id.
    PackOrUnpackResult Message::PackOrUnpack(CgsID* lpu64Field)
    {
        const s32 KI_MIN_INT32 = -0x7FFFFFFF - 1;
        const s32 KI_MAX_INT32 = 0x7FFFFFFF;
        char lacIDString[KI_CGSID_STRING_LEN];

        if (mePackOrUnpack == E_PACK_INTO_BITSTREAM)
        {
            CgsIDConvertToString(*lpu64Field, lacIDString);

            const u64 liLow  = *lpu64Field & 0xFFFFFFFFull;
            const u64 liHigh = *lpu64Field >> 32;
            CGS_ASSERT(*lpu64Field == (liLow | (liHigh << 32)),
                       "*lpValue == (liLow | (liHigh << 32))");

            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(static_cast<s32>(liLow), KI_MIN_INT32, KI_MAX_INT32, &luPacked, &liNumBits);
            if (!mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            IntQuantiser::Pack(static_cast<s32>(liHigh), KI_MIN_INT32, KI_MAX_INT32, &luPacked, &liNumBits);
            return mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (mePackOrUnpack == E_UNPACK_FROM_BITSTREAM)
        {
            const s32 liLow  = mBitstream.GetQuantisedInt(KI_MIN_INT32, KI_MAX_INT32);
            const s32 liHigh = mBitstream.GetQuantisedInt(KI_MIN_INT32, KI_MAX_INT32);
            *lpu64Field = (static_cast<u64>(static_cast<u32>(liHigh)) << 32) | static_cast<u32>(liLow);
            CgsIDConvertToString(*lpu64Field, lacIDString);
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- float field, quantised to a bit count ---------------------------------------
    PackOrUnpackResult PackOrUnpackFloat(Message* lpMessage, f32* lpfField, f32 lfMin, f32 lfMax, s32 liNumBits)
    {
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked = 0;
            FloatQuantiser::Pack(*lpfField, lfMin, lfMax, liNumBits, &luPacked);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            lpMessage->mBitstream.GetQuantisedFloat(lpfField, lfMin, lfMax, liNumBits);
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- float field, quantised to a resolution --------------------------------------
    PackOrUnpackResult PackOrUnpackFloat(Message* lpMessage, f32* lpfField, f32 lfMin, f32 lfMax, f32 lfResolution)
    {
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            FloatQuantiser::Pack(*lpfField, lfMin, lfMax, lfResolution, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            lpMessage->mBitstream.GetQuantisedFloat(lpfField, lfMin, lfMax, lfResolution);
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- Time field ----------------------------------------------------------------
    // Whole seconds as an int in [liMinSeconds, liMaxSeconds - 1], then the fraction in
    // [0, 1] at lfResolution. An unpacked fraction that reaches 1 is pulled back one step
    // (1 - resolution). Both directions log the time to the debug print.
    PackOrUnpackResult PackOrUnpackTime(Message* lpMessage, CgsSystem::Time* lpTimeField,
                                        s32 liMinSeconds, s32 liMaxSeconds, f32 lfResolution)
    {
        const f32 KF_MIN_FRACTION = 0.0f;
        const f32 KF_MAX_FRACTION = 1.0f;

        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(lpTimeField->GetSeconds(), liMinSeconds, liMaxSeconds - 1, &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lpTimeField->GetFraction(), KF_MIN_FRACTION, KF_MAX_FRACTION, lfResolution,
                                 &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            const f32 lfTime = static_cast<f32>(lpTimeField->GetSeconds()) + lpTimeField->GetFraction();
            *CgsDev::Log::gpDebugPrint << "Packed Time " << lfTime << "\n";
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            s32 liSeconds = 0;
            lpMessage->mBitstream.GetQuantisedInt(&liSeconds, liMinSeconds, liMaxSeconds - 1);

            f32 lfFraction = 0.0f;
            lpMessage->mBitstream.GetQuantisedFloat(&lfFraction, KF_MIN_FRACTION, KF_MAX_FRACTION, lfResolution);
            if (!(lfFraction < KF_MAX_FRACTION))
            {
                lfFraction = KF_MAX_FRACTION - lfResolution;
            }

            lpTimeField->SetSeconds(liSeconds);
            lpTimeField->SetFraction(lfFraction);

            const f32 lfTime = lpTimeField->GetFraction() + static_cast<f32>(lpTimeField->GetSeconds());
            *CgsDev::Log::gpDebugPrint << "Unpacked Time " << lfTime << "\n";
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // Seconds in [0, INT_MAX - 1].
    PackOrUnpackResult PackOrUnpackTime(Message* lpMessage, CgsSystem::Time* lpTimeField, f32 lfResolution)
    {
        return PackOrUnpackTime(lpMessage, lpTimeField, 0, 0x7FFFFFFF, lfResolution);
    }

    // ---- DateAndTime field -----------------------------------------------------------
    // Second and minute in [0, 59], hour in [0, 23], day in [1, 31], month in [1, 12] and
    // year in [0, 5000], in that order. A pack stops at the first field that does not fit.
    PackOrUnpackResult PackOrUnpackDateAndTime(Message* lpMessage, CgsSystem::DateAndTime* lpDateAndTime)
    {
        const s32 KI_MAX_SECOND = 59;
        const s32 KI_MAX_MINUTE = 59;
        const s32 KI_MAX_HOUR   = 23;
        const s32 KI_MIN_DAY    = 1;
        const s32 KI_MAX_DAY    = 31;
        const s32 KI_MIN_MONTH  = 1;
        const s32 KI_MAX_MONTH  = 12;
        const s32 KI_MAX_YEAR   = 5000;

        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;

            IntQuantiser::Pack(lpDateAndTime->GetSecond(), 0, KI_MAX_SECOND, &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            IntQuantiser::Pack(lpDateAndTime->GetMinute(), 0, KI_MAX_MINUTE, &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            IntQuantiser::Pack(lpDateAndTime->GetHour(), 0, KI_MAX_HOUR, &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            IntQuantiser::Pack(lpDateAndTime->GetDay(), KI_MIN_DAY, KI_MAX_DAY, &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            IntQuantiser::Pack(lpDateAndTime->GetMonth(), KI_MIN_MONTH, KI_MAX_MONTH, &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            IntQuantiser::Pack(lpDateAndTime->GetYear(), 0, KI_MAX_YEAR, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            SmartBitStream& lrStream = lpMessage->mBitstream;
            const s32 liSecond = lrStream.GetQuantisedInt(0, KI_MAX_SECOND);
            const s32 liMinute = lrStream.GetQuantisedInt(0, KI_MAX_MINUTE);
            const s32 liHour   = lrStream.GetQuantisedInt(0, KI_MAX_HOUR);
            const s32 liDay    = lrStream.GetQuantisedInt(KI_MIN_DAY, KI_MAX_DAY);
            const s32 liMonth  = lrStream.GetQuantisedInt(KI_MIN_MONTH, KI_MAX_MONTH);
            const s32 liYear   = lrStream.GetQuantisedInt(0, KI_MAX_YEAR);

            lpDateAndTime->SetSecond(liSecond);
            lpDateAndTime->SetMinute(liMinute);
            lpDateAndTime->SetHour(liHour);
            lpDateAndTime->SetDay(liDay);
            lpDateAndTime->SetMonth(liMonth);
            lpDateAndTime->SetYear(liYear);
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- PackOrUnpackBuffer --------------------------------------------------------
    // (De)serialise a raw byte buffer through the message's SmartBitStream
    // (mBitstream, +0x08): 0 packs (AddRawData; a full stream reports
    // KX_PACK_FAILED_NO_SPACE), 1 unpacks (GetRawData), any other state fires the
    // "called without telling it which it is doing" assert.
    PackOrUnpackResult Message::PackOrUnpackBuffer(char* lpcBuffer, s32 liNumBytes)
    {
        switch (mePackOrUnpack)
        {
        case E_PACK_INTO_BITSTREAM:
            if (!mBitstream.AddRawData(lpcBuffer, liNumBytes))
                return KX_PACK_FAILED_NO_SPACE;
            break;
        case E_UNPACK_FROM_BITSTREAM:
            mBitstream.GetRawData(lpcBuffer, liNumBytes);
            break;
        default:
            CGS_ASSERT_STREAM_AT(false, "CgsNetwork::Message::PackOrUnpackBuffer"
                                         << " called without telling it which it is doing\n",
                                 KAC_ARTIST_FILE, 1417);
            break;
        }
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- GetGameID @ 0x8286F3E8 ------------------------------------------------
    // The X360 build, when the game-id is still invalid, streams a diagnostic line
    // ("We have received a message with GameID -1 ...") into an ostream-like sink
    // (off_82F335C8) and then asserts mu8GameID != KU8_INVALID_GAME_ID. The sink is
    // an un-homed global stream object that this TU cannot model by name, so the
    // logging side-channel is reduced to the assert it guards (the assert is the
    // observable contract). FLAGGED: the off_82F335C8 stream-logging side effect is
    // dropped (un-homed data-global), the assert is preserved.
    u8 Message::GetGameID() const
    {
        CGS_ASSERT(mu8GameID != KU8_INVALID_GAME_ID,
                   "mu8GameID != KU8_INVALID_GAME_ID");
        return mu8GameID;
    }

    // ---- SetType @ 0x82579C30 --------------------------------------------------
    void Message::SetType(::EMessageType leType)
    {
        static const char KAC_ARTIST_FILE[] =
            "..\\..\\..\\GameShared\\GameClasses\\Network/Packeting/Messages/CgsMessage.h";
        CGS_ASSERT_AT(leType >= 0, "leType >= 0", KAC_ARTIST_FILE, 633);
        CGS_ASSERT_AT(leType < E_MESSAGE_TYPE_COUNT, "leType < E_MESSAGE_TYPE_COUNT", KAC_ARTIST_FILE, 634);
        CGS_ASSERT_AT(leType < 255, "leType < 255", KAC_ARTIST_FILE, 635);
        mi8Type = static_cast<s8>(leType);
    }

    // ---- PrepareForSend @ 0x8257D5F8 ------------------------------------------
    void Message::PrepareForSend(s32 leType, u16 lu16Frame)
    {
        CGS_ASSERT(lu16Frame != KU16_INVALID_FRAME,
            CGS_ASSERT_MESSAGE_AT(
                "lu16Frame != KU16_INVALID_FRAME", __FILE__, 594 /* ARTIST 0x8257D624 */));
        SetType(static_cast<::EMessageType>(leType));
        mu16Frame = lu16Frame;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- PrepareAck @ 0x8286F488 ----------------------------------------------
    void Message::PrepareAck(s32 leType, u16 lu16Frame, u8 lu8GameID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0,
            CGS_ASSERT_MESSAGE_AT("!IsMessageValid()", __FILE__, 888 /* ARTIST 0x8286F4BC */));
        mu16Frame = lu16Frame;
        mx8Flags  = KX8_FLAGS_ACK;
        SetType(static_cast<::EMessageType>(leType));
        mu8GameID = lu8GameID;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- PrepareNack @ 0x8286F508 ---------------------------------------------
    void Message::PrepareNack(s32 leType, u16 lu16Frame, u8 lu8GameID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0,
            CGS_ASSERT_MESSAGE_AT("!IsMessageValid()", __FILE__, 915 /* ARTIST 0x8286F53C */));
        mu16Frame = lu16Frame;
        mx8Flags  = KX8_FLAGS_NACK;
        SetType(static_cast<::EMessageType>(leType));
        mu8GameID = lu8GameID;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- IsReliable ----------------------------------------------------------------
    // Slot 0. A plain message is unreliable; ReliableMessage overrides it. The console
    // folds this `return false` leaf with every other identical leaf in the image.
    bool Message::IsReliable() const
    {
        return false;
    }

    // ---- OldMessagesAreValid -------------------------------------------------------
    // Slot 1. By default a message older than the last one received is stale; the
    // messages whose payload is an event (collectables, checkpoints, images, ...)
    // override it to accept them. Same folded `return false` leaf.
    bool Message::OldMessagesAreValid() const
    {
        return false;
    }

    // ---- PackOrUnpack() ----------------------------------------------------------
    // Slot 4. The base message carries no fields of its own: it serialises nothing
    // and reports success. The console folds this `return 0` leaf with every other
    // identical leaf in the image.
    PackOrUnpackResult Message::PackOrUnpack()
    {
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- Pack --------------------------------------------------------------------
    // Attach mBitstream to the caller's buffer (Prepare is inlined: the byte
    // misalignment of the buffer is folded into both cursors and the length), run the
    // virtual PackOrUnpack() (slot 4), report how far the write cursor
    // advanced, then detach the stream (Release, inlined as four zero stores) and mark
    // the message idle. Returns true when PackOrUnpack reported success.
    bool Message::Pack(u8* lpu8Buffer, s32 liBufferOffsetInBits, s32 liBufferLengthInBits,
                       s32* lpiBitsWritten)
    {
        mePackOrUnpack = E_PACK_INTO_BITSTREAM;
        mBitstream.Prepare(lpu8Buffer, liBufferOffsetInBits, liBufferOffsetInBits,
                           liBufferLengthInBits);

        const s32 liStartWritePosition = mBitstream.miBitWritePosition;

        const PackOrUnpackResult lxResult = PackOrUnpack();

        *lpiBitsWritten = mBitstream.miBitWritePosition - liStartWritePosition;

        mBitstream.Release();
        mePackOrUnpack = E_PACK_OR_UNPACK_COUNT;

        return lxResult == KX_PACK_OR_UNPACK_SUCCESS;
    }

    // ---- UnPack ------------------------------------------------------------------
    // Symmetric to Pack for an unpack pass: the read cursor starts at
    // liBufferReadOffsetInBits and the write cursor and length both sit at
    // liBufferLengthInBits. The flags are cleared, the virtual PackOrUnpack() (slot 4)
    // deserialises the fields, the message is marked VALID and, when the virtual
    // IsReliable() (slot 0) says so, RELIABLE. The bits consumed are the growth of
    // (read - write) across the call. The stream is then detached, the message marked
    // idle, and a failed unpack asserts.
    void Message::UnPack(u8* lpu8Buffer, s32 liBufferReadOffsetInBits, s32 liBufferLengthInBits,
                         s32* lpiBitsRead)
    {
        mePackOrUnpack = E_UNPACK_FROM_BITSTREAM;
        mBitstream.Prepare(lpu8Buffer, liBufferReadOffsetInBits, liBufferLengthInBits,
                           liBufferLengthInBits);

        const s32 liStartUnread = mBitstream.miBitWritePosition - mBitstream.miBitReadPosition;

        mx8Flags = 0;

        const PackOrUnpackResult lxResult = PackOrUnpack();

        mx8Flags |= KX8_FLAGS_VALID;
        if (IsReliable())
        {
            mx8Flags |= KX8_FLAGS_RELIABLE;
        }

        *lpiBitsRead = (mBitstream.miBitReadPosition - mBitstream.miBitWritePosition) + liStartUnread;

        mBitstream.Release();
        mePackOrUnpack = E_PACK_OR_UNPACK_COUNT;

        CGS_ASSERT(lxResult == KX_PACK_OR_UNPACK_SUCCESS,
                   "lxPackOrUnpackResult == KX_PACK_OR_UNPACK_SUCCESS");
    }

    // ---- GetPackedMessageSize ------------------------------------------------------
    // Pack the message into a stack scratch buffer of KI_MAX_PACKED_MESSAGE_SIZE bytes
    // (asserting the pack succeeded) and return the packed length rounded up to whole
    // bytes. The console also streams "CgsNetwork::Message::GetPackedMessageSize
    // returning <n>" into the global debug-print stream; that stream has no home in
    // the tree, so the log line is dropped (same precedent as GetGameID).
    s32 Message::GetPackedMessageSize()
    {
        u8  lacTmpBuffer[KI_MAX_PACKED_MESSAGE_SIZE];
        s32 liMessageSizeInBits = 0;

        const bool lbPacked = Pack(lacTmpBuffer, 0, KI_MAX_PACKED_MESSAGE_SIZE * 8,
                                   &liMessageSizeInBits);
        CGS_ASSERT(lbPacked, CGS_ASSERT_MESSAGE_AT(
            "Pack(lacTmpBuffer, 0, KI_MAX_PACKED_MESSAGE_SIZE*8, &liMessageSizeInBits)",
            __FILE__, 416 /* ARTIST 0x82880EB4 */));

        return (liMessageSizeInBits + 7) / 8;
    }

    // ---- GetEulerAnglesFromMatrix ---------------------------------------------------
    // Roll, pitch and yaw of the rotation rows (each clamped to [-1, 1] first). Pitch is the
    // arc-sine of the forward row's y; away from straight up / down, yaw comes from the
    // forward row and roll from the side and up rows. At the poles yaw and roll are not
    // separable: roll takes the whole turn and yaw is zero.
    // Native XMVectorASin supplies the console polynomial; ATan2 above restores
    // the original VMX reciprocal and quadrant operations.
    void Message::GetEulerAnglesFromMatrix(const rw::math::vpu::Matrix44Affine& lMatrix,
                                           f32* lpfRollOut, f32* lpfPitchOut, f32* lpfYawOut)
    {
        const XMVECTOR lvMinusOne = { -1.0f, -1.0f, -1.0f, 0.0f };
        const XMVECTOR lvPlusOne = { 1.0f, 1.0f, 1.0f, 0.0f };
        CGS_ASSERT(lpfRollOut && lpfPitchOut && lpfYawOut, CGS_ASSERT_MESSAGE_AT(
            "lpfRollOut && lpfPitchOut && lpfYawOut",
            __FILE__, 828 /* ARTIST 0x82881680 */));
        const XMVECTOR lvZAxis = __vminfp(__vmaxfp(__lvx(&lMatrix.zAxis, 0), lvMinusOne), lvPlusOne);
        const XMVECTOR lvXAxis = __vminfp(__vmaxfp(__lvx(&lMatrix.xAxis, 0), lvMinusOne), lvPlusOne);
        const XMVECTOR lvYAxis = __vminfp(__vmaxfp(__lvx(&lMatrix.yAxis, 0), lvMinusOne), lvPlusOne);
        const f32 lfPitch = XMVectorASin(__vspltw(lvZAxis, 1)).x;
        *lpfPitchOut = lfPitch;
        const XMVECTOR lvTrue = __vspltisw(-1);
        const XMVECTOR lvSign = __vslw(lvTrue, lvTrue);
        if (!(lfPitch < KF_ANGLE_HALF_PI))
        {
            *lpfRollOut = ATan2(__vspltw(lvXAxis, 2), __vxor(__vspltw(lvYAxis, 2), lvSign)).x;
            *lpfYawOut = 0.0f;
        }
        else if (!(lfPitch > -KF_ANGLE_HALF_PI))
        {
            *lpfRollOut = ATan2(__vxor(__vspltw(lvXAxis, 2), lvSign), __vspltw(lvYAxis, 2)).x;
            *lpfYawOut = 0.0f;
        }
        else
        {
            *lpfYawOut = ATan2(__vxor(__vspltw(lvZAxis, 0), lvSign), __vspltw(lvZAxis, 2)).x;
            *lpfRollOut = ATan2(__vxor(__vspltw(lvXAxis, 1), lvSign), __vspltw(lvYAxis, 1)).x;
        }
    }

    // ARTIST 82881978: vmsum3fp, two-step reciprocal-square-root refinement,
    // zero-length select, then normalization/clamp and the native ASin/ATan path.
    void Message::GetAnglesFromVector(__vector4 lVector,
                                      f32* lpfAngleAOut, f32* lpfAngleBOut, f32* lpfMagnitudeOut)
    {
        const XMVECTOR lvVector = lVector;
        const XMVECTOR lvZero = __vspltisw(0);
        const XMVECTOR lvLengthSquared = __vmsum3fp(lvVector, lvVector);
        const XMVECTOR lvLength = __vmulfp(lvLengthSquared, ReciprocalSqrt(lvLengthSquared));
        const XMVECTOR lvZeroLength = __vcmpeqfp(lvZero, lvLengthSquared);
        const f32 lfMagnitude = __vsel(lvLength, lvZero, lvZeroLength).x;
        *lpfMagnitudeOut = lfMagnitude;
        if (!(fabsf(lfMagnitude) > KF_MIN_ANGLES_MAGNITUDE))
        {
            *lpfAngleBOut = 0.0f;
            *lpfAngleAOut = 0.0f;
            return;
        }
        const XMVECTOR lvPlusOne = { 1.0f, 1.0f, 1.0f, 0.0f };
        const XMVECTOR lvTrue = __vspltisw(-1);
        const XMVECTOR lvSign = __vslw(lvTrue, lvTrue);
        const XMVECTOR lvMinusOne = __vxor(lvPlusOne, lvSign);
        const XMVECTOR lvDirection = __vmulfp(lvVector, ReciprocalSqrt(lvLengthSquared));
        const XMVECTOR lvUnit = __vminfp(__vmaxfp(lvDirection, lvMinusOne), lvPlusOne);
        const f32 lfAngleB = XMVectorASin(__vspltw(lvUnit, 1)).x;
        *lpfAngleBOut = lfAngleB;
        if (fabsf(lfAngleB) == KF_ANGLE_HALF_PI)
        {
            *lpfAngleAOut = 0.0f;
            return;
        }
        *lpfAngleAOut = ATan2(__vspltw(lvUnit, 2), __vspltw(lvUnit, 0)).x;
    }

    // ---- GetTimeDiffWrapped16 -------------------------------------------------------
    // Frame A (unwrapped with the wrap count) is moved onto frame B's rate, rounded to the
    // nearest frame, and frame B is unwrapped into the same wrap; when the two land at
    // least half a wrap apart, the one that is behind moves on a wrap. The difference in
    // frames becomes seconds at B's rate. For a live session and frames within a third of
    // a wrap of each other the result is cross-checked against the float reference.
    f32 GetTimeDiffWrapped16(u16 lu16FramesA, u16 lu16FramesB, s32 liNumWraps,
                             bool lbFramesAAre50Hz, bool lbFramesBAre50Hz)
    {
        s64 liFramesA = static_cast<s32>(liNumWraps * KI_FRAMES_PER_WRAP + lu16FramesA);

        CGS_ASSERT(lu16FramesA != KU16_INVALID_FRAME, "lu16FramesA != KU16_INVALID_FRAME");
        CGS_ASSERT(lu16FramesB != KU16_INVALID_FRAME, "lu16FramesB != KU16_INVALID_FRAME");

        const f32 lfTimeStep = lbFramesBAre50Hz ? KF_TIME_STEP_50HZ : KF_TIME_STEP_60HZ;

        if (lbFramesAAre50Hz)
        {
            if (!lbFramesBAre50Hz)
            {
                liFramesA = (liFramesA * 12 + 5) / 10;
            }
        }
        else if (lbFramesBAre50Hz)
        {
            liFramesA = (liFramesA * 5 + 3) / 6;
        }

        s64 liFramesB = (liFramesA / KI_FRAMES_PER_WRAP) * KI_FRAMES_PER_WRAP + lu16FramesB;
        if (!(liFramesB < liFramesA + KI_HALF_WRAP_FRAMES))
        {
            liFramesA += KI_FRAMES_PER_WRAP;
        }
        else if (!(liFramesB + KI_HALF_WRAP_FRAMES > liFramesA))
        {
            liFramesB += KI_FRAMES_PER_WRAP;
        }

        const s64 liFramesBMinusA = liFramesB - liFramesA;
        CGS_ASSERT((liFramesBMinusA < KI_HALF_WRAP_FRAMES) || ((liFramesA - liFramesB) < KI_HALF_WRAP_FRAMES),
                   "((liFramesB - liFramesA) < KU16_HALF_WRAP_FRAMES) || ((liFramesA - liFramesB) < KU16_HALF_WRAP_FRAMES)");

        f32 lfTimeDiff;
        if (liFramesB > liFramesA)
        {
            lfTimeDiff = -(static_cast<f32>(liFramesBMinusA) * lfTimeStep);
        }
        else
        {
            lfTimeDiff = static_cast<f32>(liFramesA - liFramesB) * lfTimeStep;
        }

        if (liNumWraps >= 0
            && (static_cast<u16>(liFramesBMinusA) < KU16_TIME_DIFF_CHECK_FRAMES
                || static_cast<u16>(liFramesA - liFramesB) < KU16_TIME_DIFF_CHECK_FRAMES))
        {
            const f32 lfOldTimeDiff = OLDGetTimeDiffWrapped16(lu16FramesA, lu16FramesB, liNumWraps,
                                                              lbFramesAAre50Hz, lbFramesBAre50Hz);

            f32 lfMaxError;
            if (liNumWraps < KI_TIME_DIFF_SHORT_SESSION_WRAPS)
            {
                lfMaxError = KF_TIME_DIFF_SHORT_SESSION_ERROR;
            }
            else if (liNumWraps < KI_TIME_DIFF_MEDIUM_SESSION_WRAPS)
            {
                lfMaxError = KF_TIME_DIFF_MEDIUM_SESSION_ERROR;
            }
            else
            {
                lfMaxError = KF_TIME_DIFF_LONG_SESSION_ERROR;
            }

            const f32 lfError = lfOldTimeDiff - lfTimeDiff;
            if (!(fabsf(lfError) < lfMaxError))
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "GetTimeDiffWrapped16(" << static_cast<s32>(lu16FramesA)
                        << "," << static_cast<s32>(lu16FramesB)
                        << "," << liNumWraps
                        << "," << (lbFramesAAre50Hz ? "true" : "false")
                        << "," << (lbFramesBAre50Hz ? "true" : "false")
                        << "," << KI_TIME_DIFF_REPORTED_FRAME_RANGE
                        << ") failed. Old time=" << lfOldTimeDiff
                        << " new time=" << lfTimeDiff
                        << " error=" << fabsf(lfError)
                        << " larger than max allowed error of " << lfMaxError;
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
        }

        return lfTimeDiff;
    }
}

// FOLDED FROM CgsMessage_wN1_01.cpp
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::TranslateFrame50HzTo60Hz / TranslateFrame60HzTo50Hz.
//
// A 16-bit frame counter from a console running the other simulation rate is placed on
// the local 16-bit ring and rescaled to the local rate. The frame and the local current
// frame are both extended by lu16NumWraps whole rings of KU16_WRAP_FRAMES; the frame is
// then moved one ring up or down when it lies more than half a ring from the current
// frame (a frame below one ring that must move down is mirrored instead and the result
// mirrored back). The rescale is integer and rounds to nearest: 50 -> 60 is
// (12 f + 5) / 10, 60 -> 50 is (5 f + 3) / 6, both modulo one ring.
//
// When the two frames are close, the result is cross-checked against the float
// reference (OLDTranslateFrame*) within a tolerance that grows with the wrap count.
// Kept apart from the mounted CgsMessage.cpp: mount it together with
// CgsOldFrameConversionFunctions.cpp.

namespace CgsNetwork
{
    namespace
    {
        const u64 KU16_WRAP_FRAMES      = 65535;
        const u64 KU16_HALF_WRAP_FRAMES = 32768;
        const s32 KI_DEFAULT_TOLERANCE  = 21845;   // KU16_DEFAULT_TOLERANCE
    }

    // The frame and the current frame, lu16NumWraps rings up, are summed in 32 bits and
    // sign-extended into the 64-bit working values.

    u16 TranslateFrame50HzTo60Hz(u16 lu16Frame50Hz, u16 lu16CurrentFrame50Hz, u16 lu16NumWraps)
    {
        u64 luEstimatedTime50Hz =
            static_cast<s32>(static_cast<u32>(lu16Frame50Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));
        const u64 luCurrentTime50Hz =
            static_cast<s32>(static_cast<u32>(lu16CurrentFrame50Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));

        bool lbMirrored = false;
        if (luEstimatedTime50Hz < luCurrentTime50Hz + KU16_HALF_WRAP_FRAMES)
        {
            if (!(luEstimatedTime50Hz + KU16_HALF_WRAP_FRAMES > luCurrentTime50Hz))
            {
                luEstimatedTime50Hz += KU16_WRAP_FRAMES;
            }
        }
        else if (luEstimatedTime50Hz < KU16_WRAP_FRAMES)
        {
            lbMirrored          = true;
            luEstimatedTime50Hz = KU16_WRAP_FRAMES - luEstimatedTime50Hz;
        }
        else
        {
            luEstimatedTime50Hz -= KU16_WRAP_FRAMES;
        }

        CGS_ASSERT((static_cast<u16>(luEstimatedTime50Hz - luCurrentTime50Hz) < KU16_HALF_WRAP_FRAMES)
                       || (static_cast<u16>(luCurrentTime50Hz - luEstimatedTime50Hz) < KU16_HALF_WRAP_FRAMES),
                   "((uint16_t) (luEstimatedTime50Hz - luCurrentTime50Hz) < KU16_HALF_WRAP_FRAMES) || "
                   "((uint16_t) (luCurrentTime50Hz - luEstimatedTime50Hz) < KU16_HALF_WRAP_FRAMES)");

        u16 lu16Frame60Hz = static_cast<u16>(((luEstimatedTime50Hz * 12 + 5) / 10) % KU16_WRAP_FRAMES);
        if (lbMirrored)
        {
            lu16Frame60Hz = static_cast<u16>(KU16_WRAP_FRAMES - lu16Frame60Hz);
        }

        s32 liFrameDistance = static_cast<s32>(lu16Frame50Hz) - static_cast<s32>(lu16CurrentFrame50Hz);
        if (liFrameDistance < 0)
        {
            liFrameDistance = -liFrameDistance;
        }

        if (liFrameDistance < KI_DEFAULT_TOLERANCE)
        {
            const u16 lu16OldFrame60Hz = OLDTranslateFrame50HzTo60Hz(lu16Frame50Hz, lu16CurrentFrame50Hz,
                                                                    lu16NumWraps);
            if (lu16NumWraps <= 1000)
            {
                if (lu16NumWraps > 100)
                {
                    CGS_ASSERT(lu16Frame60Hz == lu16OldFrame60Hz
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz - 1)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz - 2)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz + 1)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz + 2),
                               "Translate50HzTo60Hz(,,,) failed. lu16Frame60Hz= lu16OldFrame60Hz=");
                }
                else if (lu16NumWraps > 7)
                {
                    CGS_ASSERT(lu16Frame60Hz == lu16OldFrame60Hz
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz - 1)
                                   || lu16Frame60Hz == static_cast<u16>(lu16OldFrame60Hz + 1),
                               "Translate50HzTo60Hz(,,,) failed lu16Frame60Hz= lu16OldFrame60Hz=");
                }
                else
                {
                    CGS_ASSERT(lu16Frame60Hz == lu16OldFrame60Hz,
                               "Translate50HzTo60Hz(,,,) failed lu16Frame60Hz= lu16OldFrame60Hz=");
                }
            }
        }

        return lu16Frame60Hz;
    }

    u16 TranslateFrame60HzTo50Hz(u16 lu16Frame60Hz, u16 lu16CurrentFrame60Hz, u16 lu16NumWraps)
    {
        u64 luEstimatedTime60Hz =
            static_cast<s32>(static_cast<u32>(lu16Frame60Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));
        const u64 luCurrentTime60Hz =
            static_cast<s32>(static_cast<u32>(lu16CurrentFrame60Hz) + static_cast<u32>(lu16NumWraps) * static_cast<u32>(KU16_WRAP_FRAMES));

        bool lbMirrored = false;
        if (luEstimatedTime60Hz < luCurrentTime60Hz + KU16_HALF_WRAP_FRAMES)
        {
            if (!(luEstimatedTime60Hz + KU16_HALF_WRAP_FRAMES > luCurrentTime60Hz))
            {
                luEstimatedTime60Hz += KU16_WRAP_FRAMES;
            }
        }
        else if (luEstimatedTime60Hz < KU16_WRAP_FRAMES)
        {
            lbMirrored          = true;
            luEstimatedTime60Hz = KU16_WRAP_FRAMES - luEstimatedTime60Hz;
        }
        else
        {
            luEstimatedTime60Hz -= KU16_WRAP_FRAMES;
        }

        CGS_ASSERT((static_cast<u16>(luEstimatedTime60Hz - luCurrentTime60Hz) < KU16_HALF_WRAP_FRAMES)
                       || (static_cast<u16>(luCurrentTime60Hz - luEstimatedTime60Hz) < KU16_HALF_WRAP_FRAMES),
                   "((uint16_t) (luEstimatedTime60Hz - luCurrentTime60Hz) < KU16_HALF_WRAP_FRAMES) || "
                   "((uint16_t) (luCurrentTime60Hz - luEstimatedTime60Hz) < KU16_HALF_WRAP_FRAMES)");

        u16 lu16Frame50Hz = static_cast<u16>(((luEstimatedTime60Hz * 5 + 3) / 6) % KU16_WRAP_FRAMES);
        if (lbMirrored)
        {
            lu16Frame50Hz = static_cast<u16>(KU16_WRAP_FRAMES - lu16Frame50Hz);
        }

        s32 liFrameDistance = static_cast<s32>(lu16Frame60Hz) - static_cast<s32>(lu16CurrentFrame60Hz);
        if (liFrameDistance < 0)
        {
            liFrameDistance = -liFrameDistance;
        }

        if (liFrameDistance < KI_DEFAULT_TOLERANCE)
        {
            const u16 lu16OldFrame50Hz = OLDTranslateFrame60HzTo50Hz(lu16Frame60Hz, lu16CurrentFrame60Hz,
                                                                    lu16NumWraps);
            if (lu16NumWraps < 0x8000)
            {
                if (lu16NumWraps > 100)
                {
                    CGS_ASSERT(lu16Frame50Hz == lu16OldFrame50Hz
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 2)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 3)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 2)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 3),
                               "Translate60HzTo50Hz(,,) tol= new= old= failed");
                }
                else
                {
                    CGS_ASSERT(lu16Frame50Hz == lu16OldFrame50Hz
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz - 2)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 1)
                                   || lu16Frame50Hz == static_cast<u16>(lu16OldFrame50Hz + 2),
                               "Translate60HzTo50Hz(,,) tol= new= old= failed");
                }
            }
        }

        return lu16Frame50Hz;
    }
}

// FOLDED FROM CgsMessage_wN1_02.cpp
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"   // Magnitude / operator- (the round-trip checks)

// CgsNetwork::PackOrUnpackMatrix and PackOrUnpackVector: the affine and bounded-vector field
// primitives. They read the rotation / direction through Message::GetEulerAnglesFromMatrix
// and Message::GetAnglesFromVector (bodied in CgsMessage.cpp).

namespace CgsNetwork
{
    namespace
    {
        const f32 KF_PI      = 3.14159265f;
        const f32 KF_HALF_PI = 1.57079637f;

        // How far a vector may drift through its angle encoding before it is reported.
        const f32 KF_MAX_VECTOR_MAGNITUDE_ERROR = 0.1f;
        const f32 KF_MAX_VECTOR_ERROR           = 0.05f;
    }

    // The rotation travels as roll, pitch and yaw, each in [-pi, pi] over its own bit
    // count (a pack stops at the first angle that does not fit); then the translation
    // row, one axis at a time, in [lPosMin, lPosMax] over the per-axis bit counts, each
    // through a copy written back into the row. An unknown lifecycle state asserts and
    // still runs the translation part.
    PackOrUnpackResult PackOrUnpackMatrix(Message* lpMessage, rw::math::vpu::Matrix44Affine* lpMatrix,
                                          s32 liRollBits, s32 liPitchBits, s32 liYawBits,
                                          rw::math::vpu::Vector3 lPosMin, rw::math::vpu::Vector3 lPosMax,
                                          s32 liPosXBits, s32 liPosYBits, s32 liPosZBits)
    {
        f32 lfRoll  = 0.0f;
        f32 lfPitch = 0.0f;
        f32 lfYaw   = 0.0f;

        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            Message::GetEulerAnglesFromMatrix(*lpMatrix, &lfRoll, &lfPitch, &lfYaw);

            u32 luPacked = 0;
            FloatQuantiser::Pack(lfRoll, -KF_PI, KF_PI, liRollBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liRollBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfPitch, -KF_PI, KF_PI, liPitchBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liPitchBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfYaw, -KF_PI, KF_PI, liYawBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liYawBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }
        }
        else if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            lpMessage->mBitstream.GetQuantisedFloat(&lfRoll, -KF_PI, KF_PI, liRollBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfPitch, -KF_PI, KF_PI, liPitchBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfYaw, -KF_PI, KF_PI, liYawBits);
            Message::SetMatrixFromEulerAngles(lpMatrix, lfRoll, lfPitch, lfYaw);
        }
        else
        {
            CGS_ASSERT(false,
                       "CgsNetwork::Message::PackOrUnpack called without telling "
                       "it which it is doing\n");
        }

        f32 lfPosition = lpMatrix->wAxis.mV.x;
        PackOrUnpackResult lxResult = PackOrUnpackFloat(lpMessage, &lfPosition, lPosMin.mV.x, lPosMax.mV.x, liPosXBits);
        lpMatrix->wAxis.mV.x = lfPosition;

        lfPosition = lpMatrix->wAxis.mV.y;
        lxResult   = PackOrUnpackFloat(lpMessage, &lfPosition, lPosMin.mV.y, lPosMax.mV.y, liPosYBits) | lxResult;
        lpMatrix->wAxis.mV.y = lfPosition;

        lfPosition = lpMatrix->wAxis.mV.z;
        lxResult   = PackOrUnpackFloat(lpMessage, &lfPosition, lPosMin.mV.z, lPosMax.mV.z, liPosZBits) | lxResult;
        lpMatrix->wAxis.mV.z = lfPosition;

        return lxResult;
    }

    // A vector travels as its two direction angles (angle A in [-pi, pi], angle B in
    // [-pi/2, pi/2]) and its magnitude in [0, lfMagnitudeBound], over the three bit counts
    // (a pack stops at the first value that does not fit). Both directions check the
    // encoding: after packing, the vector rebuilt from the packed angles must be close to
    // the original; after unpacking, the rebuilt vector must survive a second trip.
    PackOrUnpackResult PackOrUnpackVector(Message* lpMessage, rw::math::vpu::Vector3* lpvField,
                                          s32 liXBits, s32 liYBits, f32 lfMagnitudeBound, s32 liZBits)
    {
        f32 lfAngleA    = 0.0f;
        f32 lfAngleB    = 0.0f;
        f32 lfMagnitude = 0.0f;

        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            Message::GetAnglesFromVector(__lvx(lpvField, 0), &lfAngleA, &lfAngleB, &lfMagnitude);

            u32 luPacked = 0;
            FloatQuantiser::Pack(lfAngleA, -KF_PI, KF_PI, liXBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liXBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfAngleB, -KF_HALF_PI, KF_HALF_PI, liYBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liYBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfMagnitude, 0.0f, lfMagnitudeBound, liZBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liZBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            rw::math::vpu::Vector3 lRebuiltVector;
            Message::GetVectorFromAngles(&lRebuiltVector, lfAngleA, lfAngleB, lfMagnitude);
            const rw::math::vpu::Vector3 lDifferenceVector = lRebuiltVector - *lpvField;
            CGS_ASSERT(rw::math::vpu::Magnitude(lDifferenceVector) < KF_MAX_VECTOR_ERROR,
                       "RwMath::Magnitude( lDifferenceVector ) < 0.05f");
        }
        else if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            lpMessage->mBitstream.GetQuantisedFloat(&lfAngleA, -KF_PI, KF_PI, liXBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfAngleB, -KF_HALF_PI, KF_HALF_PI, liYBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfMagnitude, 0.0f, lfMagnitudeBound, liZBits);
            Message::GetVectorFromAngles(lpvField, lfAngleA, lfAngleB, lfMagnitude);

            f32 lfTestMagnitude = 0.0f;
            Message::GetAnglesFromVector(__lvx(lpvField, 0), &lfAngleA, &lfAngleB, &lfTestMagnitude);
            CGS_ASSERT(fabsf(lfTestMagnitude - lfMagnitude) < KF_MAX_VECTOR_MAGNITUDE_ERROR,
                       "RwMath::Abs( lfTestMagnitude - lfMagnitude ) < 0.1f");

            rw::math::vpu::Vector3 lRebuiltVector;
            Message::GetVectorFromAngles(&lRebuiltVector, lfAngleA, lfAngleB, lfTestMagnitude);
            const rw::math::vpu::Vector3 lDifferenceVector = lRebuiltVector - *lpvField;
            CGS_ASSERT(rw::math::vpu::Magnitude(lDifferenceVector) < KF_MAX_VECTOR_ERROR,
                       "RwMath::Magnitude( lDifferenceVector ) < 0.05f");
        }
        else
        {
            CGS_ASSERT(false,
                       "CgsNetwork::Message::PackOrUnpack called without telling "
                       "it which it is doing\n");
        }

        return KX_PACK_OR_UNPACK_SUCCESS;
    }
}
