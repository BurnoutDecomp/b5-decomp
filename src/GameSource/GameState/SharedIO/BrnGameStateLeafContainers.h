#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // GameStateImageManagerBase::ImageLoadRequest (real home)
#include "GameSource/GameState/SharedIO/BrnTargetEventScore.h"              // GameStateModuleIO::TargetEventScore (real home)

// Element-type homes for the fixed-capacity Array<T,N> leaf instantiations reconstructed by the
// GameMode leaf batch. PROVISIONAL minimal records: sized to the X360 element stride; real field
// names land with each element type's own TU. Single owner -- grow in place, do not fork.

namespace BrnGameState
{
struct GridPositionAndScoreData { u8 maBlob[8]; };   // X360 stride 8 (provisional)
struct BufferedNewHighScore     { u8 maBlob[32]; };  // X360 stride 32 (provisional)

// NOTE: GameStateModuleIO::TargetEventScore is now FULLY homed in SharedIO/BrnTargetEventScore.h
// (its id/score fields were decoded by the BrnProgression::Profile TU, whose maTargetEventScores
// Array<TargetEventScore,49> embeds it). The provisional `struct TargetEventScore { u8 maBlob[40]; }`
// stub that used to live here has been removed; the Array<TargetEventScore,49> explicit-
// instantiation TU (Array_TargetEventScore_49.cpp) now reaches the real type through this include
// (mirroring the DeveloperChallengeManager / ImageManagerBase / StuntModeScoringOnline promotions).
namespace GameStateModuleIO
{
struct ChainableMultiplierInfo { s32 maField[4]; };  // X360 stride 16 (provisional)
}

// NOTE: BrnGameState::DeveloperChallengeManager (and its nested CollectedBillboard element type) is
// now FULLY homed in DeveloperChallengeManager/BrnDeveloperChallengeManager.h -- the manager's own
// TU. The provisional `class DeveloperChallengeManager { struct CollectedBillboard { u8 maBlob[16]; }; }`
// stub that used to live here has been removed; the Array<CollectedBillboard,5> explicit-instantiation
// TU (Array_CollectedBillboard_5.cpp) now includes that home directly (mirroring the
// StuntModeScoringOnline promotion below).

// NOTE: BrnGameState::GameStateImageManagerBase (and its nested ImageLoadRequest element type) is
// now FULLY homed in ImageManager/BrnGameStateImageManagerBase.h -- the image-gallery manager's own
// TU. The provisional `class GameStateImageManagerBase { struct ImageLoadRequest { ... }; }` stub
// that used to live here has been removed; the Array<ImageLoadRequest,3> explicit-instantiation TU
// (Array_ImageLoadRequest_3.cpp) now reaches the real nested type through this include (mirroring
// the DeveloperChallengeManager / StuntModeScoringOnline promotions above).
class OnlineFlybyManager
{
public:
    enum ENewRivalryCompare     : s32 { E_NEW_RIVALRY_COMPARE_DEFAULT = 0 };     // provisional
    enum EOngoingRivalryCompare : s32 { E_ONGOING_RIVALRY_COMPARE_DEFAULT = 0 }; // provisional
};

// NOTE: BrnGameState::StuntModeScoringOnline (and its nested MultiplierData element type) is now FULLY
// homed in BrnStuntModeScoringOnline.h -- the online stunt scorer's own TU. The provisional
// `struct MultiplierData { u8 maBlob[40]; }` stub that used to live here has been removed; the
// Array<MultiplierData,3> explicit-instantiation TU (Array_MultiplierData_3.cpp) now includes that
// home directly. This file keeps only the leaf element types it actually owns.
}
