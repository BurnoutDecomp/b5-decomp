#ifndef BRN_GUI_WORLD_DATA_CONTROLLER_H
#define BRN_GUI_WORLD_DATA_CONTROLLER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // CgsID (landmark/trigger lookups)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // CgsModule::EventReceiverQueue<1024,16>
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"           // BrnWorld::GlobalColourPalette, PlayerCarColourPalette

// BrnGui::WorldDataController -- the GUI-side world/progression data front-end. The cache owns
// one; it fronts the loaded trigger / progression / vehicle / player-car-colour / street
// resources for the sat-nav renderer, license components and event panels.
//
// Layout + method SHAPE are taken from the DecFIGS DWARF for this exact path
// (references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiWorldDataController.{h,cpp}) and gated on
// the X360 ARTIST ledger. The member OFFSETS in the comments are the X360 32-bit-ABI byte offsets
// proven by the accessor asm (mpTriggerData@0x424, mpProgressionData@0x444, mpVehicleList@0x464,
// mpPlayerCarColours@0x468); they are NOT host-asserted because ResourcePtr<T> and the raw
// pointers widen to 64-bit on the host and mReceiverQueue is modelled as an opaque size-holding
// blob. Callers reach the members BY NAME, so exact host offsets are immaterial to compilation.
//
// This wave homes the readiness accessors GetColourPaletteFromType / GetProgressionData /
// GetTotalNumberOfLandmarks / GetTotalNumberOfOnlineLandmarks, and keeps the two the sat-nav
// renderer already links (GetEventInfoFromEventId / GetTotalNumberOfOnlineLandmarks).
// GetRequiredWinsInRank lands in a later wave.
//
// 2026-08-02 (carousel wave): Construct + the Prepare ACQUIRE STATE MACHINE are now real
// (X360 0x82516770). Both were previously missing, which is why GuiCache::mpWorldDataController
// was never populated and every car-select component that resolves a car through the vehicle
// list bailed. See the banner in BrnGuiWorldDataController.cpp for the request/reply contract.

namespace BrnProgression { struct RaceEventData; struct ProgressionData; }
namespace BrnTrigger     { struct TriggerData; struct Landmark; struct BoxRegion; }
namespace BrnResource    { struct VehicleList; class ChallengeList; namespace GameDataIO { struct InputBuffer; } }
namespace BrnStreetData  { struct StreetData; }
namespace BrnGameState   { class  LandmarkIndex; }
// [gateui r3] CORRECTED: this file used to forward-declare `BrnGui::ChallengeList` and type
// both GetFreeburnChallengeList() and mpChallengeList with it. NO SUCH TYPE EXISTS -- nothing
// in the tree ever defines BrnGui::ChallengeList, so the accessor returned a pointer to a
// permanently-incomplete phantom and every caller had to bridge it with a reinterpret_cast.
// The real type is BrnResource::ChallengeList (SharedClasses/DataLists/ChallengeList.h:44),
// which is exactly what BrnGuiCache.h already spells for the same resource. Forward-declared
// above with the rest of the BrnResource boundary types; pointer-only use, no include added.

namespace BrnWorld
{
    // Palette selector for GetColourPaletteFromType (indexes GlobalColourPalette::maPalettes[4]).
    // Enumerators are DWARF-authoritative (PlayerCarColours.h:32); eNumPalettes == 4 and matches
    // BrnWorld::E_NUM_PALETTES in BrnGlobalColourPalette.h.
    enum EPalettesTypes
    {
        eGloss       = 0,
        eMetallic    = 1,
        ePearlescent = 2,
        eSpecial     = 3,
        eNumPalettes = 4,
    };
}

namespace BrnGui
{
    struct WorldDataController
    {
        // DWARF BrnGuiWorldDataController.h:157. State machine of the resource-acquisition flow.
        enum EWorldDataControllerState
        {
            E_WORLDDATACONTROLLERSTATE_DESTRUCTED                              = 0,
            E_WORLDDATACONTROLLERSTATE_CONSTRUCTED                             = 1,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_TRIGGERS                  = 2,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_TRIGGERS            = 3,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_VEHICLES                  = 4,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_VEHICLES            = 5,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_PROGRESSION               = 6,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_PROGRESSION         = 7,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_STREET_DATA               = 8,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_STREET_DATA         = 9,
            E_WORLDDATACONTROLLERSTATE_PLAYERCARCOLOURS                        = 10,
            E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS                      = 11,
            E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_FREEBURN_CHALLENGES       = 12,
            E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_FREEBURN_CHALLENGES = 13,
            E_WORLDDATACONTROLLERSTATE_PREPARED                                = 14,
            E_WORLDDATACONTROLLERSTATE_READY                                   = 15,
            E_WORLDDATACONTROLLERSTATE_COUNT                                   = 16,
        };

        // ---- Bring-up (bodies in BrnGuiWorldDataController.cpp) ---------------------------------

