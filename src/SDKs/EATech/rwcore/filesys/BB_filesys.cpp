// BB_filesys.cpp: the RenderWare core filesys library as one bulk-build translation unit.
// Each member also compiles on its own; this file holds only their includes.
// Evidence:
// - ARTIST holds the library as one contiguous object, 0x82BBD3E0-0x82BC0A50, with nothing else
//   inside it and the members' functions interleaved across the whole range (75 functions in 25
//   runs over the 7 members): MSVC's emission passes over one TU, as for the EA job manager's
//   BB_runtime.job_manager.cpp.
// - ARTIST inlines manager.cpp's functions into the other members and keeps an out-of-line copy
//   too: Manager::GetInstance (0x82BBD630) into Device::Start (lwz gpFileSysManager at
//   0x82BBF650, the PS3 DecFIGS body 0xE46430 calls GetInstance there), Manager::Allocate
//   (0x82BBD640) into Stream::parsechunks (0x82BBF294), Manager::Free into Stream::restartstream
//   (0x82BC0788).
// File name: EA's bulk-build prefix BB_ (attested by the PS3 DecFIGS symbols of
// BB_runtime.job_manager.cpp and BB_runtime.rwatom.cpp) with this folder's name; the original
// module name of this library is not attested.
// Members: the filesys sources of this tree, whose file split is the reconstruction's. Order: the
// linear order that keeps the most adjacent member transitions of ARTIST's layout forward (17 of
// 24, scratch bulkorder.py), first member asyncop.cpp (the object's first function,
// AsyncOp::AsyncOp 0x82BBD3E0).
// Optimisation: the whole library is size-optimised (each member's file-level
// #pragma optimize("s", on) stands in for the library flag), so the members share one region
// here and none closes it: MSVC compiles template members (ListSingle<T>::Pop) with the state in
// effect at the end of the TU, and a size-optimised caller does not inline a callee compiled
// otherwise (a closing #pragma optimize("", on) leaves Device::ThreadEntry calling Pop where
// ARTIST inlines it, 0x82BBF420). devicedriver.cpp has no pragma of its own and is covered too.

#include "asyncop.cpp"
#include "stream.cpp"
#include "xenonfiledriver.cpp"
#include "device.cpp"
#include "devicedriver.cpp"
#include "manager.cpp"
#include "handle.cpp"
