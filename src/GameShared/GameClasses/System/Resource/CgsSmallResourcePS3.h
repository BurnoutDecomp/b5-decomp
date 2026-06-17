#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"                                        // rw::BaseResourceDescriptor(s)<N> (complete templates)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor (= rw::BaseResourceDescriptors<5>)
#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h" // EMemCategory + forward decls (umbrella)

// The console in-memory resource descriptor: a compact, 3-pool form of the serialised rw
// resource descriptor. This file is named after its DecFIGS *PS3* DWARF source (which
// defines SmallResourceDescriptor / SmallResource); the X360 ARTIST target has no DWARF
// (IDA only), but the X360 IDA confirms the 3-pool shape used here.
//
// SmallResourceDescriptor derives from rw::BaseResourceDescriptors<3> -- one (size,
// alignment) pair per in-memory pool (main / graphics-system / graphics-local) -- which
// is distinct from the SERIALISED CgsResource::ResourceDescriptor (<5>, the on-disk
// memory categories). It carries no data of its own; the base supplies the three
// descriptor entries. Convert*RWDescriptor map between the 3-pool runtime form and the
// 5-entry serialised form.
//
// Reconstructed from the DecFIGS PS3 DWARF (CgsSmallResourcePS3.h). Method bodies are
// reconstructed when the Pool/ScratchPool load path that calls them is brought up.

namespace CgsResource
{
    // CgsSmallResourcePS3.h:28 - the three in-memory pools a console resource spans.
    enum ESmallResourceMemType
    {
        E_MEMTYPE_MAINMEMORY      = 0,
        E_MEMTYPE_GRAPHICS_SYSTEM = 1,
        E_MEMTYPE_GRAPHICS_LOCAL  = 2,
        E_MEMTYPE_NUMTYPES        = 3,
    };

    // CgsSmallResourcePS3.h:80
    struct SmallResourceDescriptor : public rw::BaseResourceDescriptors<3>
    {
        // RECONCILE: the X360 spells these as nested typedefs of the serialised rw
        // descriptor (ResourceDescriptor::MemoryResourceDescriptor /
        // ::GraphicsSystemResourceDescriptor / ::GraphicsLocalResourceDescriptor). The
        // generated vendor rw header does not expose those nested names; each is the
        // descriptor element type rw::BaseResourceDescriptor (one size/align pair) and
        // is layout-identical, so they are aliased to it directly here.
        typedef rw::BaseResourceDescriptor SmallMemoryDescriptor;
        typedef rw::BaseResourceDescriptor SmallGraphicsSystemDescriptor;
        typedef rw::BaseResourceDescriptor SmallGraphicsLocalDescriptor;

        SmallResourceDescriptor() {}   // PC build: allow field-by-field fill (X360 fills raw)
        SmallResourceDescriptor(const SmallMemoryDescriptor& lrMemoryDescriptor);  // :84
        SmallResourceDescriptor(u32 luSize, u32 luAlignment);                      // :90
        SmallResourceDescriptor(const ResourceDescriptor& lrDescriptor);          // :96

        void CreateFromRWDescriptor(const ResourceDescriptor& lrDescriptor);      // :103
        void ConvertToRWDescriptor(ResourceDescriptor& lrDescriptor) const;       // :127

        const SmallMemoryDescriptor&         GetMemoryResourceDescriptor() const;          // :147
        SmallMemoryDescriptor&               GetMemoryResourceDescriptor();                // :153
        const SmallGraphicsSystemDescriptor& GetGraphicsSystemResourceDescriptor() const; // :160
        SmallGraphicsSystemDescriptor&       GetGraphicsSystemResourceDescriptor();        // :166
        const SmallGraphicsLocalDescriptor&  GetGraphicsLocalResourceDescriptor() const;  // :172
        SmallGraphicsLocalDescriptor&        GetGraphicsLocalResourceDescriptor();         // :178

        u32 GetSize() const;        // :184
        u32 GetAlignment() const;   // :190
    };

    // CgsSmallResourcePS3.h:216 - the in-memory resource itself: one base pointer per
    // in-memory pool (main / graphics-system / graphics-local), i.e. the 3-pool form of
    // rw::Resource. Carries no data of its own (the base supplies the three pointers).
    // This is what ResourceHandle::Resource resolves to (the Pool's mResource is one).
    struct SmallResource : public rw::BaseResources<3>
    {
        // RECONCILE: the PS3 DWARF spells these via BaseResources<6>::BaseResource (the
        // element type); the vendor rw template's element is a raw void*, used directly.
        typedef void* SmallMemoryResource;
        typedef void* SmallGraphicsSystemResource;
        typedef void* SmallGraphicsLocalResource;

        SmallResource() {}   // PC build: allow field-by-field fill (X360 fills raw)
        SmallResource(const SmallMemoryResource& lrMemoryResource);          // :220
        SmallResource(const rw::Resource& lrResource);                       // :226
        void CreateFromRWResource(const rw::Resource& lrResource);           // :233
        void ConvertToRWResource(rw::Resource& lrResource);                 // :246
        SmallMemoryResource         GetMemoryResource() const;             // :258
        SmallMemoryResource&        GetMemoryResource();                   // :264
        SmallGraphicsSystemResource GetGraphicsSystemResource() const;     // :270
        SmallGraphicsSystemResource& GetGraphicsSystemResource();          // :276
        SmallGraphicsLocalResource  GetGraphicsLocalResource() const;      // :282
        SmallGraphicsLocalResource& GetGraphicsLocalResource();            // :288
        const char* GetResourceTypeName(u32 luMemType);                   // :294
    };
}
