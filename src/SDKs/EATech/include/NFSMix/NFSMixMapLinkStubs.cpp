// ===========================================================================
// NFSMixMap link stubs -- the l2-into-dev merge closure.
//
// The NFSMix wave declared + referenced these four NFSMixMap members
// (ProcessMixMap / NFSMixMapState::CreateMixCtls call them) but their bodies
// (@0x82B4BB98 / @0x82B4C2A8 / @0x82B4ACD8 / the ctl-data assign) are not yet
// reconstructed. FLAG link-stubs so the exe links; the sound mix path is not on
// the PC boot path yet. Replace each with the real body when its decompile lands.
// ===========================================================================

#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"

void NFSMixMap::Update3DMixCtls() {}                     // FLAG link-stub (@0x82B4BB98)
void NFSMixMap::UpdateEvtMixCtls() {}                    // FLAG link-stub (@0x82B4C2A8)
void NFSMixMap::UpdateMasterChannels() {}                // FLAG link-stub (@0x82B4ACD8)
void NFSMixMap::AssignMixCtlDataPtrs(stMixCtlProc* /*lpProc*/, int* /*lpEntry*/,
                                     int /*liObjectIndex*/, int /*liProcIdx*/) {}   // FLAG link-stub
