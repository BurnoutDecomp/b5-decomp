// =====================================================================================
// rw::audio::core::PlugInRegistry -- member-function bodies.
//
// EARenderWare "rwaudio" plug-in registry: an intrusive singly-linked list of
// PlugInDescRunTime run-time-type records. Reconstructed from BURNOUT_X360_ARTIST.XEX; the
// PowerPC asm is authoritative for every store/offset (see rw/audio/core/PlugIn.h).
// No Feb-2007 leak source and no DecFIGS DWARF exist for this type.
// =====================================================================================

#include "rw/audio/core/PlugIn.h"

namespace rw
{
namespace audio
{
namespace core
{

// The intrusive list threads PlugInDescRunTime records by their +0x24 link. The registry's
// node handle is the address of a link slot; the owning PlugInDescRunTime is (link - 0x24).
// A small "link view" keeps the walk member-by-name: a link slot is {next, id}
// laid over PlugInDescRunTime at +0x24 (next) / +0x28 (id).
struct PlugInDescRunTimeLink
{
    void *mpNext; // +0x00 == PlugInDescRunTime +0x24
    u32 muId;     // +0x04 == PlugInDescRunTime +0x28
};

static PlugInDescRunTime *InfoFromLink(void *link)
{
    // link points at PlugInDescRunTime::mpNext (+0x24); recover the owning record.
    return reinterpret_cast<PlugInDescRunTime *>(reinterpret_cast<char *>(link) - 0x24);
}

// -------------------------------------------------------------------------------------
// PlugInRegistry::CreateInstance @0x82B6DBC0
// Allocates a 24-byte, 16-aligned registry via System::New2<PlugInRegistry> and
// zero-initialises its list state.
// -------------------------------------------------------------------------------------
PlugInRegistry *PlugInRegistry::CreateInstance(System *system)
{
    PlugInRegistry *self = 0;
    System::New2<PlugInRegistry>(system, &self, 0, 24, 16, 0);
    if (self)
    {
        self->mpSystem = system; // stw r31 -> 0x10
        self->mpEnumerator = 0;      // stw 0 -> 0x0C
        self->mCurrentRegistryIndex = 0;     // stb 0 -> 0x14
    }
    return self;
}

// -------------------------------------------------------------------------------------
// PlugInRegistry::GetPlugInHandle @0x82B6A908
// Walks the link list looking for the record whose id matches; returns the owning
// PlugInDescRunTime (as the opaque handle) or null.
// -------------------------------------------------------------------------------------
void *PlugInRegistry::GetPlugInHandle(PlugInRegistry *self, int id)
{
    void *link = self->mpHead;
    if (!link)
        return 0;
    for (;;)
    {
        PlugInDescRunTimeLink *node = static_cast<PlugInDescRunTimeLink *>(link);
        PlugInDescRunTime *owner = InfoFromLink(link); // result = v2 - 9 (words) = link - 0x24
        u32 nodeId = node->muId;                // v4[1]
        link = node->mpNext;                    // v2 = *v2
        if (nodeId == static_cast<u32>(id))
            return owner;
        if (!link)
            return 0;
    }
}

// -------------------------------------------------------------------------------------
// PlugInRegistry::RegisterPlugInRunTime @0x82B6A938
// Registers `info` if its id is not already present: links it at the head, sets the
// tail on first insert, bumps the count, and snapshots the running sequence byte into
// the record. Returns the existing record on a duplicate id, else the new record.
// -------------------------------------------------------------------------------------
void *PlugInRegistry::RegisterPlugInRunTime(PlugInRegistry *self, PlugInDescRunTime *info)
{
    void *head = self->mpHead;
    void *link = self->mpHead;
    if (link)
    {
        for (;;)
        {
            PlugInDescRunTimeLink *node = static_cast<PlugInDescRunTimeLink *>(link);
            PlugInDescRunTime *owner = InfoFromLink(link);
            u32 existingId = node->muId;       // v4[1]
            link = node->mpNext;               // v4 = *v4
            if (existingId == info->muId)      // *(a2+40) == info->muId
                return owner;                  // duplicate -> existing record
            if (!link)
                break;                         // not found -> insert
        }
    }

    // Insert at the head. The node handle is &info->mpNext (PlugInDescRunTime +0x24).
    void **newLink = &info->mpNext;            // r10 = a2 + 0x24
    info->mpNext = head;                       // *(a2+36) = old head link

    if (!self->mppTail)                        // if (!*(a1+4))
        self->mppTail = newLink;               //   *(a1+4) = newLink

    char seq = self->mCurrentRegistryIndex;                // lbz 0x14
    self->muCount = self->muCount + 1;         // *(a1+8) += 1
    self->mpHead = newLink;                    // *a1 = newLink
    info->mbSeq = seq;                         // *(a2+0x32) = seq
    ++self->mCurrentRegistryIndex;                         // ++*(a1+0x14)
    return info;
}

} // namespace core
} // namespace audio
} // namespace rw
