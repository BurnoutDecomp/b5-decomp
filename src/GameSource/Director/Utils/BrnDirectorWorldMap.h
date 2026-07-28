#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector3, VecFloat
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // CgsModule::EventReceiverQueue<128,16>
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // CgsResource::ResourcePtr

// =============================================================================
// BrnDirector::WorldMap - the director's queryable world map (safe camera
// positions, lane topology, ...).
//
// DWARF home: references/DecFIGS/dwarfdump/GameSource/Director/Utils/
// BrnDirectorWorldMap.{h,cpp}. The struct shape (member order/names/offsets, the
// nested LanePosition record and the ELoadingState enum) is DWARF-authoritative
// (BrnDirectorWorldMap.h:42..149). Every field below is gated on the X360 ledger:
//   * meLoadingState is read by GetTrafficData @0x82219128 (`lwz 0xF8`, compared
//     to E_LOADING_STATE_LOADED == 6).
//   * mpTrafficData (the FIRST member, X360 +0x00) is dereferenced by
//     GetTrafficData (GetMemoryResource) and by GetLanePositionNearestPoint-
//     WithDisplacement @0x8221D078 (operator->), both called on `this`+0.
//   * mpTriggerData / mpAISectionData / mReceiverQueue are present so the member
//     order that places meLoadingState after them is faithful (Construct/LoadData
//     touch them in the DWARF .cpp). They are accessed BY NAME; absolute X360
//     offsets are NOT pinned across the wider host pointers/ResourcePtr.
//
// FLAG (host vs X360 width): a ResourcePtr<T> is 4-byte-pointer sized on the X360;
// on the PC host it is wider. meLoadingState's X360 offset 0xF8 is a target-only
// fact -- the getters reach it by name, so no offset is static_asserted.
//
// This header SUPERSEDES the prior minimal slice (which declared only
// GetSafePositionNearestPointWithDisplacement, consumed by
// BrnDirector::Camera::Utils::PositionFinder::Update); that declaration is kept.
// =============================================================================

// Resource payload types held only by pointer (via ResourcePtr) -> forward decls.
namespace BrnTraffic { struct TrafficData; }
namespace BrnTraffic { struct Hull; }        // WalkLaneLeft takes a (const Hull*)
namespace BrnTrigger { struct TriggerData; }
namespace BrnAI      { struct AISectionsData; }

namespace BrnDirector
{
    class WorldMap
    {
    public:
        // -- BrnDirectorWorldMap.h:70 nested result record --------------------
        // The lane point nearest a query point. Byte layout pinned by the X360
        // stores in GetLanePositionNearestPointWithDisplacement @0x8221D078:
        //   mPosition           @+0x00 (16B SIMD, `stvx128 v13,r0,r29`)
        //   mfSquaredDistance   @+0x10 (`stfs 0x10`, initialised to 100000.0)
        //   mfParamAlongSection @+0x14 (`stfs 0x14`)
        //   muHullIndex         @+0x18 (`stw 0x18`)
        //   muSection           @+0x1C (`stb 0x1C`)
        //   muRung              @+0x1D (`stb 0x1D`)
        //   mbValid             @+0x1E (`stb 0x1E`)
        struct LanePosition
        {
            Vector3 mPosition;            // :71  +0x00
            f32     mfSquaredDistance;    // :72  +0x10
            f32     mfParamAlongSection;  // :73  +0x14
            u32     muHullIndex;          // :74  +0x18
            u8      muSection;            // :75  +0x1C
            u8      muRung;               // :76  +0x1D
            bool    mbValid;              // :77  +0x1E
        };

        // -- BrnDirectorWorldMap.h:132 loading state machine ------------------
        enum ELoadingState
        {
            E_LOADING_STATE_TRIGGERS_NOT_REQUESTED = 0,
            E_LOADING_STATE_LOADING_TRIGGERS       = 1,
            E_LOADING_STATE_TRAFFIC_NOT_REQUESTED  = 2,
            E_LOADING_STATE_LOADING_TRAFFIC        = 3,
            E_AI_DATA_ACQUIRE_NOT_STARTED          = 4,
            E_AI_DATA_ACQUIRE_REQUESTED            = 5,
            E_LOADING_STATE_LOADED                 = 6,
        };

