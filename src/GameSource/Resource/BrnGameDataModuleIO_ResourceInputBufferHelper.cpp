// Thin explicit instantiations of the CgsModule::IOHelper<CgsResource::ResourceIO::InputBuffer>
// constructor + destructor the X360 ARTIST build emitted out-of-line for the game-data module's
// Prepare path. Both are forced from this single TU (the paired ctor 0x82665DC8 and dtor 0x82665E30),
// exactly like the committed sibling BrnWorldModuleIO_Prepare_IOHelper.cpp forces
// IOHelper<WorldEntityIO::OutputBuffer_Prepare>. Generic ctor/dtor bodies are inline in
// CgsModuleIOHelper.h (asserts at :52 / :57). Caller BrnResource::GameDataModule::Prepare.
//
//   0x82665DC8  IOHelper<ResourceIO::InputBuffer>::IOHelper(IOBufferStack*, const char*)
//               (CgsModuleIOHelper.h:52, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )")
//   0x82665E30  IOHelper<ResourceIO::InputBuffer>::~IOHelper()
//               (CgsModuleIOHelper.h:57, "mpStack->DestroyIOBuffer( &mpBuffer )")
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"                 // CgsModule::IOHelper<T>
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"      // CgsResource::ResourceIO::InputBuffer

// ---- IOHelper<ResourceIO::InputBuffer> constructor (X360 0x82665DC8) ------------------
template
CgsModule::IOHelper<CgsResource::ResourceIO::InputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);

// ---- IOHelper<ResourceIO::InputBuffer> destructor (X360 0x82665E30) -------------------
template
CgsModule::IOHelper<CgsResource::ResourceIO::InputBuffer>::~IOHelper();
