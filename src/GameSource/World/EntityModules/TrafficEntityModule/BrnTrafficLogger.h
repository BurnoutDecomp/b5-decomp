#pragma once

// =============================================================================
// BrnTrafficLogger.h  (NEW OWNING HEADER)
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
//
// Only the two functions this TU owns are bodied (in BrnTrafficLogger.cpp):
//   Construct @ 0x82751AE0 and HashState @ 0x8275DFB8.
// Reset / AllowDivergentBehaviour / SetAllowDivergentBehaviour / Dump / HACKDump and
// HashBuffer::Dump / FrameLogData are other (not-yet-reconstructed) slices of the same
// header; declared here for shape, their bodies land later. GROW this header additively
// when they do; never redefine these types.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                   // Vector4, Vector3Plus
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                         // CgsNumeric::Random (HashBuffer::mRand)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"        // BrnTraffic::ParamPlan (HashBuffer::ParamData::maPlans)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // BrnTraffic::PurgatoryInfo (HashBuffer purgatory lists)

namespace BrnTraffic
{
    // Forward declaration: the entity module whose live state HashState snapshots. Its
    // full layout/home is a separate (not-yet-reconstructed) slice; this TU only calls
    // its accessors (see BrnTrafficLogger.cpp).
    class TrafficEntityModule;

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

        CgsNumeric::Random mRand;                       // :108 +0x00
        ParamData          maParamData[400];            // :109
        StaticParamData    maStaticParamData[200];      // :110
        u32                muNumFreeParams;             // :112
        u16                mauFreeParams[400];          // :113
        u32                muNumFreeStaticParams;       // :115
        u8                 mauFreeStaticParams[200];    // :116
        u32                muNumParamsInPurgatory;      // :118
        PurgatoryInfo      maParamPurgatory[400];       // :119
        u32                muNumStaticParamsInPurgatory;// :121
        PurgatoryInfo      maStaticParamPurgatory[200]; // :122
        u32                muNumActiveHulls;            // :124
        ActiveHullData     maActiveHullData[72];        // :125
    };

    // -------------------------------------------------------------------------
    // Logger -- the deterministic-state debug logger singleton (gpLogger).
    // -------------------------------------------------------------------------
    struct Logger
    {
        // @ 0x82751AE0 -- enable divergent behaviour and install the singleton.
        void Construct();

        // @ 0x8275DFB8 -- snapshot lpModule's live traffic state into a HashBuffer and
        // return its folded 16-bit CRC. Pure read-only fingerprint (the network layer
        // compares it across machines to spot traffic-sim divergence).
        u16  HashState(const TrafficEntityModule* lpModule);

        // ---- declared-only DWARF surface (bodies land with their own slices) ----
        void Reset();                             // :178
        bool AllowDivergentBehaviour();           // :182
        void SetAllowDivergentBehaviour(bool lbAllow); // :188
        void Dump(const char* lpLogName);         // :201
        void HACKDump(const char* lpLogName);     // :208

        bool mbAllowDivergentBehaviour;           // :216
    };

    // BrnTrafficLogger.h:214 -- the Logger singleton (Construct stores `this` here).
    extern Logger* gpLogger;
}
