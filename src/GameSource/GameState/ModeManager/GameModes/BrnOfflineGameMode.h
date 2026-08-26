#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"
// Brings in the real Vector3 typedef (rw::math::vpu::Vector3, via BrnCommonTypes.h), used in
// SelectRandomDestinations' signature, plus the GameMode base.
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"
// The real BrnGameState::LandmarkIndex wrapper class (the SelectRandomDestinations output type).
// INTEGRATION NOTE (consolidated): the worker's original standalone OfflineGameMode.h
// forward-declared `struct Vector3;` and `struct LandmarkIndex;`. The real Vector3 is the
// rw::math::vpu typedef pulled in transitively above; LandmarkIndex is a real wrapper CLASS
// owned by BrnGameStateTypes.h (it used to be a `typedef u32 LandmarkIndex` stub in
// BrnGameModeParams.h -- that stub is now removed, and this is the single owner). Include the
// owning header so the class is complete at this declaration site.
#include "GameSource/GameState/BrnGameStateTypes.h"

namespace BrnTrigger
{
// Forward declarations: SelectRandomDestinations takes a const TriggerData* and walks
// its Landmark array by pointer only, so the full layouts are not needed at this
// declaration site. Their owning headers (SharedClasses/Trigger/BrnTriggerData.h,
// BrnLandmark.h) are #included by the .cpp where the bodies dereference them.
struct TriggerData;
}

namespace BrnGameState
{
// Owning header for the OfflineGameMode base. Reconstructed from the DecFIGS DWARF
// (BrnOfflineGameMode.h): derives from GameMode and adds the two debug data members
// below, plus the X360-attested Construct override @0x8232FE58, the GetFrameRateType
// override (slot 7) and the protected SelectRandomDestinations helper @0x82321E38.
//
// [!] GetFrameRateType RESTORED 2026-08-26 (wave-B fix round). This header previously gated it
// out as "PS3-only drift, absent from the X360 ledger". The image refutes that: vtable slot 7
// (vtbl+28) is the folded leaf 0x82C296C8 (`li r3,1; blr`) in ALL EIGHT offline mode vtables
// (0x820D0498/0500/0570/05E8/0650/06B8/0720/0788) and 0x827DF718 (`li r3,2; blr`) in ALL SEVEN
// online ones -- a split that is only possible if BOTH intermediate bases override the slot.
// The reason it never showed up in the ledger is COMDAT identical-code folding: a two-instruction
// leaf has no unique address to attest. ModeManager::StartGameMode @0x8234FCE8 calls it directly,
// `v17 = (*(**(a1+3480)+28))(*(a1+3480))`.
//
// The PS3-only EAddingRivalsState enum and the file-scope debug-loop members it drives are out of
// scope for the X360 spine and are not pulled in.
class OfflineGameMode : public GameMode
{
public:
    // X360 0x8232FE58. Forwards to the GameMode base, then clears mbIsOnline and seeds the
    // debug "always race to one landmark" overrides.
    virtual void Construct(ModeManager* lpModeManager);

    // Vtable slot 7 (vtbl+28). Folded leaf 0x82C296C8 == `li r3,1; blr` in all eight offline
    // vtables -> CgsSystem::E_FRAMERATEMANAGER_MULTIPLE_CAPPED.
    virtual CgsSystem::EFrameRateManagerType GetFrameRateType() const;

protected:
    // X360 0x82321E38. Scans the track's landmark list and gathers every landmark whose
    // straight-line distance from lv3Origin falls in [lfMinDistance, lfMaxDistance] and
    // whose world height sits above KF_HACK_MIN_LANDMARK_HEIGHT; when lbApplyDirectionFilter
    // is set, landmarks lying behind lv3Direction (negative dot) are also rejected. The two
    // parallel output arrays receive the landmark region indices and their AI-section
    // indices, and the count of accepted landmarks is returned. Called by the offline modes'
    // Start() (e.g. FaceOffMode::Start) to pick race destinations.
    u32 SelectRandomDestinations(const BrnTrigger::TriggerData* lpTriggerData,
                                 Vector3 lv3Origin,
                                 Vector3 lv3Direction,
                                 f32 lfMinDistance,
                                 f32 lfMaxDistance,
                                 LandmarkIndex* lpaLandmarkIndicesOut,
                                 u16* lpaAISectionIndicesOut,
                                 bool lbApplyDirectionFilter);

private:
    bool mbDebugAlwaysRaceToSingleLocation;   // console +180, OfflineGameMode::Construct @0x8232FE7C
    s32  miDebugDesignIndexOfLandmarkToAlwaysRaceTo; // console +184, `li r10,0x29` == 41 @0x8232FE80
};

// ---- VTABLE-BINDING TRIPWIRE (wave-P member-pointer recipe) -------------------------------------
// Each line below re-binds the BASE declaration's signature onto THIS class and initialises it from
// the derived member. If a derived override's signature ever drifts from GameMode's, the
// static_cast is ill-formed and this header stops compiling. That is the ONLY way a compile-only
// gate can see the failure mode that made this fix round necessary: an override whose signature
// does not match its base silently MINTS A NEW vtable slot instead of binding to the console one,
// and nothing else complains. Slot numbers refer to the 26-slot table in BrnGameMode.h.
static_assert(sizeof(static_cast<void (OfflineGameMode::*)(ModeManager*)>(&OfflineGameMode::Construct)) != 0,
              "OfflineGameMode::Construct must bind GameMode vtable slot 0");
static_assert(sizeof(static_cast<CgsSystem::EFrameRateManagerType (OfflineGameMode::*)() const>(&OfflineGameMode::GetFrameRateType)) != 0,
              "OfflineGameMode::GetFrameRateType must bind GameMode vtable slot 7");
}
