#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLogger.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::Logger::Construct  @ 0x82751AE0
//   BrnTraffic::Logger::HashState  @ 0x8275DFB8
//
// HashState builds a deterministic byte-image of the whole traffic simulation state
// (the module's RNG, every live/static param, the free + purgatory lists, and every
// active hull's light/occupancy state) into a fixed 0x12840-byte HashBuffer, then folds
// a CRC-32 of that buffer to 16 bits. The network layer compares the result across
// machines to detect traffic-sim divergence.

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<PurgatoryInfo,N> (purgatory lists)
#include "GameShared/GameClasses/Containers/CgsStack.h"      // CgsContainers::Stack<T,N> (free lists)
#include "GameShared/GameClasses/Containers/CgsSet.h"        // Set<u16,72> (active hulls)
#include "GameShared/GameClasses/Containers/CgsHash.h"       // CgsContainers::CgsHash::CalculateHash
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"       // Param, ParamTransform, ParamListNode, ParamNeedToSlowData
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h" // StaticTrafficParam
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h" // HullRuntime
#include "SharedClasses/Traffic/BrnTrafficHull.h"                                     // Hull::muNumJunctions / muNumStoplines

#include <cstddef> // offsetof
#include <cstring> // std::memset, std::memcpy

namespace BrnTraffic
{
    // The Logger singleton. Construct stores `this` here (the console's one global word);
    // nothing on this build reads it back.
    Logger* Logger::gpLogger = 0;

    // Never called: pins the snapshot layout HashState's stores attest (see the header).
    // Pointer-free, so the host layout is the console's byte for byte.
    void HashBuffer::_AssertLayout()
    {
        static_assert(sizeof(ParamData) == 0x50, "HashBuffer::ParamData is 0x50 bytes");
        static_assert(offsetof(ParamData, maPlans) == 0x14, "ParamData::maPlans at +0x14");
        static_assert(offsetof(ParamData, muParamInFront) == 0x20, "ParamData::muParamInFront at +0x20");
        static_assert(offsetof(ParamData, mfListParamAlong) == 0x2C, "ParamData::mfListParamAlong at +0x2C");
        static_assert(offsetof(ParamData, mPos) == 0x30, "ParamData::mPos at +0x30");
        static_assert(offsetof(ParamData, mDirAndAccel) == 0x40, "ParamData::mDirAndAccel at +0x40");
        static_assert(sizeof(StaticParamData) == 6, "HashBuffer::StaticParamData is 6 bytes");
        static_assert(sizeof(ActiveHullData) == 0x220, "HashBuffer::ActiveHullData is 0x220 bytes");
        static_assert(offsetof(ActiveHullData, mxStopLineStates) == 0x18, "ActiveHullData::mxStopLineStates at +0x18");

        static_assert(offsetof(HashBuffer, maParamData) == 0x30, "maParamData at +0x30");
        static_assert(offsetof(HashBuffer, maStaticParamData) == 0x7D30, "maStaticParamData at +0x7D30");
        static_assert(offsetof(HashBuffer, muNumFreeParams) == 0x81DC, "muNumFreeParams at +0x81DC");
        static_assert(offsetof(HashBuffer, mauFreeParams) == 0x81E0, "mauFreeParams at +0x81E0");
        static_assert(offsetof(HashBuffer, muNumFreeStaticParams) == 0x8500, "muNumFreeStaticParams at +0x8500");
        static_assert(offsetof(HashBuffer, mauFreeStaticParams) == 0x8504, "mauFreeStaticParams at +0x8504");
        static_assert(offsetof(HashBuffer, muNumParamsInPurgatory) == 0x85CC, "muNumParamsInPurgatory at +0x85CC");
        static_assert(offsetof(HashBuffer, maParamPurgatory) == 0x85D0, "maParamPurgatory at +0x85D0");
        static_assert(offsetof(HashBuffer, muNumStaticParamsInPurgatory) == 0x8C10, "muNumStaticParamsInPurgatory at +0x8C10");
        static_assert(offsetof(HashBuffer, maStaticParamPurgatory) == 0x8C14, "maStaticParamPurgatory at +0x8C14");
        static_assert(offsetof(HashBuffer, muNumActiveHulls) == 0x8F30, "muNumActiveHulls at +0x8F30");
        static_assert(offsetof(HashBuffer, maActiveHullData) == 0x8F38, "maActiveHullData at +0x8F38");
        static_assert(sizeof(HashBuffer) == 0x12840, "HashState clears and hashes 0x12840 bytes");
    }

    // -------------------------------------------------------------------------
    // @ 0x82751AE0 -- *result = 1; gpLogger = this. The single byte the X360 stores
    // (`li r11,1; stb r11,0(r3)`) is mbAllowDivergentBehaviour; it then publishes the
    // singleton (`stw r3, dword_8300D010`).
    // -------------------------------------------------------------------------
    void Logger::Construct()
    {
        mbAllowDivergentBehaviour = true;
        gpLogger = this;
    }

