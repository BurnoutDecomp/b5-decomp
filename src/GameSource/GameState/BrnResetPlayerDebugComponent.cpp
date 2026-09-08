#include "GameSource/GameState/BrnResetPlayerDebugComponent.h"

#include "GameSource/GameState/BrnGameStateModule.h"                 // BrnGameState::GameStateModule (full def + accessors)
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                       // CgsIDUnCompress
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<N,16>::AddEvent
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"              // CgsWorld::WorldMap2D::GetValue
#include "GameShared/GameClasses/Development/CgsStrStream.h"         // CgsDev::StrStream
#include "SharedClasses/DataLists/WheelList.h"                       // BrnResource::WheelList / WheelListEntry
#include "SharedClasses/DataLists/VehicleList.h"                     // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"               // BrnResource::VehicleListEntry
#include "SharedClasses/Trigger/BrnTriggerData.h"                    // BrnTrigger::TriggerData
#include "SharedClasses/Trigger/BrnGenericRegion.h"                  // BrnTrigger::GenericRegion (+ type-name table)
#include "SharedClasses/Trigger/BrnRegion.h"                         // BrnTrigger::BoxRegion::ComputeDirection / GetPosition
#include "SharedClasses/World/BrnWorldRegion.h"                      // BrnWorld::WorldRegion district/county helpers
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"

#include <cstring>   // strncmp

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The "Reset Player Car" debug menu
// (BrnGameState::ResetPlayerDebugComponent). It builds five menu lists off the loaded track +
// vehicle/wheel resources -- teleport locations, a car filter, the (filtered) cars, car versions
// and wheels -- and registers them plus the teleport / change-car actions with the debug UI. The
// teleport/change actions publish their requests onto the owning module's game-event carry queue.
//
// SOURCE-OF-TRUTH: behaviour + the AddEvent payload shapes/sizes are read off the X360 asm; the
// class layout / member names come from the DecFIGS DWARF; the named GameStateModule + resource
// accessors replace the X360's inlined raw-offset reads (no offset pokes in owned code).

namespace BrnGameState
{
    // ---- file-scope menu tables (DWARF BrnResetPlayerDebugComponent.cpp:37/51) ----------------------
    //
    // ARTIST pointer tables at 0x82CDB8D4 (labels) and 0x82CDB8FC (ID prefixes).
    static const s32 KI_CAR_FILTER_COUNT = 10;

    static const char* const KAPC_CAR_FILTER_STRINGS[KI_CAR_FILTER_COUNT] =
    {
        "All cars",   // filter 0 (X360 off_82CDB8D4[0])
        "Race cars", "Traffic cars", "US race cars", "Euro race cars", "Asian race cars",
        "Special race cars", "US Trophy race cars", "Euro Trophy race cars", "Asian Trophy race cars",
    };

    static const char* const KAPC_CAR_FILTER_PREFIXES[KI_CAR_FILTER_COUNT] =
    {
        "",   // filter 0: no prefix -> every car passes (X360 off_82CDB8FC[0])
        "P", "T", "PUS", "PEU", "PAS",
        "PSP", "XUS", "XEU", "XAS",
    };

    // ARTIST uses 100 car slots; the older DWARF declares 96.
    static const s32 KI_CAR_MENU_LOOP_LIMIT   = 100;   // OnChangeCarFilter break (cmpwi 0x64)
    static const s32 KI_CAR_MENU_MAX_INDEX    = 99;    // car-index SetRange clamp (cmpwi 0x63)
    static const s32 KI_VERSION_MENU_MAX_INDEX = 15;   // car-version SetRange clamp (cmpwi 0xF)
    static const s32 KI_WHEEL_MENU_MAX_INDEX  = 127;   // wheel SetRange clamp (cmpwi 0x7F)
    static const s32 KI_LOCATION_MENU_LIMIT   = 128;   // region-loop location cap (cmpwi 0x80)

