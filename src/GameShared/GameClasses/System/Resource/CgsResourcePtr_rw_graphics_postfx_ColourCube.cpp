#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"  // complete rw::graphics::postfx::ColourCube

// Per-instantiation TU for CgsResource::ResourcePtr<rw::graphics::postfx::ColourCube>.
// The body is the generic inline accessor in CgsResourcePtr.h; this .cpp only forces
// the out-of-line emission of the one symbol the X360 ARTIST build attested.
//
//   GetMemoryResource()  @ 0x824FBE78  (ICF-folded; the ARTIST symbol reads
//                                       rw::graphics::postfx::ColourCub[e])
//
// X360 body: `if (!mpResourceMemory) Begin/Fire/EndAssert("Can not instance resource
// pointer - it has no main memory resource\n", CgsResourcePtr.h:581); return
// mpResourceMemory;` -- i.e. the generic ResourcePtr<Type>::GetMemoryResource() with
// Type = rw::graphics::postfx::ColourCube. The non-const "instance" message + baked
// assert line 581 pin it to GetMemoryResource() (non-const) per the shared generic
// (CgsResourcePtr.h:575 -> baked line 581); operator->() bakes to line 544 and
// operator*() to 563/637, so 581 is uniquely GetMemoryResource(). Its two call sites
// are BrnGui::PFXNodeFader::LookupColourCube and
// BrnWorld::EnvironmentSettings::EnvironmentManager::Prepare, both instancing the
// post-fx colour-grading cube resource. Generic body inline in CgsResourcePtr.h; this
// is the thin explicit instantiation.
template rw::graphics::postfx::ColourCube*
CgsResource::ResourcePtr<rw::graphics::postfx::ColourCube>::GetMemoryResource();
