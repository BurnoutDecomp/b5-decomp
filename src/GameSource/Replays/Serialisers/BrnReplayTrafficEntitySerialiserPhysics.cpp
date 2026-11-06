#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.h"
#include "GameSource/Replays/BrnReplayQuantisedVector4.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstring>
#include <vectorintrinsics.h>

namespace BrnReplays
{
    namespace
    {
        // Native 82C5F3F8 constructs 82FFA5C0 before replay serialisation.
        QuantisedVector4Compression gTrafficPhysicsCompression(
            Vector4(-2.0f, -2.0f, -2.0f, 0.0f), Vector4(2.0f, 2.0f, 2.0f, 1.0f), 9, 9, 9, 5);
        const f32 KF_PHYSICS_TOLERANCE = 1.52587890625e-5f; // ARTIST 8209D7B0
        const char* const KPC_TRAFFIC_SOURCE = "..\\..\\..\\GameSource\\Replays/Serialisers/BrnReplayTrafficEntitySerialiser.cpp";

        // Compare exactly as the VMX sequence does: rotate x into w and use CR6.
        bool EqualVector3(__vector4 a, __vector4 b)
        {
            __vector4 av = __vrlimi(a, a, 1, 1);
            __vector4 bv = __vrlimi(b, b, 1, 1);
            unsigned int cr;
            __vcmpeqfpR(av, bv, &cr);
            return (cr & 0x80) != 0;
        }
        bool NoneGreaterVector3(__vector4 a, __vector4 b)
        {
            __vector4 diff = __vsubfp(a, b);
            __vector4 mask = __vspltisw(-1);
            mask = __vslw(mask, mask);
            diff = __vandc(diff, mask);
            diff = __vrlimi(diff, diff, 1, 1);
            const Vector4 limit(KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE);
            unsigned int cr;
            __vcmpgtfpR(diff, limit.mV, &cr);
            return (cr & 0x20) != 0;
        }
        bool NoneGreaterSelfVector3(__vector4 a)
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
    void TrafficEntitySerialiser::ReadPhysicsInfo(BrnTraffic::TrafficPhysicsInfo* lpInfo, u8 luIndex, s32)
    {
        const BrnTraffic::TrafficPhysicsInfo* lpPrevious = 0;
        if (!IsKeyFrame())
        {
            TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
            AssertPhysicsIndex(luIndex);
            if ((lpLayout->mInfoValid & (static_cast<u64>(1) << (luIndex & 63))) != 0)
                lpPrevious = &GetStaticLayout()->maPreviousPhysicsInfo[luIndex];
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
            ReadByte(&lpInfo->mDetachedPartQueue.muNumParts);
            for (u32 i = 0; i < lpInfo->mDetachedPartQueue.muNumParts; ++i)
            {
                ReadByte(&lpInfo->mDetachedPartQueue.mau8PartIndex[i]);
                ReadAsQuatPos(&lpInfo->mDetachedPartQueue.maTransforms[i]);
            }
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 881);
            lpInfo->mDetachedPartQueue.muNumParts = lpPrevious->mDetachedPartQueue.muNumParts;
            for (u32 i = 0; i < lpInfo->mDetachedPartQueue.muNumParts; ++i)
            {
                lpInfo->mDetachedPartQueue.mau8PartIndex[i] = lpPrevious->mDetachedPartQueue.mau8PartIndex[i];
                lpInfo->mDetachedPartQueue.maTransforms[i] = lpPrevious->mDetachedPartQueue.maTransforms[i];
            }
        }
        if (luMask & 2)
        {
            for (int i = 0; i < 128; ++i)
            {
                CGS_ALIGN(16) u8 laPacked[16];
                const u8* p = gTrafficPhysicsCompression.maBitCounts;
                BaseSerialiser::Read(laPacked, (p[0] + p[1] + p[2] + p[3] + 7) / 8);
                Vector4 lvOffsets;
                QuantisedVector4::UnPack(&lvOffsets, laPacked, &gTrafficPhysicsCompression);
                lpInfo->maSkinningOffsets_Scratch[i].mV = lvOffsets.mV;
            }
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 903);
            std::memcpy(lpInfo->maSkinningOffsets_Scratch, lpPrevious->maSkinningOffsets_Scratch, 2048);
        }
        if (luMask & 4)
        {
            u8 luCount; ReadByte(&luCount);
            lpInfo->miNumLightLocators = static_cast<s8>(luCount);
            for (s32 i = 0; i < lpInfo->miNumLightLocators; ++i)
            {
                f32 x, y, z;
                ReadFloat(&x); ReadFloat(&y); ReadFloat(&z);
                lpInfo->maLightLocatorPositions[i].mV = Vector4(x, y, z, 0.0f).mV;
                ReadByte(&lpInfo->maLightTagPointTypes[i]);
            }
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 922);
            lpInfo->miNumLightLocators = lpPrevious->miNumLightLocators;
            for (s32 i = 0; i < lpInfo->miNumLightLocators; ++i)
            {
                lpInfo->maLightLocatorPositions[i].mV = lpPrevious->maLightLocatorPositions[i].mV;
                lpInfo->maLightTagPointTypes[i] = lpPrevious->maLightTagPointTypes[i];
            }
        }
        if (luMask & 8)
        {
            BaseSerialiser::Read(lpInfo->mabWheelExists, 4);
            BaseSerialiser::Read(&lpInfo->mvRoadTestNormal_HeightAboveRoad, 16);
            for (int i = 0; i < 4; ++i)
            {
                f32 values[12]; BaseSerialiser::Read(values, 48);
                Matrix44Affine& lrWheel = lpInfo->maWheelTransforms[i];
                lrWheel.xAxis.mV = Vector4(values[0], values[1], values[2], 0.0f).mV;
                lrWheel.yAxis.mV = Vector4(values[3], values[4], values[5], 0.0f).mV;
                lrWheel.zAxis.mV = Vector4(values[6], values[7], values[8], 0.0f).mV;
                lrWheel.wAxis.mV = Vector4(values[9], values[10], values[11], 0.0f).mV;
            }
            u8 luFlag;
            ReadByte(&luFlag);
            lpInfo->mbIsDeforming = luFlag != 0;
            ReadByte(&luFlag);
            lpInfo->mbIsFatallyCrashing = luFlag != 0;
            ReadByte(&lpInfo->mu8RenderDamageFlags);
            BaseSerialiser::Read(lpInfo->mafGlassPaneFractureAmounts, 32);
        }
        else
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 954);
            std::memcpy(lpInfo->mabWheelExists, lpPrevious->mabWheelExists, 4);
            lpInfo->mvRoadTestNormal_HeightAboveRoad = lpPrevious->mvRoadTestNormal_HeightAboveRoad;
            std::memcpy(lpInfo->maWheelTransforms, lpPrevious->maWheelTransforms, 256);
            lpInfo->mbIsDeforming = lpPrevious->mbIsDeforming;
            lpInfo->mbIsFatallyCrashing = lpPrevious->mbIsFatallyCrashing;
            lpInfo->mu8RenderDamageFlags = lpPrevious->mu8RenderDamageFlags;
            std::memcpy(lpInfo->mafGlassPaneFractureAmounts, lpPrevious->mafGlassPaneFractureAmounts, 32);
        }
        if (lpInfo->mDetachedPartQueue.muNumParts > 20) AssertTraffic("lPhysicsInfo.muNumDetachedParts <= 20", 969);
    }

    // ARTIST 8265AE98. The comparisons deliberately follow its VMX CR6 tests,
    // including the equality branch in the position-ID loop and the self-difference
    // tests in the fourth section.
    void TrafficEntitySerialiser::WritePhysicsInfo(const BrnTraffic::TrafficPhysicsInfo* lpInfo, u8 luIndex)
    {
        if (lpInfo->mDetachedPartQueue.muNumParts > 20) AssertTraffic("lPhysicsInfo.muNumDetachedParts <= 20", 632);
        const BrnTraffic::TrafficPhysicsInfo* lpPrevious = 0;
        if (!IsKeyFrame())
        {
            TrafficEntitySerialiserStaticLayout* lpLayout = GetStaticLayout();
            AssertPhysicsIndex(luIndex);
            if ((lpLayout->mInfoValid & (static_cast<u64>(1) << (luIndex & 63))) != 0)
                lpPrevious = &GetStaticLayout()->maPreviousPhysicsInfo[luIndex];
        }
        u8 luMask = 15;
        if (lpPrevious)
        {
            if (IsKeyFrame()) AssertTraffic("!IsKeyFrame()", 660);
            if (lpInfo->mDetachedPartQueue.muNumParts == lpPrevious->mDetachedPartQueue.muNumParts)
            {
                u32 i = 0;
                for (; i < lpInfo->mDetachedPartQueue.muNumParts; ++i)
                {
                    if (lpInfo->mDetachedPartQueue.mau8PartIndex[i] != lpPrevious->mDetachedPartQueue.mau8PartIndex[i]) break;
                    bool lbEqual = true;
                    const Matrix44Affine& lrCurrent = lpInfo->mDetachedPartQueue.maTransforms[i];
                    const Matrix44Affine& lrPrevious = lpPrevious->mDetachedPartQueue.maTransforms[i];
                    lbEqual &= EqualVector3(lrCurrent.xAxis.mV, lrPrevious.xAxis.mV);
                    lbEqual &= EqualVector3(lrCurrent.yAxis.mV, lrPrevious.yAxis.mV);
                    lbEqual &= EqualVector3(lrCurrent.zAxis.mV, lrPrevious.zAxis.mV);
                    lbEqual &= EqualVector3(lrCurrent.wAxis.mV, lrPrevious.wAxis.mV);
                    if (!lbEqual) break;
                }
                if (i == lpInfo->mDetachedPartQueue.muNumParts) luMask &= ~1u;
            }
            if (std::memcmp(lpInfo->maSkinningOffsets_Scratch, lpPrevious->maSkinningOffsets_Scratch, 2048) == 0)
                luMask &= ~2u;
            if (lpInfo->miNumLightLocators == lpPrevious->miNumLightLocators)
            {
                s32 i = 0;
                for (; i < lpInfo->miNumLightLocators; ++i)
                {
                    if (lpInfo->maLightTagPointTypes[i] == lpPrevious->maLightTagPointTypes[i]) break;
                    if (NoneGreaterVector3(lpInfo->maLightLocatorPositions[i].mV, lpPrevious->maLightLocatorPositions[i].mV)) break;
                }
                if (i == lpInfo->miNumLightLocators) luMask &= ~4u;
            }
            if (std::memcmp(lpInfo->mabWheelExists, lpPrevious->mabWheelExists, 4) == 0 &&
                NoneGreaterVector3(lpInfo->mvRoadTestNormal_HeightAboveRoad.mV, lpPrevious->mvRoadTestNormal_HeightAboveRoad.mV))
            {
                const __vector4 diff = __vsubfp(__vspltw(lpInfo->mvRoadTestNormal_HeightAboveRoad.mV, 3),
                                                __vspltw(lpPrevious->mvRoadTestNormal_HeightAboveRoad.mV, 3));
                __vector4 sign = __vspltisw(-1); sign = __vslw(sign, sign);
                const Vector4 limit(KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE, KF_PHYSICS_TOLERANCE);
                const __vector4 greater = __vcmpgtfp(__vandc(diff, sign), limit.mV);
                if (greater.u[0] == 0 && lpInfo->mbIsDeforming == lpPrevious->mbIsDeforming &&
                    lpInfo->mbIsFatallyCrashing == lpPrevious->mbIsFatallyCrashing &&
                    lpInfo->mu8RenderDamageFlags == lpPrevious->mu8RenderDamageFlags)
                {
                    int i = 0;
                    while (i < 8 && lpInfo->mafGlassPaneFractureAmounts[i] == lpPrevious->mafGlassPaneFractureAmounts[i]) ++i;
                    if (i != 8)
                    {
                        int row = 0;
                        for (; row < 4; ++row)
                        {
                            const Matrix44Affine& lrWheel = lpInfo->maWheelTransforms[row];
                            bool lbNone = NoneGreaterSelfVector3(lrWheel.xAxis.mV);
                            lbNone &= NoneGreaterSelfVector3(lrWheel.yAxis.mV);
                            lbNone &= NoneGreaterSelfVector3(lrWheel.zAxis.mV);
                            lbNone &= NoneGreaterSelfVector3(lrWheel.wAxis.mV);
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
            BaseSerialiser::Write(&lpInfo->mDetachedPartQueue.muNumParts, 1);
            for (u32 i = 0; i < lpInfo->mDetachedPartQueue.muNumParts; ++i)
            {
                BaseSerialiser::Write(&lpInfo->mDetachedPartQueue.mau8PartIndex[i], 1);
                WriteAsQuatPos(&lpInfo->mDetachedPartQueue.maTransforms[i]);
            }
        }
        if (luMask & 2)
        {
            for (int i = 0; i < 128; ++i)
            {
                CGS_ALIGN(16) u8 laPacked[16];
                QuantisedVector4::Pack(laPacked, &gTrafficPhysicsCompression, Vector4(lpInfo->maSkinningOffsets_Scratch[i].mV));
                const u8* p = gTrafficPhysicsCompression.maBitCounts;
                BaseSerialiser::Write(laPacked, (p[0] + p[1] + p[2] + p[3] + 7) / 8);
            }
        }
        if (luMask & 4)
        {
            BaseSerialiser::Write(&lpInfo->miNumLightLocators, 1);
            for (s32 i = 0; i < lpInfo->miNumLightLocators; ++i)
            {
                const f32 x = lpInfo->maLightLocatorPositions[i].mV.x;
                const f32 y = lpInfo->maLightLocatorPositions[i].mV.y;
                const f32 z = lpInfo->maLightLocatorPositions[i].mV.z;
                WriteFloat(&x); WriteFloat(&y); WriteFloat(&z);
                BaseSerialiser::Write(&lpInfo->maLightTagPointTypes[i], 1);
            }
        }
        if (luMask & 8)
        {
            BaseSerialiser::Write(lpInfo->mabWheelExists, 4);
            BaseSerialiser::Write(&lpInfo->mvRoadTestNormal_HeightAboveRoad, 16);
            for (int i = 0; i < 4; ++i)
            {
                const Matrix44Affine& lrWheel = lpInfo->maWheelTransforms[i];
                const f32 values[] = {
                    lrWheel.xAxis.mV.x, lrWheel.xAxis.mV.y, lrWheel.xAxis.mV.z,
                    lrWheel.yAxis.mV.x, lrWheel.yAxis.mV.y, lrWheel.yAxis.mV.z,
                    lrWheel.zAxis.mV.x, lrWheel.zAxis.mV.y, lrWheel.zAxis.mV.z,
                    lrWheel.wAxis.mV.x, lrWheel.wAxis.mV.y, lrWheel.wAxis.mV.z,
                };
                BaseSerialiser::Write(values, 48);
            }
            BaseSerialiser::Write(&lpInfo->mbIsDeforming, 1);
            BaseSerialiser::Write(&lpInfo->mbIsFatallyCrashing, 1);
            BaseSerialiser::Write(&lpInfo->mu8RenderDamageFlags, 1);
            BaseSerialiser::Write(lpInfo->mafGlassPaneFractureAmounts, 32);
        }
    }
}
