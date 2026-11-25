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
    renderengine::RasterizerState::Parameters lParameters;
    void* lpMemory = 0;
    rw::Resource resource(lpMemory);

    for (u32 i = 0; i < E_FACTORY_RASTERIZER_STATE_COUNT; ++i)
    {
        saRasterizerStates[i] = 0;
    }

    lParameters.SetCullMode(renderengine::RasterizerState::E_CULL_BACK);
    lParameters.SetScissorTestEnable(true);
    resource = lpAllocator->Allocate(
        renderengine::RasterizerState::GetResourceDescriptor(lParameters), 0);
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK] =
        renderengine::RasterizerState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeBack ]", sourceFile, 63);

    lParameters.SetCullMode(renderengine::RasterizerState::E_CULL_FRONT);
    lParameters.SetScissorTestEnable(true);
    resource = lpAllocator->Allocate(
        renderengine::RasterizerState::GetResourceDescriptor(lParameters), 0);
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT] =
        renderengine::RasterizerState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeFront ]", sourceFile, 70);

    lParameters.SetCullMode(renderengine::RasterizerState::E_CULL_NONE);
    lParameters.SetScissorTestEnable(true);
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
