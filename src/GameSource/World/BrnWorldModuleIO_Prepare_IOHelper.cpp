// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO_Prepare_IOHelper.cpp
//
// Thin explicit instantiation of the CgsModule::IOHelper<> constructor the X360
// ARTIST build emitted out-of-line for the world module's Prepare path:
//
//   0x827C2768  CgsModule::IOHelper<BrnWorld::WorldEntityIO::OutputBuffer_Prepare>
//               ::IOHelper(IOBufferStack*, const char*)  (baked CgsModuleIOHelper.h:52,
//               "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");
//               caller BrnWorld::WorldModule::Prepare.
//
// The asm stores mpStack (=lpStack) at +0x00, then calls CreateIOBuffer<OutputBuffer_Prepare>
// with &mpBuffer (+0x04), asserting on failure, and returns `this`. The ctor body lives
// inline in CgsModuleIOHelper.h; this TU only forces the instantiation, exactly like the
// sibling BrnParticle facade (BrnParticleResourceAccessors.cpp) forces
// IOHelper<ParticleIO::PrepareOutputBuffer>. The matching DestroyIOBuffer pop (dtor,
// CgsModuleIOHelper.h:55) is emitted out-of-line at WorldModule::Prepare's use site and is
// not this TU's symbol.
// ============================================================================
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"            // CgsModule::IOHelper<T>
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h" // OutputBuffer_Prepare (complete)

// ---- IOHelper<OutputBuffer_Prepare> constructor (X360 0x827C2768) ------------------
template
CgsModule::IOHelper<BrnWorld::WorldEntityIO::OutputBuffer_Prepare>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);
