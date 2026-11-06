#include "GameSource/Resource/BrnDLCManager.h"
using namespace BrnResource;
extern "C" DLCBeatTheTeamGame* DLCProbeGame() { return &g_DLCFeatureAvailability.mBeatTheTeamGame; }
extern "C" bool DLCProbeAvailable() { return g_DLCFeatureAvailability.mBeatTheTeamGame.IsAvailable(); }
extern "C" bool DLCProbeEnabled() { return g_DLCFeatureAvailability.mBeatTheTeamGame.IsEnabled(); }
extern "C" void DLCProbeEnable() { g_DLCFeatureAvailability.mBeatTheTeamGame.SetEnabledState(true); }
