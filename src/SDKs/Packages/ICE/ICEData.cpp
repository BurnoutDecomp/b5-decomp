// ============================================================================
// SDKs/Packages/ICE/ICEData.cpp
//
// The ICE camera-take runtime TU: the global element-description schema table +
// per-channel index records (InitICEDescriptions), the ICEElementDescription
// encode/decode/raw machinery, the ICEChannel interval queries, the ICETakeData
// size/serialisation helpers, and the live ICETake evaluation engine.
//
// Built against the FROZEN committed layouts in ICEData.hpp / ICEDataEnums.hpp /
// ICEPoint.hpp. The 24 function bodies are ASSEMBLED here in dependency
// order: the static schema globals (InitICEDescriptions group) come FIRST so the
// ICETakeData / ICETake bodies that read ICEElementDescriptions[] /
// gaICEElementChannels[] see their definitions, then ICEElementDescription,
// ICEChannel, ICETakeData, and ICETake.
//
// Dependency homes wired below are MINIMAL declaration-only slices (the per-TU
// `cl /c` gate does not link): ICEMath.hpp (Round/Clamp), ICEMemory.hpp (the ICE
// edit-buffer allocator + singleton), rw/core/stdc/stdc.h (MemClear/MemCopy), and
// CgsHeapMalloc.h (the edit-heap Free). See the FLAGs at each use site.
// ============================================================================

#include "SDKs/Packages/ICE/ICEData.hpp"          // pulls ICEDataEnums.hpp + ICEPoint.hpp
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "SDKs/Packages/ICE/ICEMath.hpp"           // ICE::ICEMath::Round / Clamp (minimal slice)
#include "SDKs/Packages/ICE/ICEMemory.hpp"         // ICE::spICEMemory (ICE::ICEMemory*) / ICEMemory::GetMemory / mEditHeap
#include "rw/core/stdc/stdc.h"                      // rw::core::stdc::MemClear / MemCopy (minimal slice)
#include "SDKs/Packages/ICE/ICEFile.hpp"            // ICE::ICEFileHandler (ICETakeData::SaveData sink)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [DIAG] CgsDev::Log::gpDebugPrint (key-bounds trace)

#include <cstring>                                  // memset (ICETakeData::Construct)
#include <cstdint>                                  // uintptr_t (SetDataPointers alignment math)


// =============================================================================
// GROUP 1 -- the static element-description schema + per-channel index records +
// ICE::InitICEDescriptions.
//
// LAYOUT NOTES:
// InitICEDescriptions does NOT populate the element-description field immediates.
// The 48-entry table `ICEElementDescriptions` is a STATICALLY INITIALIZED .data
// array (88 bytes == 0x58 == frozen sizeof(ICEElementDescription)
// per entry); it is declared with the aggregate initializer below and
// the compiler emits it directly into .data. InitICEDescriptions only does runtime
// CHANNEL BOOKKEEPING: it (loop 1) seeds each of the 12 per-channel index records
// with its channel name + zeroed counts, then (loop 2) calls Prepare() on every
// element and files each element index into its channel's key-list (elements 0..27)
// or interval-list (elements 28..47).
//
// The full table -- tags, display names, ALL numeric fields, and the discreet token
// tables -- is reproduced from the .data image. Nothing here is a placeholder.
//
// eICE_NUM_ELEMENTS == 48: (a) the .data table spans 48 * 88 bytes, (b) loop 2's bound
// stops after 48 iterations, (c) matches ICETake's mValues[48].
//
// FLAG: mpTag/mpDisplayName/mpTokens are non-const char*/char** in the frozen header;
// the data is read-only .rodata at runtime, hence the const_cast<char*> wrappers. If
// the frozen header later moves to const char*, drop the casts.
//
// FLAG: mDefault/mMin/mMax use the ICEValue(f32) ctor for eICE_FLOAT/eICE_FIXED and
// the ICEValue(s32) ctor for eICE_INT/eICE_UINT/eICE_HASH so the raw 4 bytes match
// the .data image exactly (e.g. an eICE_UINT max of 100 is the integer 0x00000064).
// =============================================================================

