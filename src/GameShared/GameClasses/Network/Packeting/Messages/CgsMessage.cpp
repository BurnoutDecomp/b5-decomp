#include "CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsSmartBitStream.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsIntQuantiser.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDConvertToString
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint

#include <cmath>   // sinf, cosf, sin, cos, fmaf

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

namespace CgsNetwork
{
    // Largest packed message in bytes (file-scope in this TU).
    const s32 KI_MAX_PACKED_MESSAGE_SIZE = 1400;

    // ---- Construct @ 0x82870B90 ------------------------------------------------
    // Field initialiser the X360 build calls "Construct": stores the invalid
    // sentinels into the trailing scalars and returns the object. The asm writes
    //   stb 0xFF, 0x18  -> mu8GameID = KU8_INVALID_GAME_ID (255)
    //   stb 0x00, 0x19  -> mx8Flags  = 0
    //   stb -1,   0x1A  -> mi8Type   = -1 (KI8_INVALID_TYPE)
    //   sth -1,   0x1C  -> mu16Frame = 0xFFFF (KU16_INVALID_FRAME)
    Message* Message::Construct()
    {
        mu8GameID = KU8_INVALID_GAME_ID;   // 0x18 <- 0xFF
        mx8Flags  = 0;                     // 0x19 <- 0
        mi8Type   = static_cast<s8>(-1);   // 0x1A <- -1
        mu16Frame = KU16_INVALID_FRAME;    // 0x1C <- 0xFFFF
        return this;
    }

    // ---- GetVectorFromAngles @ 0x82870D78 --------------------------------------
    // Builds a scaled direction vector from two angles. The X360 build evaluates
    // sinf/cosf of the two angle args (the asm calls sin/cos on doubles and frsps
    // each result back to f32), assembles the unit direction
    //   ( cos(B)*cos(A), sin(B), cos(B)*sin(A), 0 )
    // on the stack, splats the magnitude across all four lanes, multiplies
    // (vmulfp128) and stores the 16-byte result through the output pointer. The
    // VMX is lowered to the equivalent scalar f32 stores it computes lane-for-lane
    // (lane0=x .. lane3=w), preserving the exact lane mapping (var_60/5C/58/54).
    void Message::GetVectorFromAngles(rw::math::vpu::Vector3* lpvOut,
                                      f32 lfAngleA, f32 lfAngleB, f32 lfMagnitude)
    {
        const f32 lfSinA = sinf(lfAngleA);
        const f32 lfCosA = cosf(lfAngleA);
        const f32 lfSinB = sinf(lfAngleB);
        const f32 lfCosB = cosf(lfAngleB);

        lpvOut->x = lfMagnitude * (lfCosB * lfCosA);   // var_60, lane 0
        lpvOut->y = lfMagnitude * lfSinB;              // var_5C, lane 1
        lpvOut->z = lfMagnitude * (lfCosB * lfSinA);   // var_58, lane 2
        lpvOut->w = 0.0f;                              // var_54, lane 3 (zeroed)
    }

    // ---- SetMatrixFromEulerAngles ---------------------------------------------------
    // Rotation rows from the negated roll / pitch / yaw (cos and sin in double, rounded to
    // single). Only the x, y and z lanes of the three rotation rows are written; the
    // translation row and the w lanes keep their values. The console fuses the four
    // multiply-adds, so they are written with fmaf.
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

        lpMatrix->xAxis.x = fmaf(lfSinYawSinPitch, lfSinRoll, lfCosYaw * lfCosRoll);
        lpMatrix->xAxis.y = lfCosPitch * lfSinRoll;
        lpMatrix->xAxis.z = fmaf(lfCosYawSinPitch, lfSinRoll, -(lfSinYaw * lfCosRoll));

        lpMatrix->yAxis.x = fmaf(lfSinYawSinPitch, lfCosRoll, -(lfCosYaw * lfSinRoll));
        lpMatrix->yAxis.y = lfCosPitch * lfCosRoll;
        lpMatrix->yAxis.z = fmaf(lfCosYawSinPitch, lfCosRoll, lfSinYaw * lfSinRoll);

