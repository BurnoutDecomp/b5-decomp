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
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"       // BrnTraffic::Param
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h" // BrnTraffic::StaticTrafficParam

#include <cstring> // std::memset, std::memcpy

namespace BrnTraffic
{
    // The Logger singleton. Construct stores `this` here (X360 dword_8300D010); the
    // network/divergence code reads it back through gpLogger.
    Logger* gpLogger = 0;

    // Bounds the param / static-param sweeps (X360 immediates: param loop `cmplwi 0x190`,
    // static-param loop counter `0xC7`). KU_MAX_PARAMS is asserted by name in the param loop
    // ("luParam < KU_MAX_PARAMS").
    static const u32 KU_MAX_PARAMS        = 400;
    static const u32 KU_MAX_STATIC_PARAMS = 199;

    // =========================================================================
    // INTERFACE-ONLY VIEWS of types whose own homes are separate (not-yet-reconstructed)
    // slices. HashState reverses the compiler's inlining back into accessor calls; the
    // accessor SHAPES below are reconstructed from HashState's reads. FLAG: these are the
    // minimal surfaces this TU calls -- when the TrafficEntityModule / HullRuntime headers
    // land they must define the SAME signatures (mirror, do not fork). Only members/methods
    // actually read by HashState are modelled; the remainder of each record is opaque.
    // =========================================================================

    // The hull's static description. Full layout is committed inline in
    // BrnTrafficHullRuntime.cpp; HashState only reads the junction / stopline counts
    // (Hull +0x02 / +0x03) to bound the active-hull copy loops.
    struct Hull
    {
        u8 GetNumJunctions() const;  // -> muNumJunctions (+0x02)
        u8 GetNumStoplines() const;  // -> muNumStoplines (+0x03)
    };

    // The per-hull light/section runtime state. Full layout is committed inline in
    // BrnTrafficHullRuntime.cpp (sizeof 0x498); HullRuntime::IsStoplineRed @ 0x827068A0 is
    // the externally-linked symbol this TU calls. GetJunctionCurrentStates() (DWARF-attested)
    // and GetSectionSpanVehicleCounts() return the two tables HashState copies wholesale; the
    // X360 inlined both as direct &table reads (HullRuntime +0x40 / +0x290).
    struct HullRuntime
    {
        bool       IsStoplineRed(u32 luStopline) const;          // @ 0x827068A0 (committed)
        const u8*  GetJunctionCurrentStates() const;             // -> &mauJunctionCurrentStates[0] (+0x40)
        const u16* GetSectionSpanVehicleCounts() const;          // -> &mauSectionSpanVehicleCount[0] (+0x290)
    };

    // FLAG (partial, asm-offset-attested): the three parallel param SoA records HashState
    // samples alongside the main Param array. Only the fields HashState reads are modelled,
    // at the offsets the X360 stores attest (the records' full interiors belong to the
    // TrafficEntityModule slice).
    struct ParamNeedToSlowData                  // X360 element stride 0x10
    {
        u16 muParamInFront;   // +0x00
        u16 maPad02;          // +0x02
        f32 mfNextParamDist;  // +0x04
    };
    struct ParamListNode                        // X360 element stride 0x08
    {
        u16 muNextParam;      // +0x00
        u16 muPrevParam;      // +0x02
        f32 mfListParamAlong; // +0x04
    };
    struct ParamTransform                       // X360 element stride 0x40
    {
        Vector4     mPos;         // position
        Vector3Plus mDirAndAccel; // direction (+ accel in the plus lane)
    };

    // The traffic entity module. HashState only reads -- through these accessors (the X360
    // inlined them to direct member reads). FLAG: signatures reconstructed from HashState;
    // mirror them when the module's own home lands.
    class TrafficEntityModule
    {
    public:
        const CgsNumeric::Random&   GetRandom() const;
        const Param*                GetParam(u32 luParam) const;
        const ParamNeedToSlowData*  GetParamNeedToSlowData(u32 luParam) const;
        const ParamListNode*        GetParamListNode(u32 luParam) const;
        const ParamTransform*       GetParamTransform(u32 luParam) const;
        const StaticTrafficParam*   GetStaticTrafficParam(u32 luParam) const;

