#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from the shipped console build of
//   CgsDepthStencilStateFactory::Construct  [EXECUTED in goal trace]
//
// Builds the fixed set of depth/stencil states the immediate-mode renderer selects
// between. GetState is inlined into its readers.

// The five depth/stencil-state slots (five consecutive file-scope words on the console,
// one per index of saDepthStencilStates). The original header and .cpp declare
// this as a PRIVATE STATIC MEMBER of CgsDepthStencilStateFactory named
// saDepthStencilStates, and this wave promotes it to exactly that: it used to be a
// TU-local array in the anonymous namespace below, which is why no reader outside this
// file could name a slot -- and why BrnPostFx.cpp carried an invented
// `gpPostFxDepthStencilState` extern for saDepthStencilStates[1]. The enum that indexes
// it moved to the header for the same reason. Nothing about the values changes.
renderengine::DepthStencilState* CgsDepthStencilStateFactory::saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_COUNT] = {};

CgsDepthStencilStateFactory::CgsDepthStencilStateFactory()
{
    // Not console-attested for this TU (declaration-only); no ledger body to reconstruct from.
}

// Build the 5 depth/stencil states into saDepthStencilStates, asserting
// each slot came back non-null. Every state shares the same fixed parameter block; only
// the comparison function (muFunction) and the depth test/write enable flags differ.
void CgsDepthStencilStateFactory::Construct(rw::IResourceAllocator* lpAllocator)
{
    const char* const sourceFile = "..\\..\\..\\GameShared\\GameClasses\\Graphics/CgsDepthStencilStateFactory.cpp";
    void* lpMemory = 0;
    rw::Resource resource(lpMemory);
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON]   = 0;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF] = 0;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF]  = 0;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON]   = 0;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON]  = 0;

    // The shared fixed portion of the parameter block (identical for all 5 states; matches
    // the default depth/stencil params renderengine::StateHelper::Initialize builds).
    renderengine::DepthStencilState::Parameters lParameters;
    lParameters.muState4           = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.muState8           = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.muStencilReadMask  = 0xFFFFFFFFu;
    lParameters.muStencilWriteMask = 0xFFFFFFFFu;
    lParameters.muState14          = 0xFFFFFFFFu;
    lParameters.muState15          = 0xFFFFFFFFu;

    lParameters.mbDepthTestEnable  = 1;
    lParameters.muFunction         = D3DCMP_LESSEQUAL;
    lParameters.mbDepthWriteEnable = 1;
    resource = lpAllocator->Allocate(
        renderengine::DepthStencilState::GetResourceDescriptor(lParameters), 0);
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON] =
        renderengine::DepthStencilState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZLEQ_ZWRITEON ]", sourceFile, 66);

    lParameters.muFunction         = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.mbDepthTestEnable  = 0;
    lParameters.mbDepthWriteEnable = 0;
    resource = lpAllocator->Allocate(
        renderengine::DepthStencilState::GetResourceDescriptor(lParameters), 0);
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF] =
        renderengine::DepthStencilState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZOFF_ZALL_ZWRITEOFF ]", sourceFile, 75);

    lParameters.mbDepthTestEnable  = 1;
    lParameters.muFunction         = D3DCMP_LESSEQUAL;
    lParameters.mbDepthWriteEnable = 0;
    resource = lpAllocator->Allocate(
        renderengine::DepthStencilState::GetResourceDescriptor(lParameters), 0);
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF] =
        renderengine::DepthStencilState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZLEQ_ZWRITEOFF ]", sourceFile, 84);

    lParameters.muFunction         = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.mbDepthTestEnable  = 1;
    lParameters.mbDepthWriteEnable = 1;
    resource = lpAllocator->Allocate(
        renderengine::DepthStencilState::GetResourceDescriptor(lParameters), 0);
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON] =
        renderengine::DepthStencilState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZALL_ZWRITEON ]", sourceFile, 94);

    lParameters.mbDepthTestEnable  = 1;
    lParameters.muFunction         = D3DCMP_GREATEREQUAL;
    lParameters.mbDepthWriteEnable = 1;
    resource = lpAllocator->Allocate(
        renderengine::DepthStencilState::GetResourceDescriptor(lParameters), 0);
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON] =
        renderengine::DepthStencilState::Initialize(resource, lParameters);
    CGS_ASSERT_AT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZGTEQ_ZWRITEON ]", sourceFile, 103);
}

// ARTIST vtable 0x820CDC34, slot +0x04 targets the shared return at 0x8284CB38.
void CgsDepthStencilStateFactory::Destruct()
{
}

// ARTIST vtable slot +0x08 targets the shared true return at 0x82C296C8.
bool CgsDepthStencilStateFactory::Prepare()
{
    return true;
}
