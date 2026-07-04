#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"

// Per-instantiation compilation home for the CgsModule::IOHelper<T> members the X360 build
// emitted out-of-line for the RouteMapModuleIO buffer types. The generic ctor/dtor bodies are
// inline in CgsModuleIOHelper.h; these thin explicit instantiations force the out-of-line
// emissions the ARTIST build attested. Both are borrowed by BrnAI::AIModule::Update.

// ---- IOHelper<InputBuffer> constructor (X360 0x82794B80, caller AIModule::Update) ------------
// Stores mpStack, pushes a typed InputBuffer via mpStack->CreateIOBuffer(&mpBuffer, lpcName) and
// asserts success (CgsModuleIOHelper.h:52).
template
CgsModule::IOHelper<BrnAI::RouteMapModuleIO::InputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);

// ---- IOHelper<OutputBuffer> destructor (X360 0x82791820, caller AIModule::Update) ------------
// Pops the borrowed output buffer via mpStack->DestroyIOBuffer(&mpBuffer) and asserts the result
// (CgsModuleIOHelper.h:57).
template
CgsModule::IOHelper<BrnAI::RouteMapModuleIO::OutputBuffer>::~IOHelper();