        // BrnDirectorWorldMap.cpp:131/135/144/172/176/190 (the assert lines the X360 bakes).
        // X360 @0x8225F5A0. The staged LOAD state machine DirectorModule::Prepare @0x822712D8
        // pumps once per stage until it returns true: request the TriggerData resource, bind it
        // from the acquire response, request + bind the traffic lanes, then the AI lane data.
        // lpRequestInterface is the GameData request interface reached from the director's
        // OUTPUT buffer (`OutputBuffer::GetResour()`), and every request/response rides this
        // object's own mReceiverQueue.
        //
        // ⚠️ CURRENTLY A DOCUMENTED QUIET GATE -- see the body in BrnDirectorWorldMap.cpp for
        // the full transcribed state machine, why it is gated, and the DELETE-when note.
        bool LoadData(void* lpRequestInterface);

        // -- BrnDirectorWorldMap.h:87 (this batch) ----------------------------
        // Nearest lane point to lPosition that also lies in the direction of
        // lDisplacement (Dot(lDisplacement, prospective - lPosition) > 0). X360
        // @0x8221D078; returns the result by value (hidden sret in r3).
        LanePosition GetLanePositionNearestPointWithDisplacement(Vector3 lPosition,
                                                                 Vector3 lDisplacement) const;

        // -- BrnDirectorWorldMap.h:90 (this batch) ----------------------------
        // The loaded TrafficData resource. Gates on meLoadingState == LOADED, then
        // hands back mpTrafficData.GetMemoryResource(). X360 @0x82219128.
        // (DWARF return: `const TrafficData* const`; the top-level const on the
        //  pointer return is a no-op for callers -> `const TrafficData*`.)
        const BrnTraffic::TrafficData* GetTrafficData() const;

        // BrnDirectorWorldMap.h:67 -- find the nearest safe position to lPosition
        // along lDisplacement; true on success with the result written into lOut.
        // Consumed by BrnDirector::Camera::Utils::PositionFinder::Update @0x8223FD50.
        // X360 @0x8223BA78: for a tiny displacement it forwards to GetSafePositionNearest;
        // otherwise it walks the lane (see .cpp) picking the direction from the displacement.
        bool GetSafePositionNearestPointWithDisplacement(Vector3 lPosition,
                                                         Vector3 lDisplacement,
                                                         Vector3& lOut) const;

        // BrnDirectorWorldMap.h:61 -- nearest safe position to lPosition (no displacement
        // bias). X360 @0x8223B750. Walks the containing hull's lane to the rung whose lane
        // frame straddles lPosition, then drops the point 2.25 below the lane along its up.
        bool GetSafePositionNearest(Vector3 lPosition, Vector3& lOut) const;

        // BrnDirectorWorldMap.h:82 -- the lane rung nearest lPosition (no direction bias).
        // X360 @0x8221CE98; returns the result by value (hidden sret).
        LanePosition GetLanePositionNearestPoint(Vector3 lPosition) const;

        // BrnDirectorWorldMap.h:109 -- the nearest Picture-Paradise generic region within
        // lfRadius of lNearTo. On a hit returns true and writes the region's box extents and
        // world transform. X360 @0x8221D2A0. Consumed by Camera::BehaviourRoadRunner::Update.
        bool GetInterestingPointNear(Vector3 lNearTo, f32 lfRadius, Vector3* lpExtentsOut,
                                     Matrix44Affine* lpTransformOut) const;

    private:
        // BrnDirectorWorldMap.h:121 -- follow left-lane neighbours from the current
        // (section, rung, parameter), rewriting all three in place into the reached lane's
        // frame, until no further left neighbour exists. X360 @0x821F7800.
        void WalkLaneLeft(const BrnTraffic::Hull* lpHull, u8* lpu8Section, u8* lpu8Rung,
                          f32* lpfParameterOnSection) const;

        // -- BrnDirectorWorldMap.h:143..149 layout (DWARF order) --------------
        CgsResource::ResourcePtr<BrnTraffic::TrafficData>    mpTrafficData;    // :143  X360 +0x00
        CgsResource::ResourcePtr<BrnTrigger::TriggerData>    mpTriggerData;    // :144
        CgsResource::ResourcePtr<BrnAI::AISectionsData>      mpAISectionData;  // :145
        CgsModule::EventReceiverQueue<128, 16>               mReceiverQueue;   // :147
        ELoadingState                                        meLoadingState;   // :149  X360 +0xF8
    };
}
