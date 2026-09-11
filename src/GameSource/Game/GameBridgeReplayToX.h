#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeReplayToX.h
//
// Support types for the BrnGame::BrnGameModule replay-bridge family
// (GameSource/Unity/../Game/GameBridgeReplayToX.cpp). Each per-frame bridge reads
// the replay module's pre/post-sim OUTPUT buffer (BrnReplays::ReplayIO::
// OutputBuffer_PreSim / OutputBuffer_PostSim, the committed homes) and republishes
// its contents into a downstream subsystem's INPUT buffer.
//
// The bridge's only support type, BrnGui::GuiReplayStatusEvent, now lives in its own
// canonical home (GameSource/Gui/Events/BrnGuiEventReplayStatus.h) -- it used to be
// re-defined here as a GuiEvent<514> boxing the status interface, which forked against the
// definition in GameSource/Gui/BrnGuiDemangledEventTypes.h and carried the wrong event id.
// This header now only pulls that home in.
//
// The GUI event sink is the REAL shared BrnGame::PushGuiEvent (GameBridgeGameStateToX.h):
// the whole record at offset 0 with sizeof(T) as its size, exactly as the console publisher
// does. The former bridge-local +4 offset hop is retired.
// ============================================================================

#include "types.hpp"
#include "GameSource/Gui/Events/BrnGuiEventReplayStatus.h"   // BrnGui::GuiReplayStatusEvent
