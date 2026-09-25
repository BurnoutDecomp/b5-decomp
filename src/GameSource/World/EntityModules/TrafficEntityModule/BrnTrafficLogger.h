#pragma once

// =============================================================================
// BrnTrafficLogger.h  (OWNING HEADER)
//
// DWARF home (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
// TrafficEntityModule/BrnTrafficLogger.h) of the traffic-system deterministic
// state-snapshot logger:
//
//   BrnTraffic::Logger      -- the singleton debug logger (gpLogger). It can
//                              snapshot the whole TrafficEntityModule's live state
//                              into a fixed HashBuffer and reduce it to a 16-bit
//                              hash (HashState), and dump that snapshot to a file
//                              (Dump/HACKDump). Used by the network code to detect
//                              client/host traffic-sim divergence.
//   BrnTraffic::HashBuffer  -- the fixed scratch record HashState fills then hashes:
//                              a copy of the module's RNG + every live param /
//                              static param / free list / purgatory list / active
//                              hull, laid out so a byte-wise CRC of the buffer is a
//                              deterministic fingerprint of the traffic state.
//
// LAYOUT IS X360-AUTHORITATIVE. The member set/order/types are the DWARF's, and the
// resulting sizeof(HashBuffer) == 0x12840 (75840) matches the X360 HashState body
// (BrnTraffic::Logger::HashState @ 0x8275DFB8): it `memset`s the buffer to 0xD2 over
// exactly 0x12840 bytes, then hashes 0x12840 bytes. The nested-record sizes are
// pinned by HashState's per-element stores:
//   * ParamData       == 0x50 (80)  -- dest cursor strides 0x50 per param; the mPos
//                                       Vector4 / mDirAndAccel Vector3Plus tail forces
//                                       the 16-byte alignment that rounds 0x4C up to 0x50.
//   * StaticParamData == 6           -- dest cursor strides 6 per static param.
//   * ActiveHullData  == 0x220 (544) -- dest cursor strides 0x220 per active hull; the
//                                       mauSectionSpanVehicleCount[256] u16 tail @ +0x20
//                                       (after the +0x18 u64 stopline mask) ends at 0x220.
// The three static-param arrays are sized by the ship pool (KU_MAX_STATIC_TRAFFIC, 199),
// not the 200 the older declaration prints: HashState's stores put muNumFreeParams at
// +0x81DC (right after 199 six-byte records), muNumStaticParamsInPurgatory at +0x8C10 and muNumActiveHulls at
// +0x8F30 (199 four-byte records after +0x8C14). Only with those extents does the record
// end at 0x12838, which the 16-byte alignment of mRand rounds to the 0x12840 HashState
// clears and hashes. _AssertLayout pins all of it.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                   // Vector4, Vector3Plus
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                         // CgsNumeric::Random (HashBuffer::mRand)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"   // KU_MAX_PARAMS, KU_MAX_STATIC_TRAFFIC, KU_MAX_ACTIVE_HULLS
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"        // BrnTraffic::ParamPlan (HashBuffer::ParamData::maPlans)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // BrnTraffic::TrafficEntityModule, PurgatoryInfo

namespace BrnTraffic
{
    // -------------------------------------------------------------------------
    // HashBuffer -- the deterministic state snapshot HashState fills then CRCs.
    // sizeof == 0x12840 (X360-authoritative, see file header).
    // -------------------------------------------------------------------------
    struct HashBuffer
    {
        // BrnTrafficLogger.h:54 -- one live traffic param, flattened for hashing.
        // sizeof == 0x50 (the Vector4/Vector3Plus tail forces 16-byte alignment).
        struct ParamData
        {
            u8          mxFlags;          // :56  +0x00
            u8          mxEffectHist;     // :57  +0x01
            u8          muSectionIndex;   // :58  +0x02
            s8          miBehaviour;      // :59  +0x03
            u16         muHullIndex;      // :60  +0x04
            f32         mfParamAlong;     // :61  +0x08
            f32         mfStopDist;       // :62  +0x0C
            f32         mfTargetSpeed;    // :63  +0x10
            ParamPlan   maPlans[2];       // :65  +0x14  (2 x 6 = 12, ends +0x20)
            u16         muParamInFront;   // :72  +0x20
            f32         mfNextParamDist;  // :73  +0x24
            u16         muNextParam;      // :76  +0x28
            u16         muPrevParam;      // :77  +0x2A
            f32         mfListParamAlong; // :78  +0x2C
            Vector4     mPos;             // :81  +0x30 (16-aligned)
            Vector3Plus mDirAndAccel;     // :82  +0x40 (16-aligned, ends +0x50)
        };

