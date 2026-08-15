#ifndef GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULEIO_H
#define GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULEIO_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                  // CgsModule::IOBuffer base
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // BrnResource::GameDataIO::RequestInterface<N>

// ============================================================================
// GameSource/Effects/Particles/ParticleModuleIO.h
//
// BrnParticle::ParticleIO::PrepareOutputBuffer — the per-frame output payload the
// particle module fills during EffectsModule::Prepare so the resource system can
// service the effect's resource requests. DWARF home: ParticleModuleIO.h:80
// (DecFIGS dwarfdump): it derives from CgsModule::IOBuffer and holds exactly one
// resource-request interface:
//
//     struct PrepareOutputBuffer : public CgsModule::IOBuffer {
//         typedef RequestInterface<4096> EffectsModuleResourceQueue;     // :32
//         typedef EffectsModuleResourceQueue ResourceRequestInterface;   // :38
//         ResourceRequestInterface mResourceRequestInterface;            // :133
//     };
//
// RequestInterface<4096> is the committed BrnResource::GameDataIO::RequestInterface
// (BrnGameDataRequestQueue.h; explicitly instantiated as <4096> in that TU). It is
// the only RequestInterface<4096> in the tree and matches the DWARF "RequestInterface
// <4096>" typedef. Its base chain is RequestQueue<4096> -> ResourceRequestQueue<4096>
// -> VariableEventQueue<4096,16>, so sizeof(PrepareOutputBuffer) == sizeof(IOBuffer) +
// (4096 + 16). Modelling the complete type here lets CgsModule::IOBufferStack::
// CreateIOBuffer<PrepareOutputBuffer> (the helper this facade TU drives) compute the
// right sizeof / placement-new.
//
// MINIMAL SLICE: only the data layout + the typedefs the IOHelper instantiation needs
// are modelled. Construct (ParticleModuleIO.h:117) IS bodied here as of 2026-08-15 --
// CreateIOBuffer<T> now calls it at every instantiation site, so it can no longer be left
// declared-only (see the note on the member). Destruct/GetResourceRequestInterface
// (:121/:126/:129) are still their own ParticleModuleIO.cpp TU, and the sibling
// DispatchInputBuffer is likewise deferred. GROW this header additively when the
// ParticleModuleIO TU lands; do NOT fork these types.
// ============================================================================
namespace BrnParticle
{
namespace ParticleIO
{
    struct PrepareOutputBuffer : public CgsModule::IOBuffer
    {
        // ParticleModuleIO.h:32 / :38 — the request queue type (committed RequestInterface<4096>).
        typedef BrnResource::GameDataIO::RequestInterface<4096> EffectsModuleResourceQueue;
        typedef EffectsModuleResourceQueue                      ResourceRequestInterface;

        // ParticleModuleIO.h:117. X360-attested by the CreateIOBuffer<PrepareOutputBuffer>
        // instantiation @0x8228E4F0, which inlines it whole after `Alloc(this, 4116, name)`:
        //     *v8 = 1;                                       -- IOBuffer::Construct
        //     VariableEventQueue<4096,16>::Construct(v8 + 4);
        //     VariableEventQueue<4096,16>::Clear(v8 + 4);
        // +4 is mResourceRequestInterface, and Construct-then-Clear on its embedded queue IS
        // RequestInterface<N>::Construct (BrnGameDataRequestQueue.h) -- so this is spelled by
        // name rather than by offset.
        // FLAG PC: homed inline here instead of the (still unwritten) ParticleModuleIO.cpp,
        // because CreateIOBuffer<T> now REFERENCES T::Construct at every instantiation site.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mResourceRequestInterface.Construct();
        }

        // ParticleModuleIO.h:121. BODIED 2026-08-15 (IO-buffer zero-fill removal audit):
        // CgsIOBufferStack.h's DestroyIOBuffer<T> is the console's mirror now and calls
        // T::Destruct, so this could no longer be declaration-only. The console instantiation
        // DestroyIOBuffer<PrepareOutputBuffer> @0x8228E5D8 (which frees 0x1014 bytes) shows the
        // call resolving straight to `CgsModule::IOBuffer::Destruct` -- this buffer's Destruct
        // ICF-folded into the base. Base-only, no member teardown (the console does NOT tear the
        // embedded request ring down here).
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        // ParticleModuleIO.h:126 / :129 — accessors; own TU.
        ResourceRequestInterface*       GetResourceRequestInterface();
        const ResourceRequestInterface* GetResourceRequestInterface() const;

    private:
        // ParticleModuleIO.h:133 — the only data member (after the IOBuffer base).
        ResourceRequestInterface mResourceRequestInterface;
    };
}
}

#endif // GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULEIO_H
