#ifndef BRN_SOUND_LOGIC_BRN_RESOURCE_REGISTRAR_H
#define BRN_SOUND_LOGIC_BRN_RESOURCE_REGISTRAR_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::ResourceRegistrar + IResourceRequester
//   GameSource/Sound/BrnResourceRegistrar.{h,cpp}  (DWARF home, BrnResourceRegistrar.h:42/337)
//
// The sound-logic streaming-resource broker. Effects / controls / state managers implement
// IResourceRequester; on attach they enqueue resource requests, on detach they call
// registrar.RemoveRequests(this). Each frame Update() drains the request queues, resolves
// bundle+resource name hashes into CgsResource::ResourceHandles via the AttribSys request
// interfaces, promotes queued->requested, and GCs unreferenced files. Consumers fetch resolved
// handles through IResourceRequester::GetAsset -> GetResource.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct 0x826B0470, Update 0x82702228, UpdateRequests 0x826B0560, UpdateQueued 0x82702108,
//   ClearUnreferancedFiles 0x826E2080, GetResource 0x826B0B08, SearchQueued 0x826B0A60,
//   RemoveRequests 0x826E2220, AddNodeToRemoveResourceCandidateList 0x82695860.
//
// This is the CANONICAL HOME: it folds the duplicate minimal IResourceRequester / ResourceRegistrar
// definitions out of BrnEffectObject.h, BrnEffectControl.h and BrnStateManager.h (which now include
// this), resolving the cross-header ODR. MINIMAL-THEN-GROW: the real method surface is declared
// here; the ~54KB of members (VariableEventQueue<4096,16> mResourceRequestInterface +
// VariableEventQueue<2048,16> mAttribSysRequestInterface @+0xCAA0/+0xD2B0, the requested/queued
// LinkedListHelper pools, the LinearHashTable removal-candidate map @+0x2F80) and the inner
// RequestedResource/QueuedResource classes + the queue-driven bodies are grown on top.
// LAYOUT NOTE: X360 byte offsets assume 4-byte ptr/vptr; members are pinned by name+sequence, not
// static_asserted on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
    class ResourceRegistrar;

    // BrnResourceRegistrar.h:337 (DWARF). The resource-requester interface BrnEffectObject and the
    // controls/state-managers implement (X360: the IResourceRequester sub-object vptr @ this+4 on
    // BrnEffectObject; the detach/registrar paths route through it).
    struct IResourceRequester
    {
        virtual ~IResourceRequester() {}
        virtual void               ResourcesAreReady() = 0;
        virtual ResourceRegistrar& GetResourceRegistrar() = 0;
    };

    // BrnResourceRegistrar.h:42 (DWARF). The streaming-resource broker.
    class ResourceRegistrar
    {
    public:
        // 0x826B0470 -- bring-up: Construct+Clear the two VariableEventQueue request interfaces and
        // InternalInit the requested/queued list pools. [grow-in: the queues/pools land with Update.]
        void Construct();

        // 0x82702228 -- per-frame: clear the request queues, resolve requested resources to handles,
        // promote queued->requested, GC unreferenced files. [grow-in: needs the queues/pools/inner
        // classes; currently a no-op so the module Update spine can call it safely.]
        void Update();

        // 0x826E2220 -- the BrnEffectObject::Detach entry point: pull a requester's outstanding
        // requests out of the broker. [grow-in: walks the requested list removing lpRequester's nodes.]
        void RemoveRequests(IResourceRequester* lpRequester);

    private:
        // [grow-in] the real ~54KB layout (the two VariableEventQueue request interfaces + the
        // requested/queued LinkedListHelper pools + the LinearHashTable removal-candidate map) lands
        // with the queue-driven Update/GetResource bodies. Placeholder keeps the type complete +
        // embeddable (SoundLogicModule holds one by value) until then.
        s32 miReserved;
    };
}
}

#endif // BRN_SOUND_LOGIC_BRN_RESOURCE_REGISTRAR_H
