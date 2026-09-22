#pragma once

// ===================================================================================
// gameshared_network_defines.h -- network-wide compile-time sizes shared by the game-side
// network managers (global scope, as in the original header). Grown additively as further
// constants from this header are needed.
// ===================================================================================

#include "types.hpp"

// The number of remote network players a manager's per-player tables hold (every per-player
// manager loop runs this many times and the asserts spell it by name).
const s32 KI_MAX_NETWORK_PLAYERS = 7;
