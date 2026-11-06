#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include <cstddef>
CGS_STATIC_ASSERT((sizeof(CgsRasterizerStateFactory)==4), "native factory vptr only");
CGS_STATIC_ASSERT((sizeof(CgsDepthStencilStateFactory)==4), "native factory vptr only");
extern "C" renderengine::RasterizerState* NativeRasterizerFactoryGet(u32 slot) { return CgsRasterizerStateFactory::GetState(slot); }
extern "C" renderengine::DepthStencilState* NativeDepthFactoryGet(u32 slot) { return CgsDepthStencilStateFactory::GetState(slot); }
