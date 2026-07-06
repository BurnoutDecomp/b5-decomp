// ============================================================================
// BrnTrafficLightCollection.cpp
//
// Bodies for BrnTraffic::TrafficLightCollection -- the read-only, fixed-up view
// over a track's baked traffic-light corona data. Reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX:
//   CalcInstanceTransform             @ 0x82753910
//   CalcArbitraryAmberCoronaTransform @ 0x82757478
//   GetCoronaPosition                 @ 0x82753820
//   GetInstanceIndexForInstanceID     @ 0x8274F590
//   GetInstanceType                   @ 0x8274F438
//   GetTrafficLightType               @ 0x8274F4A0
//
// The type + member offsets are homed in BrnTrafficLightCollection.h. Assert
// strings are the X360 rodata literals, verbatim (file/line args dropped).
// ============================================================================

#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream
#include "rw/math/vpu/vector3_operation.h"                    // IsValid
#include "rw/math/vpu/matrix44affine_operation.h"             // TransformPoint

namespace BrnTraffic
{
    // -- CalcInstanceTransform @ 0x82753910 ---------------------------------------
    Matrix44Affine TrafficLightCollection::CalcInstanceTransform(u32 luInstance) const
    {
        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");
        CGS_ASSERT(mpaPosAndYRotations, "mpaPosAndYRotations");

        return ExpandPosPlusYRotToTransform(mpaPosAndYRotations[luInstance]);
    }

    // -- CalcArbitraryAmberCoronaTransform @ 0x82757478 ---------------------------
    // World transform of the given instance's first AMBER corona: the instance's own
    // rotation with the corona's world position in the translation row. Falls back to
    // identity (and asserts, with the instance id streamed into the message) when the
    // instance has no amber corona.
    const Matrix44Affine TrafficLightCollection::CalcArbitraryAmberCoronaTransform(u32 luInstance) const
    {
        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");

        const Matrix44Affine lInstanceTransform = CalcInstanceTransform(luInstance);

        const u32 luType = GetInstanceType(luInstance);
        const TrafficLightType* lpType = GetTrafficLightType(luType);

        const u32 luGlobalCoronaBegin = lpType->muCoronaOffset;
        const u32 luGlobalCoronaEnd   = static_cast<u32>(lpType->muCoronaOffset) + lpType->muNumCoronas;

        for (u32 luGlobalCorona = luGlobalCoronaBegin; luGlobalCorona < luGlobalCoronaEnd; ++luGlobalCorona)
        {
            CGS_ASSERT(luGlobalCorona < muNumCoronas, "luCorona < muNumCoronas");
            CGS_ASSERT(mpaCoronaTypes[luGlobalCorona] < E_TRAFFICLIGHTSTATE_COUNT,
                       "mpaCoronaTypes[luCorona] < E_TRAFFICLIGHTSTATE_COUNT");

            if (GetCoronaState(luGlobalCorona) == E_TRAFFICLIGHTSTATE_AMBER)
            {
                const Vector3 lLocalCoronaPos = GetCoronaPosition(luGlobalCorona);

                // Copy the instance rotation rows; set the translation row to the corona
                // world position (TransformPoint = xAxis*x + yAxis*y + zAxis*z + wAxis).
                Matrix44Affine lResult(lInstanceTransform);
                lResult.Pos() = rw::math::vpu::TransformPoint(lInstanceTransform, lLocalCoronaPos);
                return lResult;
            }
        }

        // No amber corona on this instance: fire the assert with the instance id, then
        // fall back to the identity transform.
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "No amber light on instance " << luInstance;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        Matrix44Affine lIdentity;
        lIdentity.SetIdentity();
        return lIdentity;
    }

    // -- GetCoronaPosition @ 0x82753820 -------------------------------------------
    Vector3 TrafficLightCollection::GetCoronaPosition(u32 luCorona) const
    {
        CGS_ASSERT(luCorona < muNumCoronas, "luCorona < muNumCoronas");
        CGS_ASSERT(rw::math::vpu::IsValid(mpaCoronaPositions[luCorona]),
                   "RwMath::IsValid( mpaCoronaPositions[luCorona] )");

        return mpaCoronaPositions[luCorona];
    }

    // -- GetInstanceIndexForInstanceID @ 0x8274F590 -------------------------------
    // Resolve a persistent instance ID to its dense index via the ID hash table.
    // Returns -1 if the ID is not present. mauInstanceHashOffsets is a u16[129] at
    // member offset 0x20 (u16 index 16), which is why the asm addresses the two
    // bucket offsets as this[luHash+16]/[luHash+17].
    s32 TrafficLightCollection::GetInstanceIndexForInstanceID(u32 luInstanceID) const
    {
        const u32 luHash = luInstanceID & KU_INSTANCE_ID_HASH_MASK;
        CGS_ASSERT((luHash + 1) < KU_INSTANCE_ID_HASH_TABLE_SIZE,
                   "(luHash + 1) < KU_INSTANCE_ID_HASH_TABLE_SIZE");

        const u32 luBeginIndex = mauInstanceHashOffsets[luHash];
        const u32 luEndIndex   = mauInstanceHashOffsets[luHash + 1];
        CGS_ASSERT(luEndIndex >= luBeginIndex, "luEndIndex >= luBeginIndex");
        CGS_ASSERT(luEndIndex <= muNumTrafficLights, "luEndIndex <= muNumTrafficLights");

        for (u32 luIndex = luBeginIndex; luIndex < luEndIndex; ++luIndex)
        {
            if (mpauInstanceHashTable[luIndex] == luInstanceID)
            {
                return mpauInstanceHashToIndexLookup[luIndex];
            }
        }

        return -1;
    }

