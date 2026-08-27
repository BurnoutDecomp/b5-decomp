// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_SendSetUpAllEventStarts.cpp
//
// ⭐⭐ THE EVENT-START TABLE PRODUCER -- BrnGameState::GameStateModule::
//     SendSetUpAllEventStartsMessage @ 0x823759D0
//
// This is the ONE function in the image that ever reaches
// SetUpAllEventStartsInterface::AddEventStart @0x82361398, i.e. the ONE thing that ever puts a
// record in an event-start table -- the GameState-side one it builds here, and, one memcpy hop
// later, the GUI cache's copy at GuiCache+0x5690. Until it landed, that table was permanently
// empty on this build and BrnGui::GuiCache::GetProfileEventDisplayInfo -- reached every drive
// from SatNavRenderer::RefreshSatNavIconInfo -- walked a zero-length array and fired the
// console's own "Unable to find event start with event id: " assert on every attempt. That is
// the FLAG BrnGameStateModule.cpp's PreWorldUpdateSetupPlayerCarBringUp used to carry and which
// this TU retires; the storm's other half (the WorldDataController readiness gate) is a
// separate wire, named at the bottom of this banner.
//
// WHAT IT DOES, leg by leg, from the ARTIST asm (r31 == this, r29 == lpOutput):
//   0x82375A08  TrafficData_::GetMemor(this + 43920)  -- 43920 == mTriggerQueryManager
//               (this + 42320) + 0x640, i.e. the inlined TriggerQueryManager::GetTrafficData()
//               (the SAME adjust GameStateModule_gSR_00.cpp:305 already names).
//   0x82375A14  operator new[](0x40000)               -- a 256 KB scratch arena, adopted by a
//   0x82375A24  LinearMalloc::Construct/Create           stack LinearMalloc.
//   0x82375A48  ResourcePtr<AISectionsData>::operator->(this + 181300) then
//   0x82375A50  AISectionsData::BuildAISectionPointMap(&arena) -- the spatial index the
//               per-junction nearest-section query below needs. [FLAG: the OWNER of that
//               resource pointer differs on this build -- see THE ONE DEVIATION below.]
//   0x82375A8C  WorldMap2D::Construct(districtBlob->GetData(), ORIGIN, SIZE) over the
//               StreetManager's "Districts" handle (this + 0x47470 == mStreetManager +0x1D08).
//               The two vectors are the SAME dynamic-initialiser pair BrnStuntManager.cpp
//               recovered (unk_82FADED0 == origin, unk_82FAE140 == size) -- both call sites
//               load v1 <- unk_82FADED0 and v2 <- unk_82FAE140, byte for byte.
//   then, for every LIGHT TRIGGER of every HULL of the loaded TrafficData:
//     0x82375BB4  id = (hull << 8) | 0x39000000 | triggerIndex   (LightTriggerId::Set, inlined)
//     0x82375BC0  box = TrafficData::GetJunctionLogicBoxForTrafficLight(id)
//     0x82375BD4  KEEP ONLY junctions that have BOTH start grids:
//                 GetStartDataForTrafficLight(id, false) && GetStartDataForTrafficLight(id, true)
//                 -- an event can be raced offline AND online from these lights. The miss arm is
//                 the console's own gated WARNING print, reproduced below.
//     0x82375C04  pos = ExpandPosPlusYRotToTransform(trigger.mPosPlusYRot).Pos()
//     0x82375C28  district = WorldMap2D::GetValue(pos)   [assert != 255, cpp:5764]
//     0x82375C74  section  = AISectionsData::FindNearestAISection(pos, pointMap)
//     0x82375CAC  AddEventStart(box->GetPosition(), box->GetID(), box->GetEventJunctionID(),
//                               id, DistrictToCounty(district), section)
//   0x82375E3C  operator delete[](arena)
//   0x82375E50  memcpy(lpOutput + 0x2B0F0, &localInterface, 0x20E0)
//   0x82375E5C  lpOutput->SetSetUpAllEventStartsInterfaceIsValid(true)
//
// ⚠️ THE POSITION IN THE RECORD IS THE JUNCTION'S, NOT THE TRIGGER'S. Easy to get backwards:
// the light-trigger transform is used ONLY to sample the district map and to find the nearest AI
// section; the vector that goes INTO the record (and therefore onto the sat-nav map) is
// `lvx128 v127, r30, r16` with r16 == 0x110 and r30 == the JunctionLogicBox, i.e.
// JunctionLogicBox::GetPosition(). Two different positions, two different jobs.
//
// ⚠️ THE DISTRICT SAMPLE IS (x, z), NOT (x, y). The console vperms the Vector3 through
// unk_82CDA450 into a two-lane vector before calling WorldMap2D::GetValue @0x82375C20; the
// ground-plane pair is what every other district sample in the tree uses too
// (BoxRegion::GetPosition2D() == { x, z }, BrnRegion.h:103). Spelled as an explicit Vector2 here
// so the lane choice is readable instead of hidden in a permute mask.
//
// ⛔ THE ONE DEVIATION, and it is an OWNER swap, not a data swap. The console reads the AI-section
// resource off mProgressionManager.mpAISectionData (the `this + 181300` adjust above: 47920 +
// 133380). NOTHING IN THIS TREE BINDS THAT MEMBER -- ProgressionManager::LoadProgressionData binds
// mpProgressionData and nothing else, so operator-> there would fire the ResourcePtr's own
// "Can not instance resource pointer" assert and then BuildAISectionPointMap would run on a null.
// mStreetManager.mpAISectionData IS the same AI-lanes acquire and IS bound (StreetManager::
// LoadAIData), so it is read from there, through the accessor that names the swap.
// DELETE-WHEN ProgressionManager::mpAISectionData gets its console binder.
//
// ⚠️ [FLAG PC bring-up] TWO NULL GUARDS THE CONSOLE DOES NOT HAVE, both on the boot path and both
// one-shot-logged rather than silent -- see the bodies. The console can afford the raw
// dereference because its TrafficData / AI / district resources are all resident long before the
// one-shot latch fires; on PC any of the three can still be unbound at that moment, and a null
// here is a boot-critical AV rather than a degraded table.
//
// ⛔ WHAT THIS DOES **NOT** FIX, stated so nobody quotes this TU as closing the whole storm:
// the sat-nav's OTHER two asserts on the same frame ("E_WORLDDATACONTROLLERSTATE_READY <= meState"
// @BrnGuiWorldDataController.cpp:374 and its downstream "lpRaceEventData" @BrnSatNavRenderer.cpp
// :1307) come from BrnGui::WorldDataController, not from here. Its progression binding lands in
// this same wave (WorldDataController::Prepare2 @0x82516CB8); its meState gate does not -- that
// needs the GetFreeburnChallengeList reply, which is deliberately parked (see the banner in
// BrnGuiWorldDataController.cpp).
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer + the interface accessor
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // SetUpAllEventStartsInterface

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"           // TrafficData / GetHull / the two lookups
#include "SharedClasses/Traffic/BrnTrafficHull.h"                       // Hull::mpaLightTriggers / muNumLightTriggers
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"               // LightTrigger + KU_LIGHT_TRIGGER_ID_OWNER_TAG
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"            // BrnTraffic::KU_MAX_HULLS
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"        // JunctionLogicBox::GetID/GetEventJunctionID/GetPosition
#include "SharedClasses/AI/AISectionsResourceType.h"                    // AISectionsData::BuildAISectionPointMap / FindNearestAISection
#include "SharedClasses/World/BrnWorldRegion.h"                         // BrnWorld::WorldRegion::DistrictToCounty
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                 // CgsWorld::WorldMap2D
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"              // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"   // CgsResource::ResourceHandle
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h" // CgsResource::BinaryFileResource::GetData
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // Assert::BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint + Message::gxMessageFilterFlags

