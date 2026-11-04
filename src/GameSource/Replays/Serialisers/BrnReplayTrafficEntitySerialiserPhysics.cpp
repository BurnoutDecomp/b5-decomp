#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.h"
#include "GameSource/Replays/BrnReplayQuantisedVector4.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstring>
#include <vectorintrinsics.h>
#include <cstddef>

namespace BrnReplays
{
    namespace
    {
        // Native 82C5F3F8 constructs 82FFA5C0 before replay serialisation.
        QuantisedVector4Compression gTrafficPhysicsCompression(
            Vector4(-2.0f, -2.0f, -2.0f, 0.0f), Vector4(2.0f, 2.0f, 2.0f, 1.0f), 9, 9, 9, 5);
        const f32 KF_PHYSICS_TOLERANCE = 1.52587890625e-5f; // ARTIST 8209D7B0
        const char* const KPC_TRAFFIC_SOURCE = "..\\..\\..\\GameSource\\Replays/Serialisers/BrnReplayTrafficEntitySerialiser.cpp";

        // Field placements are independently attested by 8265AE98 and 8265B710.
        struct CGS_ALIGN(16) PhysicsState
        {
            u8 muNumDetachedParts;
            u8 mauDetachedIds[20];
            u8 maPad0015[11];
            Vector4 maDetachedTransforms[20][4];
            Vector4 mStateVector;
            Vector4 maQuantisedVectors[128];
            Vector4 maMatrices[4][4];
            Vector4 maPositions[24];
            u8 mauPositionIds[24];
            u8 mauStateFlags[4];
            u8 maPad0FCC[24];
            s8 miNumPositions;
            u8 muFlag0, muFlag1, muFlag2;
            f32 mafScalars[8];
            u8 maTail[8];
        };
        CGS_STATIC_ASSERT((offsetof(PhysicsState, maDetachedTransforms) == 32), "ARTIST detached transforms");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, mStateVector) == 1312), "ARTIST state vector");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, maQuantisedVectors) == 1328), "ARTIST vector array");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, maMatrices) == 3376), "ARTIST matrix array");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, maPositions) == 3632), "ARTIST positions");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, mauPositionIds) == 4016), "ARTIST position IDs");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, mauStateFlags) == 4040), "ARTIST state flags");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, miNumPositions) == 4068), "ARTIST position count");
        CGS_STATIC_ASSERT((offsetof(PhysicsState, mafScalars) == 4072), "ARTIST scalars");
        CGS_STATIC_ASSERT((sizeof(PhysicsState) == 4112), "ARTIST physics stride");
        CGS_STATIC_ASSERT((offsetof(TrafficEntitySerialiserStaticLayout, maPhysicsInfo) == 40480), "ARTIST physics records");
        CGS_STATIC_ASSERT((offsetof(TrafficEntitySerialiserStaticLayout, mInfoValid) == 148096), "ARTIST previous-valid bitset");
        CGS_STATIC_ASSERT((offsetof(TrafficEntitySerialiserStaticLayout, maPreviousPhysicsInfo) == 148112), "ARTIST previous physics");

        // Compare exactly as the VMX sequence does: rotate x into w and use CR6.
        bool EqualVector3(const Vector4& a, const Vector4& b)
        {
            __vector4 av = __vrlimi(a.mV, a.mV, 1, 1);
            __vector4 bv = __vrlimi(b.mV, b.mV, 1, 1);
            unsigned int cr;
            __vcmpeqfpR(av, bv, &cr);
            return (cr & 0x80) != 0;
        }
        bool NoneGreaterVector3(const Vector4& a, const Vector4& b)
        {
            __vector4 diff = __vsubfp(a.mV, b.mV);
            __vector4 mask = __vspltisw(-1);
            mask = __vslw(mask, mask);
            diff = __vandc(diff, mask);
            diff = __vrlimi(diff, diff, 1, 1);
            const Vector4 limit(KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE);
            unsigned int cr;
            __vcmpgtfpR(diff, limit.mV, &cr);
            return (cr & 0x20) != 0;
        }
        bool NoneGreaterSelfVector3(const Vector4& a)
        {
            return NoneGreaterVector3(a, a);
        }
        void AssertTraffic(const char* text, int line)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(text, KPC_TRAFFIC_SOURCE, line);
            CgsDev::Assert::EndAssert();
        }
        void AssertPhysicsIndex(u32 index)
        {
            if (index >= 25)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream stream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                stream << "invalid index : " << index << " < " << 25u;
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                    "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h", 203);
                CgsDev::Assert::EndAssert();
            }
        }
    }

    // ARTIST 8265B710; liArg4 is passed by ReadFrame but unused in this build.
    void TrafficEntitySerialiser::ReadPhysicsInfo(TrafficPhysicsInfoRecord* lpInfo, u8 luIndex, s32)
    {
        PhysicsState* lpCurrent = reinterpret_cast<PhysicsState*>(lpInfo);
        const PhysicsState* lpPrevious = 0;
        if (!IsKeyFrame())
        {
            TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
            AssertPhysicsIndex(luIndex);
            if ((lpLayout->mInfoValid & (static_cast<u64>(1) << (luIndex & 63))) != 0)
                lpPrevious = reinterpret_cast<const PhysicsState*>(GetStaticLayout()->maPreviousPhysicsInfo + 4112 * luIndex);
        }
        u8 luMask;
        ReadByte(&luMask);
        if (luMask != 15 && !lpPrevious)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream stream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            stream << "Unable to find old physics for original id " << static_cast<s32>(luIndex) << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, KPC_TRAFFIC_SOURCE, 866);
            CgsDev::Assert::EndAssert();
        }
        if (luMask & 1)
        {
            ReadByte(&lpCurrent->muNumDetachedParts);
            for (u32 i = 0; i < lpCurrent->muNumDetachedParts; ++i)
            {
                ReadByte(&lpCurrent->mauDetachedIds[i]);
                ReadAsQuatPos(&lpCurrent->maDetachedTransforms[i]);
            }
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 881);
            lpCurrent->muNumDetachedParts = lpPrevious->muNumDetachedParts;
            for (u32 i = 0; i < lpCurrent->muNumDetachedParts; ++i)
            {
                lpCurrent->mauDetachedIds[i] = lpPrevious->mauDetachedIds[i];
                std::memcpy(&lpCurrent->maDetachedTransforms[i], &lpPrevious->maDetachedTransforms[i], 64);
            }
        }
        if (luMask & 2)
        {
            for (int i = 0; i < 128; ++i)
            {
                CGS_ALIGN(16) u8 laPacked[16];
                const u8* p = gTrafficPhysicsCompression.maBitCounts;
                BaseSerialiser::Read(laPacked, (p[0] + p[1] + p[2] + p[3] + 7) / 8);
                QuantisedVector4::UnPack(&lpCurrent->maQuantisedVectors[i], laPacked, &gTrafficPhysicsCompression);
            }
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 903);
            std::memcpy(lpCurrent->maQuantisedVectors, lpPrevious->maQuantisedVectors, 2048);
        }
        if (luMask & 4)
        {
            u8 luCount; ReadByte(&luCount);
            lpCurrent->miNumPositions = static_cast<s8>(luCount);
            for (s32 i = 0; i < lpCurrent->miNumPositions; ++i)
            {
                f32 x, y, z;
                ReadFloat(&x); ReadFloat(&y); ReadFloat(&z);
                lpCurrent->maPositions[i] = Vector4(x, y, z, 0.0f);
                ReadByte(&lpCurrent->mauPositionIds[i]);
            }
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 922);
            lpCurrent->miNumPositions = lpPrevious->miNumPositions;
            for (s32 i = 0; i < lpCurrent->miNumPositions; ++i)
            {
                lpCurrent->maPositions[i] = lpPrevious->maPositions[i];
                lpCurrent->mauPositionIds[i] = lpPrevious->mauPositionIds[i];
            }
        }
        if (luMask & 8)
        {
            BaseSerialiser::Read(lpCurrent->mauStateFlags, 4);
            BaseSerialiser::Read(&lpCurrent->mStateVector, 16);
            for (int i = 0; i < 4; ++i)
            {
                f32 values[12]; BaseSerialiser::Read(values, 48);
                for (int j = 0; j < 4; ++j)
                    lpCurrent->maMatrices[i][j] = Vector4(values[j*3], values[j*3+1], values[j*3+2], 0.0f);
            }
            u8 b; ReadByte(&b); lpCurrent->muFlag0 = b != 0;
            ReadByte(&b); lpCurrent->muFlag1 = b != 0;
            ReadByte(&lpCurrent->muFlag2);
            BaseSerialiser::Read(lpCurrent->mafScalars, 32);
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 954);
            std::memcpy(lpCurrent->mauStateFlags, lpPrevious->mauStateFlags, 4);
            lpCurrent->mStateVector = lpPrevious->mStateVector;
            std::memcpy(lpCurrent->maMatrices, lpPrevious->maMatrices, 256);
            lpCurrent->muFlag0 = lpPrevious->muFlag0;
            lpCurrent->muFlag1 = lpPrevious->muFlag1;
            lpCurrent->muFlag2 = lpPrevious->muFlag2;
            std::memcpy(lpCurrent->mafScalars, lpPrevious->mafScalars, 32);
        }
        if (lpCurrent->muNumDetachedParts > 20) AssertTraffic("lPhysicsInfo.muNumDetachedParts <= 20", 969);
    }

    // ARTIST 8265AE98. The comparisons deliberately follow its VMX CR6 tests,
    // including the equality branch in the position-ID loop and the self-difference
    // tests in the fourth section.
    void TrafficEntitySerialiser::WritePhysicsInfo(const u8* lpInfo, u8 luIndex)
    {
        const PhysicsState* lpCurrent = reinterpret_cast<const PhysicsState*>(lpInfo);
        if (lpCurrent->muNumDetachedParts > 20) AssertTraffic("lPhysicsInfo.muNumDetachedParts <= 20", 632);
        const PhysicsState* lpPrevious = 0;
        if (!IsKeyFrame())
        {
            TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
            AssertPhysicsIndex(luIndex);
            if ((lpLayout->mInfoValid & (static_cast<u64>(1) << (luIndex & 63))) != 0)
                lpPrevious = reinterpret_cast<const PhysicsState*>(GetStaticLayout()->maPreviousPhysicsInfo + 4112 * luIndex);
        }
        u8 luMask = 15;
        if (lpPrevious)
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 660);
            if (lpCurrent->muNumDetachedParts == lpPrevious->muNumDetachedParts)
            {
                u32 i = 0;
                for (; i < lpCurrent->muNumDetachedParts; ++i)
                {
                    if (lpCurrent->mauDetachedIds[i] != lpPrevious->mauDetachedIds[i]) break;
                    bool lbEqual = true;
                    for (int j = 0; j < 4; ++j)
                        if (!EqualVector3(lpCurrent->maDetachedTransforms[i][j], lpPrevious->maDetachedTransforms[i][j])) lbEqual = false;
                    if (!lbEqual) break;
                }
                if (i == lpCurrent->muNumDetachedParts) luMask &= ~1u;
            }
            if (std::memcmp(lpCurrent->maQuantisedVectors, lpPrevious->maQuantisedVectors, 2048) == 0)
                luMask &= ~2u;
            if (lpCurrent->miNumPositions == lpPrevious->miNumPositions)
            {
                s32 i = 0;
                for (; i < lpCurrent->miNumPositions; ++i)
                {
                    if (lpCurrent->mauPositionIds[i] == lpPrevious->mauPositionIds[i]) break;
                    if (NoneGreaterVector3(lpCurrent->maPositions[i], lpPrevious->maPositions[i])) break;
                }
                if (i == lpCurrent->miNumPositions) luMask &= ~4u;
            }
            if (std::memcmp(lpCurrent->mauStateFlags, lpPrevious->mauStateFlags, 4) == 0 &&
                NoneGreaterVector3(lpCurrent->mStateVector, lpPrevious->mStateVector))
            {
                const __vector4 diff = __vsubfp(__vspltw(lpCurrent->mStateVector.mV, 3), __vspltw(lpPrevious->mStateVector.mV, 3));
                __vector4 sign = __vspltisw(-1); sign = __vslw(sign, sign);
                const Vector4 limit(KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE);
                const __vector4 greater = __vcmpgtfp(__vandc(diff, sign), limit.mV);
                if (greater.u[0] == 0 && lpCurrent->muFlag0 == lpPrevious->muFlag0 &&
                    lpCurrent->muFlag1 == lpPrevious->muFlag1 && lpCurrent->muFlag2 == lpPrevious->muFlag2)
                {
                    int i = 0;
                    while (i < 8 && lpCurrent->mafScalars[i] == lpPrevious->mafScalars[i]) ++i;
                    if (i != 8)
                    {
                        int row = 0;
                        for (; row < 4; ++row)
                        {
                            bool lbNone = true;
                            for (int column = 0; column < 4; ++column)
                                if (!NoneGreaterSelfVector3(lpCurrent->maMatrices[row][column])) lbNone = false;
                            if (!lbNone) break;
                        }
                        if (row == 4) luMask &= ~8u;
                    }
                }
            }
        }
        WriteByte(&luMask);
        if (luMask & 1)
        {
            BaseSerialiser::Write(&lpCurrent->muNumDetachedParts, 1);
            for (u32 i = 0; i < lpCurrent->muNumDetachedParts; ++i)
            {
                BaseSerialiser::Write(&lpCurrent->mauDetachedIds[i], 1);
                WriteAsQuatPos(&lpCurrent->maDetachedTransforms[i]);
            }
        }
        if (luMask & 2)
        {
            for (int i = 0; i < 128; ++i)
            {
                CGS_ALIGN(16) u8 laPacked[16];
                QuantisedVector4::Pack(laPacked, &gTrafficPhysicsCompression, lpCurrent->maQuantisedVectors[i]);
                const u8* p = gTrafficPhysicsCompression.maBitCounts;
                BaseSerialiser::Write(laPacked, (p[0] + p[1] + p[2] + p[3] + 7) / 8);
            }
        }
        if (luMask & 4)
        {
            BaseSerialiser::Write(&lpCurrent->miNumPositions, 1);
            for (s32 i = 0; i < lpCurrent->miNumPositions; ++i)
            {
                const f32 x = lpCurrent->maPositions[i].mV.x;
                const f32 y = lpCurrent->maPositions[i].mV.y;
                const f32 z = lpCurrent->maPositions[i].mV.z;
                WriteFloat(&x); WriteFloat(&y); WriteFloat(&z);
                BaseSerialiser::Write(&lpCurrent->mauPositionIds[i], 1);
            }
        }
        if (luMask & 8)
        {
            BaseSerialiser::Write(lpCurrent->mauStateFlags, 4);
            BaseSerialiser::Write(&lpCurrent->mStateVector, 16);
            for (int i = 0; i < 4; ++i)
            {
                f32 values[12];
                for (int j = 0; j < 4; ++j)
                {
                    values[j*3] = lpCurrent->maMatrices[i][j].mV.x;
                    values[j*3+1] = lpCurrent->maMatrices[i][j].mV.y;
                    values[j*3+2] = lpCurrent->maMatrices[i][j].mV.z;
                }
                BaseSerialiser::Write(values, 48);
            }
            BaseSerialiser::Write(&lpCurrent->muFlag0, 1); BaseSerialiser::Write(&lpCurrent->muFlag1, 1); BaseSerialiser::Write(&lpCurrent->muFlag2, 1);
            BaseSerialiser::Write(lpCurrent->mafScalars, 32);
        }
    }
}