        lpMatrix->zAxis.x = lfSinYaw * lfCosPitch;
        lpMatrix->zAxis.y = -lfSinPitch;
        lpMatrix->zAxis.z = lfCosYaw * lfCosPitch;
    }

    // ---- sub_828800F0 (quantised-int unpack helper) ----------------------------
    // Read one quantised int in [liMin, liMax] back out of the bitstream: derive
    // the field's bit width as bit_width(liMax - liMin) -- the GetNumBits formula
    // the X360 inlines here (no call: it shifts (max-min) right to zero
    // and counts the shifts) -- read that many bits via BitStream::GetBits, then
    // hand the packed offset to IntQuantiser::UnPack to fold it back onto liMin and
    // clamp. Returns the reconstructed value.
    static s32 UnPackQuantisedInt(BitStream* lpStream, s32 liMin, s32 liMax)
    {
        // Inlined GetNumBits(liMin, liMax): count the right-shifts of (max-min).
        u32 luSpan   = static_cast<u32>(liMax) - static_cast<u32>(liMin);
        s32 liNumBits = 0;
        if (liMax != liMin)
        {
            do
            {
                luSpan >>= 1;
                ++liNumBits;
            }
            while (luSpan);
        }

        const u32 luPacked = static_cast<u32>(lpStream->GetBits(liNumBits));
        s32 liValue = 0;
        IntQuantiser::UnPack(&liValue, liMin, liMax, luPacked);
        return liValue;
    }

    // ---- quantised-integer field primitives --------------------------------------
    // Every integer PackOrUnpack overload has the same shape: the lifecycle word picks
    // the direction. Packing (0) quantises the widened field into [liMin, liMax] with
    // IntQuantiser::Pack and appends the reported number of bits (a full stream reports
    // KX_PACK_FAILED_NO_SPACE); unpacking (1) reads the value back through the
    // quantised-int reader above and narrows it into the field; any other state fires
    // the "called without telling it which it is doing" assert and reports success.
    // The overloads differ only in the field width and the widening (sign- or
    // zero-extension) of the value handed to the quantiser.

    // int8_t field.
    PackOrUnpackResult Message::PackOrUnpack(s8* lpi8Field, s32 liMin, s32 liMax)
    {
        if (mePackOrUnpack == E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(static_cast<s32>(*lpi8Field), liMin, liMax, &luPacked, &liNumBits);
            return mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                           : KX_PACK_FAILED_NO_SPACE;
        }

        if (mePackOrUnpack == E_UNPACK_FROM_BITSTREAM)
        {
            *lpi8Field = static_cast<s8>(UnPackQuantisedInt(&mBitstream, liMin, liMax));
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // uint8_t field.
    PackOrUnpackResult PackOrUnpackU8(Message* lpMessage, u8* lpu8Field, s32 liMin, s32 liMax)
    {
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(static_cast<s32>(*lpu8Field), liMin, liMax, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            *lpu8Field = static_cast<u8>(UnPackQuantisedInt(&lpMessage->mBitstream, liMin, liMax));
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // int16_t field. This overload fires a plain assert string instead of the
    // assembled one.
    PackOrUnpackResult PackOrUnpackS16(Message* lpMessage, s16* lps16Field, s32 liMin, s32 liMax)
    {
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(static_cast<s32>(*lps16Field), liMin, liMax, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            *lps16Field = static_cast<s16>(UnPackQuantisedInt(&lpMessage->mBitstream, liMin, liMax));
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false, "PackOrUnpack called without telling it which operation to do\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // uint16_t field.
    PackOrUnpackResult PackOrUnpackU16(Message* lpMessage, u16* lpu16Field, s32 liMin, s32 liMax)
    {
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(static_cast<s32>(*lpu16Field), liMin, liMax, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            *lpu16Field = static_cast<u16>(UnPackQuantisedInt(&lpMessage->mBitstream, liMin, liMax));
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
    }

    // int32_t field.
    PackOrUnpackResult PackOrUnpackInt(Message* lpMessage, s32* lpiField, s32 liMin, s32 liMax)
    {
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(*lpiField, liMin, liMax, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            *lpiField = UnPackQuantisedInt(&lpMessage->mBitstream, liMin, liMax);
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpack called without telling "
                   "it which it is doing\n");
        return KX_PACK_OR_UNPACK_SUCCESS;
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
            *lpu32Field = static_cast<u32>(UnPackQuantisedInt(&lpMessage->mBitstream, liMin, liMax));
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
    PackOrUnpackResult PackOrUnpackBool(Message* lpMessage, bool* lpbField)
    {
        u8 lu8Value = 0;   // left unset by the console when the lifecycle word is invalid
        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            lu8Value = (*lpbField != false) ? 1 : 0;
        }

        const PackOrUnpackResult lxResult = PackOrUnpackU8(lpMessage, &lu8Value, 0, 1);

        if (lpMessage->mePackOrUnpack != Message::E_PACK_INTO_BITSTREAM)
        {
            *lpbField = (lu8Value == 1);
        }
        return lxResult;
    }

    // ---- CgsID field --------------------------------------------------------------
    // A CgsID travels as its low then its high 32-bit half, each a full-range int. Both
    // directions also render the id to text (the result is unused); packing first checks
    // the halves recombine to the id.
    PackOrUnpackResult PackOrUnpackCgsID(Message* lpMessage, u64* lpu64Field)
    {
        const s32 KI_MIN_INT32 = -0x7FFFFFFF - 1;
        const s32 KI_MAX_INT32 = 0x7FFFFFFF;
        char lacIDString[KI_CGSID_STRING_LEN];

        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            CgsIDConvertToString(*lpu64Field, lacIDString);

            const u64 liLow  = *lpu64Field & 0xFFFFFFFFull;
            const u64 liHigh = *lpu64Field >> 32;
            CGS_ASSERT(*lpu64Field == (liLow | (liHigh << 32)),
                       "*lpValue == (liLow | (liHigh << 32))");

            u32 luPacked  = 0;
            s32 liNumBits = 0;
            IntQuantiser::Pack(static_cast<s32>(liLow), KI_MIN_INT32, KI_MAX_INT32, &luPacked, &liNumBits);
            if (!lpMessage->mBitstream.AddBits(luPacked, liNumBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            IntQuantiser::Pack(static_cast<s32>(liHigh), KI_MIN_INT32, KI_MAX_INT32, &luPacked, &liNumBits);
            return lpMessage->mBitstream.AddBits(luPacked, liNumBits) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                      : KX_PACK_FAILED_NO_SPACE;
        }

        if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            const s32 liLow  = UnPackQuantisedInt(&lpMessage->mBitstream, KI_MIN_INT32, KI_MAX_INT32);
            const s32 liHigh = UnPackQuantisedInt(&lpMessage->mBitstream, KI_MIN_INT32, KI_MAX_INT32);
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
            BitStream* lpStream = &lpMessage->mBitstream;
            const s32 liSecond = UnPackQuantisedInt(lpStream, 0, KI_MAX_SECOND);
            const s32 liMinute = UnPackQuantisedInt(lpStream, 0, KI_MAX_MINUTE);
            const s32 liHour   = UnPackQuantisedInt(lpStream, 0, KI_MAX_HOUR);
            const s32 liDay    = UnPackQuantisedInt(lpStream, KI_MIN_DAY, KI_MAX_DAY);
            const s32 liMonth  = UnPackQuantisedInt(lpStream, KI_MIN_MONTH, KI_MAX_MONTH);
            const s32 liYear   = UnPackQuantisedInt(lpStream, 0, KI_MAX_YEAR);

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
        if (mePackOrUnpack == E_PACK_INTO_BITSTREAM)
        {
            return mBitstream.AddRawData(lpcBuffer, liNumBytes) ? KX_PACK_OR_UNPACK_SUCCESS
                                                                : KX_PACK_FAILED_NO_SPACE;
        }

        if (mePackOrUnpack == E_UNPACK_FROM_BITSTREAM)
        {
            mBitstream.GetRawData(lpcBuffer, liNumBytes);
            return KX_PACK_OR_UNPACK_SUCCESS;
        }

        CGS_ASSERT(false,
                   "CgsNetwork::Message::PackOrUnpackBuffer called without telling "
                   "it which it is doing\n");
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
    Message* Message::SetType(s32 leType)
    {
        CGS_ASSERT(leType >= 0, "leType >= 0");
        CGS_ASSERT(leType < KI_E_MESSAGE_TYPE_COUNT, "leType < E_MESSAGE_TYPE_COUNT");
        CGS_ASSERT(leType < 255, "leType < 255");
        mi8Type = static_cast<s8>(leType);
        return this;
    }

    // ---- PrepareForSend @ 0x8257D5F8 ------------------------------------------
    void Message::PrepareForSend(s32 leType, u16 lu16Frame)
    {
        CGS_ASSERT(lu16Frame != KU16_INVALID_FRAME, "lu16Frame != KU16_INVALID_FRAME");
        SetType(leType);
        mu16Frame = lu16Frame;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- PrepareAck @ 0x8286F488 ----------------------------------------------
    void Message::PrepareAck(s32 leType, u16 lu16Frame, u8 lu8GameID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0, "!IsMessageValid()");
        mu16Frame = lu16Frame;
        mx8Flags  = KX8_FLAGS_ACK;
        SetType(leType);
        mu8GameID = lu8GameID;
        mx8Flags |= KX8_FLAGS_VALID;
    }

    // ---- PrepareNack @ 0x8286F508 ---------------------------------------------
    void Message::PrepareNack(s32 leType, u16 lu16Frame, u8 lu8GameID)
    {
        CGS_ASSERT((mx8Flags & KX8_FLAGS_VALID) == 0, "!IsMessageValid()");
        mu16Frame = lu16Frame;
        mx8Flags  = KX8_FLAGS_NACK;
        SetType(leType);
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
        CGS_ASSERT(lbPacked,
                   "Pack(lacTmpBuffer, 0, KI_MAX_PACKED_MESSAGE_SIZE*8, &liMessageSizeInBits)");

        return (liMessageSizeInBits + 7) / 8;
    }
}
