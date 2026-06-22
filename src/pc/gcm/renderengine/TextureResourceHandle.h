#pragma once

#include "types.hpp"

// renderengine::Texture-specialised resource-handle conversion. The X360 image attests
//
//   CgsResourceHandle<renderengine::Texture>::operator renderengine::Texture*  0x82286130
//
// the IDA-demangled name truncates to "renderengine::Texture>::operator class renderengine::
// Texture *". It dereferences the handle's stored resource memory twice (the handle points at a
// CgsResource::ResourceHandle whose mpResourceMemory is the Texture object) and asserts the
// resource pointer is non-null, with the message + site from CgsResourceHandle.h:490.
//
// The handle is reconstructed here minimally: the single stored pointer the operator reads and
// the conversion itself. The particle renderers (the call sites) use it to fetch the bound
// texture from a resource handle.

namespace renderengine
{
    class Texture;

    // A handle onto a renderengine::Texture resource. mpHandle points at the underlying resource
    // handle whose first word (mpResourceMemory) is the Texture object; the conversion returns it.
    class TextureResourceHandle
    {
    public:
        // Fetch the bound Texture*. Asserts the resource has main-memory backing (X360 site
        // CgsResourceHandle.h:490).
        operator Texture*() const;

        Texture** mpHandle;   // +0x00  -> CgsResource::ResourceHandle (its +0 == the Texture*)
    };
}
