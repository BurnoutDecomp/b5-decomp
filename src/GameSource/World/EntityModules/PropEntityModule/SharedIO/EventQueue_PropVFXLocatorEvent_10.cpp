#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

// Explicit-instantiation TU for the PropVFXLocatorEvent-element fixed-capacity queue
// the X360 build emitted out-of-line. The generic body is committed inline in
// CgsEventQueue.h (EventQueue<T,N>::Construct calls BaseEventQueue<T>::Construct(
// maEvents, KI_LENGTH)); this TU only forces the per-instantiation emission.
//
// CgsModule::EventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent, 10>::Construct  @ 0x8228DB20
// Points the base queue at its inline maEvents buffer (this + 0x10, 16-aligned because
// PropVFXLocatorEvent carries a Matrix44Affine), sets miMaxLength = 10, clears
// miLength. The X360 asserts the buffer is non-null ("lpEventBuffer != NULL",
// CgsBaseEventQueue.h:160) before storing it -- that assert lives in
// BaseEventQueue::Construct. Capacity 10 == KU_PROP_VFX_QUEUE_SIZE.
template void
CgsModule::EventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent, 10>::Construct();
