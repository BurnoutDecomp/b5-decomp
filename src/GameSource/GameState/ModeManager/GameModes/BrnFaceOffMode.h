#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// FaceOffMode is a concrete offline game mode. Bases (OfflineGameMode -> GameMode) are #included
// from their owning headers, never forked. Only GetName and GetOutroTimeout are owned by this
// TU (DWARF-attested); the rest of the mode belongs to BrnFaceOffMode.cpp.
//
// NOT-YET-RECONSTRUCTED OVERRIDES (vtable 0x820D0500, checked 2026-08-26): FaceOffMode also
// overrides slot 5 Start (0x82330440), slot 2 PreWorldUpdate (0x82330670) and slot 15
// FillInGameModeSpecificResults (0x82315CC0) -- the DWARF declares exactly that set, plus slot 8
// below. They are NOT declared here because no body exists in the tree, and a declaration with no
// definition is an unresolved external the moment this mode's vtable is emitted. Until they land,
// FaceOffMode inherits the GameMode base for those three.
class FaceOffMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;
    virtual f32         GetOutroTimeout() const;

    // Slot 8 (vtbl+32), LANDED 2026-09-25 (crash parity FX-SCENARIOS). X360 0x827EAB30 -- a body
    // shared by ICF (IDA files it as MovieVideoRenderer::GetRenderingDelay): `lis/lfs f1,
    // flt_82001CC0 ; blr` == 0.0f. DWARF BrnFaceOffMode.h:99 declares the override. With 0.0
    // ModeManager::StartModeIntro's `mfDurationSeconds > 0.0f` (fcmpu/bgt) leaves mbDoIntro false:
    // a Face Off has NO intro. Until then it inherited the GameMode base's 6.0.
    virtual f32         GetIntroDurationSeconds() const;

private:
    // DWARF: BrnFaceOffMode.cpp:27. The mode's fixed outro timeout; the X360 GetOutroTimeout body
    // returns 0.0, so this constant is 0.0f for this build (same shape as RaceMode).
    static const f32 KF_OUTRO_TIME_SECONDS;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (FaceOffMode::*)() const>(&FaceOffMode::GetName)) != 0,
              "FaceOffMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (FaceOffMode::*)() const>(&FaceOffMode::GetOutroTimeout)) != 0,
              "FaceOffMode::GetOutroTimeout must bind GameMode vtable slot 16");
static_assert(sizeof(static_cast<f32 (FaceOffMode::*)() const>(&FaceOffMode::GetIntroDurationSeconds)) != 0,
              "FaceOffMode::GetIntroDurationSeconds must bind GameMode vtable slot 8");
}