    // ------------------------------------------------------------------------------------------------
    // Construct @ X360 0x82357940
    // ------------------------------------------------------------------------------------------------
    void ResetPlayerDebugComponent::Construct(GameStateModule* lpGameStateModule)
    {
        // Base init (the X360 folds the CgsDev::DebugComponent::Construct body in via ICF).
        DebugComponent::Construct();

        CGS_ASSERT(lpGameStateModule != nullptr, "lpGameStateModule != NULL");

        mpGameStateModule        = lpGameStateModule;
        miCurrentLocationIndex   = 0;
        miCurrentCarFilter       = 1;
        miCurrentCarIndex        = 0;
        miCurrentCarVersionIndex = 0;
        miCurrentWheelIndex      = 0;
        mbShowCarInfo            = false;
    }

    void ResetPlayerDebugComponent::Destruct()
    {
        DebugComponent::Destruct();
    }

    // ------------------------------------------------------------------------------------------------
    // GetName @ X360 0x823579C8
    // ------------------------------------------------------------------------------------------------
    const char* ResetPlayerDebugComponent::GetName() const
    {
        return "Reset Player Car";
    }

    // ------------------------------------------------------------------------------------------------
    // TeleportCar @ X360 0x82382BC0 (the body the TeleportCarCallback trampoline forwards to)
    //
    // Publish a "teleport player car" event carrying the currently-selected location's position and
    // direction. The X360 reads the two 16-byte vectors out of maLocationPositions/Directions at
    // miCurrentLocationIndex and packs them into a 32-byte event record.
    // ------------------------------------------------------------------------------------------------
    void ResetPlayerDebugComponent::TeleportCar()
    {
        GameStateModuleIO::TeleportPlayerCarEvent lEvent = {};
        lEvent.mPosition = maLocationPositions[miCurrentLocationIndex];
        lEvent.mDirection = maLocationDirections[miCurrentLocationIndex];
        mpGameStateModule->GetDebugGameEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent),
            GameStateModuleIO::E_EVENT_TELEPORT_PLAYER_CAR, sizeof(lEvent));
    }

    // ARTIST 0x82382B20 checks the full current-mode pointer, then queues the
    // selected model and wheel with camera-reset and keep-reset-section enabled.
    void ResetPlayerDebugComponent::ChangeCar()
    {
        if (mpGameStateModule->GetModeManager()->GetCurrentGameMode() != nullptr)
            return;
        GameStateModuleIO::ChangePlayerCarEvent lEvent = {};
        lEvent.mCarModelId = maCarVersionIds[miCurrentCarVersionIndex];
        lEvent.mWheelModelId = mpGameStateModule->GetWheelList()->GetWheelData(miCurrentWheelIndex)->mID;
        lEvent.mbResetPlayerCamera = true;
        lEvent.mbKeepResetSection = true;
        mpGameStateModule->GetDebugGameEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent),
            GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR, sizeof(lEvent));
    }

    // ------------------------------------------------------------------------------------------------
    // TeleportCarCallback @ X360 0x82382BC0 / ChangeCarCallback @ 0x82382C28
    // Static menu-callback trampolines (tail-call to the member body; the void* is this component).
    // ------------------------------------------------------------------------------------------------
    void ResetPlayerDebugComponent::TeleportCarCallback(void* lpData)
    {
        static_cast<ResetPlayerDebugComponent*>(lpData)->TeleportCar();
    }

    void ResetPlayerDebugComponent::ChangeCarCallback(void* lpData)
    {
        static_cast<ResetPlayerDebugComponent*>(lpData)->ChangeCar();
    }

    void ResetPlayerDebugComponent::OnChangeCarFilterCallback(void* /*lpData*/, void* lpUserData)
    {
        static_cast<ResetPlayerDebugComponent*>(lpUserData)->OnChangeCarFilter();
    }

    void ResetPlayerDebugComponent::OnChangeCarSelectionCallback(void* /*lpData*/, void* lpUserData)
    {
        static_cast<ResetPlayerDebugComponent*>(lpUserData)->OnChangeCarSelection();
    }

    // ------------------------------------------------------------------------------------------------
    // OnChangeCarFilter @ X360 0x82382C30
    //
    // Rebuild the car-name menu list, keeping only vehicles whose display name starts with the
    // currently-selected filter's prefix and that are not "hidden" (the X360 skips an entry whose
    // VehicleListEntry parent-id qword @+8 is non-zero). For each kept vehicle, build a "<name> -
    // <id>" label, record the vehicle id, and pre-select the option matching the player's active
    // car. Finally clamp the index range and re-run the selection.
    // ------------------------------------------------------------------------------------------------
    void ResetPlayerDebugComponent::OnChangeCarFilter()
    {
        const BrnResource::VehicleList* lpVehicleList = mpGameStateModule->GetVehicleList();
        const char* lpcFilterPrefix = KAPC_CAR_FILTER_PREFIXES[miCurrentCarFilter];

        miCurrentCarIndex = 0;
        s32 liKeptCount = 0;

        const s32 liVehicleCount = lpVehicleList->GetVehicleCount();
        for (s32 liVehicle = 0; liVehicle < liVehicleCount; ++liVehicle)
        {
            if (liKeptCount >= KI_CAR_MENU_LOOP_LIMIT)
            {
                break;
            }

            const BrnResource::VehicleListEntry* lpEntry = lpVehicleList->GetVehicleData(liVehicle);

            // Skip "child"/variant entries (the X360 tests the parent-id qword @+8 != 0).
            if (lpEntry->GetParentId() != 0)
            {
                continue;
            }

            // Expand the car id to its printable form and apply the filter prefix.
            char lacCarId[KI_CGSID_STRING_LEN];
            const CgsID lCarId = lpEntry->GetId();
            CgsIDUnCompress(lCarId, lacCarId);

            const s32 liPrefixLen = static_cast<s32>(::strlen(lpcFilterPrefix));
            if (::strncmp(lacCarId, lpcFilterPrefix, liPrefixLen) != 0)
            {
                continue;
            }

            // Build the "<vehicle name> - <id>" label into this option's backing string. The X360
            // streams the entry's display name (entry+0x30), falling back to "<NULLSTRING>" when null.
            char* lpcLabel = maCarStrings[liKeptCount];
            lpcLabel[0] = '\0';
            CgsDev::StrStream lLabelStream(lpcLabel, KI_CAR_TEXT_LENGTH);
            const char* lpcVehicleName = lpEntry->GetName();
            lLabelStream << (lpcVehicleName != nullptr ? lpcVehicleName : "<NULLSTRING>");
            lLabelStream << " - ";
            lLabelStream << lacCarId;

            maCarNames[liKeptCount].miValue = liKeptCount;
            maCarNames[liKeptCount].mpcName = lpcLabel;
            maCarIds[liKeptCount]           = lCarId;

            // Pre-select this option when it is the player's currently-active car (X360 compares the
            // active player car id at GSM+0x456D8 against this entry's id).
            if (mpGameStateModule->GetActivePlayerCarId() == lCarId)
            {
                miCurrentCarIndex = liKeptCount;
            }

            ++liKeptCount;
        }

        s32 liMaxIndex = liKeptCount - 1;
        if (liMaxIndex >= KI_CAR_MENU_MAX_INDEX)   // X360 clamps to 99
        {
            liMaxIndex = KI_CAR_MENU_MAX_INDEX;
        }
        SetRange(&miCurrentCarIndex, 0, liMaxIndex);

        OnChangeCarSelection();
    }

    // ------------------------------------------------------------------------------------------------
    // ARTIST 0x82376F80: select the default wheel, collect the parent and its
    // variants, then select the first variant. Compare full 64-bit IDs.
    void ResetPlayerDebugComponent::OnChangeCarSelection()
    {
        const BrnResource::VehicleList* lpVehicleList = mpGameStateModule->GetVehicleList();
        const BrnResource::WheelList* lpWheelList = mpGameStateModule->GetWheelList();
        const CgsID lCarId = maCarIds[miCurrentCarIndex];
        const s32 liCarIndex = lpVehicleList->GetVehicleIndex(lCarId);
        const BrnResource::VehicleListEntry* lpCar = lpVehicleList->GetVehicleData(liCarIndex);
        for (s32 liWheel = 0; liWheel < lpWheelList->GetWheelCount(); ++liWheel)
        {
            if (::_stricmp(lpWheelList->GetWheelData(liWheel)->macName, lpCar->GetDefaultWheelName()) == 0)
            {
                miCurrentWheelIndex = liWheel;
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "Wheel index = " << miCurrentWheelIndex << "\n";
                break;
            }
        }

        s32 liVersionCount = 0;
        for (s32 liVehicle = 0; liVehicle < lpVehicleList->GetVehicleCount() &&
             liVersionCount < KI_MAX_CAR_VERSION_NAME_COUNT; ++liVehicle)
        {
            const BrnResource::VehicleListEntry* lpEntry = lpVehicleList->GetVehicleData(liVehicle);
            if (lpEntry->GetId() != lCarId && lpEntry->GetParentId() != lCarId)
                continue;
            char lacId[KI_CGSID_STRING_LEN];
            CgsIDUnCompress(lpEntry->GetId(), lacId);
            CgsDev::StrStream lLabel(maCarVersionStrings[liVersionCount], KI_CAR_TEXT_LENGTH);
            lLabel << (lpEntry->GetName() ? lpEntry->GetName() : "<NULLSTRING>") << " - " << lacId;
            maCarVersionNames[liVersionCount].miValue = liVersionCount;
            maCarVersionNames[liVersionCount].mpcName = maCarVersionStrings[liVersionCount];
            maCarVersionIds[liVersionCount] = lpEntry->GetId();
            ++liVersionCount;
        }
        SetRange(&miCurrentCarVersionIndex, 0, liVersionCount - 1);
        miCurrentCarVersionIndex = 0;
    }

    // ------------------------------------------------------------------------------------------------
    // OnActivate @ X360 0x82390F50
    //
    // (Re)build every menu list off the loaded resources and register the variables + actions.
    // ------------------------------------------------------------------------------------------------
    void ResetPlayerDebugComponent::OnActivate()
    {
        const BrnTrigger::TriggerData* lpTriggerData = mpGameStateModule->GetTriggerQueryManager()->GetTriggerData();

        // --- car-filter list: seed the 10 filter options from the label table -----------------------
        for (s32 liFilter = 0; liFilter < KI_CAR_FILTER_COUNT; ++liFilter)
        {
            maCarFilterNames[liFilter].miValue = liFilter;
            maCarFilterNames[liFilter].mpcName = KAPC_CAR_FILTER_STRINGS[liFilter];
        }

        // --- car list / car-version list: start every option pointing at the empty string ----------
        for (s32 liCar = 0; liCar < KI_MAX_CAR_NAME_COUNT; ++liCar)
        {
            maCarNames[liCar].miValue = 0;
            maCarNames[liCar].mpcName = "";
            maCarStrings[liCar][0]    = '\0';
        }
        for (s32 liVersion = 0; liVersion < KI_MAX_CAR_VERSION_NAME_COUNT; ++liVersion)
        {
            maCarVersionNames[liVersion].miValue = 0;
            maCarVersionNames[liVersion].mpcName = "";
            maCarVersionStrings[liVersion][0]    = '\0';
        }

        // --- wheel list: enumerate the loaded wheels, recording the option-id and the index that
        //     matches the player's currently-equipped wheel ------------------------------------------
        const BrnResource::WheelList* lpWheelList = mpGameStateModule->GetWheelList();
        const s32 liWheelCount = lpWheelList->GetWheelCount();
        for (s32 liWheel = 0; (liWheel < liWheelCount) && (liWheel < KI_MAX_WHEEL_NAME_COUNT); ++liWheel)
        {
            // (X360 also guards liWheel >= 128 == KI_MAX_WHEEL_NAME_COUNT, matched by the loop bound.)
            const BrnResource::WheelListEntry* lpWheelEntry = lpWheelList->GetWheelData(liWheel);

            // Pre-select the wheel that matches the player's currently-equipped wheel (X360 compares
            // this entry's id at WheelData+0 against the active player wheel id at GSM+0x456E0).
            if (lpWheelEntry->mID == mpGameStateModule->GetActivePlayerWheelId())
            {
                miCurrentWheelIndex = liWheel;
            }

            maWheelNames[liWheel].miValue = liWheel;
            // X360 stores &entry->macName (the wheel record's name field, entry + 8) as the option name.
            maWheelNames[liWheel].mpcName = lpWheelEntry->macName;
        }

        // --- teleport-location list: walk the track's generic regions, transform each region's box
        //     into world space, sample the district map for its county/district and build a label ----
        s32 liLocationCount = 0;
        const s32 liRegionCount = lpTriggerData->GetGenericRegionCount();
        for (s32 liRegion = 0; liRegion < liRegionCount; ++liRegion)
        {
            if (liLocationCount >= KI_LOCATION_MENU_LIMIT)   // X360 caps at 128 (cmpwi 0x80)
            {
                break;
            }

            const BrnTrigger::GenericRegion* lpRegion = lpTriggerData->GetGenericRegion(liRegion);

            // Sample the district map at the region's centre (a w lane of 0 is filled before the
            // GetValue(Vector3) call; an off-map sample reads back as E_DISTRICT_INVALID).
            Vector3 lSamplePos = lpRegion->GetBoxRegion()->GetPosition();
            lSamplePos.w = 0.0f;

            CgsWorld::WorldMap2D* lpDistrictMap = mpGameStateModule->GetDistrictMap();
            const u8 luDistrictByte = lpDistrictMap->GetValue(lSamplePos);

            // Off-map sample (255) -> the INVALID district (X360 falls back to district id 18 ==
            // E_DISTRICT_INVALID == E_DISTRICT_VALID_COUNT); an in-range sample is range-asserted.
            BrnWorld::EDistrict leDistrict;
            if (luDistrictByte == CgsWorld::KU_INVALID_WORLD_MAP_VALUE)
            {
                leDistrict = BrnWorld::E_DISTRICT_INVALID;
            }
            else
            {
                CGS_ASSERT(luDistrictByte < BrnWorld::E_DISTRICT_COUNT, "leDistrict < E_DISTRICT_COUNT");
                leDistrict = static_cast<BrnWorld::EDistrict>(luDistrictByte);
            }
            const BrnWorld::ECounty leCounty = BrnWorld::WorldRegion::DistrictToCounty(leDistrict);

            // Only enclose-able regions get a teleport entry (the X360 keeps meType == 0 == JUNK_YARD
            // or meType == 1 == GAS_STATION).
            const BrnTrigger::GenericRegion::Type leType = lpRegion->GetType();
            if (!(leType == BrnTrigger::GenericRegion::E_TYPE_GAS_STATION ||
                  leType == BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD))
            {
                continue;
            }

            // Build "<N>: <region type> in <district>, <county>" into this location's backing
            // string, where N == liLocationCount + 1 is the 1-based ordinal of this teleport entry.
            char* lpcLabel = maLocationStrings[liLocationCount];
            CgsDev::StrStream lLabelStream(lpcLabel, KI_LOCATION_TEXT_LENGTH);

            // Stream the 1-based ordinal then ": " BEFORE the type name. The X360 (@0x8239127C /
            // 0x823912A8) loads liLocationCount (var_140), computes `liLocationCount + 1` (addi
            // r5,r11,1) and renders it via StrStreamBase::AppendFormat -- the integer operator<<
            // overload -- choosing one of two runtime format strings by region type (the JUNK_YARD
            // path @0x823912A8 vs the GAS_STATION path @0x8239127C; both kept types are < 3 so the
            // ordinal is always emitted). It then streams var_FC == ": " (asc_82010A2C) @0x823912C0.
            lLabelStream << (liLocationCount + 1);
            lLabelStream << ": ";

            CGS_ASSERT(static_cast<u32>(leType) < BrnTrigger::GenericRegion::E_TYPE_COUNT,
                       "meType < E_TYPE_COUNT");
            const char* lpcTypeName =
                BrnTrigger::GenericRegion::KAPC_GENERIC_REGION_TYPE_STRINGS[static_cast<s32>(leType)];
            if (lpcTypeName == nullptr)
            {
                lpcTypeName = "<NULLSTRING>";
            }
            lLabelStream << lpcTypeName;
            lLabelStream << " in ";
            lLabelStream << BrnWorld::WorldRegion::DistrictToString(leDistrict);
            lLabelStream << ", ";
            lLabelStream << BrnWorld::WorldRegion::CountyToString(leCounty);

            // ARTIST 0x823913F0..0x82391410: position + facing * dimensionZ.
            const BrnTrigger::BoxRegion* lpBox = lpRegion->GetBoxRegion();
            const Vector3 lFacing   = lpBox->ComputeDirection();
            const Vector3 lPosition = lpBox->GetPosition();
            const f32     lfDimZ    = lpBox->GetDimensionZ();

            Vector3 lSpawnPoint;
            lSpawnPoint.x = lFacing.x * lfDimZ + lPosition.x;
            lSpawnPoint.y = lFacing.y * lfDimZ + lPosition.y;
            lSpawnPoint.z = lFacing.z * lfDimZ + lPosition.z;
            lSpawnPoint.w = lFacing.w * lfDimZ + lPosition.w;

            maLocationDirections[liLocationCount] = lFacing;       // X360 +0x37A0
            maLocationPositions[liLocationCount]  = lSpawnPoint;   // X360 +0x2FA0

            maLocationNames[liLocationCount].miValue = liLocationCount;
            maLocationNames[liLocationCount].mpcName = lpcLabel;
            ++liLocationCount;
        }

        // --- register the action + the five menu variables -----------------------------------------
        RegisterFunction(&ResetPlayerDebugComponent::TeleportCarCallback, this, "Teleport player car");

        RegisterVariable(&miCurrentLocationIndex, "Location");
        SetOptions(&miCurrentLocationIndex, maLocationNames);
        SetRange(&miCurrentLocationIndex, 0, liLocationCount - 1);

        RegisterFunction(&ResetPlayerDebugComponent::ChangeCarCallback, this, "Change player car");

        RegisterVariable(&miCurrentCarFilter, "Car filter");
        SetOptions(&miCurrentCarFilter, maCarFilterNames);
        SetRange(&miCurrentCarFilter, 0, KI_CAR_FILTER_COUNT - 1);
        SetChangeCallback(&miCurrentCarFilter, &ResetPlayerDebugComponent::OnChangeCarFilterCallback, this);

        RegisterVariable(&miCurrentCarIndex, "Car");
        SetOptions(&miCurrentCarIndex, maCarNames);
        SetChangeCallback(&miCurrentCarIndex, &ResetPlayerDebugComponent::OnChangeCarSelectionCallback, this);
        {
            // X360 reads the vehicle count at VehicleList+0x3400 (GetVehicleCount), clamps to 99.
            s32 liMax = mpGameStateModule->GetVehicleList()->GetVehicleCount() - 1;
            if (liMax >= KI_CAR_MENU_MAX_INDEX)
            {
                liMax = KI_CAR_MENU_MAX_INDEX;
            }
            SetRange(&miCurrentCarIndex, 0, liMax);
        }

        RegisterVariable(&miCurrentCarVersionIndex, "Car version");
        SetOptions(&miCurrentCarVersionIndex, maCarVersionNames);
        // X360 re-arms the selection hook on the car-index slot here (same callback as above).
        SetChangeCallback(&miCurrentCarIndex, &ResetPlayerDebugComponent::OnChangeCarSelectionCallback, this);
        {
            // X360 reads the same vehicle count, clamps the version range to 15.
            s32 liMax = mpGameStateModule->GetVehicleList()->GetVehicleCount() - 1;
            if (liMax >= KI_VERSION_MENU_MAX_INDEX)
            {
                liMax = KI_VERSION_MENU_MAX_INDEX;
            }
            SetRange(&miCurrentCarVersionIndex, 0, liMax);
        }

        RegisterVariable(&miCurrentWheelIndex, "Wheel");
        SetOptions(&miCurrentWheelIndex, maWheelNames);
        {
            // X360 reads the wheel count at WheelList+0x1000 (GetWheelCount), clamps to 127.
            s32 liMax = lpWheelList->GetWheelCount() - 1;
            if (liMax >= KI_WHEEL_MENU_MAX_INDEX)
            {
                liMax = KI_WHEEL_MENU_MAX_INDEX;
            }
            SetRange(&miCurrentWheelIndex, 0, liMax);
        }

        RegisterVariable(&mbShowCarInfo, "Show car info");

        OnChangeCarFilter();
    }
}
