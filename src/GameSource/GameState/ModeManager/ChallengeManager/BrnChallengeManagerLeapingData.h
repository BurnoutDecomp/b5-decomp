#pragma once

// The provisional stand-alone home of BrnGameState::ChallengeManager::{CarLeapingData,
// StoredLeapingData} (32-byte X360 pool-element blobs) has been grown in place into the
// manager's real header, exactly as the original thin slice planned ("single owner --
// grow in place"). This forward keeps the ObjectPool_CarLeapingData_7.cpp /
// ObjectPool_StoredLeapingData_7.cpp instantiation TUs' include lines valid.
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