namespace BrnGameState
{

namespace
{
    // Verbatim X360-baked assert paths for this function's three asserts.
    const char* const KAC_LIGHT_TRIGGER_H =
        "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficLightTrigger.h";
    const char* const KAC_GAMESTATEMODULE_CPP =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp";

    // The scratch arena the AI-section point map is built out of: `lis r3, 4` @0x82375A10, i.e.
    // operator new[](0x40000). Freed at 0x82375E3C before the publish.
    const size_t KN_AI_POINT_MAP_ARENA_BYTES = 0x40000;

    // ⭐ The district-map world rect. THE SAME PAIR BrnStuntManager.cpp recovered from the MSVC
    // dynamic-initialiser thunks (0x82C4CD88 -> unk_82FADED0, 0x82C4CDC8 -> unk_82FAE140); both
    // are zero in the static image, which is why they are absent from the JSON export set. The
    // argument registers settle which is which and BOTH call sites agree byte for byte:
    //     StuntManager::Prepare           @0x8239CA70  lvx128 v1 <- unk_82FADED0  (== ORIGIN)
    //     SendSetUpAllEventStartsMessage  @0x82375A78  lvx128 v1 <- unk_82FADED0
    //                                     @0x82375A6C  lvx128 v2 <- unk_82FAE140  (== SIZE)
    // Duplicated here rather than shared because StuntManager's copies are TU-local file statics;
    // if either pair is ever re-tuned, both must move together.
    const Vector2 KV2_DISTRICT_MAP_WORLD_ORIGIN = { -4208.0f, -3846.0f, 0.0f, 0.0f }; // unk_82FADED0
    const Vector2 KV2_DISTRICT_MAP_WORLD_SIZE   = {  8270.0f,  6101.0f, 0.0f, 0.0f }; // unk_82FAE140
}

// ============================================================================
// X360 0x823759D0 -- SendSetUpAllEventStartsMessage. See the file banner.
// ============================================================================
void GameStateModule::SendSetUpAllEventStartsMessage(GameStateModuleIO::OutputBuffer* lpOutput)
{
    // The console's `GetMemor(this + 43920)` -- TriggerQueryManager::GetTrafficData(), inlined.
    const BrnTraffic::TrafficData* const lpTrafficData = mTriggerQueryManager.GetTrafficData();

    // ⚠️ [FLAG PC bring-up] NOT IN THE X360 BINARY. The console dereferences the answer directly
    // (`lhz r10, 2(r21)` @0x82375A98) because B5TRAFFIC.BNDL is resident long before this latch;
    // on PC the acquire can still be outstanding. Publishing an EMPTY-but-valid table here would
    // be the silent-drop shape -- the GUI would then resolve nothing and never be told why -- so
    // the message is not sent at all and the valid flag stays false, exactly as it is before the
    // latch fires. The one-shot log names the missing resource.
    // DELETE-WHEN the traffic-data acquire is proven complete at the one-shot latch on PC.
    if (lpOutput == 0 || lpTrafficData == 0)
    {
        static bool sbNoTrafficDataLogged = false;
        if (!sbNoTrafficDataLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbNoTrafficDataLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[event-starts] SKIPPED: SendSetUpAllEventStartsMessage has no TrafficData "
                   "(TriggerQueryManager::GetTrafficData() == NULL); the GUI event-start table "
                   "stays empty this boot.\n";
        }
        return;
    }

    // ---- the AI-section point map (0x82375A14..0x82375A50) ---------------------------------
    // See the banner's ⛔ for why this reads the StreetManager's copy of the resource.
    const BrnAI::AISectionsData* const lpAISectionsData = mStreetManager.GetAISectionData();

    u8* const lpArena = new u8[KN_AI_POINT_MAP_ARENA_BYTES];
    CgsMemory::LinearMalloc lArena;
    lArena.Construct();
    lArena.Create(lpArena, KN_AI_POINT_MAP_ARENA_BYTES);

    // ⚠️ [FLAG PC bring-up] The console has no null arm here either (its `sub_82367718` asserts
    // and returns the pointer regardless). A record with no AI-section index is still a USEFUL
    // record -- the sat-nav lookups key on the event id, not on the section -- so the table is
    // still published; only this one field falls back to the tree's own invalid sentinel.
    // DELETE-WHEN the AI-lanes acquire is proven complete at the one-shot latch.
    BrnAI::AISectionPointMap* lpAISectionPointMap = 0;
    if (lpAISectionsData != 0)
    {
        lpAISectionPointMap = lpAISectionsData->BuildAISectionPointMap(&lArena);
    }
    else if (CgsDev::Log::gpDebugPrint != 0)
    {
        static bool sbNoAILogged = false;
        if (!sbNoAILogged)
        {
            sbNoAILogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[event-starts] DEGRADED: no AISectionsData bound; every event start records "
                   "AI section KI_INVALID_SECTION_INDEX.\n";
        }
    }

    // ---- the district map (0x82375A54..0x82375A8C) ------------------------------------------
    // The console builds this unconditionally off the StreetManager's "Districts" handle; the
    // same not-ready shape StreetManager::SetupParRivals already guards for is guarded here, and
    // an unbound map simply leaves mbDistrictMapBound false so the per-junction county falls back
    // to E_COUNTY_INVALID (the value the StuntManager's own off-map arm uses).
    CgsWorld::WorldMap2D lDistrictMap;
    bool                 lbDistrictMapBound = false;
    {
        const CgsResource::ResourceHandle* const lpHandle =
            mStreetManager.GetDistrictMapResourceHandle();
        if (lpHandle != 0 && lpHandle->mpResourceMemory != 0)
        {
            // The handle slot holds a pointer to the resource-memory pointer; the X360 folds
            // BinaryFileResource::GetData() into its caller as `base + *(u32*)(base + 4)`
            // (0x82375A80..0x82375A88), the same three loads SetupParRivals @0x8233F5EC makes.
            const CgsResource::BinaryFileResource* const lpBlob =
                *reinterpret_cast<const CgsResource::BinaryFileResource* const*>(
                    lpHandle->mpResourceMemory);
            if (lpBlob != 0)
            {
                lDistrictMap.Construct(lpBlob->GetData(),
                                       KV2_DISTRICT_MAP_WORLD_ORIGIN,
                                       KV2_DISTRICT_MAP_WORLD_SIZE);
                lbDistrictMapBound = true;
            }
        }
    }

    // ---- the table itself (the console's var_2190 stack local) ------------------------------
    // `li r11, -1 / stw r11, var_C0` is the Array ctor's sentinel; `li r11, 0 / stw r11, var_C0`
    // @0x82375A90 is the Construct that clears it. Both are reproduced by these two lines.
    GameStateModuleIO::SetUpAllEventStartsInterface lEventStarts;
    lEventStarts.Construct();

    const u32 luNumHulls = lpTrafficData->muNumHulls;   // `lhz r10, 2(r21)`
    for (u32 luHull = 0; luHull < luNumHulls; ++luHull)
    {
        const BrnTraffic::Hull* const lpHull = lpTrafficData->GetHull(luHull);
        if (lpHull == 0)
        {
            continue;   // [GUARD] GetHull carries the console's own bounds assert
        }

        const u32 luNumLightTriggers = lpHull->muNumLightTriggers;   // `lbz r27, 0xE(r18)`
        for (u32 luTrigger = 0; luTrigger < luNumLightTriggers; ++luTrigger)
        {
            // BrnTraffic::LightTriggerId::Set(luHull, luTrigger), inlined by the console at
            // 0x82375B74..0x82375BB4. Both bounds asserts are baked at BrnTrafficLightTrigger.h
            // :211/:212 and both are reproduced -- this code PACKS the handle, so a fire means
            // the shipped lane graph changed shape. (The console compares the SCALED index
            // `luHull * 4` against 0x640; 0x640 / 4 == KU_MAX_HULLS == 400.)
            if (luHull >= BrnTraffic::KU_MAX_HULLS)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("luHull < KU_MAX_HULLS", KAC_LIGHT_TRIGGER_H, 211);
                CgsDev::Assert::EndAssert();
            }
            if (luTrigger >= 256u)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("luLightTriggerIndex < 256", KAC_LIGHT_TRIGGER_H, 212);
                CgsDev::Assert::EndAssert();
            }

            const u32 luLightTriggerId =
                (luHull << 8) | BrnTraffic::KU_LIGHT_TRIGGER_ID_OWNER_TAG | luTrigger;

            const BrnTraffic::JunctionLogicBox* const lpJunctionLogicBox =
                lpTrafficData->GetJunctionLogicBoxForTrafficLight(luLightTriggerId);

            // THE EVENT FILTER: keep the junction only if it has BOTH start grids. `li r5, 0`
            // then `li r5, 1` @0x82375BC8/@0x82375BE0 -- offline first, online second, and a
            // null from EITHER drops straight into the WARNING arm below.
            const bool lbHasEventData =
                (lpTrafficData->GetStartDataForTrafficLight(luLightTriggerId, false) != 0) &&
                (lpTrafficData->GetStartDataForTrafficLight(luLightTriggerId, true)  != 0);

            if (!lbHasEventData || lpJunctionLogicBox == 0)
            {
                // The console's own gated diagnostic (0x82375CB8..0x82375D94), streamed into
                // gpDebugPrint behind `CgsDev::Message::gxMessageFilterFlags & 1`. It prints the
                // trigger id in hex and the two junction ids -- which is exactly the information
                // needed to find a junction whose lane data and progression data disagree.
                // ⚠️ lpJunctionLogicBox != 0 is a PC GUARD folded into the same arm: the console
                // reads the box's ids inside this print WITHOUT a null test (`lwz r28, 0x38(r30)`
                // @0x82375CD4), so a null box would AV in its own warning.
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "WARNING: Missing event data for junction. Light trigger = "
                        << luLightTriggerId;
                    if (lpJunctionLogicBox != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << ", junction id=" << lpJunctionLogicBox->GetID()
                            << ", event junction id=" << lpJunctionLogicBox->GetEventJunctionID();
                    }
                    *CgsDev::Log::gpDebugPrint << "\n";
                }
                continue;
            }

