// ============================================================================
// BrnEffectsModuleIO_DispatchInputBuffer_IOHelper.cpp
//
// Thin explicit instantiation of the CgsModule::IOHelper<> constructor the X360
// ARTIST build emitted out-of-line for the effects module's Dispatch path:
//
//   0x823C19C0  CgsModule::IOHelper<BrnEffects::EffectsIO::DispatchInputBuffer>
//               ::IOHelper(IOBufferStack*, const char*)  (baked CgsModuleIOHelper.h:52,
//               "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");
//               caller BrnGame::BrnGameModule::DoDispatch.
//
// The asm stores mpStack (=lpStack) at +0x00, then calls
// CreateIOBuffer<DispatchInputBuffer> with &mpBuffer (+0x04), asserting on failure,
// and returns `this`. The ctor body lives inline in CgsModuleIOHelper.h; this TU only
// forces the instantiation, exactly like the committed sibling
// BrnWorldModuleIO_Prepare_IOHelper.cpp forces IOHelper<OutputBuffer_Prepare>. The
// matching DestroyIOBuffer pop (dtor, CgsModuleIOHelper.h:55) is emitted out-of-line at
// BrnGameModule::DoDispatch's use site and is not this TU's symbol.
// ============================================================================
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"                    // CgsModule::IOHelper<T>
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h" // DispatchInputBuffer (complete)

// ---- IOHelper<DispatchInputBuffer> constructor (X360 0x823C19C0) -------------------
template
CgsModule::IOHelper<BrnEffects::EffectsIO::DispatchInputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);
