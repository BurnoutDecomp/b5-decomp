#pragma once

// Race-car -> traffic-system interface + its two event payloads. Reconstructed from the
// DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h).
// This is the home for the two traffic-system event element structs and their owning
// interface; the OutputBuffer_PostScene buffer embeds RaceCarToTrafficInterface by value
// and the X360 build instantiates EventQueue<RemoveRival,34>::Construct +
// BaseEventQueue<CreateRival/RemoveRival>::Append against these element types.
//
// Element sizes are X360-authoritative (XMemCpy strides): CreateRivalInTrafficSystemEvent
// is sizeof==48 (2x Vector3 SIMD (16+16) + EDistrict(4) + EGlobalRaceCarIndex(4) + s8 ->
// 48 with SIMD alignment), RemoveRivalFromTrafficSystemEvent is sizeof==1 (single s8).
#include "types.hpp"                                              // s8/s32/u32/f32
#include "BrnCommonTypes.h"                                       // Vector3
#include "SharedClasses/World/BrnWorldRegion.h"                   // BrnWorld::EDistrict
#include "GameSource/BurnoutConstants.h"                          // EGlobalRaceCarIndex
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue<T,N>

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    // DWARF :49 -- request to spawn a rival into the traffic system. sizeof == 48.
    struct alignas(16) CreateRivalInTrafficSystemEvent
    {
    public:
        void                Construct(Vector3 lSpawnPosition, Vector3 lSpawnDirection,
                                      BrnWorld::EDistrict leDistrict,
                                      EGlobalRaceCarIndex leRaceCarIndex, s8 li8RivalIndex); // :59
        Vector3             GetSpawnPosition() const;             // :62
        Vector3             GetSpawnDirection() const;            // :63
        BrnWorld::EDistrict GetDistrict() const;                  // :64
        EGlobalRaceCarIndex GetRaceCarIndex() const;              // :65
        s8                  GetRivalIndex() const;                // :66

    private:
        Vector3             mSpawnPosition;   // :70
        Vector3             mSpawnDirection;  // :71
        BrnWorld::EDistrict meDistrict;       // :72
        EGlobalRaceCarIndex meRaceCarIndex;   // :73
        s8                  miRivalIndex;     // :74
    };

    // DWARF :87 -- request to remove a rival from the traffic system. sizeof == 1.
    struct RemoveRivalFromTrafficSystemEvent
    {
    public:
        void Construct(s8 li8RivalIndex);     // :93
        s8   GetRivalIndex() const;           // :96

    private:
        s8   miRivalIndex;                    // :100
    };

    // DWARF :114 -- the per-frame race-car -> traffic interface published on the
    // PostScene output buffer.
    struct RaceCarToTrafficInterface
    {
    public:
        // DWARF :120 -- per-interface flags.
        enum Flag : s32
        {
            E_FLAG_PLAYER_IS_POWER_PARKING         = 0,
            E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND = 1,
            E_FLAG_COUNT                           = 2
        };

        void Construct();                                                         // :130
        void RequestCreateRival(Vector3 lSpawnPosition, Vector3 lSpawnDirection,
                                BrnWorld::EDistrict leDistrict,
                                EGlobalRaceCarIndex leRaceCarIndex, s8 li8RivalIndex); // :140
        void RemoveRival(s8 li8RivalIndex);                                       // :145
        void SetFlag(Flag leFlag, bool lbValue);                                  // :151
        bool IsFlagSet(Flag leFlag) const;                                        // :155
        void SetShowtimeTrafficDensityScale(f32 lfScale);                         // :160
        f32  GetShowtimeTrafficDensityScale() const;                              // :163
        const CgsModule::EventQueue<CreateRivalInTrafficSystemEvent, 34>* GetCreateRivalQueue() const; // :166
        const CgsModule::EventQueue<RemoveRivalFromTrafficSystemEvent, 34>* GetRemoveRivalQueue() const; // :167

    private:
        CgsModule::EventQueue<CreateRivalInTrafficSystemEvent, 34>  mCreateRivalQueue;        // :171
        CgsModule::EventQueue<RemoveRivalFromTrafficSystemEvent, 34> mRemoveRivalQueue;       // :172
        u32 muFlags;                                                                          // :174
        f32 mfShowtimeTrafficDensityScale;                                                    // :175
    };
}
}
