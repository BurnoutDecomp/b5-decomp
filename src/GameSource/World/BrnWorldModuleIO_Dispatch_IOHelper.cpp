// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO_Dispatch_IOHelper.cpp
//
// Thin explicit instantiations of the CgsModule::IOHelper<> constructor the X360
// build emitted out-of-line for BrnGame::BrnGameModule::DoDispatch's per-frame
// graphics-dispatch INPUT/OUTPUT buffer helpers:
//
//   0x823C18F0  CgsModule::IOHelper<BrnWorldIO::DispatchInputBuffer>
//               ::IOHelper(IOBufferStack*, const char*)
//   0x823C1958  CgsModule::IOHelper<BrnWorldIO::DispatchOutputBuffer>
//               ::IOHelper(IOBufferStack*, const char*)
//
// Each asm stores mpStack (=lpStack) at +0x00, then calls CreateIOBuffer<BufferType>
// with &mpBuffer (+0x04), asserting on failure ("mpStack->CreateIOBuffer( &mpBuffer,
// lpcName )", baked CgsModuleIOHelper.h:52), and returns `this`. The ctor body lives
// inline in CgsModuleIOHelper.h (line 35); these TUs only force the instantiation,
// exactly like the sibling BrnWorldModuleIO_Prepare_IOHelper.cpp forces
// IOHelper<OutputBuffer_Prepare>. The matching DestroyIOBuffer pop (dtor) is emitted
// out-of-line at DoDispatch's use site and is not these symbols.
// ============================================================================
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"                 // CgsModule::IOHelper<T>
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"           // BrnWorldIO::DispatchInputBuffer (complete)
#include "GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h"          // BrnWorldIO::DispatchOutputBuffer (complete)

// ---- IOHelper<DispatchInputBuffer> constructor (X360 0x823C18F0) --------------------
template
CgsModule::IOHelper<BrnWorldIO::DispatchInputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);

// ---- IOHelper<DispatchOutputBuffer> constructor (X360 0x823C1958) -------------------
template
CgsModule::IOHelper<BrnWorldIO::DispatchOutputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);
