// ============================================================================
// GameShared/GameClasses/Graphics/CgsStateFactoryLinkStubs.cpp
//
// Vtable filler for the three render-state factories (CgsBlendStateFactory,
// CgsDepthStencilStateFactory, CgsRasterizerStateFactory): each is polymorphic
// (virtual Construct/Destruct/Prepare), so wherever one is constructed its vtable
// names every virtual, and a virtual with no definition in the link is an LNK2019.
//
// Not reconstructions: the export set holds only the three Construct bodies
// (0x827EB2D8 / 0x827EBBA0 / 0x827EBF30) and no Destruct / Prepare symbol for any
// of the three, and no xref names one. Nothing in the tree calls Destruct or
// Prepare on any factory.
//
// DELETE-WHEN a console body for any of the six is recovered: write it in the
// owning .cpp and delete the matching definition here.
// ============================================================================

#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"

// CgsBlendStateFactory.cpp:235 (DWARF). No X360 symbol, no X360 body, no caller.
void CgsBlendStateFactory::Destruct()
{
}

// CgsBlendStateFactory.cpp:250 (DWARF). No X360 symbol, no X360 body, no caller.
// Returns true because the DWARF types it `bool` and every Prepare-shaped virtual in the
// tree's module protocol answers "ready"; nothing observes it today.
bool CgsBlendStateFactory::Prepare()
{
    return true;
}

// CgsDepthStencilStateFactory.cpp:116 (DWARF). No X360 symbol, no X360 body, no caller.
void CgsDepthStencilStateFactory::Destruct()
{
}

// CgsDepthStencilStateFactory.cpp:131 (DWARF). No X360 symbol, no X360 body, no caller.
bool CgsDepthStencilStateFactory::Prepare()
{
    return true;
}

// CgsRasterizerStateFactory.cpp:90 (DWARF). No X360 symbol, no X360 body, no caller.
void CgsRasterizerStateFactory::Destruct()
{
}

// CgsRasterizerStateFactory.cpp:105 (DWARF). No X360 symbol, no X360 body, no caller.
bool CgsRasterizerStateFactory::Prepare()
{
    return true;
}