        // X360-INLINED into BrnGui::GuiModule::Construct @0x82518028 (the seven stores at
        // guiModule+307836..+309028 immediately before the BaseEventReceiverQueue::Clear on
        // guiModule+307844): meState = CONSTRUCTED, the 1024/16 receiver queue bound to its
        // embedded buffer and cleared, and the two raw resource pointers nulled.
        void Construct();

        // X360 0x82516770 -- the resumable resource-acquisition state machine. Returns true once
        // the controller reaches E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS (the state every
        // readiness accessor gates on); false while a reply is still outstanding, exactly as the
        // console's own "no event on the receiver queue yet" arm does. The console drives it from
        // BrnGui::GuiModule::Prepare stage 14 with the module scheduler's GameData INPUT buffer.
        bool Prepare(BrnResource::GameDataIO::InputBuffer* lpGameDataInput);

        // ⭐⭐ X360 0x82516CB8 -- THE SECOND, INDEPENDENT ACQUIRE MACHINE, and THE ONLY WRITER OF
        // mpProgressionData / mpStreetData anywhere in the image. Two acquires by name hash,
        // "ProgressionData" then "StreetData", each bound with CreateFromHandle off its
        // AcquireResourceResponse. Its console driver is BrnGui::GuiModule::Prepare2 @0x825194B8.
        //
        // ⚠️ IT HAS ITS OWN STATE WORD. Prepare drives meState (WDC+0x00, the word every readiness
        // accessor asserts on); THIS drives meState2 (WDC+0x04) -- `lwz r10, 4(r28)` at its switch
        // head @0x82516CCC against Prepare's `lwz r11, 0(r28)`. So reaching the end of Prepare2
        // does NOT satisfy "E_WORLDDATACONTROLLERSTATE_READY <= meState"; it only makes the
        // progression/street resources readable. The two facts were conflated once already -- the
        // banner in the .cpp said "some other producer fills them" without naming this function.
        //
        // ⛔ BOTH MACHINES SHARE mReceiverQueue, SO THEY MUST NEVER BE PUMPED IN THE SAME PHASE:
        // each stage consumes "the one event on the queue" without checking what it is, so a reply
        // to one machine would be eaten by the other. See the driver in BrnGuiModule.cpp
        // (PrepareWorldData2) for the sequencing that guarantees exclusive use.
        bool Prepare2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput);

        // The live state (the readiness gate the accessors assert on). Exposed so the driver can
        // stop pumping without re-entering the machine.
        EWorldDataControllerState GetState() const { return meState; }

        // The Prepare2 machine's own state (WDC+0x04). Exposed for the same reason as GetState().
        EWorldDataControllerState GetState2() const { return meState2; }

        // [PC bring-up helper -- NOT an X360 method, and deliberately NOT a state test.]
        // True once the stage-6/7 "CarColours" acquire has bound a resource with real memory
        // behind it. GetColourPaletteFromType @0x824BDA40 has NO meState gate (verified: the
        // asm only compares lType against 4), so a caller that wants to know whether the
        // palette is readable must ask about the RESOURCE, not about meState. Same
        // HasMemoryResource() idiom ProgressionManager::GetProgressionData already uses, and
        // for the same reason -- calling operator-> on an unbound ResourcePtr fires its own
        // assert before the caller can decide anything.
        bool HasPlayerCarColours() const { return mpPlayerCarColours.HasMemoryResource(); }

        // [PC bring-up helper -- NOT an X360 method.] Same RESOURCE-not-STATE question as
        // HasPlayerCarColours above, for the pointer Prepare2 binds. The driver's one-shot
        // diagnostic reads it; GetProgressionData() still carries the console's meState assert.
        bool HasProgressionData() const { return mpProgressionData.HasMemoryResource(); }

        // [PC bring-up helper -- NOT an X360 method.] Third of the same family, added
        // 2026-08-27 with the challenge-list wave. It matters NOW because that wave lets
        // Prepare reach WFPLAYERCARCOLOURS for the first time, which un-gates the five
        // meState-asserting accessors -- and FOUR of them (GetTotalNumberOfLandmarks,
        // GetTotalNumberOfOnlineLandmarks, GetLandmarkInfoFrom{Index,ID}, GetTriggerVolumeRegion)
        // go straight through mpTriggerData->... with no null path of their own. Stage 2/3's
        // "TriggerData" acquire is answered even when Triggers.dat is not yet resident (a null
        // memory pointer, not a failure), so the binding is NOT guaranteed. The driver's one-shot
        // diagnostic reports it; nothing branches on it yet.
        bool HasTriggerData() const { return mpTriggerData.HasMemoryResource(); }

        // ---- Accessors (bodies in BrnGuiWorldDataController.cpp) --------------------------------

