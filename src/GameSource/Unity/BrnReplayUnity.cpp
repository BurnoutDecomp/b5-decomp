// BrnReplayUnity: one of ARTIST's unity translation units (DWARF compile unit BrnReplayUnity.cpp),
// ARTIST 0x8264B400-0x82661054.
// Each member also compiles on its own; this file holds only their includes.
// Include form: as ARTIST's other unity TUs' __FILE__ strings show (relative to this directory).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 18/24 transitions forward);
// first member: the one PS3's static constructor names (_GLOBAL__I_ of BrnReplays::ReplayModule::Construct).

#include "../Replays/BrnReplayModule.cpp"
#include "../Replays/BrnReplayModule_Prepare.cpp"  // holds functions of BrnReplayModule.cpp (x360 tree split)
#include "../Replays/BrnReplayDebugComponent.cpp"
#include "../Replays/BrnReplayBaseSerialiser.cpp"
#include "../Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.cpp"
#include "../Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.cpp"
#include "../Replays/Serialisers/BrnReplayRaceCarSerialiserFrame.cpp"  // holds functions of BrnReplayRaceCarEntitySerialiser.cpp (x360 tree split)
#include "../Replays/Serialisers/BrnReplayGameModuleSerialiser.cpp"
#include "../Replays/Stream/BrnReplayReadStream.cpp"
#include "../Replays/Stream/BrnReplayWriteStream.cpp"
#include "../Replays/Stream/BrnReplayDiskReadStream.cpp"
#include "../Replays/BrnReplayModuleIO.cpp"