            // The light trigger's own world pose. ExpandPosPlusYRotToTransform(mPosPlusYRot) is
            // LightTrigger::GetTransform(); its translation row IS the packed lane, so Pos() is
            // the trigger centre with no extra accessor (the TriggerQueryManager precedent).
            // The console calls it TWICE with identical arguments (0x82375C04 and 0x82375C58) --
            // the compiler could not keep the result live across the WorldMap2D::GetValue call.
            // One call, one local: same inputs, same answer.
            const BrnTraffic::LightTrigger& lrTrigger = lpHull->mpaLightTriggers[luTrigger];
            const Vector3 lv3TriggerPosition = lrTrigger.GetTransform().Pos();

            // The district sample -- ground-plane lanes, see the banner's ⚠️.
            BrnWorld::ECounty leCounty = BrnWorld::E_COUNTY_INVALID;
            if (lbDistrictMapBound)
            {
                const Vector2 lv2Ground = { lv3TriggerPosition.x, lv3TriggerPosition.z, 0.0f, 0.0f };
                const u8 luMapValue = lDistrictMap.GetValue(lv2Ground);
                if (luMapValue == CgsWorld::KU_INVALID_WORLD_MAP_VALUE)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "luMapValue != CgsWorld::KU_INVALID_WORLD_MAP_VALUE",
                        KAC_GAMESTATEMODULE_CPP, 5764);
                    CgsDev::Assert::EndAssert();
                }
                // The console passes the raw sample straight through, INCLUDING the 255 it just
                // asserted about (`mr r3, r27` @0x82375C7C is the untouched byte). Reproduced:
                // DistrictToCounty carries its own range assert and answers E_COUNTY_INVALID for
                // anything past E_DISTRICT_COUNT, so the assert is a report, not a gate.
                leCounty = BrnWorld::WorldRegion::DistrictToCounty(
                    static_cast<BrnWorld::EDistrict>(luMapValue));
            }

            // The nearest AI section to the trigger (0x82375C74).
            s16 li16AISectionIndex = static_cast<s16>(0x7FFF);   // BrnWorld::KI_INVALID_SECTION_INDEX
            if (lpAISectionPointMap != 0)
            {
                li16AISectionIndex = static_cast<s16>(
                    lpAISectionsData->FindNearestAISection(lv3TriggerPosition, lpAISectionPointMap));
            }

            // 0x82375CAC -- the record. See the ⚠️ in the banner: the POSITION is the junction's
            // (+0x110), not the trigger's.
            lEventStarts.AddEventStart(lpJunctionLogicBox->GetPosition(),
                                       static_cast<s32>(lpJunctionLogicBox->GetID()),
                                       static_cast<s32>(lpJunctionLogicBox->GetEventJunctionID()),
                                       luLightTriggerId,
                                       static_cast<s32>(leCounty),
                                       li16AISectionIndex);
        }
    }

    delete[] lpArena;   // 0x82375E3C `operator delete__`

    // 0x82375E50 -- `memcpy(out + 0x2B0F0, &local, 0x20E0)`, spelled as the assignment of the
    // named member it lands on (the interface is a pointer-free POD; its _AssertLayout pins the
    // 0x20E0 the console's literal names).
    lpOutput->GetSetUpAllEventStartsInterface() = lEventStarts;

    // 0x82375E5C -- and the flag the GameState->Gui bridge polls.
    lpOutput->SetSetUpAllEventStartsInterfaceIsValid(true);

    // [DIAG] NOT IN THE X360 BINARY -- the one line that proves the table filled, once.
    // DELETE-WHEN the event-icon path has a regression test behind it.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[event-starts] published " << static_cast<s32>(lEventStarts.GetNumEventStarts())
            << " event starts from " << static_cast<s32>(luNumHulls) << " hulls\n";
    }
}

} // namespace BrnGameState
