// ============================================================================
// b5-decomp/src/GameSource/Graphics/BrnRendererModuleIO_Dispatch_IOHelper.cpp
//
// Thin explicit instantiations of the CgsModule::IOHelper<> constructor the X360
// ARTIST build emitted out-of-line for the renderer module's per-frame graphics
// INPUT/OUTPUT buffer helpers:
//
//   0x823C1820  CgsModule::IOHelper<RendererIO::InputBuffer>
//               ::IOHelper(IOBufferStack*, const char*)
//   0x823C1888  CgsModule::IOHelper<RendererIO::OutputBuffer>
//               ::IOHelper(IOBufferStack*, const char*)
//
// Each asm stores mpStack (=lpStack) at +0x00, then calls CreateIOBuffer<BufferType>
// with &mpBuffer (+0x04), asserting on failure ("mpStack->CreateIOBuffer( &mpBuffer,
// lpcName )", baked CgsModuleIOHelper.h:52), and returns `this`. The ctor body lives
// inline in CgsModuleIOHelper.h (line 35); these TUs only force the instantiation,
// exactly like the committed siblings BrnWorldModuleIO_Dispatch_IOHelper.cpp and
// BrnEffectsModuleIO_DispatchInputBuffer_IOHelper.cpp force their IOHelper<> ctors.
// The matching DestroyIOBuffer pop (dtor, CgsModuleIOHelper.h:55) is emitted
// out-of-line at the use sites and is not these symbols.
//
// Callers of these two out-of-line ctors: BrnGame::BrnGameModule::DoDispatch and
// MainGameFlowStateInitialLoadingScreen::Update.
// ============================================================================
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"   // CgsModule::IOHelper<T>
#include "GameSource/Graphics/BrnRendererModuleIO.h"           // RendererIO::InputBuffer / OutputBuffer (complete)

// ---- IOHelper<RendererIO::InputBuffer> constructor (X360 0x823C1820) ----------------
template
CgsModule::IOHelper<RendererIO::InputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);

// ---- IOHelper<RendererIO::OutputBuffer> constructor (X360 0x823C1888) ---------------
template
CgsModule::IOHelper<RendererIO::OutputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);
