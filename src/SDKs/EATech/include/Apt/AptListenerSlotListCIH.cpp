// Explicit instantiation of AptListenerSlotList<IAptListener>.
// Forces add() @ 0x82ADBCE0 and remove() @ 0x82ADBC28.
// Callers: AptKey::sMethod_addListener/removeListener,
//   AptDisplayList::_addToSetCaches, AptCIH::AssociateInstToClass,
//   AptCIH::objectMemberSet, AptCIH::ClearCIH.
#include "SDKs/EATech/include/Apt/AptListenerSlotList.h"

template struct AptListenerSlotList<IAptListener>;
