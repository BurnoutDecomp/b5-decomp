#pragma once
#include "rw/rwcore_structs.h"  // CANONICAL HOME: rw::IResourceAllocator (+ _vtbl),
                                //                 rw::LinearResourceAllocator,
                                //                 rw::ResourceAllocatorRegistry,
                                //                 rw::SystemAllocatorGeneric / GeneralResourceAllocator.

// ===========================================================================
// rw/core/resource/resourceallocator.h -- EATech SDK-path mirror for the
// RenderWare core RESOURCE ALLOCATOR interface family (rw:: namespace).
//
// This is the DWARF/source path the X360 build compiled the allocator interface
// from (DecFIGS attributes the inlined rw::IResourceAllocator code to this
// header). The Burnout PC decomp homes that rw:: allocator vocabulary --
// layout-faithful to rwcore.pdb -- in the canonical vendor header
// rw/rwcore_structs.h. To avoid an ODR fork this SDK-path header does NOT
// redefine those types; it re-roots the canonical home so a `#include` of the
// EATech path resolves to the one true definition. This mirrors the sibling
// re-root resource.h exactly (same include spelling).
//
// COMMITTED & REUSED BY NAME (do not re-home / redefine):
//   rw::IResourceAllocator        : sizeof 8 (one vptr). Pure-virtual-ish resource
//                                   allocator interface, base EA::Allocator::ICoreAllocator
//                                   in the X360 DWARF; modelled minimally on PC with
//                                   DoAllocate + Free (rwcore_structs.h).         (rwcore_structs.h)
//   rw::IResourceAllocator_vtbl   : 9-slot vtable image (Alloc x2 / Free / AllocDebug x2
//                                   / DoAllocate / DoFree / DoFreeDisposable).    (rwcore_structs.h)
//   rw::LinearResourceAllocator   : sizeof 144 (bump allocator over the 4 pools). (rwcore_structs.h)
//   rw::ResourceAllocatorRegistry : process-wide default-allocator slot +
//                                   Get/SetDefaultAllocator.                      (rwcore_structs.h)
//   rw::SystemAllocatorGeneric / rw::core::GeneralResourceAllocator               (rwcore_structs.h)
//   rw::Resource / rw::ResourceDescriptor / rw::MemoryResource /
//   rw::DisposableResource                                                        (rwcore_structs.h)
//
// CROSS-BUILD DRIFT (already documented in rwcore_structs.h): the X360 build's
// inline allocator helpers carve/free a FIVE-entry serialised descriptor
// (rw::BaseResourceDescriptors<5>, 40B); the PC rwcore.lib narrowed
// rw::ResourceDescriptor to <4> (32B). The committed IResourceAllocator is
// therefore modelled with the minimal <4> DoAllocate/Free surface the engine
// dispatches through (a single vptr, sizeof 8 per rwcore.pdb); the bit-exact
// rwcore.lib chunk layout of the X360 <5> path is intentionally elided there.
//
// ---------------------------------------------------------------------------
// THIS TU's DOSSIER FUNCTIONS (56) -- NONE is an inline accessor newly homed in
// this file. They split into two buckets, both already covered elsewhere:
//
// (A) FOUR rw::IResourceAllocator inline methods that DecFIGS attributes to this
//     header. They are the X360 <5>-descriptor-build inlines that dispatch
//     through the allocator vtable; the committed canonical home models this
//     surface minimally (DoAllocate + Free), as documented above. They are NOT
//     re-bodied here -- doing so would fork/redefine the committed
//     rw::IResourceAllocator (an ODR violation):
//
//       rw::IResourceAllocator::Alloc                  @ 0x8266ABD8
//           -> builds a <5> ResourceDescriptor { size=arg, align=1 } on the stack
//              and tail-calls vtbl[+16] (DoAllocate). Inline of the carve path.
//       rw::IResourceAllocator::AllocateMemoryResource @ 0x823FF7D0  [EXECUTED in goal trace]
//           -> builds a <5> ResourceDescriptor from a (size,align) pair and
//              tail-calls vtbl[+16] (DoAllocate); returns the slot-0 MemoryResource.
//       rw::IResourceAllocator::DoFreeDisposable       @ 0x826645F8
//           -> the default DoFreeDisposable: for the disposable base-resource
//              types (indices 1 and 4 of 5) move the pointer into a fresh
//              Resource, null the source, then virtual-dispatch DoFree (vtbl+20).
//       rw::IResourceAllocator::Free                   @ 0x8227D048
//           -> zero-fill a Resource, store ptr in slot 0, virtual-dispatch
//              DoFree (vtbl+20). Used by rw::core::debug formatter teardown.
//
// (B) FIFTY-TWO functions owned by OTHER TUs that merely INLINE the allocator
//     template (CreateObject factories carve their object through the resource
//     allocator; module ctors / Release / DumpStats / operator new touch it).
//     DecFIGS attributes them here only because the allocator inline lands in
//     each owner. Each is bodied in its own ledger key, NOT here. A representative
//     spread (home = its own owning TU):
//
//       BrnCoronaManager::Release                                @ 0x823F7720
//       BrnResource::GameDataIO::AllocatorList::DebugDumpStats    @ 0x82669E40
//       BrnResource::GameDataModule::RegisterResourceTypes        @ 0x82667EA8
//       BrnSound::Logic::*::CreateObject  (x ~30, e.g.
//           Brn3DEffectControl @ 0x826C8578, BrnEffectObject @ 0x826AF570,
//           BrnState @ 0x826C8390, BrnStateManager @ 0x826FB5F0, ...)
//       BrnSound::Vehicles::*::CreateObject (x ~18, e.g.
//           Car3DControl @ 0x826E4CA8, Engine3dControl @ 0x826F3A28,
//           Engines::EngineControl @ 0x826CC438, Wheels::WheelControl @ 0x826E5548, ...)
//       CgsResource::BundleLoaderModule::Construct                @ 0x828EBAF8
//       CgsResource::DebugComponent::DumpStats                    @ 0x828EE038
//       CgsSceneManager::JobCoarseResultBuffer::Construct         @ 0x828BB128
//       CgsSound::Logic::Cgs3dEffectControl::CreateObject         @ 0x826C3DF0
//       CgsSound::MemBase::operator new                           @ 0x826AB5C8
//       CgsSound::Playback::Content::operator new                 @ 0x826C0788
//       renderengine::CoronaRenderer::Release                     @ 0x8227B490
//       rw::graphics::postfx::Vignette::Release                   @ 0x823F93D0
//
// All allocator behaviour these owners exercise (the inline descriptor build +
// the rw::IResourceAllocator dispatch surface) is already present in the
// committed canonical home; this mirror header adds NO new function body.
// ===========================================================================
