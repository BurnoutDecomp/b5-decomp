#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"          // CgsSound::Logic::Content (x4 banks)
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"    // BrnSound::Logic::BrnStateManager (base)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h" // TrafficSoundEntity

// BrnSound::Logic::Traffic::TrafficStateManager - the traffic-vehicle sound
// domain: 32 attachable traffic-entity slots plus the engine/horn AEMS banks and
// CSIS interfaces. Class shape / member names / enums from the DecFIGS DWARF
// (BrnTrafficStateManager.h); gated on the X360 ledger. This TU bodies the
// destructor; the rest of the DWARF surface (Prepare @cpp:84, ExitGamePlay,
// UpdateParams, ResourcesAreReady, the slot machinery, the type-info factory
// statics) is its own ledger functions (declaration-only here).
namespace CgsSound { namespace Logic { class State; } }

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{
    // DWARF BrnTrafficStateManager.h:38.
    enum ETrafficSize
    {
        E_SMALL     = 0,
        E_MEDIUM    = 1,
        E_LARGE     = 2,
        E_MAX_SIZES = 3,
    };

    struct TrafficStateManager : public BrnStateManager
    {
        // DWARF h:102 -- the distance-sort scratch record.
        struct SortResult
        {
            VecFloat mfDistance;   // h:110
            u16      muIndex;      // h:111

            // DWARF h:105 -- its own ledger function (declaration-only).
            static bool LessThanDistance(const SortResult& lrA, const SortResult& lrB);
        };

        // DWARF h:115 -- one attachable traffic-entity slot.
        struct Slot
        {
            BrnTraffic::BrnTrafficIO::TrafficSoundEntity mEntity;          // h:117
            bool                                          mbActive;        // h:118
            CgsSound::Logic::State*                       mpAttachedState; // h:119

            // DWARF h:116 -- its own ledger function (declaration-only).
            Slot();
        };

        static const u32 KU_NUM_SLOTS = 32;   // DWARF h:143 (maSlots[32])

        // @0x826FC308 (this TU) -- teardown: the four Content member dtors run the
        // release pattern, the base StateManager tears its content pool down; the
        // C++ body is empty.
        virtual ~TrafficStateManager();

        // DWARF cpp -- their own ledger functions (declaration-only here).
        TrafficStateManager();
        virtual bool Prepare();
        virtual void ExitGamePlay();
        virtual void UpdateParams(f32 lfTimeStep);
        virtual void ResourcesAreReady();
        const CgsSound::Logic::Content& GetEngineAemsBank();   // h:154
        const CgsSound::Logic::Content& GetHornAemsBank();     // h:171
        ETrafficSize TrafficClassToSize(u8 lu8TrafficClass);   // h:189

    protected:
        Slot                    maSlots[KU_NUM_SLOTS];    // h:143
        CgsSound::Logic::Content mEngineAemsBank;         // h:144 (X360 +0xCA0)
        CgsSound::Logic::Content mHornAemsBank;           // h:145 (X360 +0xCAC)
        CgsSound::Logic::Content mEngineCsisInterface;    // h:146 (X360 +0xCB8)
        CgsSound::Logic::Content mHornCsisInterface;      // h:147 (X360 +0xCC4)
    };
}
}
}