    // The module's reset leg calls this through mpLogger; the console's callee is a bare return.
    void Logger::Reset()
    {
    }

    // -------------------------------------------------------------------------
    // @ 0x8275DFB8 -- deterministic traffic-state fingerprint.
    //
    // Every module read below is a direct member load in the console (the Logger is a friend
    // of TrafficEntityModule, ParamTransform and HullRuntime); the asserts it fires are the
    // ones of the module accessors it inlines -- GetParam's bound, GetHull's and
    // GetHullRuntime's -- plus the containers' own.
    // -------------------------------------------------------------------------
    u16 Logger::HashState(const TrafficEntityModule* lpModule)
    {
        // Fill the whole snapshot with a fixed byte so every unwritten/padding byte hashes
        // deterministically, then overwrite the live state below.
        HashBuffer lHashBuffer;
        std::memset(&lHashBuffer, 0xD2, sizeof(HashBuffer));

        // The module's RNG, copied whole (six 8-byte words).
        lHashBuffer.mRand = lpModule->mRand;

        // ---- params ----------------------------------------------------------
        for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
        {
            CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

            const Param&           lrParam = lpModule->maParams[luParam];
            HashBuffer::ParamData* lpData  = &lHashBuffer.maParamData[luParam];

            if (!lrParam.IsAlive())
            {
                // Dead slot: record just a zero flag byte (the rest stays at the 0xD2 fill).
                lpData->mxFlags = 0;
                continue;
            }

            // Identity / behaviour: the flag byte keeps only ALIVE and IN_PURGATORY, the effect
            // byte only its three history bits.
            lpData->mxFlags        = static_cast<u8>(lrParam.mxFlags & (Param::E_FLAG_ALIVE | Param::E_FLAG_IN_PURGATORY));
            lpData->mxEffectHist   = static_cast<u8>(lrParam.mxEffectAndHistoryState & 0x0E);
            lpData->muHullIndex    = lrParam.muHullIndex;
            lpData->muSectionIndex = lrParam.muSectionIndex;
            lpData->miBehaviour    = lrParam.miBehaviour;
            lpData->mfParamAlong   = lrParam.mfParamAlong;
            lpData->mfStopDist     = lrParam.mfStopDist;
            lpData->mfTargetSpeed  = lrParam.mfTargetSpeed;

            // Queued plans: the type byte always, then the payload of that type. muDirection
            // is never sampled; bytes a type does not use keep the 0xD2 fill.
            for (u32 luPlan = 0; luPlan < KU_PARAM_NUM_PLANS; ++luPlan)
            {
                const ParamPlan& lrPlan     = lrParam.maPlans[luPlan];
                ParamPlan&       lrPlanData = lpData->maPlans[luPlan];
                lrPlanData.muType = lrPlan.muType;

                switch (lrPlan.muType)
                {
                case ParamPlan::E_TYPE_NONE:
                    break;

                case ParamPlan::E_TYPE_CHANGE_LANE:
                    lrPlanData.mChangeLaneData.muRungToCarryOut = lrPlan.mChangeLaneData.muRungToCarryOut;
                    lrPlanData.mChangeLaneData.muNewSection     = lrPlan.mChangeLaneData.muNewSection;
                    lrPlanData.mChangeLaneData.muNeighbourData  = lrPlan.mChangeLaneData.muNeighbourData;
                    break;

                case ParamPlan::E_TYPE_CHANGE_SECTION:
                    lrPlanData.mChangeSectionData.muNewSection = lrPlan.mChangeSectionData.muNewSection;
                    lrPlanData.mChangeSectionData.muNewHull    = lrPlan.mChangeSectionData.muNewHull;
                    break;

                default:
                    // The console appends the plan type (%d) to this message.
                    CGS_ASSERT(false, "Invalid param plan in log: ");
                    break;
                }
            }

            // Parallel records sampled alongside the param.
            const ParamNeedToSlowData& lrSlowData = lpModule->maParamNeedToSlowData[luParam];
            lpData->muParamInFront  = lrSlowData.muParamInFront;
            lpData->mfNextParamDist = lrSlowData.mfNextParamDist;

            const ParamListNode& lrNode = lpModule->maParamListNodes[luParam];
            lpData->muNextParam      = lrNode.muNextParam;
            lpData->muPrevParam      = lrNode.muPrevParam;
            lpData->mfListParamAlong = lrNode.mfParamAlong;

            // The deterministic position goes in as (x, y, z, 0); the direction/acceleration
            // quad is copied whole.
            const ParamTransform& lrTransform = lpModule->maParamTransforms[luParam];
            lpData->mPos.x       = lrTransform.mPos.x;
            lpData->mPos.y       = lrTransform.mPos.y;
            lpData->mPos.z       = lrTransform.mPos.z;
            lpData->mPos.w       = 0.0f;
            lpData->mDirAndAccel = lrTransform.mDirAndAccel;
        }

        // ---- static (parked) params -----------------------------------------
        for (u32 luParam = 0; luParam < KU_MAX_STATIC_TRAFFIC; ++luParam)
        {
            const StaticTrafficParam&    lrParam = lpModule->maStaticTrafficParams[luParam];
            HashBuffer::StaticParamData* lpData  = &lHashBuffer.maStaticParamData[luParam];

            if (lrParam.IsAlive())
            {
                lpData->mxFlags                    = static_cast<u8>(lrParam.mxFlags & StaticTrafficParam::E_FLAG_ALIVE);
                lpData->muHullIndex                = lrParam.muHull;
                lpData->muStaticTrafficIndexOnHull = lrParam.muStaticTrafficIndexOnHull;
                lpData->muVehicleType              = lrParam.muVehicleType;
            }
            else
            {
                lpData->mxFlags = 0;
            }
        }

        // ---- free param list -------------------------------------------------
        lHashBuffer.muNumFreeParams = static_cast<u32>(lpModule->mFreeParams.GetLength());
        for (u32 luFreeParam = 0; luFreeParam < static_cast<u32>(lpModule->mFreeParams.GetLength()); ++luFreeParam)
        {
            lHashBuffer.mauFreeParams[luFreeParam] = lpModule->mFreeParams[static_cast<s32>(luFreeParam)];
        }

        // ---- free static param list ------------------------------------------
        lHashBuffer.muNumFreeStaticParams = static_cast<u32>(lpModule->mFreeStaticParamStack.GetLength());
        for (u32 luFreeStaticParam = 0;
             luFreeStaticParam < static_cast<u32>(lpModule->mFreeStaticParamStack.GetLength());
             ++luFreeStaticParam)
        {
            lHashBuffer.mauFreeStaticParams[luFreeStaticParam] =
                lpModule->mFreeStaticParamStack[static_cast<s32>(luFreeStaticParam)];
        }

        // ---- param purgatory list --------------------------------------------
        lHashBuffer.muNumParamsInPurgatory = lpModule->maPurgatoryList.GetLength();
        for (u32 luPurgParam = 0; luPurgParam < lpModule->maPurgatoryList.GetLength(); ++luPurgParam)
        {
            lHashBuffer.maParamPurgatory[luPurgParam] = lpModule->maPurgatoryList.GetItem(luPurgParam);
        }

        // ---- static param purgatory list -------------------------------------
        lHashBuffer.muNumStaticParamsInPurgatory = lpModule->mStaticParamPurgatoryList.GetLength();
        for (u32 luPurgSParam = 0; luPurgSParam < lpModule->mStaticParamPurgatoryList.GetLength(); ++luPurgSParam)
        {
            lHashBuffer.maStaticParamPurgatory[luPurgSParam] =
                lpModule->mStaticParamPurgatoryList.GetItem(luPurgSParam);
        }

        // ---- active hulls ----------------------------------------------------
        lHashBuffer.muNumActiveHulls = lpModule->mActiveHulls.GetLength();
        for (u32 luActiveHull = 0; luActiveHull < lpModule->mActiveHulls.GetLength(); ++luActiveHull)
        {
            const Hull*                 lpHull        = lpModule->GetHull(lpModule->mActiveHulls[luActiveHull]);
            const HullRuntime*          lpHullRuntime = lpModule->GetHullRuntime(lpModule->mActiveHulls[luActiveHull]);
            HashBuffer::ActiveHullData* lpData        = &lHashBuffer.maActiveHullData[luActiveHull];

            lpData->muHullIndex    = lpModule->mActiveHulls[luActiveHull];
            lpData->muNumJunctions = lpHull->muNumJunctions;
            lpData->muNumStopLines = lpHull->muNumStoplines;

            for (u32 luJunction = 0; luJunction < lpHull->muNumJunctions; ++luJunction)
            {
                lpData->mauJunctionStates[luJunction] = lpHullRuntime->GetJunctionCurrentStates()[luJunction];
            }

            // Stopline red-state packed into a 64-bit mask, one bit per stopline.
            lpData->mxStopLineStates = 0;
            for (u32 luStopline = 0; luStopline < lpHull->muNumStoplines; ++luStopline)
            {
                if (lpHullRuntime->IsStoplineRed(luStopline))
                {
                    lpData->mxStopLineStates |= (static_cast<u64>(1) << luStopline);
                }
            }

            // The whole section-span occupancy table (256 u16 == 0x200 bytes).
            std::memcpy(lpData->mauSectionSpanVehicleCount,
                        lpHullRuntime->mauSectionSpanVehicleCount,
                        sizeof(lpData->mauSectionSpanVehicleCount));
        }

        // Fold the CRC-32 of the whole snapshot to 16 bits.
        const u32 luHash32 = CgsContainers::CgsHash::CalculateHash(
            reinterpret_cast<char*>(&lHashBuffer), static_cast<int>(sizeof(HashBuffer)));
        return static_cast<u16>((luHash32 >> 16) ^ luHash32);
    }
}
