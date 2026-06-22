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
// are modelled. The Construct/Destruct/GetResourceRequestInterface members
// (ParticleModuleIO.h:117/121/126/129) are their own ParticleModuleIO.cpp TU and are
// left declared-only — they are not bodied by this BrnParticle facade TU. The sibling
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

        // ParticleModuleIO.h:117 / :121 — placement-new'd by CreateIOBuffer<>; own TU.
        void Construct();
        void Destruct();

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