    // -- GetInstanceType @ 0x8274F438 ---------------------------------------------
    u32 TrafficLightCollection::GetInstanceType(u32 luInstance) const
    {
        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");

        return mpauInstanceTypes[luInstance];
    }

    // -- GetTrafficLightType @ 0x8274F4A0 -----------------------------------------
    const TrafficLightType* TrafficLightCollection::GetTrafficLightType(u32 luType) const
    {
        CGS_ASSERT(luType < muNumTrafficLightTypes, "luType < muNumTrafficLightTypes");

        return &mpaTrafficLightTypes[luType];
    }

    // -- RenderCoronasForInstance @ 0x827571B8 ------------------------------------
    // Submit this instance's coronas (one per active-state corona) to the world
    // corona buffer. Back-face + distance culled against the camera, with a gentle
    // distance-based size boost so distant lights stay legible.
    void TrafficLightCollection::RenderCoronasForInstance(
        u32 luInstance,
        u32 luActiveStates,
        BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
        Vector3 lCameraPosition,
        Vector3 lCameraDirection,
        VecFloat lfCullDistSq) const
    {
        // corona colour state -> corona archetype (RED->Red=7, AMBER->Amber=6, GREEN->Green=5).
        static const BrnCoronaType KAE_STATE_TO_CORONATYPE_MAPPING[E_TRAFFICLIGHTSTATE_COUNT] =
        {
            eCoronaTypeTrafficLightRed,    // E_TRAFFICLIGHTSTATE_RED
            eCoronaTypeTrafficLightAmber,  // E_TRAFFICLIGHTSTATE_AMBER
            eCoronaTypeTrafficLightGreen,  // E_TRAFFICLIGHTSTATE_GREEN
        };
        // corona colour state -> active-mask bit (1 << state); AND'd with luActiveStates to gate.
        static const u8 KAU_TRAFFICLIGHTSTATE_BITS[E_TRAFFICLIGHTSTATE_COUNT] = { 1, 2, 4 };

        CGS_ASSERT(luInstance < muNumTrafficLights, "luInstance < muNumTrafficLights");

        const Matrix44Affine lInstanceTransform = CalcInstanceTransform(luInstance);

        // Camera -> instance delta (the affine's translation row is the instance world pos).
        const Vector3 lDir = rw::math::vpu::operator-(lInstanceTransform.Pos(), lCameraPosition);

        // Back-face cull: skip the instance when it faces away from the camera view direction.
        if (rw::math::vpu::Dot(lCameraDirection, lDir) <= 0.0f)
            return;

        // Distance cull.
        if (rw::math::vpu::MagnitudeSquared(lDir) > static_cast<f32>(lfCullDistSq.x))
            return;

        // Distance-based size boost: scale = Lerp(1, 2, max(dist - 8, 0) * K).
        // K is the console's lazily-cached broadcast of 1/142 (flt_820BE730).
        static const f32 KF_CORONA_SCALE_CONSTANT = 0.0070422534f;
        const f32 lfDistFromCamera = rw::math::vpu::Magnitude(lDir);
        const f32 lfClampedDist    = (lfDistFromCamera - 8.0f) > 0.0f ? (lfDistFromCamera - 8.0f) : 0.0f;
        const f32 lfSizeParam      = lfClampedDist * KF_CORONA_SCALE_CONSTANT;
        // Lerp(1.0f, 2.0f, lfSizeParam) == 1.0f + (2.0f - 1.0f) * lfSizeParam.
        const f32 lfLightScale     = 1.0f + (2.0f - 1.0f) * lfSizeParam;

        const u32 luType = GetInstanceType(luInstance);
        const TrafficLightType* lpType = GetTrafficLightType(luType);

        const u32 luGlobalCoronaBegin = lpType->muCoronaOffset;
        const u32 luGlobalCoronaEnd   = static_cast<u32>(lpType->muCoronaOffset) + lpType->muNumCoronas;

        for (u32 luGlobalCorona = luGlobalCoronaBegin; luGlobalCorona < luGlobalCoronaEnd; ++luGlobalCorona)
        {
            const ETrafficLightState leState = GetCoronaState(luGlobalCorona);
            if ((KAU_TRAFFICLIGHTSTATE_BITS[leState] & luActiveStates) != 0)
            {
                const Vector3 lLocalCoronaPos = GetCoronaPosition(luGlobalCorona);
                const ETrafficLightState leCoronaState = GetCoronaState(luGlobalCorona);

                const Vector3 lCoronaPosition =
                    rw::math::vpu::TransformPoint(lInstanceTransform, lLocalCoronaPos);

                // Direction = the instance's forward (transform zAxis); opacity fixed at 1.0f.
                lpCoronaSubmissionInterface->AddCorona(
                    lCoronaPosition,
                    lInstanceTransform.zAxis,
                    lfLightScale,
                    1.0f,
                    KAE_STATE_TO_CORONATYPE_MAPPING[leCoronaState]);
            }
        }
    }
}