        const CgsContainers::Stack<u16, 400>& GetFreeParams() const;
        const CgsContainers::Stack<u8, 200>&  GetFreeStaticParams() const;
        const Array<PurgatoryInfo, 400>&      GetParamPurgatory() const;
        const Array<PurgatoryInfo, 199>&      GetStaticParamPurgatory() const;
        const Set<u16, 72>&                   GetActiveHulls() const;
        const Hull&                           GetHull(u16 luHull) const;
        const HullRuntime&                    GetHullRuntime(u16 luHull) const;
    };

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

    // -------------------------------------------------------------------------
    // @ 0x8275DFB8 -- deterministic traffic-state fingerprint.
    // -------------------------------------------------------------------------
    u16 Logger::HashState(const TrafficEntityModule* lpModule)
    {
        // Fill the whole snapshot with a fixed byte (X360 `memset(&lHashBuffer, 0xD2,
        // sizeof(HashBuffer))`) so every unwritten/padding byte hashes deterministically,
        // then overwrite the live state below.
        HashBuffer lHashBuffer;
        std::memset(&lHashBuffer, 0xD2, sizeof(HashBuffer));

        // Snapshot the module's RNG (X360 copies the 0x30-byte CgsNumeric::Random from the
        // module into HashBuffer::mRand as six 8-byte words).
        lHashBuffer.mRand = lpModule->GetRandom();

        // ---- params ----------------------------------------------------------
        for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
        {
            CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

            const Param*               lpParam = lpModule->GetParam(luParam);
            HashBuffer::ParamData*     lpData  = &lHashBuffer.maParamData[luParam];

            if (!lpParam->IsAlive())
            {
                // Dead slot: record just a zero flag byte (the rest stays at the 0xD2 fill).
                lpData->mxFlags = 0;
                continue;
            }

            // Identity / behaviour (the X360 masks the flag/effect bytes it samples:
            // mxFlags & (E_FLAG_ALIVE | E_FLAG_IN_PURGATORY); mxEffectAndHistoryState & 0x0E).
            lpData->mxFlags        = static_cast<u8>(lpParam->mxFlags & (Param::E_FLAG_ALIVE | Param::E_FLAG_IN_PURGATORY));
            lpData->mxEffectHist   = static_cast<u8>(lpParam->mxEffectAndHistoryState & 0x0E);
            lpData->muHullIndex    = lpParam->muHullIndex;
            lpData->muSectionIndex = lpParam->muSectionIndex;
            lpData->miBehaviour    = lpParam->miBehaviour;
            lpData->mfParamAlong   = lpParam->mfParamAlong;
            lpData->mfStopDist     = lpParam->mfStopDist;
            lpData->mfTargetSpeed  = lpParam->mfTargetSpeed;

            // Queued plans: copy the type byte always, then the active union payload per
            // type (X360 type switch). muDirection is deliberately NOT sampled. The union
            // halfword (muNewHull / muNeighbourData @ +0x02) and the u8 (muNewSection @ +0x04)
            // are copied for both CHANGE_LANE and CHANGE_SECTION; CHANGE_LANE additionally
            // copies muRungToCarryOut (@ +0x05). E_TYPE_NONE copies only the type; anything
            // else asserts "Invalid param plan in log". The bytes the case does not touch keep
            // the 0xD2 fill (so the hash distinguishes the plan types deterministically).
            for (u32 luPlan = 0; luPlan < KU_PARAM_NUM_PLANS; ++luPlan)
            {
                const ParamPlan& lrPlan      = lpParam->maPlans[luPlan];
                ParamPlan&       lrPlanData   = lpData->maPlans[luPlan];
                lrPlanData.muType = lrPlan.muType;

                switch (lrPlan.muType)
                {
                case ParamPlan::E_TYPE_NONE:
                    break;

                case ParamPlan::E_TYPE_CHANGE_LANE:
                    lrPlanData.mChangeLaneData.muNeighbourData  = lrPlan.mChangeLaneData.muNeighbourData;
                    lrPlanData.mChangeLaneData.muNewSection     = lrPlan.mChangeLaneData.muNewSection;
                    lrPlanData.mChangeLaneData.muRungToCarryOut = lrPlan.mChangeLaneData.muRungToCarryOut;
                    break;

                case ParamPlan::E_TYPE_CHANGE_SECTION:
                    lrPlanData.mChangeSectionData.muNewHull    = lrPlan.mChangeSectionData.muNewHull;
                    lrPlanData.mChangeSectionData.muNewSection = lrPlan.mChangeSectionData.muNewSection;
                    break;

                default:
                    CGS_ASSERT(false, "Invalid param plan in log");
                    break;
                }
            }

            // Parallel SoA records sampled alongside the param.
            const ParamNeedToSlowData* lpSlowData = lpModule->GetParamNeedToSlowData(luParam);
            lpData->muParamInFront  = lpSlowData->muParamInFront;
            lpData->mfNextParamDist = lpSlowData->mfNextParamDist;

            const ParamListNode* lpNode = lpModule->GetParamListNode(luParam);
            lpData->muNextParam      = lpNode->muNextParam;
            lpData->muPrevParam      = lpNode->muPrevParam;
            lpData->mfListParamAlong = lpNode->mfListParamAlong;

            const ParamTransform* lpTransform = lpModule->GetParamTransform(luParam);
            lpData->mPos         = lpTransform->mPos;
            lpData->mDirAndAccel = lpTransform->mDirAndAccel;
        }

        // ---- static (parked) params -----------------------------------------
        for (u32 luParam = 0; luParam < KU_MAX_STATIC_PARAMS; ++luParam)
        {
            const StaticTrafficParam*       lpParam = lpModule->GetStaticTrafficParam(luParam);
            HashBuffer::StaticParamData*    lpData  = &lHashBuffer.maStaticParamData[luParam];

            if (lpParam->IsAlive())
            {
                lpData->mxFlags                    = static_cast<u8>(lpParam->mxFlags & StaticTrafficParam::E_FLAG_ALIVE);
                lpData->muHullIndex                = lpParam->muHull;
                lpData->muStaticTrafficIndexOnHull = lpParam->muStaticTrafficIndexOnHull;
                lpData->muVehicleType              = lpParam->muVehicleType;
            }
            else
            {
                lpData->mxFlags = 0;
            }
        }

        // ---- free param list (Stack<u16,400>) --------------------------------
        const CgsContainers::Stack<u16, 400>& lrFreeParams = lpModule->GetFreeParams();
        lHashBuffer.muNumFreeParams = static_cast<u32>(lrFreeParams.GetLength());
        for (u32 luFreeParam = 0; luFreeParam < lHashBuffer.muNumFreeParams; ++luFreeParam)
        {
            lHashBuffer.mauFreeParams[luFreeParam] = lrFreeParams[static_cast<s32>(luFreeParam)];
        }

        // ---- free static param list (Stack<u8,200>) --------------------------
        const CgsContainers::Stack<u8, 200>& lrFreeStaticParams = lpModule->GetFreeStaticParams();
        lHashBuffer.muNumFreeStaticParams = static_cast<u32>(lrFreeStaticParams.GetLength());
        for (u32 luFreeStaticParam = 0; luFreeStaticParam < lHashBuffer.muNumFreeStaticParams; ++luFreeStaticParam)
        {
            lHashBuffer.mauFreeStaticParams[luFreeStaticParam] = lrFreeStaticParams[static_cast<s32>(luFreeStaticParam)];
        }

        // ---- param purgatory list (Array<PurgatoryInfo,400>) -----------------
        const Array<PurgatoryInfo, 400>& lrParamPurgatory = lpModule->GetParamPurgatory();
        lHashBuffer.muNumParamsInPurgatory = lrParamPurgatory.GetLength();
        for (u32 luPurgParam = 0; luPurgParam < lHashBuffer.muNumParamsInPurgatory; ++luPurgParam)
        {
            lHashBuffer.maParamPurgatory[luPurgParam] = lrParamPurgatory[luPurgParam];
        }

        // ---- static param purgatory list (Array<PurgatoryInfo,199>) ----------
        const Array<PurgatoryInfo, 199>& lrStaticParamPurgatory = lpModule->GetStaticParamPurgatory();
        lHashBuffer.muNumStaticParamsInPurgatory = lrStaticParamPurgatory.GetLength();
        for (u32 luPurgSParam = 0; luPurgSParam < lHashBuffer.muNumStaticParamsInPurgatory; ++luPurgSParam)
        {
            lHashBuffer.maStaticParamPurgatory[luPurgSParam] = lrStaticParamPurgatory[luPurgSParam];
        }

        // ---- active hulls (Set<u16,72>) --------------------------------------
        const Set<u16, 72>& lrActiveHulls = lpModule->GetActiveHulls();
        lHashBuffer.muNumActiveHulls = lrActiveHulls.GetLength();
        for (u32 luActiveHull = 0; luActiveHull < lHashBuffer.muNumActiveHulls; ++luActiveHull)
        {
            const u16                     luHull        = lrActiveHulls[luActiveHull];
            const Hull&                   lrHull        = lpModule->GetHull(luHull);
            const HullRuntime&            lrHullRuntime = lpModule->GetHullRuntime(luHull);
            HashBuffer::ActiveHullData*   lpData        = &lHashBuffer.maActiveHullData[luActiveHull];

            // Junction count / stopline count come from the hull's static description; the
            // X360 reads them off the resolved Hull (muNumJunctions @ +0x02, muNumStoplines
            // @ +0x03). HashState samples them via the Set's hull index.
            lpData->muHullIndex    = luHull;
            lpData->muNumJunctions = lrHull.GetNumJunctions();
            lpData->muNumStopLines = lrHull.GetNumStoplines();

            const u8* lpJunctionStates = lrHullRuntime.GetJunctionCurrentStates();
            for (u32 luJunction = 0; luJunction < lpData->muNumJunctions; ++luJunction)
            {
                lpData->mauJunctionStates[luJunction] = lpJunctionStates[luJunction];
            }

            // Stopline red-state packed into a 64-bit mask, one bit per stopline.
            lpData->mxStopLineStates = 0;
            for (u32 luStopline = 0; luStopline < lpData->muNumStopLines; ++luStopline)
            {
                if (lrHullRuntime.IsStoplineRed(luStopline))
                {
                    lpData->mxStopLineStates |= (static_cast<u64>(1) << luStopline);
                }
            }

            // Whole section-span occupancy table (256 u16 == 0x200 bytes).
            std::memcpy(lpData->mauSectionSpanVehicleCount,
                        lrHullRuntime.GetSectionSpanVehicleCounts(),
                        sizeof(lpData->mauSectionSpanVehicleCount));
        }

        // Fold the CRC-32 of the whole snapshot to 16 bits (X360: h ^= h >> 16; h &= 0xFFFF).
        const u32 luHash32 = CgsContainers::CgsHash::CalculateHash(
            reinterpret_cast<char*>(&lHashBuffer), static_cast<int>(sizeof(HashBuffer)));
        const u16 luHash   = static_cast<u16>((luHash32 >> 16) ^ luHash32);
        return luHash;
    }
}
