// =====================================================================================
// rw::audio::core::PlugInRegistry -- member-function bodies.
//
// EARenderWare "rwaudio" plug-in registry: an intrusive singly-linked list of
// PlugInDescRunTime run-time-type records. Reconstructed from BURNOUT_X360_ARTIST.XEX; the
// PowerPC asm is authoritative for every store/offset (see rw/audio/core/PlugIn.h).
// No LegacySrc reference source and no DecFIGS DWARF exist for this type.
// =====================================================================================

#include "rw/audio/core/PlugIn.h"

#include <cstddef>   // offsetof (host record layout -- see InfoFromLink)

namespace rw
{
namespace audio
{
namespace core
{

PlugInDescRunTime *PlugInRegistry::GetPlugInDescRunTimeFromNode(ListNode *node) const
{
    return reinterpret_cast<PlugInDescRunTime *>(
        reinterpret_cast<char *>(node) - offsetof(PlugInDescRunTime, listNode));
}

// -------------------------------------------------------------------------------------
// PlugInRegistry::CreateInstance @0x82B6DBC0
// Allocates a 16-aligned registry via System::New2<PlugInRegistry> and zero-initialises
// its list state.
//
// ALLOCATION SIZE (X360-literal trap): the asm passes the size EXPLICITLY --
// `li r6, 0x18` @0x82B6DBD8 -- and 0x18 (24) IS the console sizeof(PlugInRegistry)
// (three 4-byte pointers + u32 + int + char). On the host those three pointers widen to
// 8 bytes and sizeof(PlugInRegistry) is 40, so the console immediate under-allocates by
// 16 bytes: the mpSystem store below (host +0x18..+0x1F) and mCurrentRegistryIndex (host
// +0x20, the byte RegisterPlugInRunTime post-increments) both land past the block.
// New2 falls back to sizeof(T) only when the size argument is 0 (PlugIn.h:490), so the
// non-zero console 24 must be replaced by the host expression rather than by 0 -- that
// also preserves the asm's explicit-size call shape.
// -------------------------------------------------------------------------------------
#if defined(_XBOX) && defined(_MSC_VER)
// Use the size form emitted by the original Xbox 360 rwAudio common object.
#pragma optimize("s", on)
#endif
PlugInRegistry *PlugInRegistry::CreateInstance(System *system)
{
    PlugInRegistry *instance;
    // X360: li r6,0x18 -- console sizeof(PlugInRegistry) == 24; host sizeof == 40.
    system->New2<PlugInRegistry>(&instance, 0, static_cast<unsigned>(sizeof(PlugInRegistry)), 16, 0);
    PlugInRegistry *const self = instance;
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
PlugInRegistry::PlugInHandle PlugInRegistry::GetPlugInHandle(u32 guid) const
{
    ListNode *node = mPlugInDescRunTimeList.GetHead();
    while (node)
    {
        PlugInDescRunTime *info = GetPlugInDescRunTimeFromNode(node);
        node = node->GetNext();
        if (info->muId == guid)
            return info;
    }
    return 0;
}

// PlugInRegistry::RegisterPlugInRunTime @0x82B6A938
PlugInRegistry::PlugInHandle PlugInRegistry::RegisterPlugInRunTime(PlugInDescRunTime *info)
{
    ListNode *node = mPlugInDescRunTimeList.GetHead();
    while (node)
    {
        PlugInDescRunTime *existing = GetPlugInDescRunTimeFromNode(node);
        node = node->GetNext();
        if (existing->muId == info->muId)
            return existing;
    }

    mPlugInDescRunTimeList.Push(reinterpret_cast<ListNode *>(&info->listNode));
    info->mbSeq = mCurrentRegistryIndex++;
    return info;
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", off)
#endif

} // namespace core
} // namespace audio
} // namespace rw
