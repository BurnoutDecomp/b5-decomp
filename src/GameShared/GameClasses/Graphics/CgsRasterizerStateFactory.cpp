#include "SDKs/RenderEngineClub/MAIN/components/src/states/rasterizerstate.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from the shipped console build of
//   CgsRasterizerStateFactory::Construct
//
// Builds the fixed set of three rasteriser states the immediate-mode renderer, the
// post-fx chain and the dispatch interpreter select between.

// Declared in the original header and defined in the .cpp -- the three consecutive
// file-scope words, now the class's private static table rather than a TU-local array,
// so that its readers (BrnPostFx::Render wants slot 2) can name a slot.
renderengine::RasterizerState* CgsRasterizerStateFactory::saRasterizerStates[E_FACTORY_RASTERIZER_STATE_COUNT] = {};

void CgsRasterizerStateFactory::Construct(rw::IResourceAllocator* lpAllocator)
{
    const char* const sourceFile = "..\\..\\..\\GameShared\\GameClasses\\Graphics/CgsRasterizerStateFactory.cpp";
    void* lpMemory = 0;
    rw::Resource resource(lpMemory);
    renderengine::RasterizerState::Parameters lParameters;

    lParameters.muMultiSampleMask = 0xffffffff;
    lParameters.muPrimitiveResetIndex = 0xffff;
    lParameters.mbPrimitiveResetEnable = 1;
    lParameters.mbMultiSampleAntiAlias = 1;
    lParameters.mbViewportEnable = 1;
    lParameters.mbScissorTestEnable = 1;

    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK] = 0;
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT] = 0;
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE] = 0;

    lParameters.muFillMode = 0;
    lParameters.muCullMode = renderengine::RasterizerState::E_CULL_BACK;
    lParameters.mbHalfPixelOffset = 0;
    resource = lpAllocator->Allocate(
        renderengine::RasterizerState::GetResourceDescriptor(lParameters), 0);
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK] =
        renderengine::RasterizerState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeBack ]", sourceFile, 63);

    lParameters.mbScissorTestEnable = 1;
    lParameters.muCullMode = (lParameters.muCullMode & 4) | renderengine::RasterizerState::E_CULL_FRONT;
    resource = lpAllocator->Allocate(
        renderengine::RasterizerState::GetResourceDescriptor(lParameters), 0);
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT] =
        renderengine::RasterizerState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeFront ]", sourceFile, 70);

    lParameters.mbScissorTestEnable = 1;
    lParameters.muCullMode &= 4u;
    resource = lpAllocator->Allocate(
        renderengine::RasterizerState::GetResourceDescriptor(lParameters), 0);
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE] =
        renderengine::RasterizerState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeNone ]", sourceFile, 77);
}

// ARTIST vtable 0x820CDC28, slot +0x04 targets the shared return at 0x8284CB38.
void CgsRasterizerStateFactory::Destruct()
{
}

// ARTIST vtable slot +0x08 targets the shared true return at 0x82C296C8.
bool CgsRasterizerStateFactory::Prepare()
{
    return true;
}
