#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Gui/CustomRenderer/Renderers/BrnBoostMessageSlot.h"   // BoostMessage element home

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in
// CgsArray.h) for the BrnGui::BoostMessageSlot,3 leaf instantiation (the boost-message
// manager's fixed 3-slot array) -- the committed Array_/EventQueue_ explicit-instantiation
// pattern.
//   X360 0x8241D600 = Array<BoostMessage,3u>::Append  (memcpy 0x44; count @ +0xCC; CgsArray.h:225/226)
//   X360 0x8241DD08 = Array<BoostMessage,3u>::GetItem  (returns &maElements[i]; CgsArray.h:556/557)
template void                       Array<BrnGui::BoostMessage, 3>::Append(const BrnGui::BoostMessage&);
template BrnGui::BoostMessage&       Array<BrnGui::BoostMessage, 3>::GetItem(u32);
template const BrnGui::BoostMessage& Array<BrnGui::BoostMessage, 3>::GetItem(u32) const;
