#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"

// Element-type homes for the fixed-capacity Array<T,N> leaf instantiations reconstructed by the
// GameMode leaf batch. PROVISIONAL minimal records: sized to the X360 element stride; real field
// names land with each element type's own TU. Single owner -- grow in place, do not fork.

namespace BrnGameState
{
struct GridPositionAndScoreData { u8 maBlob[8]; };   // X360 stride 8 (provisional)
struct BufferedNewHighScore     { u8 maBlob[32]; };  // X360 stride 32 (provisional)

namespace GameStateModuleIO
{
struct TargetEventScore        { u8 maBlob[40]; };   // X360 stride 40 (provisional)
struct ChainableMultiplierInfo { s32 maField[4]; };  // X360 stride 16 (provisional)
}

// Minimal owner-class slices carrying only the nested element type each Array instantiation needs.
class DeveloperChallengeManager
{
public:
    struct CollectedBillboard { u8 maBlob[16]; };    // X360 stride 16 (provisional)
};

class GameStateImageManagerBase
{
public:
    struct ImageLoadRequest                          // DWARF BrnGameStateImageManagerBase.h:174 (12 bytes)
    {
        s32 miImageIndex;            // 0x00
        s32 miSlotIndex;             // 0x04
        s32 meImageGalleryImageType; // 0x08  (GameStateModuleIO::EImageGalleryType; s32 placeholder)
    };
};

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
