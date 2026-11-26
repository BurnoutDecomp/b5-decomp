#include "GameShared/GameClasses/Core/CgsCoreConstants.h"

// CgsCoreConstants.cpp:51 (DWARF; literal as LegacySrc CgsCoreConstants.h:56 spells it).
const f32 CgsCore::KF_MILES_TO_METRES = 1609.344f;

// CgsCoreConstants.cpp:53. ARTIST keeps it in writable .data at 0x82F31928 (0x3EE4E26D), the home of a
// constant initialised from another constant rather than from a literal: LegacySrc :58 derives it from
// KF_MILES_TO_METRES (1609.344f / 3600.0f rounds to the same 0x3EE4E26D).
const f32 CgsCore::KF_MPH_TO_MPS = CgsCore::KF_MILES_TO_METRES / (60.0f * 60.0f);

// CgsCoreConstants.cpp:54. ARTIST 0x830180B0, dynamically initialised at 0x82C6D0C0.
const f32 CgsCore::KF_MPS_TO_MPH = 1.0f / CgsCore::KF_MPH_TO_MPS;
