// ===========================================================================
// MassiveAdClient3 -- wave partfile 02 (class:MassiveAdClient3::CMassiveAdObjectModelDynamic).
//
// This partfile carries NO function bodies. Its two assigned functions --
//     CMassiveAdObjectModelDynamic::CreateSlaveM @ 0x82BDE850
//     CMassiveAdObjectModelDynamic::Initialize   @ 0x82BDEEF8
// -- are fully reconstructed but CANNOT compile against the tree as it stands:
// they read/write base and collaborator members that no committed header
// declares yet (the spec's §5 CMassiveAdObject carve and the §6 friendships had
// NOT landed when this partfile was written -- verified by reading
// MassiveAdClient3AdObject.h / MassiveAdClient3ZoneManager.h /
// MassiveAdClient3Subscriber.h, not by trusting the ledger). Both bodies are
// parked verbatim, with their exact unblock lines, at:
//     scratchpad/waveN/parked/MassiveAdClient3_02_CreateSlaveM.cpp
//     scratchpad/waveN/parked/MassiveAdClient3_02_Initialize.cpp
//
// What this partfile DOES land is a compile check for the new slave home
// MassiveAdClient3ObjectModel.h (spec §4) -- the un-homed CMassiveAdObjectModel
// that off_82187214 names and that CreateSlaveM constructs inline. Including it
// next to this TU's own header proves the new class parses, that its inlined
// ctor chains the committed CMassiveAdObject base ctor with the measured
// argument shape, and that the two headers agree.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModelDynamic.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModel.h"  // CMassiveAdObjectModel (the slave)
