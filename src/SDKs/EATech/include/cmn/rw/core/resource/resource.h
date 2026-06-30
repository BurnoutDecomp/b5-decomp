#pragma once
#include "rw/rwcore_structs.h"  // CANONICAL HOME: rw::BaseResourceDescriptor, rw::BaseResources<N>,
                                //                 rw::BaseResourceDescriptors<N> (and operator+=).

// ===========================================================================
// rw/core/resource/resource.h -- EATech SDK-path mirror for the RenderWare core
// RESOURCE descriptor family (rw:: namespace).
//
// This is the DWARF/source path the X360 build compiled the resource templates
// from (DecFIGS attributes the inlined `rw::BaseResourceDescriptors<N>` /
// `rw::BaseResources<N>` code to this header). The Burnout PC decomp homes that
// `rw::` type vocabulary -- layout-faithful to rwcore.pdb -- in the canonical
// vendor header rw/rwcore_structs.h, which ~50 committed TUs already include by
// name. To avoid an ODR fork this SDK-path header does NOT redefine those
// templates; it re-roots the canonical home so a `#include` of the EATech path
// resolves to the one true definition.
//
// COMMITTED & REUSED BY NAME (do not re-home / redefine):
//   rw::BaseResourceDescriptor          { uint32_t m_size; uint32_t m_alignment; }   (rwcore_structs.h)
//   rw::BaseResources<Count>            { void* m_baseResources[Count]; }            (rwcore_structs.h)
//   rw::BaseResourceDescriptors<Count>  { BaseResourceDescriptor[Count]; ... += }    (rwcore_structs.h)
//   rw::Resource           : BaseResources<4>                                        (rwcore_structs.h)
//   rw::ResourceDescriptor : BaseResourceDescriptors<4>                              (rwcore_structs.h)
//   CgsResource::ResourceDescriptor = rw::BaseResourceDescriptors<5>   (CgsResourceType.h)
//
// CROSS-BUILD DRIFT (already documented in rwcore_structs.h): the X360 game build
// instantiates the *serialised* descriptor with FIVE entries
// (rw::BaseResourceDescriptors<5>, 40B); the PC rwcore.lib narrowed
// rw::ResourceDescriptor to <4> (32B). The committed generic per-Count
// `operator+=` reproduces the X360 asm store-for-store (round size up to the
// other entry's alignment, widen alignment to the max, add size); see
// renderware/src/rw/BaseResourceDescriptors_3.cpp for the proven out-of-line
// pattern (the X360 <5> instance's operator+= is exercised inline by the
// GetSerialisedResourceDescriptor handlers below).
//
// ---------------------------------------------------------------------------
// THIS TU's DOSSIER FUNCTIONS (5) -- all are large module constructors / resource
// handlers that the X360 build *inlined* the resource-descriptor template code
// into, which is why DecFIGS attributes them to this header path. Each function's
// owning home (its own ledger key / TU) is recorded here; none of them is an
// inline accessor of this header, and none is reconstructed in this file:
//
//   BrnGui::GuiModule::GuiModule                              @ 0x827E5B28
//       -> GuiModule subsystem ctor (BrnGui). Touches NO resource template.
//          Home: the BrnGui::GuiModule TU.
//   CgsResource::ResourceModule::ResourceModule              @ 0x827E1F68
//       -> Resource subsystem module ctor. Touches NO resource template.
//          Home: the CgsResource::ResourceModule TU.
//   WorldModule::WorldModule                                 @ 0x827E5998
//       -> World subsystem module ctor. Touches NO resource template.
//          Home: the WorldModule TU.
//   CgsResource::AptDataHeaderType::GetSerialisedResourceDescriptor @ 0x82502C30
//       -> Apt-data resource handler. Builds a <5> descriptor by raw stores:
//          slot0 = { m_size = *(header+0x10), m_alignment = 16 };
//          slots1..4 = { m_size = *(header+0x10), m_alignment = 1 }.
//          Home: the CgsResource::AptDataHeaderType handler TU
//          (b5-decomp/src/GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.cpp).
//   CgsResource::SnrResourceType::GetSerialisedResourceDescriptor  @ 0x826C21B0
//       -> SNR (streamed sound) resource handler. Builds a <5> descriptor:
//          slot0 = { m_size = 1, m_alignment = 8 }; slots1..4 = { 0, 1 };
//          then `+= ` a <5> descriptor of { m_size = *(header+4), m_alignment = 1 }
//          across all five entries -- the call @0x826C2238 to the committed
//          rw::BaseResourceDescriptors<5>::operator+= (inline in rwcore_structs.h).
//          Home: the CgsResource::SnrResourceType handler TU.
//
// All resource-descriptor behaviour those handlers exercise (the inline <5>
// fills + rw::BaseResourceDescriptors<5>::operator+=) is already present in the
// committed canonical home; this mirror header adds NO new function body.
// ===========================================================================