        // BrnTrafficLogger.h:86 -- one live static (parked) param, flattened.
        // sizeof == 6 (u16-aligned, 5 used bytes + 1 trailing pad).
        struct StaticParamData
        {
            u16 muHullIndex;                // :88  +0x00
            u8  muStaticTrafficIndexOnHull; // :89  +0x02
            u8  mxFlags;                    // :90  +0x03
            u8  muVehicleType;              // :91  +0x04  (+1 pad -> 6)
        };

        // BrnTrafficLogger.h:95 -- one active hull's light/section-occupancy snapshot.
        // sizeof == 0x220 (the +0x18 u64 stopline mask then mauSectionSpanVehicleCount
        // [256] u16 @ +0x20 ends at 0x220).
        struct ActiveHullData
        {
            u16 muHullIndex;                     // :97  +0x00
            u8  muNumJunctions;                  // :98  +0x02
            u8  muNumStopLines;                  // :99  +0x03
            u8  mauJunctionStates[16];           // :101 +0x04
            u64 mxStopLineStates;                // :102 +0x18 (8-aligned)
            u16 mauSectionSpanVehicleCount[256]; // :104 +0x20 (ends +0x220)
        };

        CgsNumeric::Random mRand;                                             // :108 +0x00
        ParamData          maParamData[KU_MAX_PARAMS];                        // :109 +0x30
        StaticParamData    maStaticParamData[KU_MAX_STATIC_TRAFFIC];          // :110 +0x7D30
        u32                muNumFreeParams;                                   // :112 +0x81DC
        u16                mauFreeParams[KU_MAX_PARAMS];                      // :113 +0x81E0
        u32                muNumFreeStaticParams;                             // :115 +0x8500
        u8                 mauFreeStaticParams[KU_MAX_STATIC_TRAFFIC];        // :116 +0x8504
        u32                muNumParamsInPurgatory;                            // :118 +0x85CC
        PurgatoryInfo      maParamPurgatory[KU_MAX_PARAMS];                   // :119 +0x85D0
        u32                muNumStaticParamsInPurgatory;                      // :121 +0x8C10
        PurgatoryInfo      maStaticParamPurgatory[KU_MAX_STATIC_TRAFFIC];     // :122 +0x8C14
        u32                muNumActiveHulls;                                  // :124 +0x8F30
        ActiveHullData     maActiveHullData[KU_MAX_ACTIVE_HULLS];             // :125 +0x8F38

        static void _AssertLayout();
    };

    // -------------------------------------------------------------------------
    // Logger -- the deterministic-state debug logger singleton (gpLogger). One byte:
    // TrafficEntityModule::Construct allocates it (size 1, alignment 16) from the debug
    // allocator and owns it through mpLogger.
    // -------------------------------------------------------------------------
    struct Logger
    {
    public:
        // @ 0x82751AE0 -- enable divergent behaviour and install the singleton.
        void Construct();                                  // :173

        // :178. TrafficEntityModule::Reset calls it through mpLogger; the callee is an empty
        // function (the console's call lands on a lone `blr`).
        void Reset();

        bool AllowDivergentBehaviour();                    // :182 (declared; no reader here)

        // :188. Header inline: TrafficEntityModule::EnterStartingUpState and ResetEventData
        // both expand it as one byte store through mpLogger.
        void SetAllowDivergentBehaviour(bool lbAllowDivergentBehaviour)
        {
            mbAllowDivergentBehaviour = lbAllowDivergentBehaviour;
        }

        // @ 0x8275DFB8 -- snapshot lpModule's live traffic state into a HashBuffer and
        // return its folded 16-bit CRC. Pure read-only fingerprint (the network layer
        // compares it across machines to spot traffic-sim divergence).
        u16  HashState(const TrafficEntityModule* lpModule);   // :196

        void Dump(const char* lpcLogName);                 // :201 (declared; debug dump)
        void HACKDump(const char* lpcLogName);             // :208 (declared; debug dump)

    private:
        // :214 -- the Logger singleton (Construct stores `this` here).
        static Logger* gpLogger;

        bool mbAllowDivergentBehaviour;                    // :216
    };
}
