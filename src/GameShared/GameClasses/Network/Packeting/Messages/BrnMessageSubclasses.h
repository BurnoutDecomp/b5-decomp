#pragma once

#include "types.hpp"
#include "CgsMessage.h"

// BrnNetwork message subclasses whose Release() bodies are homed in
// CgsMessageSubclasses.cpp. Each Release() clears the inherited Message VALID flag
// (mx8Flags +0x19); HullSyncMessage additionally empties its hull array (+0x54 is the
// array's live-element count). Both classes have their real owning headers; this file
// only gathers them for that TU.
#include "GameSource/Network/Messages/BrnHullSyncMessage.h"
#include "GameSource/Network/Messages/BrnUpdateMessage.h"
