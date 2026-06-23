#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCreateMatchOption.h"   // CreateMatchOption element home

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in
// CgsArray.h) for the BrnGui::CreateMatchOptions,75 leaf instantiation (OnlineGameOptions'
// create-match option list) -- the committed Array_/EventQueue_ explicit-instantiation
// pattern.
//   X360 0x82488B40 = Array<CreateMatchOption,75u>::Append  (8-byte stride; count @ +0x258; CgsArray.h:225/226)
//   X360 0x82488DC8 = Array<CreateMatchOption,75u>::GetItem (returns &maElements[i]; CgsArray.h:556/557)
template void                          Array<BrnGui::CreateMatchOption, 75>::Append(const BrnGui::CreateMatchOption&);
template BrnGui::CreateMatchOption&       Array<BrnGui::CreateMatchOption, 75>::GetItem(u32);
template const BrnGui::CreateMatchOption& Array<BrnGui::CreateMatchOption, 75>::GetItem(u32) const;
