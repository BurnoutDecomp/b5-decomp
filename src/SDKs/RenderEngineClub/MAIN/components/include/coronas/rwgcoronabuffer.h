#pragma once

// =================================================================================================
// coronas/rwgcoronabuffer.h -- THE DUPLICATE-TYPE MERGE IS DONE (coronas step 1, 2026-08-17).
//
// This header used to declare THREE things that already had homes, or needed one:
//
//   1. `renderengine::Corona` and the full `renderengine::CoronaBuffer` -- a SECOND declaration of
//      types GameSource/Graphics/BrnCoronaManager.h also declared (as a cut-down slice). Both now
//      live ONCE, in BrnCoronaManager.h, at the DWARF shape. They were moved DOWN into that header
//      rather than that header being made to include this one, because BrnCoronaManager.h has a
//      53-TU includer closure and a new transitive include would have cascaded through all of them;
//      the two types need only Vector2/Vector3 + rw::RGBA, which that header already had.
//
//   2. `class BrnCoronaManager` -- a THIRD declaration of the manager, spelled in GUEST OFFSETS
//      (4-byte pointers, 20-byte rw::Resource) with `u32` members standing in for host pointers.
//      DELETED. Its one body, Construct @0x823FCD90, is now bodied in
//      GameSource/Graphics/BrnCoronaManager.cpp against the REAL members.
//
//   3. `namespace rw { namespace math { struct Vector2 / Vector3 }; }` -- two 8/12-byte structs
//      standing in for the real 16-byte alignas(16) rw::math::vpu types. DELETED; they were a
//      guest-layout fork of a type the vendor header owns
//      (vendor/renderware/include/rw/math/vpu/types.h:23), and the ONE consumer (the Iterator::Write
//      convenience overload) now takes the real types.
//
// ...plus a file-local anonymous-namespace `ResourceAllocator` shim and a set of file-scope corona
// globals (`gpCoronaBlendStateAdditive` / `guCoronaRenderStateWord` / `gpCoronaAtlasUVTable` ...)
// that were an INVENTED host home for state the console keeps in renderengine::CoronaRenderer's own
// private statics -- the DWARF names all of them (s_blendState / s_numSubImages / s_atlasUVs ...).
// They now live exactly once, in coronas/rwgcoronarenderer.h, and are written through the three
// DWARF setters (SetBlendState / SetDepthStencilStates / SetTextureAtlas) that
// BrnCoronaManager::Construct and ::SetTextureAtlas inline on the console.
//
// The file is kept (rather than deleted) as a one-line shim so that the mirrored console path stays
// visible in the tree and so its own .cpp keeps a header to include.
// =================================================================================================

#include "GameSource/Graphics/BrnCoronaManager.h"   // renderengine::Corona, renderengine::CoronaBuffer
