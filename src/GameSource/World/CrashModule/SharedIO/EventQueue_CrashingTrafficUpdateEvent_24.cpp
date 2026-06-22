#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"

// CgsModule::EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent, 24>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (24) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent, 24>::Construct();

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent>::AddEvent @ 0x825581E0
// Unconditional append: assert mpEvents != NULL, assert miLength < miMaxLength (both non-gating
// tripwires), copy the 80-byte element (muVehicleId head + 64-byte Matrix44Affine tail) into
// mpEvents[miLength], bump miLength, return true. Element stride 80 confirmed from the asm
// (5*count<<4). Called by NetworkInputInterface::AddTrafficUpdate / NetworkOutputInterface::AddOwnedTrafficUpdate.
template bool CgsModule::BaseEventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent>::AddEvent(const BrnWorld::CrashIO::CrashingTrafficUpdateEvent&);

// CgsModule::BaseEventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent>::Append @ 0x823C4148
// Merges all live events from a source queue onto this queue's tail: assert mpEvents != NULL,
// assert source.miLength + miLength <= miMaxLength, block-copy (XMemCpy modelled as memcpy)
// source's events at stride 80, advance miLength, return true. Called by NetworkInputInterface::operator=,
// BrnNetworkModuleIO/BrnNetworkModule post-sim paths, and UpdateOutputBuffer::SetCrashNetworkOutputInterface.
template bool CgsModule::BaseEventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent>::Append(const CgsModule::BaseEventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent>&);
