#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from the shipped console build of
//   CgsDepthStencilStateFactory::Construct  [EXECUTED in goal trace]
//
// Builds the fixed set of depth/stencil states the immediate-mode renderer selects
// between. Only Construct is console-attested for this TU (Destruct / Prepare / GetState
// are declaration-only -- see the header).

// The five depth/stencil-state slots (five consecutive file-scope words on the console,
// one per index of saDepthStencilStates). The original header and .cpp declare
// this as a PRIVATE STATIC MEMBER of CgsDepthStencilStateFactory named
// saDepthStencilStates, and this wave promotes it to exactly that: it used to be a
// TU-local array in the anonymous namespace below, which is why no reader outside this
// file could name a slot -- and why BrnPostFx.cpp carried an invented
// `gpPostFxDepthStencilState` extern for saDepthStencilStates[1]. The enum that indexes
// it moved to the header for the same reason. Nothing about the values changes.
renderengine::DepthStencilState* CgsDepthStencilStateFactory::saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_COUNT] = {};

namespace
{

    // Carve the backing store for one depth/stencil state through the supplied resource
    // allocator, then initialise it. The console sizes it via
    // DepthStencilState::GetResourceDescriptor (slot-0 {size, align}), carves it via
    // lpAllocator's vtable slot +0x10 (DoAllocate),
    // then DepthStencilState::Initialize. Mirrors renderengine::StateHelper::Initialize's
    // AllocateStateResource + CreateDefaultBlendState pattern.
    renderengine::DepthStencilState* CreateDepthStencilState(
        rw::IResourceAllocator* lpAllocator,
        const renderengine::DepthStencilState::Parameters* lpParameters)
    {
        renderengine::ResourceDescriptor5 lDescriptor;
        renderengine::DepthStencilState::GetResourceDescriptor(&lDescriptor, lpParameters);

        rw::ResourceDescriptor lAllocDescriptor;
        lAllocDescriptor.m_baseResourceDescriptors[0].m_size      = lDescriptor.maEntries[0].muSize;
        lAllocDescriptor.m_baseResourceDescriptors[0].m_alignment = lDescriptor.maEntries[0].muAlignment;
        lAllocDescriptor.m_baseResourceDescriptors[1].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[1].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;

        rw::Resource lResource = lpAllocator->DoAllocate(lAllocDescriptor, nullptr);
        renderengine::DepthStencilState* lpStateHandle =
            static_cast<renderengine::DepthStencilState*>(lResource.m_baseResources[0]);

        return renderengine::DepthStencilState::Initialize(&lpStateHandle, lpParameters);
    }
}

CgsDepthStencilStateFactory::CgsDepthStencilStateFactory()
{
    // Not console-attested for this TU (declaration-only); no ledger body to reconstruct from.
}

// Build the 5 depth/stencil states into saDepthStencilStates, asserting
// each slot came back non-null. Every state shares the same fixed parameter block; only
// the comparison function (muFunction) and the depth test/write enable flags differ.
void CgsDepthStencilStateFactory::Construct(rw::IResourceAllocator* lpAllocator)
{
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON]   = nullptr;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF] = nullptr;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF]  = nullptr;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON]   = nullptr;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON]  = nullptr;

    // The shared fixed portion of the parameter block (identical for all 5 states; matches
    // the default depth/stencil params renderengine::StateHelper::Initialize builds).
    renderengine::DepthStencilState::Parameters lParameters = {};
    lParameters.muState4           = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.muState8           = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.muStencilReadMask  = 0xFFFFFFFFu;
    lParameters.muStencilWriteMask = 0xFFFFFFFFu;
    lParameters.muState14          = 0xFFFFFFFFu;
    lParameters.muState15          = 0xFFFFFFFFu;

    lParameters.muFunction         = 3u;
    lParameters.mbDepthTestEnable  = 1;
    lParameters.mbDepthWriteEnable = 1;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON] =
        CreateDepthStencilState(lpAllocator, &lParameters);
    CGS_ASSERT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZLEQ_ZWRITEON ]");

    lParameters.muFunction         = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.mbDepthTestEnable  = 0;
    lParameters.mbDepthWriteEnable = 0;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF] =
        CreateDepthStencilState(lpAllocator, &lParameters);
    CGS_ASSERT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZOFF_ZALL_ZWRITEOFF ]");

    lParameters.muFunction         = 3u;
    lParameters.mbDepthTestEnable  = 1;
    lParameters.mbDepthWriteEnable = 0;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF] =
        CreateDepthStencilState(lpAllocator, &lParameters);
    CGS_ASSERT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZLEQ_ZWRITEOFF ]");

    lParameters.muFunction         = renderengine::DepthStencilState::E_FUNCTION_ALWAYS; // == 7
    lParameters.mbDepthTestEnable  = 1;
    lParameters.mbDepthWriteEnable = 1;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON] =
        CreateDepthStencilState(lpAllocator, &lParameters);
    CGS_ASSERT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZALL_ZWRITEON ]");

    lParameters.muFunction         = 6u;
    lParameters.mbDepthTestEnable  = 1;
    lParameters.mbDepthWriteEnable = 1;
    saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON] =
        CreateDepthStencilState(lpAllocator, &lParameters);
    CGS_ASSERT(saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON],
               "saDepthStencilStates[ eFactoryDepthStencilState_ZON_ZGTEQ_ZWRITEON ]");
}

// FLAG PC-platform leaf: no console body or caller; vtable filler.
void CgsDepthStencilStateFactory::Destruct()
{
}

// FLAG PC-platform leaf: no console body or caller; vtable filler.
bool CgsDepthStencilStateFactory::Prepare()
{
    return true;
}
