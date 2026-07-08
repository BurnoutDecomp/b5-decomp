// =====================================================================================
// rw::audio::core::DecoderRegistry -- member-function bodies.
//
// EARenderWare "rwaudio" decoder registry: an intrusive singly-linked list of DecoderDesc
// run-time-type records that streaming codecs register into and that the players look a
// codec up in by GUID. Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/offset (see rw/audio/core/DecoderRegistry.h). No Feb-2007
// leak source and no DecFIGS DWARF exist for this type. Direct sibling of PlugInRegistry.
//
//   CreateInstance                 @0x82B6C728
//   GetDecoderHandle               @0x82B67C80
//   RegisterDecoder                @0x82B67CB0
//   RegisterStandardRunTimeDecoders@0x82B6B538
// =====================================================================================

#include "rw/audio/core/DecoderRegistry.h"
#include "rw/audio/core/Xas1Dec.h"  // Xas1Dec::GetDecoderDesc
#include "rw/audio/core/XasDec.h"   // XasDec::GetDecoderDesc
#include "rw/audio/core/EaXmaDec.h" // EaXmaDec::GetDecoderDesc

namespace rw
{
namespace audio
{
namespace core
{

// A link slot points at DecoderDesc::mpNext (+0x10); recover the owning record.
static DecoderDesc *InfoFromLink(void *link)
{
    return reinterpret_cast<DecoderDesc *>(reinterpret_cast<char *>(link) - 0x10);
}

// -------------------------------------------------------------------------------------
// DecoderRegistry::CreateInstance @0x82B6C728
// Allocate a 16-aligned registry via System::New2<DecoderRegistry> and stash the owning
// System back-pointer. The asm passes size=0 / align=16 (li r6,0; li r7,0x10) to New2 --
// reproduced verbatim; the templated allocator resolves the real object size. Only
// mpSystem (+0x0C) is written on the success path; the list state is zero from New2.
// -------------------------------------------------------------------------------------
DecoderRegistry *DecoderRegistry::CreateInstance(System *system)
{
    DecoderRegistry *self = 0;
    System::New2<DecoderRegistry>(system, &self, 0, 0, 16, 0);
    if (self)
        self->mpSystem = system; // stw r31 -> 0xC
    return self;
}

// -------------------------------------------------------------------------------------
// DecoderRegistry::GetDecoderHandle @0x82B67C80
// Walk the link list looking for the record whose GUID matches `id`; return the owning
// DecoderDesc (as the opaque handle) or null.
// -------------------------------------------------------------------------------------
DecoderDesc *DecoderRegistry::GetDecoderHandle(DecoderRegistry *self, int id)
{
    void *link = self->mpHead;
    if (!link)
        return 0;
    for (;;)
    {
        DecoderDescLink *node = static_cast<DecoderDescLink *>(link);
        DecoderDesc *owner = InfoFromLink(link); // result = link - 0x10
        u32 nodeId = node->muId;                 // v4 = v2[1]
        link = node->mpNext;                     // v2 = *v2
        if (nodeId == static_cast<u32>(id))
            return owner;
        if (!link)
            return 0;
    }
}

// -------------------------------------------------------------------------------------
// DecoderRegistry::RegisterDecoder @0x82B67CB0
// Register `info` if its GUID is not already present: link it at the head, set the tail on
// the first insert, and bump the count. Returns the existing record on a duplicate GUID,
// else the newly-registered `info`. (Unlike PlugInRegistry there is no sequence byte.)
// -------------------------------------------------------------------------------------
DecoderDesc *DecoderRegistry::RegisterDecoder(DecoderRegistry *self, DecoderDesc *info)
{
    void *head = self->mpHead;
    void *link = self->mpHead;
    if (link)
    {
        for (;;)
        {
            DecoderDescLink *node = static_cast<DecoderDescLink *>(link);
            DecoderDesc *owner = InfoFromLink(link);
            u32 existingId = node->muId; // v6 = v4[1]
            link = node->mpNext;         // v4 = *v4
            if (existingId == info->muId) // v6 == *(a2+20)
                return owner;             // duplicate -> existing record
            if (!link)
                break;                    // not found -> insert
        }
    }

    // Insert at the head. The node handle is &info->mpNext (DecoderDesc +0x10).
    void **newLink = &info->mpNext;   // r10 = a2 + 0x10
    info->mpNext = head;              // *(a2+16) = old head link

    if (!self->mppTail)               // if (!a1[1])
        self->mppTail = newLink;      //   a1[1] = newLink

    u32 count = self->muCount;        // v8 = a1[2]
    self->mpHead = newLink;           // *a1 = newLink
    self->muCount = count + 1;        // a1[2] = v8 + 1
    return info;
}

// -------------------------------------------------------------------------------------
// DecoderRegistry::RegisterStandardRunTimeDecoders @0x82B6B538
// Register the three run-time (software) decoders in order: XAS1, XAS0, then XMA. Each
// codec's GetDecoderDesc hands back its static descriptor (the pseudocode's
// `XasDec::GetDecoderDesc(v3)` etc. are Hex-Rays artifacts -- the accessors take no args;
// r3 merely still holds the prior RegisterDecoder return). Returns the last RegisterDecoder
// result (the XMA descriptor), matching r3 at return.
// -------------------------------------------------------------------------------------
DecoderDesc *DecoderRegistry::RegisterStandardRunTimeDecoders(DecoderRegistry *self)
{
    RegisterDecoder(self, Xas1Dec::GetDecoderDesc());
    RegisterDecoder(self, XasDec::GetDecoderDesc());
    return RegisterDecoder(self, EaXmaDec::GetDecoderDesc());
}

} // namespace core
} // namespace audio
} // namespace rw
