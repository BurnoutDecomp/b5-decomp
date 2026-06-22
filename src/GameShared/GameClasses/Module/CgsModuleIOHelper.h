#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"   // CgsModule::IOBufferStack::CreateIOBuffer<T>
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

// ============================================================================
// GameShared/GameClasses/Module/CgsModuleIOHelper.h
//
// CgsModule::IOHelper<BufferType> — a tiny RAII wrapper used by the module
// Prepare/Dispatch paths to grab one typed IO buffer off an IOBufferStack for
// the duration of a scope. DWARF home: CgsModuleIOHelper.h:43 (DecFIGS dwarfdump
// lists the IOHelper<...> instantiations: PrepareOutputBuffer, DispatchInputBuffer,
// RendererIO::Input/OutputBuffer, CgsInput::InputIO::OutputBuffer, ...). The two
// stored pointers are at +0x00 (mpStack) and +0x04 (mpBuffer); there are no other
// members.
//
// The ctor (CgsModuleIOHelper.h:49) stores the stack pointer then asks it to push a
// BufferType, asserting on failure (baked file "..\\..\\..\\GameShared\\GameClasses\\
// Module/CgsModuleIOHelper.h", line 52, cond "mpStack->CreateIOBuffer( &mpBuffer,
// lpcName )"). The dtor (line 55) pops it again (mpStack->DestroyIOBuffer(&mpBuffer));
// the X360 build emits that pop out-of-line at every use site, so this header keeps
// the bodies inline and each instantiation TU forces only the members it owns. This
// BrnParticle facade TU owns the IOHelper<BrnParticle::ParticleIO::PrepareOutputBuffer>
// CONSTRUCTOR (X360 0x82295FD0, BrnEffects::EffectsModule::Prepare's "Particles"
// helper); the matching DestroyIOBuffer pops are forced by their own caller TUs.
// ============================================================================
namespace CgsModule
{
    template <class BufferType>
    struct IOHelper
    {
        // CgsModuleIOHelper.h:49. X360 0x82295FD0: stores mpStack = lpStack, then
        // CGS_ASSERT( mpStack->CreateIOBuffer( &mpBuffer, lpcName ) ).
        IOHelper(IOBufferStack* lpStack, const char* lpcName)
        {
            mpStack = lpStack;
            CGS_ASSERT(mpStack->CreateIOBuffer(&mpBuffer, lpcName),
                "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");
        }

        // CgsModuleIOHelper.h:55. Pops the buffer back off the stack.
        ~IOHelper()
        {
            mpStack->DestroyIOBuffer(&mpBuffer);
        }

        // CgsModuleIOHelper.h:60 / :65 — access the borrowed buffer.
        operator BufferType*() const { return mpBuffer; }
        BufferType* operator->()     { return mpBuffer; }

    private:
        IOBufferStack* mpStack;    // +0x00  CgsModuleIOHelper.h:71
        BufferType*    mpBuffer;   // +0x04  CgsModuleIOHelper.h:72
    };
}
