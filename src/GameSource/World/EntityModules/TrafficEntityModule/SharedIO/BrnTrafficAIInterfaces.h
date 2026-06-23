#pragma once

// Traffic-AI shared-IO interface + its event/entity payloads. Reconstructed from the DecFIGS
// DWARF (GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h)
// with element sizes/offsets confirmed against the X360 build.
//
// The OutputBuffer_PostScene buffer embeds TrafficAIInterface by value; its
// GetTrafficAIInterface() accessor (X360 0x827111C0) returns the sub-interface, whose producer
// (ConvertSceneResultsToTrafficDataForAI @ 0x82728518) reads the u16 entity count at offset 0
// and memcpy's 176-byte TrafficAIEntity records starting at byte offset 16 -- i.e.
// sizeof(TrafficAIEntity) == 176 and the active-entity cap is 256.
//
// The X360 build instantiates EventQueue<RivalInTrafficUpdateEvent,34>::Construct (0x82760750)
// against the RivalInTrafficUpdateEvent element type defined here.
#include "types.hpp"                                       // u16/s32/f32
#include "BrnCommonTypes.h"                                // Vector2, Vector3
#include "GameSource/BurnoutConstants.h"                   // EActiveRaceCarIndex, EGlobalRaceCarIndex
#include "GameShared/GameClasses/Module/CgsEventQueue.h"   // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Containers/CgsArray.h"    // Array<T,N>

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // DWARF :37/:38 -- traffic-AI proximity caps.
    const s32 KI_MAX_TRAFFIC_NEAR_A_RACECAR = 32;
    const s32 KI_MAX_TRAFFIC_NEAR_RACECARS  = 256;

    // DWARF :57 -- bounding-box corner count.
    const s32 KI_BB_NUM_CORNERS = 8;

    // DWARF :54 -- one AI-visible traffic vehicle. sizeof == 176 (X360-authoritative: the
    // ConvertSceneResultsToTrafficDataForAI memcpy stride is 176 bytes / 44 dwords). The two
    // Vector2 lanes and the Vector3[8] corner buffer are 16-byte SIMD, so:
    //   mVelocity @0, mCentre @16, meNearbyRaceCarIndex @32, maBBCorners @48 (8*16=128) -> 176.
    struct alignas(16) TrafficAIEntity
    {
        Vector2             mVelocity;             // :74
        Vector2             mCentre;               // :75
        EActiveRaceCarIndex meNearbyRaceCarIndex;  // :76
        Vector3             maBBCorners[8];        // :77 (KI_BB_NUM_CORNERS)
    };

    // DWARF :90 -- a rival's per-frame update pushed to the traffic system. The
    // EventQueue<RivalInTrafficUpdateEvent,34>::Construct (X360 0x82760750) points the base
    // queue at maEvents at struct offset 16, i.e. the 12-byte BaseEventQueue base is padded to
    // 16 by this element's 16-byte (Vector3 SIMD) alignment.
    struct alignas(16) RivalInTrafficUpdateEvent
    {
    public:
        void                Construct(Vector3 lTargetPosition, Vector3 lTargetDirection,
                                      f32 lfSpeed, EGlobalRaceCarIndex leRaceCarIndex); // :99
        Vector3             GetTargetPosition() const;   // :103
        Vector3             GetTargetDirection() const;  // :104
        f32                 GetSpeed() const;            // :105
        EGlobalRaceCarIndex GetRaceCarIndex() const;     // :106

    private:
        Vector3             mTargetPosition;   // :110
        Vector3             mTargetDirection;  // :111
        f32                 mfSpeed;           // :112
        EGlobalRaceCarIndex meRaceCarIndex;    // :113
    };

    // DWARF :128 -- the per-frame traffic -> AI interface published on the PostScene output
    // buffer. The PostScene buffer reaches it via GetTrafficAIInterface() (X360 0x827111C0).
    struct TrafficAIInterface
    {
    public:
        void                   Construct();                                          // :137
        void                   AddTrafficEntity(const TrafficAIEntity& lrEntity);    // :142
        const TrafficAIEntity* GetTrafficEntity(u16 lu16Index) const;                // :145
        u16                    GetTrafficEntityCount() const;                        // :148
        void                   SetRivalAddedToTraffic(EGlobalRaceCarIndex leIndex);  // :154
        void                   SetRivalRemovedFromTraffic(EGlobalRaceCarIndex leIndex); // :159
        void                   UpdateRivalInTrafficSystem(Vector3 lTargetPosition, Vector3 lTargetDirection,
                                                          f32 lfSpeed, EGlobalRaceCarIndex leRaceCarIndex); // :168
        const CgsModule::EventQueue<RivalInTrafficUpdateEvent, 34>* GetUpdateRivalQueue() const; // :172
        const Array<EGlobalRaceCarIndex, 34>* GetAddedRivals() const;                // :173
        const Array<EGlobalRaceCarIndex, 34>* GetRemovedRivals() const;              // :174

    private:
        u16             mu16EntityCount;                                  // :179 (offset 0)
        TrafficAIEntity maActiveEntityList[256];                          // :180 (offset 16)
        CgsModule::EventQueue<RivalInTrafficUpdateEvent, 34> mUpdateRivalQueue; // :182
        Array<EGlobalRaceCarIndex, 34> mAddedRivals;                      // :184
        Array<EGlobalRaceCarIndex, 34> mRemovedRivals;                    // :185
    };
}
}