        // DWARF h:93 / X360 0x82501270 -- the landmark whose region index equals lLandmarkIndex
        // (linear scan of the trigger data's landmark table; asserts + fires a not-found message).
        const BrnTrigger::Landmark* GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex) const;

        // DWARF h:98 / X360 0x82501480 -- the landmark whose id equals lLandmarkID (linear scan).
        const BrnTrigger::Landmark* GetLandmarkInfoFromID(CgsID lLandmarkID) const;

        // DWARF h:103 / X360 0x82501698 -- the OFFLINE race-event record for an event id (sat-nav
        // renderer). Walks the progression event-junction table for a matching junction id.
        const BrnProgression::RaceEventData* GetEventInfoFromEventId(u32 luEventId) const;

        // DWARF h:108 / X360 0x82501740 -- the ONLINE race-event record for an event id.
        const BrnProgression::RaceEventData* GetOnlineEventInfoFromEventId(u32 luEventId) const;

        // DWARF h:112 / X360 0x8248E6D8 -- total landmark count (mpTriggerData->miLandmarkCount).
        s32 GetTotalNumberOfLandmarks() const;

        // DWARF h:116 / X360 0x824286E0 -- online landmark count (mpTriggerData->miOnlineLandmarkCount).
        s32 GetTotalNumberOfOnlineLandmarks() const;

        // DWARF h:122 / X360 0x82501AC0 -- copy the generic-region box for trigger lTriggerID into
        // *lpRegion (linear scan of the trigger data's generic-region table; fires on empty/miss).
        void GetTriggerVolumeRegion(CgsID lTriggerID, BrnTrigger::BoxRegion* lpRegion) const;

        // DWARF h:133 / X360 0x824F3AF0 -- the loaded vehicle-list resource (raw pointer member).
        const BrnResource::VehicleList* GetVehicleList() const;

        // DWARF h:137 / X360 0x824F3AF8 -- the loaded freeburn-challenge-list resource.
        const BrnResource::ChallengeList* GetFreeburnChallengeList() const;

        // DWARF h:147 / X360 0x824BDA40 -- the lType'th player-car colour palette entry.
        const BrnWorld::PlayerCarColourPalette* GetColourPaletteFromType(BrnWorld::EPalettesTypes lType) const;

        // DWARF h:151 / X360 0x82428818 -- the loaded progression resource (ResourcePtr operator->).
        const BrnProgression::ProgressionData* GetProgressionData() const;

    private:
        // Full DWARF-faithful member set (BrnGuiWorldDataController.h:182-193), in source order.
        // mReceiverQueue is the REAL CgsModule::EventReceiverQueue<1024,16> (2026-08-02): the X360
        // Construct's `*(wdc+0x18) = 1024 / *(wdc+0x1C) = 16 / *(wdc+0x08) = wdc+0x20` pins both the
        // capacity/alignment pair and the embedded 1024-byte buffer at +0x20, and Prepare drives it
        // through GetLength/GetFirstEvent/Clear. X360 offsets stay in the comments and are NOT
        // host-asserted (the base's buffer pointer widens on x64; callers reach members BY NAME).
        EWorldDataControllerState meState;               // X360 +0x000  (DWARF :182)
        // ⭐ [event-starts wave 2026-08-27] THE "4 BYTES OF ALIGNMENT/UNKNOWN" BELOW ARE NOT
        // ALIGNMENT -- they are Prepare2's own state word. Prepare2 @0x82516CB8 opens with
        // `lwz r10, 4(r28) ; cmplwi r10, 5 ; bgt default` and writes the same slot at every one of
        // its five stage latches, exactly as Prepare does to +0x00. Named rather than padded now.
        EWorldDataControllerState meState2;              // X360 +0x004  (Prepare2's machine)
        CgsModule::EventReceiverQueue<1024, 16> mReceiverQueue;  // X360 base FIELDS +0x008 / buffer +0x20
        // ^ Prepare @0x82516770 pins the console queue handle at this+0x8 (mpUser = this+8,
        //   Clear(this+8), count read at this+0x10, buffer at +0x20 = +0x8 + the 0x18 base).
        //   ⛔ ONE QUEUE, TWO MACHINES: Prepare2 uses THIS SAME queue (its requests all carry
        //   `&this->field_8` as mpUser). See Prepare2's declaration for why that makes the two
        //   mutually exclusive. Host layout is by-name; the X360 offsets are comments only.
        s32                       miResourceCount;       // X360 +0x420  (DWARF :184)

        CgsResource::ResourcePtr<BrnTrigger::TriggerData>          mpTriggerData;      // X360 +0x424  (DWARF :186)
        CgsResource::ResourcePtr<BrnProgression::ProgressionData>  mpProgressionData;  // X360 +0x444  (DWARF :188)
        const BrnResource::VehicleList*                           mpVehicleList;      // X360 +0x464  (DWARF :189)
        CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>    mpPlayerCarColours; // X360 +0x468  (DWARF :190)
        CgsResource::ResourcePtr<BrnStreetData::StreetData>       mpStreetData;       // X360 +0x488  (DWARF :191)
        const BrnResource::ChallengeList*                         mpChallengeList;    // DWARF :193
    };
}

#endif // BRN_GUI_WORLD_DATA_CONTROLLER_H