namespace ICE
{

// eICE_NUM_ELEMENTS (48) and the extern ICEElementDescriptions[] declaration now
// live in ICEDataEnums.hpp so every ICE TU shares the one schema table.

// -----------------------------------------------------------------------------
// ICE_EPSILON -- the ICE package's shared float tolerance (ICEData.hpp:54 declares
// it extern; this is its definition).
//
// VALUE RECOVERED, not guessed: it is the X360 .rodata slot flt_8207AB94, which
// exactly six functions load. In two of them Hex-Rays folded the literal into the
// pseudocode -- ICE::Cubic1D::Update @0x8252CE70 (`if ( *(result + 36) <=
// 0.0000099999997 )`, and its asm loads flt_8207AB94 for that very compare at
// 0x8252CF24/0x8252CF2C) and ICEAuthor::JumpToNextKey @0x825381C0 -- so the slot's
// value prints as 0.0000099999997, i.e. the float nearest 1e-5 (0x3727C5AC).
// The other four users are the two epsilon guards in ICETake::GetSlope @0x825303C0,
// the `size > ICE_EPSILON` assert in the private ICETake::SetParameter @0x82530668
// (ICEData.cpp:2473), JumpToPreviousKey and ICEAuthor::DeleteTakeSegment.
// -----------------------------------------------------------------------------
extern const f32 ICE_EPSILON;
const f32 ICE_EPSILON = 1.0e-5f;   // X360 flt_8207AB94

// eICE_NUM_CHANNELS == 12 (see docs/ICEData.md ICEChannels). The 12 channel names, in
// channel order, seeded into the per-channel index records by InitICEDescriptions loop 1.
// (A .rodata string list.)
static const s32 eICE_NUM_CHANNELS = 12;
static const char* const KAPC_ICE_CHANNEL_NAMES[eICE_NUM_CHANNELS] =
{
    "MAIN", "BLEND", "RAWFOCUS", "SHAKE", "TIME", "TAG",
    "OVERLAY", "LETTERBOX", "FADE", "POSTFX", "ASSEMBLY", "SHAKE_DATA"
};

// -----------------------------------------------------------------------------
// Discreet-value token tables (eICE_UINT elements). Each is a .rodata char* array the
// table entries point at; reproduced verbatim from the .data image.
// -----------------------------------------------------------------------------
static const char* const kapcICETokens_NoYes[]   = { "No", "Yes" };
static const char* const kapcICETokens_Space[]   = { "Car", "World", "Hybrid", "Scene", "Car 2", "TrafficLight", "Takedown", "Impact", "ReverseTakedown", "Gameplay", "Heading", "Bystander", "Heading2", "LooseHeading" };
static const char* const kapcICETokens_BlendCurve[] = { "Linear", "Sinusoidal", "Exponential Symmetrical", "Exponential Out X-Cubed" };
static const char* const kapcICETokens_Interp[]  = { "Slerp", "Rotate About Car" };
static const char* const kapcICETokens_ShakeType[] = { "None", "Jog", "Still", "WalkFast", "WalkSlow", "Procedural" };
static const char* const kapcICETokens_FadeColor[] = { "Black", "White", "Red", "Green", "Blue" };

// -----------------------------------------------------------------------------
// THE GLOBAL ELEMENT-DESCRIPTIONS TABLE (statically initialized).
// Field order matches the frozen ICEElementDescription:
//   mpTag, mpDisplayName, miChannelNumber, mDataType, miDataBits,
//   mDefault, mMin, mMax,
//   mfQuantSlotsLo, mfQuantRangeLo, mfQuantSlotsHi, mfQuantRangeHi,   (all 0 on disk --
//                                                                      Prepare() fills these)
//   miCubicLinear, miTangentScale, mInputType, miIncButton, miDecButton,
//   mfIncDecSpeed, mfIncDecAccel, mfIncDecDecel,
//   mpTokens, miTokens
// -----------------------------------------------------------------------------
ICEElementDescription ICEElementDescriptions[eICE_NUM_ELEMENTS] =
{
    // [0] EYE_X -- channel 0 (MAIN)
    { const_cast<char*>("EYE_X"), const_cast<char*>("Eye X"), 0, eICE_FLOAT, 32,
      ICEValue(0.0f), ICEValue(-100000.0f), ICEValue(100000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      28, 7, eICE_INPUT_ANALOG, 0, 0,
      0.0010000000474974513f, 0.0010000000474974513f, 0.0010000000474974513f,
      0, 0 },
    // [1] EYE_Y -- channel 0 (MAIN)
    { const_cast<char*>("EYE_Y"), const_cast<char*>("Eye Y"), 0, eICE_FLOAT, 32,
      ICEValue(5.0f), ICEValue(-100000.0f), ICEValue(100000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      28, 7, eICE_INPUT_ANALOG, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [2] EYE_Z -- channel 0 (MAIN)
    { const_cast<char*>("EYE_Z"), const_cast<char*>("Eye Z"), 0, eICE_FLOAT, 32,
      ICEValue(-8.0f), ICEValue(-100000.0f), ICEValue(100000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      28, 7, eICE_INPUT_ACCELERATED, 0, 0,
      0.0010000000474974513f, 0.0010000000474974513f, 0.0010000000474974513f,
      0, 0 },
    // [3] LOOK_X -- channel 0 (MAIN)
    { const_cast<char*>("LOOK_X"), const_cast<char*>("Look X"), 0, eICE_FLOAT, 32,
      ICEValue(0.0f), ICEValue(-100000.0f), ICEValue(100000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      29, 8, eICE_INPUT_ANALOG, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [4] LOOK_Y -- channel 0 (MAIN)
    { const_cast<char*>("LOOK_Y"), const_cast<char*>("Look Y"), 0, eICE_FLOAT, 32,
      ICEValue(0.0f), ICEValue(-100000.0f), ICEValue(100000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      29, 8, eICE_INPUT_ANALOG, 20, 18,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [5] LOOK_Z -- channel 0 (MAIN)
    { const_cast<char*>("LOOK_Z"), const_cast<char*>("Look Z"), 0, eICE_FLOAT, 32,
      ICEValue(0.0f), ICEValue(-100000.0f), ICEValue(100000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      29, 8, eICE_INPUT_ACCELERATED, 0, 0,
      2.0f, 0.10000000149011612f, 2.0f,
      0, 0 },
    // [6] DUTCH -- channel 0 (MAIN)
    { const_cast<char*>("DUTCH"), const_cast<char*>("Dutch"), 0, eICE_FIXED, 10,
      ICEValue(0.0f), ICEValue(-0.25f), ICEValue(0.25f),
      0.0f, 0.0f, 0.0f, 0.0f,
      28, 7, eICE_INPUT_ACCELERATED, 16, 14,
      0.10000000149011612f, 0.009999999776482582f, 0.10000000149011612f,
      0, 0 },
    // [7] TANGENT_EYE -- channel 0 (MAIN)
    { const_cast<char*>("TANGENT_EYE"), const_cast<char*>("Tangent Eye"), 0, eICE_FIXED, 6,
      ICEValue(1.0f), ICEValue(0.0f), ICEValue(8.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ANALOG, -1, -1,
      2.0f, 1.0f, 1.0f,
      0, 0 },
    // [8] TANGENT_LOOK -- channel 0 (MAIN)
    { const_cast<char*>("TANGENT_LOOK"), const_cast<char*>("Tangent Look"), 0, eICE_FIXED, 6,
      ICEValue(1.0f), ICEValue(0.0f), ICEValue(8.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ANALOG, -1, -1,
      2.0f, 1.0f, 1.0f,
      0, 0 },
    // [9] LENS_LENGTH -- channel 0 (MAIN)
    { const_cast<char*>("LENS_LENGTH"), const_cast<char*>("Lens Length"), 0, eICE_FIXED, 9,
      ICEValue(24.0f), ICEValue(5.0f), ICEValue(500.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      28, 7, eICE_INPUT_ANALOG, 10, 12,
      15.0f, 5.0f, 30.0f,
      0, 0 },
    // [10] CAMERA_BLEND_AMOUNT -- channel 1 (BLEND)
    { const_cast<char*>("CAMERA_BLEND_AMOUNT"), const_cast<char*>("Camera Blend Amount"), 1, eICE_UINT, 7,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)100),
      0.0f, 0.0f, 0.0f, 0.0f,
      -2, -1, eICE_INPUT_DISCREET, 36, 35,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [11] CAMERA_LAG_AMOUNT -- channel 1 (BLEND)
    { const_cast<char*>("CAMERA_LAG_AMOUNT"), const_cast<char*>("Camera Lag Amount"), 1, eICE_UINT, 7,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)100),
      0.0f, 0.0f, 0.0f, 0.0f,
      -2, -1, eICE_INPUT_DISCREET, 32, 31,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [12] NEAR_FOCUS -- channel 2 (RAWFOCUS)
    { const_cast<char*>("NEAR_FOCUS"), const_cast<char*>("Near Focus"), 2, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(0.0f), ICEValue(10000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      38, 16, eICE_INPUT_ACCELERATED, 32, 31,
      20.0f, 1.0f, 50.0f,
      0, 0 },
    // [13] FAR_FOCUS -- channel 2 (RAWFOCUS)
    { const_cast<char*>("FAR_FOCUS"), const_cast<char*>("Far Focus"), 2, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(0.0f), ICEValue(10000.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      38, 16, eICE_INPUT_ACCELERATED, 36, 35,
      20.0f, 1.0f, 50.0f,
      0, 0 },
    // [14] BLUR_FALLOFF -- channel 2 (RAWFOCUS)
    { const_cast<char*>("BLUR_FALLOFF"), const_cast<char*>("Blur Falloff"), 2, eICE_FIXED, 7,
      ICEValue(0.0f), ICEValue(0.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      38, 16, eICE_INPUT_ACCELERATED, 12, 10,
      0.20000000298023224f, 0.019999999552965164f, 0.20000000298023224f,
      0, 0 },
    // [15] BLUR_INTENSITY -- channel 2 (RAWFOCUS)
    { const_cast<char*>("BLUR_INTENSITY"), const_cast<char*>("Blur Intensity"), 2, eICE_FIXED, 7,
      ICEValue(0.0f), ICEValue(0.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      38, 16, eICE_INPUT_ACCELERATED, 16, 14,
      0.20000000298023224f, 0.019999999552965164f, 0.20000000298023224f,
      0, 0 },
    // [16] TANGENT_RAWFOCUS -- channel 2 (RAWFOCUS)
    { const_cast<char*>("TANGENT_RAWFOCUS"), const_cast<char*>("Tangent Focus"), 2, eICE_FIXED, 6,
      ICEValue(1.0f), ICEValue(0.0f), ICEValue(8.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ANALOG, -1, -1,
      2.0f, 1.0f, 1.0f,
      0, 0 },
    // [17] SHAKE_AMPLITUDE -- channel 3 (SHAKE)
    { const_cast<char*>("SHAKE_AMPLITUDE"), const_cast<char*>("Shake Amplitude"), 3, eICE_FIXED, 7,
      ICEValue(0.0f), ICEValue(0.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 32, 31,
      0.20000000298023224f, 0.05000000074505806f, 0.20000000298023224f,
      0, 0 },
    // [18] SHAKE_FREQUENCY -- channel 3 (SHAKE)
    { const_cast<char*>("SHAKE_FREQUENCY"), const_cast<char*>("Shake Frequency"), 3, eICE_FIXED, 7,
      ICEValue(0.0f), ICEValue(0.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 36, 35,
      0.20000000298023224f, 0.05000000074505806f, 0.20000000298023224f,
      0, 0 },
    // [19] TIME_SCALE -- channel 4 (TIME)
    { const_cast<char*>("TIME_SCALE"), const_cast<char*>("Time Scale"), 4, eICE_UINT, 7,
      ICEValue((s32)100), ICEValue((s32)0), ICEValue((s32)100),
      0.0f, 0.0f, 0.0f, 0.0f,
      -2, -1, eICE_INPUT_DISCREET, 36, 35,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [20] LETTERBOX -- channel 7 (LETTERBOX)
    { const_cast<char*>("LETTERBOX"), const_cast<char*>("Letterbox"), 7, eICE_UINT, 7,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)100),
      0.0f, 0.0f, 0.0f, 0.0f,
      -2, -1, eICE_INPUT_DISCREET, 36, 35,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [21] FADE -- channel 8 (FADE)
    { const_cast<char*>("FADE"), const_cast<char*>("Fade"), 8, eICE_UINT, 7,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)100),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_DISCREET, 36, 35,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [22] SHAKE_QUAT_X -- channel 11 (SHAKE_DATA)
    { const_cast<char*>("SHAKE_QUAT_X"), const_cast<char*>("SHAKE_QUAT_X"), 11, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(-1.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [23] SHAKE_QUAT_Y -- channel 11 (SHAKE_DATA)
    { const_cast<char*>("SHAKE_QUAT_Y"), const_cast<char*>("SHAKE_QUAT_Y"), 11, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(-1.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [24] SHAKE_QUAT_Z -- channel 11 (SHAKE_DATA)
    { const_cast<char*>("SHAKE_QUAT_Z"), const_cast<char*>("SHAKE_QUAT_Z"), 11, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(-1.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [25] SHAKE_POS_X -- channel 11 (SHAKE_DATA)
    { const_cast<char*>("SHAKE_POS_X"), const_cast<char*>("SHAKE_POS_X"), 11, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(-1.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [26] SHAKE_POS_Y -- channel 11 (SHAKE_DATA)
    { const_cast<char*>("SHAKE_POS_Y"), const_cast<char*>("SHAKE_POS_Y"), 11, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(-1.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [27] SHAKE_POS_Z -- channel 11 (SHAKE_DATA)
    { const_cast<char*>("SHAKE_POS_Z"), const_cast<char*>("SHAKE_POS_Z"), 11, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(-1.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ACCELERATED, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // --- elements 28..47 are "interval" descriptions (filed into the channel's
    //     interval-list by loop 2's i >= 28 branch) ---
    // [28] CUBIC_EYE -- channel 0 (MAIN)
    { const_cast<char*>("CUBIC_EYE"), const_cast<char*>("Cubic Eye"), 0, eICE_UINT, 1,
      ICEValue((s32)1), ICEValue((s32)0), ICEValue((s32)1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_NoYes), 2 },
    // [29] CUBIC_LOOK -- channel 0 (MAIN)
    { const_cast<char*>("CUBIC_LOOK"), const_cast<char*>("Cubic Look"), 0, eICE_UINT, 1,
      ICEValue((s32)1), ICEValue((s32)0), ICEValue((s32)1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_NoYes), 2 },
    // [30] SPACE_EYE -- channel 0 (MAIN)
    { const_cast<char*>("SPACE_EYE"), const_cast<char*>("Eye Space"), 0, eICE_UINT, 4,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)14),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_Space), 14 },
    // [31] SPACE_LOOK -- channel 0 (MAIN)
    { const_cast<char*>("SPACE_LOOK"), const_cast<char*>("Look Space"), 0, eICE_UINT, 4,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)14),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_Space), 14 },
    // [32] AVATAR_EYE -- channel 0 (MAIN)
    { const_cast<char*>("AVATAR_EYE"), const_cast<char*>("Avatar Eye"), 0, eICE_UINT, 5,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)31),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [33] AVATAR_LOOK -- channel 0 (MAIN)
    { const_cast<char*>("AVATAR_LOOK"), const_cast<char*>("Avatar Look"), 0, eICE_UINT, 5,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)31),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [34] CONSTRAIN_TO_CARS -- channel 0 (MAIN)
    { const_cast<char*>("CONSTRAIN_TO_CARS"), const_cast<char*>("Constrain to Cars"), 0, eICE_UINT, 1,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_NoYes), 2 },
    // [35] CONSTRAIN_TO_WORLD -- channel 0 (MAIN)
    { const_cast<char*>("CONSTRAIN_TO_WORLD"), const_cast<char*>("Constrain to World"), 0, eICE_UINT, 1,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_NoYes), 2 },
    // [36] BLEND_CURVE -- channel 1 (BLEND)
    { const_cast<char*>("BLEND_CURVE"), const_cast<char*>("Blend Curve"), 1, eICE_UINT, 3,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)4),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_BlendCurve), 4 },
    // [37] INTERPOLATE_TYPE -- channel 1 (BLEND)
    { const_cast<char*>("INTERPOLATE_TYPE"), const_cast<char*>("Interpolate Type"), 1, eICE_UINT, 2,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)2),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_Interp), 2 },
    // [38] CUBIC_RAWFOCUS -- channel 2 (RAWFOCUS)
    { const_cast<char*>("CUBIC_RAWFOCUS"), const_cast<char*>("Cubic Focus"), 2, eICE_UINT, 1,
      ICEValue((s32)1), ICEValue((s32)0), ICEValue((s32)1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_NoYes), 2 },
    // [39] RAWFOCUS_OVERRIDE -- channel 2 (RAWFOCUS)
    { const_cast<char*>("RAWFOCUS_OVERRIDE"), const_cast<char*>("Override"), 2, eICE_UINT, 1,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_NoYes), 2 },
    // [40] SHAKE_TYPE -- channel 3 (SHAKE).  FLAG: display name is literally "Shack Type"
    //      in the shipped .data (a source typo); preserved verbatim.
    { const_cast<char*>("SHAKE_TYPE"), const_cast<char*>("Shack Type"), 3, eICE_UINT, 5,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)6),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_ShakeType), 6 },
    // [41] EVENT_TAG -- channel 5 (TAG).  eICE_HASH max == 0xFFFFFFFF.
    { const_cast<char*>("EVENT_TAG"), const_cast<char*>("Event Tag"), 5, eICE_HASH, 32,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)-1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [42] OVERLAY -- channel 6 (OVERLAY)
    { const_cast<char*>("OVERLAY"), const_cast<char*>("Overlay"), 6, eICE_UINT, 4,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)15),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [43] FADE_TO_COLOR -- channel 8 (FADE)
    { const_cast<char*>("FADE_TO_COLOR"), const_cast<char*>("Fade to"), 8, eICE_UINT, 3,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)5),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_FadeColor), 5 },
    // [44] POSTFX_HOOK -- channel 9 (POSTFX).  eICE_UINT max == 0xFFFFFFFF.
    { const_cast<char*>("POSTFX_HOOK"), const_cast<char*>("PostFX Hook"), 9, eICE_UINT, 32,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)-1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      0, 0 },
    // [45] TAKE_START -- channel 10 (ASSEMBLY)
    { const_cast<char*>("TAKE_START"), const_cast<char*>("Take Start"), 10, eICE_FIXED, 16,
      ICEValue(0.0f), ICEValue(0.0f), ICEValue(1.0f),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ANALOG, 0, 0,
      1.0f, 1.0f, -1.0f,
      0, 0 },
    // [46] TAKE_NUMBER -- channel 10 (ASSEMBLY).  eICE_UINT max == 0xFFFFFFFF.
    { const_cast<char*>("TAKE_NUMBER"), const_cast<char*>("Take Number"), 10, eICE_UINT, 32,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)-1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_ANALOG, 0, 0,
      1.0f, 1.0f, -1.0f,
      0, 0 },
    // [47] CONTAINS_SUBTAKE -- channel 10 (ASSEMBLY)
    { const_cast<char*>("CONTAINS_SUBTAKE"), const_cast<char*>("Contains Subtake"), 10, eICE_UINT, 1,
      ICEValue((s32)0), ICEValue((s32)0), ICEValue((s32)1),
      0.0f, 0.0f, 0.0f, 0.0f,
      -1, -1, eICE_INPUT_BOOL, 0, 0,
      1.0f, 1.0f, 1.0f,
      const_cast<char**>(kapcICETokens_NoYes), 2 }
};

// -----------------------------------------------------------------------------
// Named slots into the two tables above, used by the public ICETake::SetParameter
// (which reaches them as raw mValues[] byte offsets in the asm). Each name is read
// straight off the table entry at that index -- nothing is inferred.
//   channel 10 == "ASSEMBLY" in KAPC_ICE_CHANNEL_NAMES
//   [17] SHAKE_AMPLITUDE / [18] SHAKE_FREQUENCY   (channel 3, SHAKE)
//   [45] TAKE_START / [46] TAKE_NUMBER / [47] CONTAINS_SUBTAKE   (channel 10)
// -----------------------------------------------------------------------------
static const s32 KI_ICE_ASSEMBLY_CHANNEL             = 10;
static const s32 KI_ICE_ELEMENT_SHAKE_AMPLITUDE      = 17;
static const s32 KI_ICE_ELEMENT_SHAKE_FREQUENCY      = 18;
static const s32 KI_ICE_ELEMENT_TAKE_START           = 45;
static const s32 KI_ICE_ELEMENT_TAKE_NUMBER          = 46;
static const s32 KI_ICE_ELEMENT_CONTAINS_SUBTAKE     = 47;

// -----------------------------------------------------------------------------
// Per-channel element-index records (a global, 12 * 204 bytes).
// InitICEDescriptions seeds these from the table above so the take-evaluation code can,
// for a given channel, walk just the elements that belong to it -- split into the
// "key" elements (description index < 28) and "interval" elements (index >= 28).
//
// Layout proven from InitICEDescriptions field offsets (+0x00/+0x04/+0x08/+0x78/+0x7C)
// and ICETake::SetParameter's reads of the same global:
//   sizeof == 204 == 4 + 4 + 28*4 + 4 + 20*4.
//
// FLAG: ICEElementChannel/gaICEElementChannels modeled here; move to its real home +
// grow when that TU lands. This struct + global is a SEPARATE, not-yet-reconstructed
// global (shared with the ICETake evaluation TUs); modelled here only so
// InitICEDescriptions + ICETake::SetParameter compile against one definition. When its
// real home/name surfaces, MOVE this there and GROW that header (do NOT keep a fork).
// -----------------------------------------------------------------------------
struct ICEElementChannel
{
    const char* mpName;                  // @0x00  channel name (seeded by loop 1)
    s32         miNumKeyElements;         // @0x04  count of "key" element indices
    s32         maiKeyElements[28];       // @0x08  element indices with desc index <  28
    s32         miNumIntervalElements;    // @0x78  count of "interval" element indices
    s32         maiIntervalElements[20];  // @0x7C  element indices with desc index >= 28
};

ICEElementChannel gaICEElementChannels[eICE_NUM_CHANNELS];

// -----------------------------------------------------------------------------
// ICE::InitICEDescriptions
//
// Runtime initializer for the ICE element-description system. Does NOT build the
// description table (that is statically initialized above) -- it builds the per-channel
// index records: clears each channel record and sets its name (loop 1), then runs
// Prepare() on every element and files each element index into its channel's key- or
// interval-list (loop 2).
//
// Prepare is logically void (the frozen header declares it void) and its return value
// is never used (the sole caller BrnDirector::ICEWrapper::Prepare ignores it), so this
// is implemented as void.
// -----------------------------------------------------------------------------
void InitICEDescriptions()
{
    // Loop 1: clear each per-channel record and seed its name.
    for (s32 liChannel = 0; liChannel < eICE_NUM_CHANNELS; ++liChannel)
    {
        ICEElementChannel& lrChannel = gaICEElementChannels[liChannel];
        lrChannel.mpName               = KAPC_ICE_CHANNEL_NAMES[liChannel];
        lrChannel.miNumKeyElements     = 0;
        lrChannel.miNumIntervalElements = 0;
    }

    // Loop 2: prepare every element and file its index under its channel.
    for (s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement)
    {
        ICEElementDescription& lrDescription = ICEElementDescriptions[liElement];
        lrDescription.Prepare();

        ICEElementChannel& lrChannel = gaICEElementChannels[lrDescription.miChannelNumber];
        if (liElement >= 28)
            lrChannel.maiIntervalElements[lrChannel.miNumIntervalElements++] = liElement;
        else
            lrChannel.maiKeyElements[lrChannel.miNumKeyElements++] = liElement;
    }
}

} // namespace ICE


// =============================================================================
// GROUP 2 -- ICE::ICEElementDescription bodies (7): Encode, Decode, GetRawInt,
// SetRawInt, GetValue, SetValue, Prepare.
//
// Reconstructed against the frozen layout in SDKs/Packages/ICE/ICEDataEnums.hpp.
// The eICE_FIXED quantization in Encode/Decode and the slot/range precompute in
// Prepare match docs/ICEData.md; GetRawInt/SetRawInt are big-endian bit-stream
// read/write.
// =============================================================================

namespace ICE
{

// ---------------------------------------------------------------------------
// Encode -- the eICE_FIXED quantiser: the inverse of Decode. Clamp the float into
// [mMin, mMax], take its offset from mDefault, map that offset to a slot index (the
// hi range for non-negative offsets, the lo range otherwise) biased by
// mfQuantSlotsLo, and round the slot to the nearest integer.
//   clamped = Clamp(value, mMin, mMax);
//   offset  = clamped - mDefault;
//   slot    = (offset >= 0) ? mfQuantSlotsHi * offset / mfQuantRangeHi + mfQuantSlotsLo
//                           : mfQuantSlotsLo * offset / mfQuantRangeLo + mfQuantSlotsLo;
//   return round-to-nearest(slot);
// (mDefault @+0x14, mMin @+0x18, mMax @+0x1C, mfQuantSlotsLo @+0x20,
//  mfQuantRangeLo @+0x24, mfQuantSlotsHi @+0x28, mfQuantRangeHi @+0x2C.) The round
// is done inline: truncate toward zero, then bias the truncation down to a floor and
// add one when the fractional part is >= 0.5 -- the same rule
// ICEMath::RoundFloatToInt applies.
// ---------------------------------------------------------------------------
u32 ICEElementDescription::Encode(f32 lfValue) const
{
    const f32 lfMin     = mMin.GetFloat();
    const f32 lfMax     = mMax.GetFloat();
    const f32 lfDefault = mDefault.GetFloat();

    // Clamp into [mMin, mMax] (low bound then high bound, branchless selects).
    f32 lfClamped = (lfMin - lfValue >= 0.0f) ? lfMin : lfValue;
    lfClamped = (lfMax - lfClamped >= 0.0f) ? lfClamped : lfMax;

    const f32 lfOffset = lfClamped - lfDefault;

    f32 lfSlot;
    if (lfOffset >= 0.0f)
    {
        lfSlot = (mfQuantSlotsHi * lfOffset) / mfQuantRangeHi + mfQuantSlotsLo;
    }
    else
    {
        lfSlot = (mfQuantSlotsLo * lfOffset) / mfQuantRangeLo + mfQuantSlotsLo;
    }

    // Round to nearest (truncate-toward-zero baseline, bias down to a floor, then
    // +1 when the fractional part reaches 0.5).
    s32 liSlot = (s32)lfSlot;          // truncate toward zero
    f32 lfFloor = (f32)liSlot;
    if (lfFloor > lfSlot)
        lfFloor -= 1.0f;
    if ((lfSlot - lfFloor) >= 0.5f)
        ++liSlot;

    return (u32)liSlot;
}

// ---------------------------------------------------------------------------
// Decode. The eICE_FIXED dequantiser: map a quantised integer slot
// back to a float using the precomputed (Prepare'd) lo/hi slot counts and value
// spans. Matches docs/ICEData.md eICE_FIXED:
//   result = mDefault;
//   if ((value - quantSlotsLo) >= 0) result += quantRangeHi*(value-quantSlotsLo)/quantSlotsHi;
//   else                             result += quantRangeLo*(value-quantSlotsLo)/quantSlotsLo;
// where mfQuantSlotsLo=value@+0x20, mfQuantRangeLo=+0x24, mfQuantSlotsHi=+0x28,
// mfQuantRangeHi=+0x2C, mDefault=+0x14. The slot index is converted as a signed
// int to f32 (sign-extend to 64-bit, then integer-to-float).
// ---------------------------------------------------------------------------
f32 ICEElementDescription::Decode(u32 luEncoded) const
{
    // The slot index is converted as a signed integer to float.
    f32 lfSlot = (f32)(s32)luEncoded;

    f32 lfOffset = lfSlot - mfQuantSlotsLo;
    f32 lfVal;
    if (lfOffset >= 0.0f)
    {
        lfVal = (mfQuantRangeHi * lfOffset) / mfQuantSlotsHi;
    }
    else
    {
        lfVal = (mfQuantRangeLo * lfOffset) / mfQuantSlotsLo;
    }

    return lfVal + mDefault.GetFloat();
}

// ---------------------------------------------------------------------------
// GetRawInt. Read one element's miDataBits-wide value out of the
// packed element-data buffer as a big-endian bit field. liIndex selects the
// (liIndex)th value of width miDataBits within lpElementData. Returns the raw,
// right-justified bits (no sign extension -- that is done by the caller for
// eICE_INT). docs: "All values are big endian except eICE_FLOAT".
// ---------------------------------------------------------------------------
u32 ICEElementDescription::GetRawInt(u8* lpElementData, s32 liIndex) const
{
    s32 liBitSize  = miDataBits;
    s32 liBitStart = liBitSize * liIndex;
    s32 liBitEnd   = liBitStart + liBitSize;

    // Byte span that the bit field touches.
    s32 liByteStart = liBitStart >> 3;
    s32 liByteEnd   = (liBitEnd + 7) >> 3;

    // Right-shift to drop the trailing bits of the last byte (can be negative if
    // the field ends past a byte boundary in the iteration accounting below).
    s32 liShiftRight = 8 * liByteEnd - liBitEnd;

    u64 lu64Mask = ((u64)1 << liBitSize) - 1;

    u8* lpDataStart = lpElementData + liByteStart;

    u32 luRet = 0;
    // Walk the touched bytes most-significant first (big endian): the masked
    // window starts aligned to the last byte and shifts right one byte each step.
    u64 luTemp = lu64Mask << liShiftRight;
    for (s32 liLoop = liByteEnd - liByteStart - 1; liLoop >= 0; --liLoop)
    {
        u32 luByte = (u32)lpDataStart[liLoop] & (u32)luTemp;
        if (liShiftRight > 0)
        {
            luByte >>= liShiftRight;
        }
        else if (liShiftRight < 0)
        {
            luByte <<= -liShiftRight;
        }
        luRet |= luByte;

        liShiftRight -= 8;
        luTemp >>= 8;
    }

    return luRet;
}

// ---------------------------------------------------------------------------
// SetRawInt. The inverse of GetRawInt: store luValue as a
// big-endian miDataBits-wide field into lpElementData at slot liIndex, preserving
// the surrounding bits. The shift amount here is liShiftLeft (positive) where
// GetRawInt's liShiftRight could go negative; same byte span.
// ---------------------------------------------------------------------------
void ICEElementDescription::SetRawInt(u8* lpElementData, s32 liIndex, u32 luValue) const
{
    s32 liBitSize  = miDataBits;
    s32 liBitStart = liBitSize * liIndex;
    s32 liBitEnd   = liBitStart + liBitSize;

    s32 liByteStart = liBitStart >> 3;
    s32 liByteEnd   = (liBitEnd + 7) >> 3;

    // Left-justify the field within the final byte of the span.
    s32 liShiftLeft = 8 * liByteEnd - liBitEnd;

    u64 lu64Mask = (((u64)1 << liBitSize) - 1) << liShiftLeft;  // inverted below per-byte
    u64 lu64Val  = ((u64)luValue) << liShiftLeft;

    // The asm carries the mask already complemented (not) so the per-byte AND
    // clears the field bits before OR-ing the value bits in.
    lu64Mask = ~lu64Mask;

    u8* lpDataStart = lpElementData + liByteStart;

    // Walk the touched bytes most-significant first, clearing then setting each.
    for (s32 liLoop = liByteEnd - liByteStart - 1; liLoop >= 0; --liLoop)
    {
        u8 lu8Cleared = (u8)(lu64Mask & (u64)lpDataStart[liLoop]);
        lpDataStart[liLoop] = (u8)(lu8Cleared | (u8)lu64Val);

        lu64Val  >>= 8;
        lu64Mask >>= 8;
    }
}

// ---------------------------------------------------------------------------
// GetValue. Read+interpret one element's value from the packed
// element-data buffer, dispatching on mDataType:
//   eICE_INT (0)   : sign-extend the miDataBits raw int into 32 bits.
//   eICE_UINT (1)  : raw bits (token index; the token table indirection is done
//                    by a higher layer -- this returns the raw int).
//   eICE_HASH (2)  : raw bits as an unsigned int.
//   eICE_FIXED (3) : Decode() the raw slot to a float.
//   eICE_FLOAT (4) : reinterpret the 32-bit native-endian word as IEEE float.
//   default        : mDefault (returned as the raw int word).
// liIndex is the value index within lpElementData. (lpElementData is u8* for the
// bit-stream cases; the eICE_FLOAT path indexes it as a 32-bit word array.)
// ---------------------------------------------------------------------------
ICEValue ICEElementDescription::GetValue(u8* lpElementData, s32 liIndex) const
{
    ICEValue lValue;

    switch (mDataType)
    {
        case eICE_INT:
        {
            // Sign-extend the miDataBits-wide field: shift up to the top then
            // arithmetic-shift back down.
            s32 liShift = 32 - miDataBits;
            s32 liRaw   = ((s32)GetRawInt(lpElementData, liIndex) << liShift) >> liShift;
            lValue.Set(liRaw);
            break;
        }

        case eICE_UINT:
        case eICE_HASH:
        {
            lValue.Set((s32)GetRawInt(lpElementData, liIndex));
            break;
        }

        case eICE_FIXED:
        {
            u32 luRaw = GetRawInt(lpElementData, liIndex);
            lValue.Set(Decode(luRaw));
            break;
        }

        case eICE_FLOAT:
        {
            // Native-endian 32-bit IEEE float (docs: eICE_FLOAT is native endian).
            lValue.Set(((const f32*)lpElementData)[liIndex]);
            break;
        }

        default:
        {
            lValue.Set(mDefault.GetSignedInt());
            break;
        }
    }

    return lValue;
}

// ---------------------------------------------------------------------------
// SetValue. Write one element's value into the packed element-data
// buffer, dispatching on mDataType:
//   eICE_INT / eICE_UINT / eICE_HASH (0,1,2): store the raw int bits directly.
//   eICE_FIXED (3) : Encode() the float to a quantised slot, then store the bits.
//   eICE_FLOAT (4) : store the float word natively.
//   default        : no-op.
// The int/float is read through the ICEValue argument (lrValue): the int
// cases pass lrValue.GetSignedInt() to SetRawInt; the float cases read
// lrValue.GetFloat(). The eICE_FIXED branch calls Encode (GROUP 2 above).
// ---------------------------------------------------------------------------
void ICEElementDescription::SetValue(u8* lpElementData, s32 liIndex, const ICEValue& lrValue) const
{
    switch (mDataType)
    {
        case eICE_INT:
        case eICE_UINT:
        case eICE_HASH:
        {
            SetRawInt(lpElementData, liIndex, lrValue.GetUnsignedInt());
            break;
        }

        case eICE_FIXED:
        {
            u32 luEncoded = Encode(lrValue.GetFloat());
            SetRawInt(lpElementData, liIndex, luEncoded);
            break;
        }

        case eICE_FLOAT:
        {
            ((f32*)lpElementData)[liIndex] = lrValue.GetFloat();
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Prepare. Validate the description and precompute the eICE_FIXED
// quantisation slot/range fields. The four quant fields are zeroed up front, then
// (for eICE_FIXED) filled per docs/ICEData.md:
//   quantRangeLo = mDefault - mMin;          (mfQuantRangeLo @+0x24)
//   quantRangeHi = mMax - mDefault;          (mfQuantRangeHi @+0x2C)
//   maxValue     = (1 << miDataBits) - 1;
//   quantSlotsHi = Round(quantRangeHi * maxValue / (mMax - mMin));  (mfQuantSlotsHi @+0x28)
//   quantSlotsLo = maxValue - quantSlotsHi;                          (mfQuantSlotsLo @+0x20)
// eICE_FLOAT forces miDataBits = 32. eICE_INT is asserted "not to be trusted".
// (CGS_ASSERT used; ICE::ICEMath::Round is declared in ICEMath.hpp.)
// ---------------------------------------------------------------------------
void ICEElementDescription::Prepare()
{
    CGS_ASSERT(mDataType < eICE_NUM_DATA_TYPES, "mDataType < eICE_NUM_DATA_TYPES");
    CGS_ASSERT(mInputType < eICE_NUM_INPUT_TYPES, "mInputType < eICE_NUM_INPUT_TYPES");
    CGS_ASSERT(miChannelNumber < eICE_NUM_CHANNELS, "miChannelNumber < eICE_NUM_CHANNELS");

    // miDataBits must already equal Clamp(miDataBits, 1, 32).
    s32 liClampedBits;
    if (miDataBits >= 1)
    {
        liClampedBits = miDataBits;
        if (miDataBits > 32)
        {
            liClampedBits = 32;
        }
    }
    else
    {
        liClampedBits = 1;
    }
    CGS_ASSERT(miDataBits == liClampedBits, "miDataBits == ICEMath::Clamp( miDataBits, 1, 32)");

    // The cubic-linear / tangent-scale element indices must point at descriptions
    // in this element's own channel. ICEElementDescriptions[] is the global element
    // table (indexed by element number; entry stride is the description size). It is
    // owned by this TU (the GROUP 1 global above).
    //
    // ⚠️⚠️ THE GUARD IS UNSIGNED, AND IT MATTERS. It used to be spelled `miCubicLinear <=
    // 0x2F` on an s32 -- a SIGNED test -- and 32 of the 48 table rows carry the "no such
    // element" sentinel -1 in both slots. Signed, -1 passes the guard, the inner
    // `< eICE_NUM_ELEMENTS` tripwire also passes (-1 < 48), and the line below then evaluates
    // ICEElementDescriptions[-1]: an OUT-OF-BOUNDS READ 88 bytes before the table, whose
    // garbage never matches miChannelNumber, so the assert fires for every sentinel row.
    // MEASURED the first time this function ever ran on PC: 78 + 78 asserts in one boot.
    //
    // The console's own test @0x8252EEF0 is the `subfic/subfe/addi` carry idiom over the
    // value the preceding `lwz` ZERO-EXTENDED into a 64-bit register -- i.e. exactly
    // `(u32)miCubicLinear <= 0x2Fu`, which -1 (0xFFFFFFFF) fails, so the console skips both
    // checks for a sentinel row. Reproduced as the unsigned compare it is. The INNER tripwire
    // stays SIGNED (`cmpwi r11, 0x30` / `blt` at 0x8252EF14): the two comparisons genuinely
    // differ in signedness, which is why the outer one reads like a duplicate of the inner one
    // and is not.
    if (static_cast<u32>(miCubicLinear) <= 0x2Fu)
    {
        CGS_ASSERT(miCubicLinear < eICE_NUM_ELEMENTS, "this->miCubicLinear < eICE_NUM_ELEMENTS");
        CGS_ASSERT(miChannelNumber == ICEElementDescriptions[miCubicLinear].miChannelNumber,
                   "this->miChannelNumber == ICE::ICEElementDescriptions[ miCubicLinear].miChannelNumber");
    }
    if (static_cast<u32>(miTangentScale) <= 0x2Fu)
    {
        CGS_ASSERT(miTangentScale < eICE_NUM_ELEMENTS, "miTangentScale < eICE_NUM_ELEMENTS");
        CGS_ASSERT(miChannelNumber == ICEElementDescriptions[miTangentScale].miChannelNumber,
                   "miChannelNumber == ICE::ICEElementDescriptions[ miTangentScale].miChannelNumber");
    }

    // Zero the precomputed quantisation fields (only eICE_FIXED fills them).
    mfQuantRangeLo = 0.0f;
    mfQuantRangeHi = 0.0f;
    mfQuantSlotsLo = 0.0f;
    mfQuantSlotsHi = 0.0f;

    switch (mDataType)
    {
        case eICE_FLOAT:
        {
            CGS_ASSERT(mMax.GetFloat() >= mMin.GetFloat(), "mMax.GetFloat() >= mMin.GetFloat()");
            miDataBits = 32;
            break;
        }

        case eICE_FIXED:
        {
            f32 lfMin     = mMin.GetFloat();
            f32 lfMax     = mMax.GetFloat();
            f32 lfDefault = mDefault.GetFloat();

            CGS_ASSERT(lfMax > lfMin, "lfMax > lfMin");
            // Default must already be clamped into [lfMin, lfMax].
            CGS_ASSERT(lfDefault == ICEMath::Clamp(lfDefault, lfMin, lfMax),
                       "lfDefault == ICEMath::Clamp( lfDefault, lfMin, lfMax)");

            f32 lfMaxValue = (f32)(((s32)1 << miDataBits) - 1);

            mfQuantRangeLo = lfDefault - lfMin;   // @+0x24
            mfQuantRangeHi = lfMax - lfDefault;   // @+0x2C

            f32 lfSlotsHi  = ICEMath::Round((mfQuantRangeHi * lfMaxValue) / (lfMax - lfMin));
            mfQuantSlotsHi = lfSlotsHi;           // @+0x28
            mfQuantSlotsLo = lfMaxValue - lfSlotsHi;  // @+0x20
            break;
        }

        case eICE_INT:
        {
            CGS_ASSERT(false,
                       "Hopefully we'll never see this, because eICE_INT is not to be trusted - JonE");
            s32 liMin = 1 << (miDataBits - 1);
            CGS_ASSERT(mMax.GetSignedInt() <= ~liMin, "mMax.GetSignedInt() <= ~lfMin");
            CGS_ASSERT(mMin.GetSignedInt() >= liMin, "mMin.GetSignedInt() >= lfMin");
            break;
        }

        case eICE_UINT:
        case eICE_HASH:
        {
            u32 luMax = 0xFFFFFFFFu >> (32 - miDataBits);
            CGS_ASSERT(mMax.GetUnsignedInt() <= luMax, "mMax.GetUnsignedInt() <= lfMax");
            break;
        }

        default:
            break;
    }
}

} // namespace ICE


// =============================================================================
// GROUP 3 -- ICE::ICEChannel bodies (5): GetIntervalParameter, GetIntervalBracket,
// GetIntervalSize, IsHardCut, SetParameter.
//
// Frozen layout (ICEDataEnums.hpp): ICEChannel { u16 mu16Keys @0; u16 mu16Intervals @2;
//   u16 mu16CurrentInterval @4; u16* mpKeyIndices @8; ICEParameter* mpParameters @C }.
// Constants used: 0.0f, 1.0f, and the parameter dequantiser 1/65535
// (== 1/ICE_PARAMETER_MAX; the same factor ICEParameter::GetValue divides by).
// =============================================================================

namespace ICE
{

// ICE::ICEChannel::GetIntervalParameter(u16) const
//
// The unit-interval parameter (in [0,1]) at the boundary indexed by lu16Interval.
// Boundary 0 is the take start (0.0); boundary >= mu16Intervals is the take end
// (1.0); every interior boundary reads the packed parameter stored for the prior
// interval (mpParameters[lu16Interval - 1]) and dequantises it back to the unit
// interval. mpParameters entries are u16-packed ICEParameter records, so the read
// is the same packed-value / 65535 dequantise ICEParameter::GetValue performs.
f32 ICEChannel::GetIntervalParameter(u16 lu16Interval) const
{
    if (lu16Interval == 0)
        return 0.0f;

    if (lu16Interval >= mu16Intervals)
        return 1.0f;

    // mpParameters[lu16Interval - 1] read as its packed u16 then dequantised by
    // 1/ICE_PARAMETER_MAX. The packed halfword is loaded zero-extended and converted
    // as a (non-negative) integer to float.
    const u16 lu16Packed = *mpParameters[lu16Interval - 1].GetPackedPtr();
    return (f32)(s32)lu16Packed * (1.0f / (f32)ICE_PARAMETER_MAX);
}

// ICE::ICEChannel::GetIntervalBracket(f32*, f32*) const   @0x8252D6B8
//
// Returns the [start,end] parameter values bracketing the channel's CURRENT interval.
// When no interval is current (mu16CurrentInterval == 0xFFFF) the bracket is the full
// unit range.
//
// ⭐ OVERLOAD CORRECTED 2026-08-01. This body used to be attached to the
// GetIntervalBracket(u16, f32*, f32*) sibling, with a comment noting that the u16
// argument was ignored. It is not ignored -- it was never there: 0x8252D6B8 takes
// exactly THREE registers (r3 = this, r4 = lpfStart, r5 = lpfEnd) and reads the
// interval from `lhz r9, 4(r3)` == mu16CurrentInterval, and its ONE caller,
// ICETake::Delete @0x8253C528, calls it with three registers too
// (0x8253C5C4-0x8253C5D0). Both overloads are DWARF-declared (ICEDataEnums.hpp:397
// and :399); this is the :399 one. The interval-argument sibling has no X360 body
// and no caller in the tree, so it stays declaration-only rather than being given
// this one's semantics.
//
// The console writes the END first (the 0x8252D708 call returns into `stfs f1,
// 0(r5)`) and the START second; order preserved.
void ICEChannel::GetIntervalBracket(f32* lpfStart, f32* lpfEnd) const
{
    const u16 lu16Current = mu16CurrentInterval;

    if (lu16Current == 0xFFFF)
    {
        *lpfStart = 0.0f;
        *lpfEnd   = 1.0f;
    }
    else
    {
        *lpfEnd   = GetIntervalParameter((u16)(lu16Current + 1));
        *lpfStart = GetIntervalParameter(lu16Current);
    }
}

// ICE::ICEChannel::GetIntervalSize(u16) const
//
// Width of the interval that starts at lu16Interval: the parameter delta between the
// next key and this one. The 0xFFFF sentinel (no/invalid interval) yields the full 1.0.
f32 ICEChannel::GetIntervalSize(u16 lu16Interval) const
{
    if (lu16Interval == 0xFFFF)
        return 1.0f;

    const f32 lfEnd   = GetIntervalParameter((u16)(lu16Interval + 1));
    const f32 lfStart = GetIntervalParameter(lu16Interval);
    return lfEnd - lfStart;
}

// ICE::ICEChannel::IsHardCut(u16, s32) const
//
// Detects a discontinuous ("hard cut") interval. liElement selects a neighbouring
// interval (offset 2*liElement, so 0 = this interval, -1/+1 = the prior/next pair edge);
// lu16Interval is the interval whose edge we compare against.
//
// It is a hard cut when either:
//   (a) the neighbour index falls outside the valid interval range [0, mu16Intervals-1], or
//   (b) the key indices bracketing the two interval edges differ by more than one key
//       (i.e. a key was skipped between them -> a discontinuity in the key run).
bool ICEChannel::IsHardCut(u16 lu16Interval, s32 liElement) const
{
    const s32 liNumIntervals = (s32)mu16Intervals;
    const s32 liLastInterval  = liNumIntervals - 1;
    const s32 liNeighbour     = 2 * liElement + (s32)lu16Interval - 1;

    // Clamp the neighbour into [0, liLastInterval]; if clamping changed it, the
    // requested neighbour was out of range -> treat as a hard cut.
    s32 liClamped;
    if (liLastInterval >= 0)
    {
        const s32 liFloored = (liNeighbour >= 0) ? liNeighbour : 0;
        liClamped = (liLastInterval >= liFloored) ? liFloored : liLastInterval;
    }
    else
    {
        liClamped = liNeighbour;
    }

    if (liNeighbour != liClamped)
        return true;

    // The key index sitting at the left edge of a given interval: interval 0 maps to key 0,
    // the final interval maps to the last key (mu16Keys - 2), and every interior interval n
    // reads the stored key index for its left boundary (mpKeyIndices[n - 1] via GetKeyIndex).
    // (Inlined helper, de-duplicated from its two call sites.)
    struct EdgeKey
    {
        const ICEChannel* lpChannel;
        s16 operator()(s32 liInterval) const
        {
            if (liInterval == 0)
                return 0;
            if (liInterval + 1 < (s32)lpChannel->GetNumIntervals())
                return lpChannel->GetKeyIndex((u16)(liInterval - 1));
            return (s16)(lpChannel->GetNumKeys() - 2);
        }
    };
    const EdgeKey lGetEdgeKey = { this };

    const s16 li16KeyHere      = lGetEdgeKey((s32)lu16Interval);
    const s16 li16KeyNeighbour = lGetEdgeKey(liNeighbour);

    s32 liKeyDelta = (s32)li16KeyHere - (s32)li16KeyNeighbour;
    if (liKeyDelta < 0)
        liKeyDelta = -liKeyDelta;

    return liKeyDelta > 1;
}

// ICE::ICEChannel::SetParameter(f32, f32*, f32*)
//
// Locate the interval that contains the playback parameter lfParameter, write the
// unit-interval bracket of that interval back through *lpfStart / *lpfEnd, advance
// mu16CurrentInterval to the located interval, and return whether the current
// interval actually changed.
//
// The bracket is tracked as a pair of packed ICEParameter values (lStartParam /
// lEndParam, init to 0.0 / 1.0) so the search compares against the same packed
// query value (lQueryParam = param(lfParameter)) the take stores; the located
// bracket is dequantised to floats only at the end.
//
// FAST PATH: if the cached current interval is still in range, test whether
// lfParameter still falls inside it; if so accept it without searching.
//
// SLOW PATH: clamp lfParameter into [interval-0 start, take end]; if it was already
// in range, binary-search the interval-boundary parameters for the containing
// interval, narrowing [lo,hi] and tightening the bracket each step.
bool ICEChannel::SetParameter(f32 lfParameter, f32* lpfStart, f32* lpfEnd)
{
    ICEParameter lStartParam(0.0f);   // start-of-bracket packed value
    ICEParameter lEndParam(1.0f);     // end-of-bracket packed value

    const u16 lu16NumIntervals = mu16Intervals;
    u16 lu16Found = 0xFFFF;           // located interval index (0xFFFF == none yet)

    if (lu16NumIntervals != 0)
    {
        // The packed unit-interval value being searched for.
        ICEParameter lQueryParam(lfParameter);

        // The candidate interval for the fast path: the cached current interval,
        // capped at numIntervals - 1.
        const u16 lu16Current = mu16CurrentInterval;
        s32 liCandidate = (s32)lu16NumIntervals - 1;
        if (liCandidate >= 0)
        {
            if (liCandidate >= (s32)lu16Current)
                liCandidate = (s32)lu16Current;
        }
        else
        {
            liCandidate = (s32)lu16Current;
        }

        // --- FAST PATH: is lfParameter still inside the current interval? ---
        if ((s32)lu16Current == liCandidate)
        {
            ICEParameter lBracketStart(GetIntervalParameter(lu16Current));
            ICEParameter lBracketEnd(GetIntervalParameter((u16)(lu16Current + 1)));
            // Accept when start <= query <= end (packed comparisons).
            if (lBracketEnd >= lQueryParam && lQueryParam >= lBracketStart)
            {
                lStartParam = lBracketStart;
                lEndParam   = lBracketEnd;
                lu16Found   = lu16Current;
            }
        }

        // --- SLOW PATH: binary search if the fast path missed. ---
        if (lu16Found == 0xFFFF)
        {
            s32 liHi = (s32)lu16NumIntervals - 1;
            u16 lu16Lo = 0;

            const f32 lfStart0 = GetIntervalParameter(0);             // interval-0 start (0.0)
            const f32 lfEnd    = GetIntervalParameter(lu16NumIntervals); // take end (1.0)

            // Clamp lfParameter into [lfStart0, lfEnd]; only search if it was
            // already in range (the clamp left it unchanged).
            f32 lfClamped = (lfParameter < lfStart0) ? lfStart0 : lfParameter;
            if (lfClamped > lfEnd)
                lfClamped = lfEnd;

            if (lfParameter == lfClamped)
            {
                lStartParam = ICEParameter(lfStart0);
                lEndParam   = ICEParameter(lfEnd);

                if (lu16NumIntervals != 1)
                {
                    do
                    {
                        const u16 lu16Mid = (u16)((((liHi - (s32)lu16Lo) >> 1) + (s32)lu16Lo));
                        ICEParameter lMidParam(GetIntervalParameter((u16)(lu16Mid + 1)));
                        if (lQueryParam < lMidParam)
                        {
                            // query lies below the midpoint -> search lower half.
                            lEndParam = lMidParam;
                            liHi      = (s32)lu16Mid;
                        }
                        else
                        {
                            // query lies at/above the midpoint -> search upper half.
                            lStartParam = lMidParam;
                            lu16Lo      = (u16)(lu16Mid + 1);
                        }
                    }
                    while ((s32)lu16Lo < liHi);
                }

                lu16Found = lu16Lo;
            }
        }
    }

    // Dequantise the located bracket back to unit floats.
    *lpfStart = lStartParam.GetValue();
    *lpfEnd   = lEndParam.GetValue();

    // Advance the cached current interval; report whether it changed.
    if (mu16CurrentInterval == lu16Found)
        return false;

    mu16CurrentInterval = lu16Found;
    return true;
}

} // namespace ICE


// =============================================================================
// GROUP 4 -- ICE::ICETakeData bodies (5): ComputeActualSize, ComputeEditSize,
// Construct, operator=, FixDown.
//
// The five bodies:
//   ICETakeData::Construct
//   ICETakeData::ComputeEditSize
//   ICETakeData::ComputeActualSize
//   ICETakeData::FixDown
//   ICETakeData::operator=
//
// The Compute*Size pair walks the static schema table ICE::ICEElementDescriptions
// (GROUP 1 above; eICE_NUM_ELEMENTS == 48). The element's required value count is
// keyed off the take's mElementCounts[channel]: element index < 28 uses mu16Keys,
// index >= 28 uses mu16Intervals. Per-element on-disk value bytes =
// ((miDataBits * count + 31) >> 3) & ~3, accumulated 4-byte-aligned over all 48.
// =============================================================================

namespace ICE {

// ---------------------------------------------------------------------------
// ICETakeData::ComputeActualSize
//
// On-disk size of THIS take: the 100-byte fixed header (bTNode + miGuid + name +
// mfLength + muAllocated + mElementCounts[12]), then the variable-data block:
//   totalIndices    = sum over 12 channels of max(mu16Intervals - 2, 0)
//   totalParameters = sum over 12 channels of max(mu16Intervals - 1, 0)
//   header(100) + 2*totalIndices  (u16 index list)
//               + 2*totalParameters (ICEParameter list), padded to 4 bytes
// then the per-element value bytes appended (each rounded up to 4), using the REAL
// per-channel counts in mElementCounts.
// ---------------------------------------------------------------------------
u32 ICETakeData::ComputeActualSize() const
{
    s32 liTotalIndices    = 0;
    s32 liTotalParameters = 0;

    for ( s32 liChannel = 0; liChannel < 12; ++liChannel )
    {
        const u32 luIntervals = mElementCounts[liChannel].mu16Intervals;
        CGS_ASSERT(luIntervals < 10000, "num_intervals < 10000");

        // max(intervals - 2, 0) indices and max(intervals - 1, 0) parameters.
        if ( (s32)(luIntervals - 2) > 0 )
            liTotalIndices += (s32)(luIntervals - 2);
        if ( (s32)(luIntervals - 1) > 0 )
            liTotalParameters += (s32)(luIntervals - 1);
    }

    // Header (100) + index list (u16 each) rounded to even, + parameter list (u16
    // each), the whole variable header rounded up to a 4-byte boundary.
    u32 luSize = (u32)((((2 * liTotalIndices + 101) & ~1) + 2 * liTotalParameters + 3) & ~3);

    // Append each element's packed value run, rounded up to 4 bytes.
    for ( s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement )
    {
        const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
        const s32 liChannel = lrDesc.miChannelNumber;

        const u32 luCount = (liElement >= 28)
                          ? mElementCounts[liChannel].mu16Intervals
                          : mElementCounts[liChannel].mu16Keys;

        luSize += (u32)(((lrDesc.miDataBits * luCount + 31) >> 3) & ~3u);
    }

    CGS_ASSERT(luSize < (2 * 1024), "size < (2*1024)");
    return luSize;
}

// ---------------------------------------------------------------------------
// ICETakeData::ComputeEditSize
//
// Edit-buffer size: identical shape to ComputeActualSize but using the FIXED edit
// caps instead of the real counts -- every channel is sized for the maximum editable
// run (ICE_MAX_EDIT_INTERVALS = 99 intervals -> 97 indices + 98 parameters; key runs
// at ICE_MAX_EDIT_KEYS = 100). The header + index + parameter region is therefore the
// constant 4780 (== ((100 + 2*(12*97) + 2*(12*98)) rounded)), and each element's value
// run is sized with count = 100 (key element, index < 28) or 99 (interval, index >= 28).
//
// (Note the value-run rounding uses +7 here vs +31 in ComputeActualSize -- the edit
//  path rounds the bit count up to the next BYTE; ComputeActualSize rounds the packed
//  on-disk run up to the next 4-BYTE word. Both then mask the running size to ~3.)
// ---------------------------------------------------------------------------
u32 ICETakeData::ComputeEditSize()
{
    // 100-byte header + max-cap index list (97/channel) + parameter list (98/channel).
    u32 luSize = 4780;

    for ( s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement )
    {
        const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];

        // Edit caps: key elements (index < 28) hold ICE_MAX_EDIT_KEYS values,
        // interval elements (index >= 28) hold ICE_MAX_EDIT_INTERVALS values.
        const s32 liCount = (liElement >= 28) ? ICE_MAX_EDIT_INTERVALS : ICE_MAX_EDIT_KEYS;

        luSize = (u32)((((lrDesc.miDataBits * liCount + 7) >> 3) + luSize + 3) & ~3);
    }

    return luSize;
}

// ---------------------------------------------------------------------------
// ICETakeData::Construct
//
// Zero-init: clear the bTNode base (8 bytes) and miGuid, clear the 32-byte take-name
// buffer, set mfLength = 1.0, muAllocated = 1, zero all 12 mElementCounts, then clear
// the edit-buffer variable-data region from offset 100 up to ComputeEditSize().
// ---------------------------------------------------------------------------
void ICETakeData::Construct()
{
    // bTNode base (Next/Prev) + miGuid.
    mPadNodeBase[0] = 0; mPadNodeBase[1] = 0; mPadNodeBase[2] = 0; mPadNodeBase[3] = 0;
    mPadNodeBase[4] = 0; mPadNodeBase[5] = 0; mPadNodeBase[6] = 0; mPadNodeBase[7] = 0;
    miGuid = 0;

    // Clear the 32-byte take name.
    rw::core::stdc::MemClear(macTakeName, KI_MAX_TAKENAME_LENGTH);

    mfLength    = 1.0f;
    muAllocated = 1;

    for ( s32 liChannel = 0; liChannel < 12; ++liChannel )
    {
        mElementCounts[liChannel].mu16Intervals = 0;
        mElementCounts[liChannel].mu16Keys      = 0;
    }

    // Zero the variable-data / edit region beyond the fixed 100-byte header.
    const u32 luEditSize = ComputeEditSize();
    memset((u8*)this + 100, 0, luEditSize - 100);
}

// ---------------------------------------------------------------------------
// ICETakeData::operator=
//
// Copy the take name (32 bytes), miGuid, and mfLength, then bulk-copy the variable
// data + mElementCounts block from offset 52 up to the SOURCE's ComputeActualSize()
// (so exactly the source's on-disk payload, mElementCounts + variable data, is
// copied). The bTNode base (Next/Prev) is intentionally NOT copied.
// ---------------------------------------------------------------------------
ICETakeData& ICETakeData::operator=(const ICETakeData& lrOther)
{
    rw::core::stdc::MemCopy(macTakeName, lrOther.macTakeName, KI_MAX_TAKENAME_LENGTH);
    miGuid   = lrOther.miGuid;
    mfLength = lrOther.mfLength;

    // Copy mElementCounts (@0x34) + the variable-data tail, sized by the source's
    // actual on-disk size. luSize is the payload length from offset 52 (0x34) onward.
    const u32 luSize = lrOther.ComputeActualSize() - 52;
    rw::core::stdc::MemCopy(&mElementCounts[0], &lrOther.mElementCounts[0], luSize);

    CGS_ASSERT(luSize < (2 * 1024), "luSize < (2*1024)");
    return *this;
}

// ---------------------------------------------------------------------------
// ICETakeData::FixUp
//
// RECOVERED 2026-08-01. The X360 symbol for this body is FOLDED: the per-entry call in
// DictionaryResourceType<ICE::ICETakeData>::FixUp @0x82665FF8 targets 0x82839600, which
// IDA labels CgsGeometric::PolygonSoupListSpatialMap::Construct -- an ICF fold, because
// the two functions compile to the identical four instructions:
//     0x82839600  li   r11, 0
//     0x82839604  stw  r11, 0(r3)
//     0x82839608  stw  r11, 4(r3)
//     0x8283960C  blr
// i.e. zero the first two words of the take and return. Those two words are the bTNode
// base's Next/Prev links, and this is the exact mirror of FixDown's first action ("null
// the intrusive-tree links ... they are rebuilt on load"). The resource argument is
// unread -- the loaded take needs no pointer relocation, because ICETake::SetDataPointers
// derives every channel pointer from the take base at USE time (see its body below).
//
// So relocate-on-load for a take is: unlink it. Nothing else.
// ---------------------------------------------------------------------------
void ICETakeData::FixUp(const CgsResource::Resource& lrResource)
{
    (void)lrResource;   // unread by the X360 body (r4 is never touched)

    // bNode::Next @0x00 / bNode::Prev @0x04 -- mPadNodeBase is the committed placeholder
    // for the not-yet-reconstructed bTNode/bNode base, so clearing its two words IS the
    // console's two stores (NOT an offset hack).
    *(u32*)&mPadNodeBase[0] = 0;   // bNode::Next
    *(u32*)&mPadNodeBase[4] = 0;   // bNode::Prev
}

// ---------------------------------------------------------------------------
// ICETakeData::FixDown
//
// Serialization fix-DOWN: turn live (in-memory, pointer-bearing) take data back into
// the relocatable on-disk form. Clears the bTNode base pointers (Next/Prev @0x00,
// @0x04), then drives the rebind through a temporary ICETake:
//   1. ICETake::SetDataPointers(this, /*edit=*/false) -- bind the live channel
//      pointers over this take's data so the per-channel counts can be validated.
//   2. Validate every element description tagged eICE_FLOAT (mDataType == eICE_FLOAT
//      == 4): the value count for that element (mu16Keys for index>=28, else
//      mu16Intervals) must stay under 5000.
//   3. ICETake::SetDataPointers(0, false) -- unbind (relocate the pointers back to
//      null / serialized form).
// The pointer "relocation" is performed entirely inside ICETake::SetDataPointers; the
// only direct pointer write here is zeroing the two bTNode link words.
// ---------------------------------------------------------------------------
void ICETakeData::FixDown(const CgsResource::Resource& lrResource)
{
    (void)lrResource;   // resource arg is unused (relocation is self-contained)

    // Null the intrusive-tree links (bNode::Next @0x00, bNode::Prev @0x04) before
    // serialization -- they are rebuilt on load. mPadNodeBase is the placeholder for
    // the not-yet-reconstructed bTNode/bNode base; clearing its 8 bytes zeroes both
    // link words (NOT an offset hack -- it is the documented base placeholder).
    *(u32*)&mPadNodeBase[0] = 0;   // bNode::Next
    *(u32*)&mPadNodeBase[4] = 0;   // bNode::Prev

    // A scratch ICETake drives the pointer rebind: the ICETake ctor runs on a stack
    // buffer (ICE::ICETake::ICETake) then SetDataPointers.
    //
    // WIRING NOTE (not an algorithm change): the frozen ICETake header declares no
    // default constructor, and its mCubics[28] member type Cubic1D declares only
    // Cubic1D(s16,f32), so ICETake is not default-constructible here. The scratch
    // take is used ONLY as a `this` for the two SetDataPointers calls, which write
    // (never read uninitialised) every member they subsequently consult. So a raw,
    // suitably-aligned stack buffer reinterpreted as ICETake is the faithful
    // stand-in for a stack ICETake -- no constructor runs, matching that
    // the rebind is self-contained. FLAG: replace with a real `ICETake lTake;` once
    // the ICETake default ctor (and Cubic1D's) are reconstructed into the headers.
    alignas(ICETake) u8 laTakeStorage[sizeof(ICETake)];
    ICETake& lTake = *reinterpret_cast<ICETake*>(laTakeStorage);
    lTake.SetDataPointers(this, false);    // bind live pointers over this take's data

    for ( s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement )
    {
        const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];

        // Only FLOAT-typed elements are range-checked here (mDataType == eICE_FLOAT).
        if ( lrDesc.mDataType == eICE_FLOAT )
        {
            const s32 liChannel = lrDesc.miChannelNumber;
            const u16 lu16NumData = (liElement >= 28)
                                  ? mElementCounts[liChannel].mu16Intervals
                                  : mElementCounts[liChannel].mu16Keys;
            CGS_ASSERT(lu16NumData < 5000, "num_data < 5000");
        }
    }

    lTake.SetDataPointers(0, false);   // unbind: relocate pointers back to serialized form
}

} // namespace ICE


// =============================================================================
// GROUP 5 -- ICE::ICETake bodies (15): Construct, NewEditBuffer, FreeEditBuffer,
// SetDataPointers, SetSubTake(ptr), SetSubTake(guid), GetValue(2-arg), SetValue,
// GetValueFloat(1-arg + 2-arg), GetValueInt(1-arg + 2-arg), GetSlope,
// SetParameter (private per-channel 3-arg) and SetParameter (PUBLIC 3-arg).
//
// Built against the frozen layout in ICEData.hpp (ICETake), ICEDataEnums.hpp
// (ICEChannel, ICEElementDescription, ICEValue), and ICEPoint.hpp (Cubic1D).
//
// ⭐ FOUR OF THESE WERE RECOVERED FROM UNNAMED SUBS (2026-08-01, ICE take-runtime
// wave). The X360 export set carries their bodies but not their symbol names, so
// an earlier pass concluded they "have no body anywhere" and the whole TU stayed
// out of the exe source list. Each is pinned by its caller set, not by a guess:
//   sub_8252F848 -> ICETake::GetValueFloat(s32, u16) const   (called by GetSlope,
//                   the private SetParameter, KeyAnimShakeController::Update,
//                   CameraShakeICEController::Update, 6 ICEAuthor/ICEController ops)
//   sub_8252F8F0 -> ICETake::GetValueInt(s32, u16) const
//   sub_82534118 -> ICETake::SetParameter(f32, bool, bool)   (called by
//                   KeyAnimController::Prepare/Update, ICEWrapper::PlayMovie,
//                   ICETake::Construct/NewEditBuffer/PopUndo/ChangeSize/SetSubTake)
//   sub_82533360 -> ICETake::SetSubTake(s32, bool)           (its own assert names
//                   "..\..\..\SDKs\Packages\ICE\ICEData.cpp" line 2575, so this TU
//                   is its home on the console's own evidence)
//
// External symbols this TU provides / references:
//   * ICE::ICEElementDescriptions[] + eICE_NUM_ELEMENTS  -- GROUP 1 above.
//   * ICE::gaICEElementChannels (ICEElementChannel[12])  -- GROUP 1 above (see FLAG).
//   * ICE::ICEMemory::GetMemory + ICE::spICEMemory (the ICE::ICEMemory* singleton)
//     + its embedded mEditHeap (CgsMemory::HeapMalloc at +0x520) -- ICEMemory.hpp.
//   * CGS_ASSERT; ICE_INVALID_INTERVAL / ICE_EPSILON (frozen header constants).
//   * ICETake private helpers MarkChannelFromSubTake / FlushUndo;
//     ICETakeData::Construct / ComputeEditSize / GetElementCounts / IsAllocated;
//     ICEChannel / Cubic1D accessors -- all frozen.
// =============================================================================

namespace ICE {

// ---------------------------------------------------------------------------
// ICETake::Construct(const IResourceManager*)
//
// Lifecycle init for a live take: store the resource-manager pointer, clear the
// playback parameters, reset every runtime channel to "no data", then bind the
// (still-null) data pointers and seed the parameter to 0.
//
// The +0x72C undo-list head is self-linked here (0x8253B374/0x8253B388/0x8253B38C:
// `addi r9, r31, 0x72C; stw r9, 0(r9); stw r9, 4(r9)`). It is REQUIRED, not
// cosmetic: FlushUndo/DiscardUndo/PushUndo all terminate on `head->Next == &head`,
// so an unlinked head walks uninitialised memory the first time NewEditBuffer runs.
// (An older FLAG here said the member was absent from the frozen header and skipped
// the self-link; ICEData.hpp has carried mUndoList[2] since the undo round landed,
// so the store is restored.)
//
// The parameter-seed tail is the PUBLIC SetParameter(f32,bool,bool) -- confirmed:
// the callee at 0x8253B3EC is sub_82534118, whose caller set is
// KeyAnimController::Prepare/Update + ICEWrapper::PlayMovie, and the argument
// mapping is exactly `f1 = 0.0f, r5 = 1, r6 = 0` (the reserved r4 slot belongs to
// the float). See that body below.
// ---------------------------------------------------------------------------
void ICETake::Construct(const IResourceManager* lpResourceManager)
{
    mpResourceManager = lpResourceManager;
    mfParameter    = 0.0f;
    mfSubParameter = 0.0f;
    mbNewSubTakeThisFrame = false;

    // Self-link the undo-history head (empty list == head points at itself).
    mUndoList[0] = mUndoList;   // Next
    mUndoList[1] = mUndoList;   // Prev

    // Reset all 12 runtime channels: no keys/intervals, no current interval
    // (0xFFFF sentinel), null key/parameter pointers.
    for (s32 liChannel = 0; liChannel < 12; ++liChannel)
    {
        mChannels[liChannel].SetNumKeys(0);
        mChannels[liChannel].SetNumIntervals(0);
        mChannels[liChannel].SetKeyData(0);
        mChannels[liChannel].SetParameterData(0);
        mChannels[liChannel].SetCurrentInterval(ICE_INVALID_INTERVAL);
    }

    // Bind the (null) data pointers, then seed the playback parameter to 0.
    SetDataPointers(0, false);
    SetParameter(0.0f, /*lbForce=*/true, /*lbWrap=*/false);  // all-channels parameter seed (this,_,1,0,0.0)
}

// ---------------------------------------------------------------------------
// ICETake::NewEditBuffer()
//
// Allocate a fresh editable take-data buffer sized for the maximum editable run,
// construct it, bind the live channel/element pointers over it (edit mode), seed
// the parameter, and clear the undo history.
//
// ALLOCATOR: this is the member call spICEMemory->GetMemory(size), where spICEMemory
// is the ICE memory-manager singleton (an ICE::ICEMemory*). GetMemory allocates from
// the manager's embedded edit heap (mEditHeap, +0x520). Both are owned by the ICE
// memory TU (ICEMemory.hpp).
// ---------------------------------------------------------------------------
void ICETake::NewEditBuffer()
{
    // WIRING NOTE (not an algorithm change): ICETakeData::ComputeEditSize is called
    // static-style (it reads no `this` -- the size is derived purely from the static
    // ICEElementDescriptions[] schema + the fixed edit caps). The frozen ICEData.hpp
    // declares ComputeEditSize as a non-static member, so it must be invoked on an
    // object; a throwaway take supplies that `this`, and since ComputeEditSize ignores
    // it the computed size is identical. FLAG: drop the temporary if ComputeEditSize is
    // ever re-declared static in the frozen header.
    ICETakeData lSizer;
    const u32 luEditSize = lSizer.ComputeEditSize();

    ICETakeData* lpEditData = (ICETakeData*)spICEMemory->GetMemory(luEditSize);
    lpEditData->Construct();

    // The fresh edit buffer is bound as the PRIMARY take data (lbEdit=false ->
    // mpTakeData=buf, mpSubTakeData=0), which is what FreeEditBuffer later frees via
    // mpTakeData. (NOT edit mode.)
    SetDataPointers(lpEditData, false);
    // Seed the playback parameter across all channels (see Construct's FLAG).
    SetParameter(0.0f, /*lbForce=*/true, /*lbWrap=*/false);
    FlushUndo();
}

// ---------------------------------------------------------------------------
// ICETake::FreeEditBuffer()
//
// Release the editable take-data buffer (if one is allocated), drop the pointer,
// and clear the undo history. muAllocated != 0 marks a heap-owned edit buffer
// (vs. a resource-owned, non-freeable take).
//
// ALLOCATOR: this frees through spICEMemory's embedded edit heap at +0x520
// (spICEMemory->mEditHeap), the SAME heap GetMemory allocated the buffer from.
// Free is a member of that heap: spICEMemory->mEditHeap.Free(block).
// ---------------------------------------------------------------------------
void ICETake::FreeEditBuffer()
{
    ICETakeData* lpTakeData = mpTakeData;
    if (lpTakeData != 0 && lpTakeData->IsAllocated())
    {
        spICEMemory->mEditHeap.Free(lpTakeData);
        mpTakeData = 0;
        FlushUndo();
    }
}

// ---------------------------------------------------------------------------
// ICETake::SetDataPointers(ICETakeData*, bool)
//
// Bind (or, when lpTakeData == 0, unbind) the runtime channel and element pointers
// into a serialised ICETakeData's variable-data block. lbEdit selects sub-take
// edit semantics: channels with no source data in the primary take are marked as
// coming from the sub-take (mxSubTakeChannels bit set) and are the ONLY channels
// bound to the incoming (sub-take) data -- the primary-supplied channels and the
// assembly channel keep their primary binding (sense corrected 2026-08-05; see
// the pass-2 note below).
//
// FLAG (offsets): the index/parameter/value base arithmetic is matched store-for-store
// against the asm (index base = take+100; parameter base = even-align(index_base +
// 2*totalIndices); value base = 4-align(parameter base + 2*totalParameters)). Each
// element's value run advances the cursor by ((miDataBits * count + 31) >> 3) & ~3.
// ---------------------------------------------------------------------------
void ICETake::SetDataPointers(ICETakeData* lpTakeData, bool lbEdit)
{
    mxSubTakeChannels = 0;

    if (lpTakeData == 0)
    {
        // ---- Unbind: reset every channel and element pointer ----
        for (s32 liChannel = 0; liChannel < 12; ++liChannel)
        {
            // In edit mode, a channel already flagged as sub-take-sourced, OR
            // channel 10 (assembly), OR a channel whose primary take has data is
            // left bound to the sub-take; others are cleared.
            if (lbEdit)
            {
                if (liChannel == 10 || mpTakeData->GetElementCounts()[liChannel].mu16Intervals != 0)
                    continue;
                MarkChannelFromSubTake(liChannel);
            }

            mChannels[liChannel].SetNumKeys(0);
            mChannels[liChannel].SetNumIntervals(0);
            mChannels[liChannel].SetKeyData(0);
            mChannels[liChannel].SetParameterData(0);
            mChannels[liChannel].SetCurrentInterval(ICE_INVALID_INTERVAL);
        }

        // Clear each element's data pointer (and the parallel value slot) unless,
        // in edit mode, the element's channel is sourced from the sub-take.
        for (s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement)
        {
            const s32 liChannel = ICEElementDescriptions[liElement].miChannelNumber;
            if (!lbEdit || (mxSubTakeChannels & (1 << liChannel)) != 0)
            {
                mValues[liElement] = (s32)0;
                mpElementData[liElement] = 0;
            }
        }
    }
    else
    {
        const ICEElementCount* lpCounts = lpTakeData->GetElementCounts();

        // ---- Pass 1: total index + parameter counts over all 12 channels ----
        s32 liTotalIndices    = 0;
        s32 liTotalParameters = 0;
        for (s32 liChannel = 0; liChannel < 12; ++liChannel)
        {
            const s32 liIntervals = lpCounts[liChannel].mu16Intervals;

            s32 liIdx = liIntervals - 2;
            if (liIdx < 0) liIdx = 0;
            liTotalIndices += liIdx;

            s32 liPar = liIntervals - 1;
            if (liPar < 0) liPar = 0;
            liTotalParameters += liPar;
        }

        // ---- Base pointers into the variable-data block ----
        u8* lpBase = (u8*)lpTakeData;
        u16*          lpIndexBase = (u16*)(lpBase + 100);
        // Parameter list starts after the index list, 2-byte aligned. (The align is
        // 32-bit integer math; use uintptr_t so the same alignment is
        // pointer-width-correct on the 64-bit PC host -- identical low-bit semantics.)
        ICEParameter* lpParamBase = reinterpret_cast<ICEParameter*>(
            (reinterpret_cast<uintptr_t>(lpBase + 100) + 2 * liTotalIndices + 1) & ~static_cast<uintptr_t>(1));
        // Value region starts after the parameter list, 4-byte aligned.
        u8* lpValueCursor = reinterpret_cast<u8*>(
            (reinterpret_cast<uintptr_t>(lpParamBase) + 2 * liTotalParameters + 3) & ~static_cast<uintptr_t>(3));

        // ---- Pass 2: bind each channel's key-index / parameter pointers ----
        u16*          lpIndexCursor = lpIndexBase;
        ICEParameter* lpParamCursor = lpParamBase;
        for (s32 liChannel = 0; liChannel < 12; ++liChannel)
        {
            const u16 lu16Intervals = lpCounts[liChannel].mu16Intervals;
            const u16 lu16Keys      = lpCounts[liChannel].mu16Keys;

            // ⭐ BIND SENSE CORRECTED 2026-08-05 (the junkyard-reveal wave). The console's
            // edit-mode branch (@0x8252FD50, the `v48 != 10 && *(primary counts) == 0` test
            // whose mark FALLS THROUGH INTO the bind at LABEL_46) binds a channel to the
            // sub-take ONLY when the channel is not 10/assembly AND the primary take has no
            // intervals for it -- i.e. exactly the channels the sub-take is supplying.
            // Channels the primary supplies, and the assembly channel itself, are LEFT
            // BOUND to the primary. The previous reconstruction inverted this (bound the
            // complement), so binding a sub-take REBOUND the assembly channel onto the
            // sub-take's empty channel 10: on the next public SetParameter the assembly
            // track read 0 intervals, CONTAINS_SUBTAKE evaluated 0, the whole sub-take arm
            // was skipped, and NO channel was ever driven again -- the authored game-intro
            // reveal froze on its first sub-take frame (measured: subParam pinned at
            // 0.001662 while param swept 0->1).
            bool lbBind = !lbEdit;
            if (lbEdit && liChannel != 10 &&
                mpTakeData->GetElementCounts()[liChannel].mu16Intervals == 0)
            {
                CGS_ASSERT(liChannel < 32, "liChannel<32");
                mxSubTakeChannels |= (1 << liChannel);
                lbBind = true;
            }

            if (lbBind)
            {
                mChannels[liChannel].SetNumKeys((s16)lu16Keys);
                mChannels[liChannel].SetNumIntervals((s16)lu16Intervals);
                mChannels[liChannel].SetKeyData(lpIndexCursor);
                mChannels[liChannel].SetParameterData(lpParamCursor);
            }

            // Advance the index/parameter cursors regardless (the layout is fixed).
            s32 liIdx = lu16Intervals - 2;
            if (liIdx < 0) liIdx = 0;
            lpIndexCursor += liIdx;

            s32 liPar = lu16Intervals - 1;
            if (liPar < 0) liPar = 0;
            lpParamCursor += liPar;
        }

        // ---- Pass 3: bind each element's packed-value-data pointer ----
        for (s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement)
        {
            const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
            const s32 liChannel = lrDesc.miChannelNumber;

            if (!lbEdit || (mxSubTakeChannels & (1 << liChannel)) != 0)
            {
                mpElementData[liElement] = lpValueCursor;
            }

            // Interval elements (index >= 28) use mu16Intervals; key elements use
            // mu16Keys. Advance the value cursor by the packed run size.
            const u16 lu16Count = (liElement >= 28)
                                ? lpCounts[liChannel].mu16Intervals
                                : lpCounts[liChannel].mu16Keys;
            lpValueCursor += ((lrDesc.miDataBits * lu16Count + 31) >> 3) & ~3u;
        }
    }

    // ---- Store the take pointers ----
    if (lbEdit)
    {
        mpSubTakeData = lpTakeData;
    }
    else
    {
        mpTakeData    = lpTakeData;
        mpSubTakeData = 0;
    }
}

// ---------------------------------------------------------------------------
// ICETake::SetSubTake(s32 liGuid, bool lbForce)   @0x82533360 (X360 sub_82533360)
//
// Resolve a take id through the bound resource manager and bind the result as this
// take's SUB-take. Its own assert names this file and line 2575, which is how the
// unnamed sub is pinned to this TU.
//
//     lwz r3, 0(r31)    ; mpResourceManager
//     lwz r11, 0(r3)    ; vptr
//     lwz r11, 0(r11)   ; vtable slot 0 == IResourceManager::GetTakeData(s32) -- see the
//                       ; slot-identification note on the body below (an earlier reading
//                       ; called this slot the ID overload; the image's vtable says no)
//     bctrl             ; r4 (the guid) is NOT reloaded -- the caller already left
//                       ; it there, so the argument passes straight through
//     assert(result, "lpSubTakeData", "..\\..\\..\\SDKs\\Packages\\ICE\\ICEData.cpp", 2575)
//     SetDataPointers(result, /*lbEdit=*/1)
//     if (lbForce) SetParameter(0.0f, true, true)
//
// NOTE the asymmetry with the pointer overload below: this one does NOT go through
// it (no branch to 0x82530E00) -- it calls SetDataPointers itself. Preserved.
// ---------------------------------------------------------------------------
    // ⭐ WHICH OVERLOAD SLOT 0 IS (2026-08-05, the junkyard-reveal wave). The vtable the
    // dispatch above indexes is at 0x820CEA3C in the image: slot 0 = 0x821F6A60, slot 1 =
    // GetTakeData(CgsResource::ID) @0x821F6A00. 0x821F6A60 is UNNAMED in the export set (a
    // known hole class) but its body is nine instructions: the ICEAuthor edited-take overlay,
    // then `bl 0x8267BEC0` == ICEList::GetICETakeDataFromGuid -- i.e. slot 0 is
    // **GetTakeData(s32 liGuid)**, the take-NUMBER resolve (MSVC lays adjacent overloaded
    // virtuals into the vtable in reverse declaration order, which is how the second-declared
    // s32 overload lands at slot 0). So this plain overload-resolved call IS the console
    // dispatch. MEASURED (BrnGame.log [ice-subtake]): DMV_IntroA's assembly names sub-take
    // id 606019 == GameIntro_01_NewWoosh's miGuid -- the guid space, NOT the dictionary-key
    // space, exactly as that body implies. An interim revision routed this through the ID
    // overload (dictionary keys are name-hashes) and every resolve missed; do not repeat it.
void ICETake::SetSubTake(s32 liGuid, bool lbForce)
{
    const ICETakeData* lpSubTakeData = mpResourceManager->GetTakeData(liGuid);

    CGS_ASSERT(lpSubTakeData != 0, "lpSubTakeData");   // ICEData.cpp:2575

    SetDataPointers(const_cast<ICETakeData*>(lpSubTakeData), true);

    if (lbForce)
    {
        SetParameter(0.0f, /*lbForce=*/true, /*lbWrap=*/true);
    }
}

// ---------------------------------------------------------------------------
// ICETake::SetSubTake(const ICETakeData*, bool)   @0x82530E00
//
// Bind a sub-take's data pointers (edit mode), then (if lbForce) re-seed the
// sub-take parameter so the new sub-take takes effect this frame.
// ---------------------------------------------------------------------------
void ICETake::SetSubTake(const ICETakeData* lpSubTakeData, bool lbForce)
{
    SetDataPointers(const_cast<ICETakeData*>(lpSubTakeData), true);
    if (lbForce)
    {
        // Re-apply the parameter so the freshly bound sub-take is evaluated this
        // frame: the all-channels parameter seed (this, _, 1, 1, 0.0f) -- i.e. the
        // public SetParameter(f32,bool,bool) with lbForce and lbWrap both set.
        SetParameter(0.0f, /*lbForce=*/true, /*lbWrap=*/true);
    }
}

// ---------------------------------------------------------------------------
// ICETake::GetValue(s32 liChannel, u16 lu16Key) const
//
// Read the decoded value of element liChannel (an element index, 0..47) at sample
// lu16Key. (The parameter is named liChannel to match the frozen declaration, but
// it indexes the element-description table, not a channel.) When the element's
// channel has no keys, the element's default value is returned; otherwise the
// value is decoded from the bound element-data buffer.
// ---------------------------------------------------------------------------
ICEValue ICETake::GetValue(s32 liChannel, u16 lu16Key) const
{
    CGS_ASSERT(liChannel < eICE_NUM_ELEMENTS, "liElementNumber < eICE_NUM_ELEMENTS");

    const ICEElementDescription& lrDesc = ICEElementDescriptions[liChannel];
    const ICEChannel& lrChannel = mChannels[lrDesc.miChannelNumber];

    // is_key for the first 28 elements -> bound by GetNumKeys; the rest are
    // interval elements -> bound by GetNumIntervals.
    s16 li16Limit = lrChannel.GetNumIntervals();
    if (li16Limit <= 0)
    {
        // No data in this channel: return the element's default value.
        return lrDesc.mDefault;
    }

    if (liChannel < 28)
        li16Limit = lrChannel.GetNumKeys();

    // [DIAG -- BRING-UP SCAFFOLDING, NOT CONSOLE CODE, capped at 20 lines.] The whole-run
    // assert set is dominated by this one bound (12 of 15 as of 2026-08-02, all through
    // BehaviourIceAnim::Prepare -> KeyAnimController::Prepare -> SetParameter -> GetSlope).
    // Print WHICH element, WHICH channel and WHICH index over WHICH limit, so the next wave
    // can decide between a data-port fault and a transcription fault in the bracket-key
    // arithmetic without another instrumented build. Remove with the ICE-camera campaign.
    if (lu16Key >= (u16)li16Limit)
    {
        static s32 s_iKeyBoundReports = 0;
        if (s_iKeyBoundReports < 20 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++s_iKeyBoundReports;
            *CgsDev::Log::gpDebugPrint
                << "[ice-bounds] element " << liChannel
                << " channel " << lrDesc.miChannelNumber
                << " key " << (s32)lu16Key
                << " limit " << (s32)li16Limit
                << " (numKeys " << (s32)lrChannel.GetNumKeys()
                << ", numIntervals " << (s32)lrChannel.GetNumIntervals() << ")\n";
        }
    }

    CGS_ASSERT(lu16Key < (u16)li16Limit,
               "lu16Index < ( is_key ? lChannel.GetNumKeys() : lChannel.GetNumIntervals())");

    return lrDesc.GetValue(mpElementData[liChannel], lu16Key);
}

// ---------------------------------------------------------------------------
// ICETake::SetValue(s32 liChannel, u16 lu16Key, ICEValue lValue)
//
// Write lValue into element liChannel (element index) at sample lu16Key, encoding
// it into the bound element-data buffer per the element's data type. (liChannel
// matches the frozen declaration but indexes the element-description table.)
// ---------------------------------------------------------------------------
void ICETake::SetValue(s32 liChannel, u16 lu16Key, ICEValue lValue)
{
    CGS_ASSERT(liChannel < eICE_NUM_ELEMENTS, "liElementNumber < eICE_NUM_ELEMENTS");

    const ICEElementDescription& lrDesc = ICEElementDescriptions[liChannel];
    const ICEChannel& lrChannel = mChannels[lrDesc.miChannelNumber];

    // is_key for the first 28 elements (GetNumKeys), interval elements otherwise.
    const s16 li16Limit = (liChannel >= 28)
                        ? lrChannel.GetNumIntervals()
                        : lrChannel.GetNumKeys();

    CGS_ASSERT(lu16Key < (u16)li16Limit,
               "lu16Index < ( lbIsKey ? lChannel.GetNumKeys() : lChannel.GetNumIntervals())");

    lrDesc.SetValue(mpElementData[liChannel], lu16Key, lValue);
}

// ---------------------------------------------------------------------------
// ICETake::GetValueFloat(s32, u16) const   @0x8252F848  (X360 sub_8252F848)
//
// The value of element liChannel AT SAMPLE lu16Key as a float. Unlike the 1-arg
// sibling below (which reads the already-evaluated mValues[] table), this one
// decodes the element out of the bound data buffer through GetValue, then applies
// the same IsFloat interpretation. The asm is GetValue into a stack ICEValue, then
// the identical `mDataType == 3 || mDataType == 4` test and the same
// int->float fcfid/frsp conversion as the 1-arg form:
//     bl ICE::ICETake::GetValue                          (0x8252F86C)
//     lwz r11, 0xC(&ICEElementDescriptions[liChannel])   (0x8252F880, 0x58 stride)
//     cmpwi 3 / cmpwi 4  -> IsFloat                      (0x8252F884-0x8252F894)
//     lfs  f1, <slot>    | lwz+extsw+fcfid+frsp          (0x8252F8A8 / 0x8252F8C0)
// ---------------------------------------------------------------------------
f32 ICETake::GetValueFloat(s32 liChannel, u16 lu16Key) const
{
    const ICEValue lValue = GetValue(liChannel, lu16Key);

    if (ICEElementDescriptions[liChannel].IsFloat())
        return lValue.GetFloat();

    // Non-float element: convert the stored signed int to float.
    return (f32)lValue.GetSignedInt();
}

// ---------------------------------------------------------------------------
// ICETake::GetValueInt(s32, u16) const   @0x8252F8F0  (X360 sub_8252F8F0)
//
// The value of element liChannel at sample lu16Key as an int: GetValue, then the
// same rounding the 1-arg sibling applies (truncate toward zero, bias down to a
// floor, +1 once the fractional part reaches 0.5) for a float-decoding element,
// or the stored int verbatim otherwise.
// ---------------------------------------------------------------------------
s32 ICETake::GetValueInt(s32 liChannel, u16 lu16Key) const
{
    const ICEValue lValue = GetValue(liChannel, lu16Key);

    if (!ICEElementDescriptions[liChannel].IsFloat())
        return lValue.GetSignedInt();

    const f32 lfValue = lValue.GetFloat();
    s32 liResult = (s32)lfValue;        // truncate toward zero
    f32 lfFloor = (f32)liResult;
    if (lfFloor > lfValue)
        lfFloor -= 1.0f;
    if ((lfValue - lfFloor) >= 0.5f)
        ++liResult;

    return liResult;
}

// ---------------------------------------------------------------------------
// ICETake::GetValueFloat(s32) const   @0x8252C0B8
//
// The decoded value of element liElement as a float, read straight out of the
// take's per-element value table mValues[liElement] (@+0x14). (The parameter is
// named liChannel in the frozen declaration but indexes the element table.) The
// element's data type decides the interpretation: a float-decoding element
// (eICE_FIXED or eICE_FLOAT, ICEElementDescription::IsFloat) returns the stored
// word as an IEEE float; any other type returns its stored int converted to float.
// ---------------------------------------------------------------------------
f32 ICETake::GetValueFloat(s32 liChannel) const
{
    if (ICEElementDescriptions[liChannel].IsFloat())
        return mValues[liChannel].GetFloat();

    // Non-float element: convert the stored signed int to float.
    return (f32)mValues[liChannel].GetSignedInt();
}

// ---------------------------------------------------------------------------
// ICETake::GetValueInt(s32) const
//
// The decoded value of element liElement as an int, read straight out of the take's
// per-element value table mValues[liElement] (@+0x14). (The parameter is named
// liChannel in the frozen declaration but indexes the element table.) A float-
// decoding element (IsFloat) returns the stored float rounded to the nearest int;
// any other type returns its stored int directly.
// ---------------------------------------------------------------------------
s32 ICETake::GetValueInt(s32 liChannel) const
{
    if (!ICEElementDescriptions[liChannel].IsFloat())
        return mValues[liChannel].GetSignedInt();

    // Float element: round the stored float to the nearest int (truncate toward
    // zero, bias down to a floor, +1 when the fractional part reaches 0.5).
    const f32 lfValue = mValues[liChannel].GetFloat();
    s32 liResult = (s32)lfValue;        // truncate toward zero
    f32 lfFloor = (f32)liResult;
    if (lfFloor > lfValue)
        lfFloor -= 1.0f;
    if ((lfValue - lfFloor) >= 0.5f)
        ++liResult;

    return liResult;
}

// ---------------------------------------------------------------------------
// ICETake::GetSlope(s32 liChannel, s32 liElement, u16 lu16Key, s32 liSide) const
//   (private)
//
// Compute the tangent (slope) for element liChannel's Hermite cubic at interval
// lu16Key on the requested side (liSide == 0 -> incoming/left, else outgoing/right).
//
// FLAG (tangent math): the base slope contribution is the CUBIC value delta
// (ValDesired-Val), the gate is miCubicLinear (+0x30) not miTangentScale,
// and the FINAL multiply is the miTangentScale (+0x34) partner sample. The internal
// float value-sampler (an inlined GetValueFloat-by-element) is modelled via
// GetValueFloat.
//
// ⭐ THE "focused review of this one function is recommended" NOTE HAS BEEN ACTED ON
// (2026-08-02) AND IT FOUND TWO REAL DEFECTS -- see the corrected block at (3) below.
// Both were in the finite-difference sample: the wrong index space (channel where the
// console passes the element) and the wrong key (liSampleKey where the console passes
// li16KeyNeighbour, an off-by-one on the incoming side). Between them they produced the
// entire ICEData.cpp:1851 key-bounds assert family. The parameter NAMES here are the
// frozen declaration's and are misleading: liChannel is the ELEMENT index (it indexes
// ICEElementDescriptions) and liElement is the CHANNEL index (it indexes mChannels).
// The X360 pins both: `dword_82F28880[22*a2]` vs `16*a3 + a1` @0x825303C0.
// ---------------------------------------------------------------------------
f32 ICETake::GetSlope(s32 liChannel, s32 liElement, u16 lu16Key, s32 liSide) const
{
    const ICEElementDescription& lrDesc = ICEElementDescriptions[liChannel];

    // (1) The cubic's value delta is the base slope contribution.
    const Cubic1D& lrCubic = mCubics[liChannel];
    const f32 lfCubicDelta = lrCubic.ValDesired - lrCubic.Val;

    // (2) Cubic-linear gate (description field +0x30, the per-channel cubic-linear
    // table indexed by liChannel).
    // When the gate FAILS the function returns the raw cubic delta with no further
    // processing (no blend, no tangent-scale multiply) -- matches the asm early-out.
    const u32 luCubicLinear = (u32)lrDesc.miCubicLinear;
    bool lbGate = (luCubicLinear != 0xFFFFFFFEu);
    if (lbGate && luCubicLinear <= 0x2F)
    {
        // Gate PASSES (proceed with the blend/tangent math) iff the gating element's
        // value is NON-zero; a zero gating value early-returns the raw cubic delta.
        // (The gate is the `!= 0` predicate on the gating element's value.)
        lbGate = (GetValue((s32)luCubicLinear, lu16Key).GetSignedInt() != 0);
    }
    if (!lbGate)
    {
        return lfCubicDelta;
    }

    const ICEChannel& lrChannel = mChannels[liElement];
    const s32 liStep = (liSide == 0) ? -1 : 1;

    // Left-edge value key of interval lu16Key (is_key bracket selection), offset by
    // the side. Computed unconditionally -- it feeds both the blend and the final
    // tangent-scale sample (r24 in the asm).
    s16 li16KeyHere;
    if (lu16Key == 0)
        li16KeyHere = 0;
    else if (lu16Key + 1 < (u16)lrChannel.GetNumIntervals())
        li16KeyHere = lrChannel.GetKeyIndex((u16)(lu16Key - 1));
    else
        li16KeyHere = (s16)(lrChannel.GetNumKeys() - 2);

    const s32 liSampleKey = (s32)(u16)li16KeyHere + liSide;   // r24
    const s32 liNeighbour = (s32)lu16Key + liStep;            // v15

    // (3) Blend a finite difference with the interval-size ratio. The hard-cut guard
    // only blends when the neighbour interval is in range and its bracket key is
    // contiguous with this one; otherwise the slope stays at the raw cubic delta.
    f32 lfSlope = lfCubicDelta;
    bool lbInRange = true;
    const s32 liLastInterval = (s32)lrChannel.GetNumIntervals() - 1;

    // The NEIGHBOUR interval's left-edge key (the X360's v21). Hoisted out of the
    // range test 2026-08-02 because the blend below samples it -- which is what the
    // console does too: v21 is computed at LABEL_15, reached BOTH by falling through the
    // range test and by the `numIntervals - 1 < 0` goto that skips it.
    s16 li16KeyNeighbour;
    if (liNeighbour == 0)
        li16KeyNeighbour = 0;
    else if (liNeighbour + 1 < (s32)lrChannel.GetNumIntervals())
        li16KeyNeighbour = lrChannel.GetKeyIndex((u16)(liNeighbour - 1));
    else
        li16KeyNeighbour = (s16)(lrChannel.GetNumKeys() - 2);

    if (liLastInterval >= 0)
    {
        s32 liClamped = (liNeighbour >= 0) ? liNeighbour : 0;
        if (liLastInterval < liClamped) liClamped = liLastInterval;

        if (liNeighbour != liClamped ||
            (u16)liSampleKey != (u16)((liSide ^ 1) + (s32)(u16)li16KeyNeighbour))
            lbInRange = false;
    }

    if (lbInRange)
    {
        // Finite difference of the sampled element across the NEIGHBOUR's bracket key.
        //
        // ⭐⭐ CORRECTED 2026-08-02 -- BOTH ARGUMENTS WERE WRONG, AND THEY WERE THE WHOLE
        // ICEData.cpp:1851 ASSERT FAMILY (12 of the 15 whole-run asserts). The X360 body
        // @0x825303C0 is unambiguous about both:
        //     sub_8252F848(a1, a2, (v21 + 1));   // GetValueFloat(element, keyNeighbour + 1)
        //     sub_8252F848(a1, a2, v21);         // GetValueFloat(element, keyNeighbour)
        // where the register roles are pinned by the two table indexings earlier in the same
        // body: `dword_82F28880[22*a2]` is the ELEMENT-description table (so a2 == liChannel,
        // the element index) and `16*a3 + a1` is mChannels[] (so a3 == liElement, the channel
        // index), and v21 is EdgeKey(liNeighbour) == li16KeyNeighbour.
        //
        //   (1) ELEMENT vs CHANNEL. This passed liElement -- the CHANNEL index -- where
        //       GetValueFloat wants an ELEMENT index. GetValue then bounds the read against
        //       ICEElementDescriptions[channelIndex].miChannelNumber, i.e. a DIFFERENT
        //       channel from the one whose key indices produced the index. Measured:
        //       "[ice-bounds] element 3 channel 0 key 2 limit 2 (numKeys 2, numIntervals 1)"
        //       -- element 3 is what channel index 3 decodes to, and its channel 0 has 2 keys.
        //   (2) WHICH KEY. This sampled {liSampleKey, liSampleKey + 1}; the console samples
        //       {li16KeyNeighbour, li16KeyNeighbour + 1}. Under the guard just above
        //       (liSampleKey == (liSide ^ 1) + li16KeyNeighbour) the two agree on the
        //       OUTGOING side (liSide == 1) and are OFF BY ONE on the INCOMING side
        //       (liSide == 0), where liSampleKey == li16KeyNeighbour + 1 -- so the incoming
        //       tangent read one key past the end of a 2-key channel. That is exactly the
        //       "key 2, limit 2" the diagnostic captured.
        const f32 lfValNext = GetValueFloat(liChannel, (u16)(li16KeyNeighbour + 1));
        const f32 lfValPrev = GetValueFloat(liChannel, (u16)li16KeyNeighbour);

        // Interval sizes: this interval (lu16Key) and the neighbour interval.
        const f32 lfSizeThis  = lrChannel.GetIntervalSize(lu16Key);
        const f32 lfSizeOther = lrChannel.GetIntervalSize((u16)liNeighbour);

        // weight = (1 - sizeOther/sum) * (sizeThis/sizeOther), guarded against zero
        // sizes; the cubicDelta is folded in scaled by (sizeOther/sum).
        //
        // CORRECTED 2026-08-01: both guards were transcribed as the literal
        // 0.00000099999997f (== 1e-6f, 0x358637BD) -- one decimal place too small.
        // The X360 loads flt_8207AB94 for BOTH compares (0x825305D4/0x825305DC and
        // 0x825305EC), and that slot is ICE_EPSILON == 1e-5f (0x3727C5AC); see its
        // definition at the top of this TU. Spelled through the named constant, as
        // the console does.
        f32 lfWeight = 1.0f;
        f32 lfRatio  = 0.0f;   // f10 = sizeOther / sum
        const f32 lfSum = lfSizeThis + lfSizeOther;
        if (lfSum > ICE_EPSILON)
        {
            lfRatio  = lfSizeOther / lfSum;
            lfWeight = 1.0f - lfRatio;
        }
        if (lfSizeOther > ICE_EPSILON)
        {
            lfWeight *= (lfSizeThis / lfSizeOther);
        }

        lfSlope = (lfValNext - lfValPrev) * lfWeight + lfRatio * lfCubicDelta;
    }

    // (4) Tangent-scale partner multiply (description field +0x34, the per-channel
    // tangent-scale table indexed by liChannel). Sample the partner element at the bracket key.
    f32 lfScale = 1.0f;
    const u32 luTangentScale = (u32)lrDesc.miTangentScale;
    if (luTangentScale <= 0x2F)
    {
        lfScale = GetValueFloat((s32)luTangentScale, (u16)liSampleKey);
    }

    return lfSlope * lfScale;
}

// ---------------------------------------------------------------------------
// ICETake::SetParameter(s32 liChannel, f32 lfParameter, bool lbForce)
//   (PRIVATE overload)
//
// Advance one channel to playback parameter lfParameter. First update the channel's
// current interval (ICEChannel::SetParameter); if it changed (or lbForce), rebuild
// the per-element Hermite cubic followers for that interval's bracketing keys, then
// evaluate every element in the channel at the normalised position within the
// interval and write the decoded value into mValues[].
//
// FLAG (gaICEElementChannels): the per-channel element schedule (a global) is the
// SAME static record InitICEDescriptions seeds and GROUP 1
// models as ICE::gaICEElementChannels -- referenced here so consolidation links to
// one definition (do NOT re-fork; move + grow when its real home lands).
// ---------------------------------------------------------------------------
bool ICETake::SetParameter(s32 liChannel, f32 lfParameter, bool lbForce)
{
    ICEChannel& lrChannel = mChannels[liChannel];

    // Per-channel element schedule: lists the channel's KEY elements (desc index < 28)
    // at miNumKeyElements/maiKeyElements and its INTERVAL elements (index >= 28) at
    // miNumIntervalElements/maiIntervalElements.
    const ICEElementChannel& lrSched = gaICEElementChannels[liChannel];

    // Advance the channel's current interval; start/end of the new interval come
    // back through the out-params (ICEChannel::SetParameter fills them).
    f32 lfStart = 0.0f;
    f32 lfEnd   = 0.0f;
    const bool lbChanged = lrChannel.SetParameter(lfParameter, &lfStart, &lfEnd);

    if (lbChanged || lbForce)
    {
        const u16 lu16IntervalKey = (u16)lrChannel.GetCurrentInterval();

        if (lrChannel.GetNumIntervals() > 0)
        {
            // Resolve the key index sitting at the current interval's left edge.
            s16 li16BracketKey;
            if (lu16IntervalKey == 0)
                li16BracketKey = 0;
            else if (lu16IntervalKey + 1 < (u16)lrChannel.GetNumIntervals())
                li16BracketKey = lrChannel.GetKeyIndex((u16)(lu16IntervalKey - 1));
            else
                li16BracketKey = (s16)(lrChannel.GetNumKeys() - 2);

            // Build a Hermite cubic follower for every KEY element of this channel.
            for (s32 liElem = 0; liElem < lrSched.miNumKeyElements; ++liElem)
            {
                const s32 liElement = lrSched.maiKeyElements[liElem];
                Cubic1D& lrCubic = mCubics[liElement];

                const f32 lfStartVal = GetValueFloat(liElement, (u16)li16BracketKey);
                if (lfStartVal != lrCubic.ValDesired) lrCubic.state = 2;
                lrCubic.Val = lfStartVal;

                const f32 lfEndVal = GetValueFloat(liElement, (u16)(li16BracketKey + 1));
                if (lfEndVal != lrCubic.Val)          lrCubic.state = 2;
                lrCubic.ValDesired = lfEndVal;

                const f32 lfSlopeIn = GetSlope(liElement, liChannel, lu16IntervalKey, 0);
                if (lfSlopeIn != lrCubic.dValDesired) lrCubic.state = 2;
                lrCubic.dVal = lfSlopeIn;

                const f32 lfSlopeOut = GetSlope(liElement, liChannel, lu16IntervalKey, 1);
                lrCubic.dValDesired = lfSlopeOut;

                // Hermite coefficients: h(t) = C0 t^3 + C1 t^2 + C2 t + C3 through
                // (Val, dVal) -> (ValDesired, dValDesired).
                const f32 lfDelta  = lrCubic.ValDesired - lrCubic.Val;
                const f32 lfdStart = lrCubic.dVal;
                lrCubic.Coeff[3] = lrCubic.Val;
                lrCubic.Coeff[2] = lfdStart;
                lrCubic.Coeff[0] = -((lfDelta * 2.0f) - (lfdStart + lfSlopeOut));
                lrCubic.Coeff[1] = -((lfdStart * 2.0f) - ((lfDelta * 3.0f) - lfSlopeOut));
            }
        }
        else
        {
            // Degenerate (no interval): collapse every key follower to the default.
            for (s32 liElem = 0; liElem < lrSched.miNumKeyElements; ++liElem)
            {
                const s32 liElement = lrSched.maiKeyElements[liElem];
                const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
                Cubic1D& lrCubic = mCubics[liElement];

                const f32 lfDefault = lrDesc.GetDefaultFloat();
                if (lfDefault != lrCubic.Val)        lrCubic.state = 2;
                lrCubic.ValDesired = lfDefault;
                if (lfDefault != lrCubic.ValDesired) lrCubic.state = 2;
                lrCubic.Val         = lfDefault;
                lrCubic.dValDesired = 0.0f;
                lrCubic.dVal        = 0.0f;

                const f32 lfDelta  = lrCubic.ValDesired - lrCubic.Val;
                const f32 lfdStart = lrCubic.dVal;
                const f32 lfdEnd   = lrCubic.dValDesired;
                lrCubic.Coeff[3] = lrCubic.Val;
                lrCubic.Coeff[2] = lfdStart;
                lrCubic.Coeff[0] = -((lfDelta * 2.0f) - (lfdStart + lfdEnd));
                lrCubic.Coeff[1] = -((lfdStart * 2.0f) - ((lfDelta * 3.0f) - lfdEnd));
            }
        }

        // Interval elements: resample directly at the current interval index.
        for (s32 liElem = 0; liElem < lrSched.miNumIntervalElements; ++liElem)
        {
            const s32 liElement = lrSched.maiIntervalElements[liElem];
            mValues[liElement] = GetValue(liElement, (u16)lrChannel.GetCurrentInterval());
        }
    }

    // ---- Evaluate every KEY element at the normalised position in the interval ----
    const f32 lfSize = lfEnd - lfStart;
    CGS_ASSERT(lfSize > ICE_EPSILON, "size > ICE_EPSILON");
    const f32 lfPos = (lfParameter - lfStart) / lfSize;

    for (s32 liElem = 0; liElem < lrSched.miNumKeyElements; ++liElem)
    {
        const s32 liElement = lrSched.maiKeyElements[liElem];
        const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
        const Cubic1D& lrCubic = mCubics[liElement];

        // Horner evaluation of the cubic at lfPos.
        const f32 lfValue = ((((lrCubic.Coeff[0] * lfPos) + lrCubic.Coeff[1]) * lfPos)
                              + lrCubic.Coeff[2]) * lfPos + lrCubic.Coeff[3];

        if (lrDesc.mDataType == eICE_FIXED || lrDesc.mDataType == eICE_FLOAT)
        {
            mValues[liElement] = lfValue;
        }
        else
        {
            // Round half-up to nearest integer (matches the asm floor/+0.5 path).
            s32 liRounded = (s32)lfValue;
            f32 lfFloor = (f32)liRounded;
            if (lfFloor > lfValue) lfFloor = (f32)(liRounded - 1);
            if ((lfValue - lfFloor) >= 0.5f) ++liRounded;
            mValues[liElement] = liRounded;
        }
    }

    return lbChanged;
}

// ---------------------------------------------------------------------------
// ICETake::SetParameter(f32 lfParameter, bool lbForce, bool lbWrap)
//   (PUBLIC overload)   @0x82534118  (X360 sub_82534118)
//
// ⭐ THE TAKE-LEVEL PLAYBACK DRIVER. This is what BrnDirector::KeyAnimController
// (Prepare + Update) and BrnDirector::ICEWrapper::PlayMovie call once per frame to
// advance a camera take; the private per-channel SetParameter above is its worker.
//
// Structure, instruction for instruction from 0x82534118:
//   1. clamp lfParameter into [0,1] with the fsel pair at 0x82534130-0x82534160
//      (f29 = flt_82001CC0 = 0.0f low bound, f31 = flt_82001C98 = 1.0f high bound
//      -- the same two rodata slots ICEChannel::GetIntervalBracket stores as its
//      0.0/1.0 bracket). That is ICEMath::Clamp(v, 0, 1).
//   2. unless lbForce, bail out when the clamped value already equals mfParameter
//      -- but STILL fall through the common tail, which stores
//      mbNewSubTakeThisFrame = 0 and returns 0 (`beq cr6, loc_825342C4`).
//   3. store mfParameter, then choose one of two paths on the ASSEMBLY channel
//      (channel 10, read as `lhz r11, 0x176(r31)` == mChannels[10].mu16Intervals):
//        * assembly channel empty OR lbWrap -> drive all 12 channels at
//          mfParameter with the caller's lbForce (loc_825342A0).
//        * otherwise -> the assembly path below.
//   4. assembly path: advance channel 10 first and keep its "changed" result; that
//      result is BOTH the return value and mbNewSubTakeThisFrame. Then, if the
//      take says it has a sub-take (element 47 CONTAINS_SUBTAKE), bind the sub-take
//      named by element 46 (TAKE_NUMBER, the guid) and map the assembly-local
//      parameter into the sub-take's own timeline:
//              sub = (param - assemblyIntervalStart) * (takeLen / subTakeLen)
//                    + element45 (TAKE_START)
//          clamped into [0,1] and stored as mfSubParameter. Every channel except 10
//          is then advanced at mfSubParameter if it is flagged sub-take-sourced
//          (mxSubTakeChannels bit) or at mfParameter otherwise; a sub-take-sourced
//          channel is forced when EITHER the assembly channel changed this frame or
//          the caller asked (0x8253424C-0x82534264).
//      If there is no sub-take, the two SHAKE elements (17 SHAKE_AMPLITUDE and
//      18 SHAKE_FREQUENCY, `stfs f29, 0x58/0x5C(r31)`) are zeroed instead.
//
// The three element indices are pinned by the mValues[] base (+0x14, 4-byte
// stride): 0xD0 -> [47] CONTAINS_SUBTAKE, 0xCC -> [46] TAKE_NUMBER, 0xC8 -> [45]
// TAKE_START -- all three are channel-10 (ASSEMBLY) elements in the schema table at
// the top of this TU, which is exactly what an assembly track needs.
// ---------------------------------------------------------------------------
bool ICETake::SetParameter(f32 lfParameter, bool lbForce, bool lbWrap)
{
    const f32 lfClamped = ICEMath::Clamp(lfParameter, 0.0f, 1.0f);

    // The channel-10 result: the return value AND mbNewSubTakeThisFrame. Stays
    // false on the all-channels path and on the unchanged-parameter early-out.
    bool lbSubTakeChanged = false;

    if (lbForce || mfParameter != lfClamped)
    {
        const s16 li16AssemblyIntervals =
            mChannels[KI_ICE_ASSEMBLY_CHANNEL].GetNumIntervals();

        mfParameter = lfClamped;

        if (li16AssemblyIntervals <= 0 || lbWrap)
        {
            // No assembly track (or the caller asked for the flat drive): every
            // channel runs on the take's own parameter.
            for (s32 liChannel = 0; liChannel < eICE_NUM_CHANNELS; ++liChannel)
            {
                SetParameter(liChannel, lfClamped, lbForce);
            }
        }
        else
        {
            lbSubTakeChanged = SetParameter(KI_ICE_ASSEMBLY_CHANNEL, lfClamped, lbForce);

            if (mValues[KI_ICE_ELEMENT_CONTAINS_SUBTAKE].GetSignedInt() != 0)
            {
                // Bind the sub-take the assembly track currently names (not forced:
                // this is the per-frame follow, not a seek).
                SetSubTake(mValues[KI_ICE_ELEMENT_TAKE_NUMBER].GetSignedInt(), false);

                f32 lfSubParameter = lfClamped;
                if (mpSubTakeData != 0)
                {
                    const f32 lfTakeStart = mValues[KI_ICE_ELEMENT_TAKE_START].GetFloat();
                    const f32 lfSegmentStart =
                        mChannels[KI_ICE_ASSEMBLY_CHANNEL].GetIntervalStart();
                    const f32 lfTimeScale =
                        mpTakeData->GetLength() / mpSubTakeData->GetLength();

                    lfSubParameter = (lfClamped - lfSegmentStart) * lfTimeScale + lfTakeStart;
                }
                mfSubParameter = ICEMath::Clamp(lfSubParameter, 0.0f, 1.0f);

                for (s32 liChannel = 0; liChannel < eICE_NUM_CHANNELS; ++liChannel)
                {
                    if (liChannel == KI_ICE_ASSEMBLY_CHANNEL)
                        continue;

                    if ((mxSubTakeChannels & (1 << liChannel)) != 0)
                    {
                        SetParameter(liChannel, mfSubParameter,
                                     lbSubTakeChanged || lbForce);
                    }
                    else
                    {
                        SetParameter(liChannel, mfParameter, lbForce);
                    }
                }
            }
            else
            {
                // No sub-take under the playhead: silence the shake pair.
                mValues[KI_ICE_ELEMENT_SHAKE_AMPLITUDE] = 0.0f;
                mValues[KI_ICE_ELEMENT_SHAKE_FREQUENCY] = 0.0f;
            }
        }
    }

    mbNewSubTakeThisFrame = lbSubTakeChanged;
    return lbSubTakeChanged;
}

} // namespace ICE

// ============================================================================
// ICE::ICETakeData::SaveData -- the debug XML/text dumper. Walks the take through
// a scratch ICETake and prints
// the decoded values via ICEFileHandler::FilePrintf. NOT the binary byte-stream
// writer (that is SetDataPointers / ComputeActualSize).
// ============================================================================
namespace ICE
{

// ICE::Precision -- number of significant decimal digits to display for a value.
// The integer magnitude scan (cap at 7, halve the budget once per power of ten
// down to a floor of 1) bounds how many fractional digits are extracted; the
// extraction loop pulls one decimal digit at a time until the fractional part hits
// zero or the digit budget runs out. The internal math runs in f64 so the digit
// extraction matches the value the caller passes as f32.
int Precision(f32 lfValue)
{
    const f64 lfDoubleValue = (f64)lfValue;

    // Scan the integer magnitude: the budget caps the fractional digits extracted.
    f64 lfScaled = lfDoubleValue;
    s32 liDigitBudget = 7;
    while (lfScaled > 1.0)
    {
        if (liDigitBudget <= 1)
            break;
        lfScaled = lfScaled * 0.1;
        --liDigitBudget;
    }

    s32 liResult = 1;

    // Fractional part of the input value via truncate-then-floor: (f64)(s32)x
    // truncates toward zero; if the truncation rose above the value (negative case)
    // step it down by one to get the true floor.
    f64 lfFloor = (f64)(s32)lfDoubleValue;
    if (lfFloor > lfDoubleValue)
        lfFloor = lfFloor - 1.0;
    f64 lfFraction = lfDoubleValue - lfFloor;

    if (liDigitBudget > 1)
    {
        do
        {
            if (lfFraction == 0.0)
                break;

            const f64 lfShifted = lfFraction * 10.0;
            ++liResult;

            f64 lfDigitFloor = (f64)(s32)lfShifted;
            if (lfDigitFloor > lfShifted)
                lfDigitFloor = lfDigitFloor - 1.0;

            lfFraction = lfShifted - lfDigitFloor;
        }
        while (liResult < liDigitBudget);
    }

    return liResult;
}

void ICETakeData::SaveData(ICEFileHandler* lpHandler, s32 liTakeNumber)
{
    // ---- Scratch ICETake bound over THIS take's data (read-only walk) ----
    // WIRING NOTE (same as ICETakeData::FixDown): the frozen ICETake header declares
    // no default constructor (its Cubic1D[28] member type declares only Cubic1D(s16,f32)),
    // so ICETake is not default-constructible here. The ICETake ctor runs on a stack
    // buffer then SetDataPointers; the take is used ONLY as a `this` for the
    // SetDataPointers + value-query calls, which write before they read. A raw aligned
    // stack buffer reinterpreted as ICETake is the faithful stand-in.
    // FLAG: replace with a real `ICETake lTake;` once the ICETake (and Cubic1D) default
    // ctors are reconstructed into the headers.
    alignas(ICETake) u8 laTakeStorage[sizeof(ICETake)];
    ICETake& lTake = *reinterpret_cast<ICETake*>(laTakeStorage);
    lTake.SetDataPointers(this, false);   // bind live pointers over this take's data

    // ---- Header tags ----
    const u32 luDataSize = ComputeActualSize();
    lpHandler->FilePrintf("<DATASIZE>%d</DATASIZE>\n", luDataSize);
    lpHandler->FilePrintf("<TAKE_NUMBER>%d</TAKE_NUMBER>\n", liTakeNumber);
    lpHandler->FilePrintf("<LENGTH>%f</LENGTH>\n", mfLength);

    // ---- Per-channel descriptions ----
    for (s32 liChannel = 0; liChannel < 12; ++liChannel)
    {
        const ICEElementCount& lrCount = mElementCounts[liChannel];
        if (lrCount.mu16Intervals != 0)
        {
            lpHandler->FilePrintf(
                "<CHANNEL_DESCRIPTION name=\"%s\" index=\"%d\"> <NUMINTERVALS>%d</NUMINTERVALS>"
                "  <NUMKEYS>%d</NUMKEYS></CHANNEL_DESCRIPTION>\n",
                gaICEElementChannels[liChannel].mpName,
                liChannel,
                lrCount.mu16Intervals,
                lrCount.mu16Keys);
        }
    }

    // ---- Per-channel key-index + parameter dumps (from the runtime channels) ----
    for (s32 liChannel = 0; liChannel < 12; ++liChannel)
    {
        // The loop count is the channel's interval count taken from the take header
        // (mElementCounts[ch].mu16Intervals). The bracket reads below use the runtime
        // channel the scratch take bound (GetNumIntervals/GetKeyData/GetNumKeys/
        // GetParameterData), exactly as the asm reads the bound ICEChannel.
        const u16 lu16Intervals = mElementCounts[liChannel].mu16Intervals;
        if (lu16Intervals == 0)
            continue;

        lpHandler->FilePrintf("<CHANNEL name=\"%s\" index=\"%d\">",
                              gaICEElementChannels[liChannel].mpName, liChannel);

        // --- KEY_INDICIES: the key index at the left edge of each interval ---
        lpHandler->FilePrintf("<KEY_INDICIES>");
        for (u32 luInterval = 0; luInterval < lu16Intervals; ++luInterval)
        {
            // Interval-key bracket (== ICETake::GetIntervalKey(channel, interval)):
            //   interval 0          -> key 0
            //   interior interval n -> mpKeyIndices[n-1]
            //   final interval      -> mu16Keys - 2
            u16 lu16Key;
            if (luInterval == 0)
            {
                lu16Key = 0;
            }
            else if ((luInterval + 1) < (u32)(u16)lTake.GetNumIntervals(liChannel))
            {
                lu16Key = lTake.GetKeyData(liChannel)[luInterval - 1];
            }
            else
            {
                lu16Key = (u16)(lTake.GetNumKeys(liChannel) - 2);
            }

            lpHandler->FilePrintf("<INTERVAL_KEY interval=\"%d\">%d</INTERVAL_KEY>\n",
                                  luInterval, lu16Key);
        }
        lpHandler->FilePrintf("</KEY_INDICIES>");

        // --- PARAMETERS: the unit-interval parameter at each interval start ---
        // (== ICEChannel::GetIntervalParameter: 0 -> 0.0, >= numIntervals -> 1.0, else
        //  mpParameters[n-1]/65535). Note the bound is `j <= lu16Intervals` -- the final
        //  pass prints the implicit 1.0 end parameter.
        lpHandler->FilePrintf("<PARAMETERS>");
        for (u32 luInterval = 0; luInterval <= lu16Intervals; ++luInterval)
        {
            f32 lfParameter;
            if (luInterval == 0)
            {
                lfParameter = 0.0f;
            }
            else if (luInterval < (u32)(u16)lTake.GetNumIntervals(liChannel))
            {
                lfParameter = lTake.GetParameterData(liChannel)[luInterval - 1].GetValue();
            }
            else
            {
                lfParameter = 1.0f;
            }

            lpHandler->FilePrintf("<INTERVAL_START interval=\"%d\">%f</INTERVAL_START>",
                                  luInterval, lfParameter);
        }
        lpHandler->FilePrintf("</PARAMETERS>");

        lpHandler->FilePrintf("</CHANNEL>");
    }

    // ---- Per-element value dumps ----
    for (s32 liElement = 0; liElement < eICE_NUM_ELEMENTS; ++liElement)
    {
        const ICEElementDescription& lrDesc = ICEElementDescriptions[liElement];
        const s32 liChannel = lrDesc.miChannelNumber;

        // Value count: interval elements (index >= 28) use mu16Intervals, key elements
        // use mu16Keys (same key/interval rule as the on-disk sizing).
        const u16 lu16Count = (liElement >= 28)
                            ? mElementCounts[liChannel].mu16Intervals
                            : mElementCounts[liChannel].mu16Keys;
        if (lu16Count == 0)
            continue;

        lpHandler->FilePrintf("<ELEMENT name=\"%s\" index=\"%d\">\n", lrDesc.mpTag, liElement);

        for (s32 liValue = 0; liValue < (s32)lu16Count; ++liValue)
        {
            lpHandler->FilePrintf("<VALUE index=\"%d\">", liValue);

            if (lrDesc.IsFloat())
            {
                // FIXED/FLOAT: read the decoded scalar (FIXED is dequantised to float;
                // FLOAT is the native word reinterpreted), then format via ICE::Precision.
                const ICEValue lValue = lTake.GetValue(liElement, (u16)liValue);
                const f32 lfValue = (lrDesc.mDataType == eICE_FIXED || lrDesc.mDataType == eICE_FLOAT)
                                  ? lValue.GetFloat()
                                  : (f32)lValue.GetSignedInt();

                // ICE::Precision returns the number of significant decimal digits to
                // display for the value; cast to f32 to fit the <FLOAT> tag's %f field.
                const f32 lfPrecision = (f32)Precision(lfValue);
                lpHandler->FilePrintf("<FLOAT precision=\"%f\" string=\"%f\">%x</FLOAT>",
                                      lfPrecision, lfValue);
            }
            else if (lrDesc.mDataType == eICE_HASH)
            {
                const s32 liRaw = lTake.GetValueInt(liElement, (u16)liValue);
                // FLAG (shipped quirk): the asm suppresses <HASH> output for the single
                // eICE_HASH element EVENT_TAG (ICEElementDescriptions[41]) via the guard
                // `&ICEElementDescriptions[41] != &lrDesc`. Since 41 is the ONLY HASH
                // element this means <HASH> is effectively never emitted; preserved verbatim.
                if (&ICEElementDescriptions[41] != &lrDesc)
                {
                    lpHandler->FilePrintf("<HASH>%x</HASH>", liRaw);
                }
            }
            else   // eICE_INT / eICE_UINT (token-table lookup)
            {
                bool lbPrinted = false;
                const s32 liRaw = lTake.GetValueInt(liElement, (u16)liValue);

                if (lrDesc.mpTokens != 0)
                {
                    if (liRaw >= lrDesc.miTokens)
                    {
                        // Out-of-range token index -> assert. (Builds the
                        // "Could not find token value ... saving <name>." message into
                        // the assert stream and fires it.)
                        // FLAG: the CgsDev::Assert::StrStreamBase machinery is out of
                        // scope here; modeled as a single CGS_ASSERT carrying the message.
                        CGS_ASSERT(false, "Could not find token value");
                    }
                    else
                    {
                        lpHandler->FilePrintf("<INT text=\"%s\">%d</INT>",
                                              lrDesc.mpTokens[liRaw], liRaw);
                        lbPrinted = true;
                    }
                }

                if (!lbPrinted)
                {
                    lpHandler->FilePrintf("<UNKNOWN>%d</UNKNOWN>", liRaw);
                }
            }

            lpHandler->FilePrintf("</VALUE>\n");
        }

        lpHandler->FilePrintf("</ELEMENT>\n");
    }

    // ---- Unbind the scratch take ----
    lTake.SetDataPointers(0, false);
}

} // namespace ICE
