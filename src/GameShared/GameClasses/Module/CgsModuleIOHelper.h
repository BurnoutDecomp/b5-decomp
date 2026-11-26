#pragma once
#define CGS_ARTIST_CGSMODULEIOHELPER_FILE "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleIOHelper.h"
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"   // CgsModule::IOBufferStack::CreateIOBuffer<T>
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // ARTIST assert locations (h:52 / h:57)

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
            // ARTIST bakes CgsModuleIOHelper.h:52 into every inlined copy (81 sites, e.g.
            // BrnGameModule::DoUpdate_Effects 0x823DD118 `li r5, 0x34`).
            CGS_ASSERT_AT(mpStack->CreateIOBuffer(&mpBuffer, lpcName),
                "mpStack->CreateIOBuffer( &mpBuffer, lpcName )",
                    CGS_ARTIST_CGSMODULEIOHELPER_FILE, 52);
        }

        // CgsModuleIOHelper.h:55. Pops the buffer back off the stack. The X360 dtor
        // (attested via IOHelper<RouteMapModuleIO::OutputBuffer>::~IOHelper @0x82791820)
        // asserts the DestroyIOBuffer result (CgsModuleIOHelper.h:57).
        ~IOHelper()
        {
            // CgsModuleIOHelper.h:57 (60 sites, e.g. DoUpdate_Effects 0x823DD284 `li r5, 0x39`).
            CGS_ASSERT_AT(mpStack->DestroyIOBuffer(&mpBuffer),
                "mpStack->DestroyIOBuffer( &mpBuffer )",
                    CGS_ARTIST_CGSMODULEIOHELPER_FILE, 57);
        }

        // CgsModuleIOHelper.h:60 / :65 — access the borrowed buffer.
        operator BufferType*() const { return mpBuffer; }
        BufferType* operator->()     { return mpBuffer; }

    private:
        IOBufferStack* mpStack;    // +0x00  CgsModuleIOHelper.h:71
        BufferType*    mpBuffer;   // +0x04  CgsModuleIOHelper.h:72
    };
}

#undef CGS_ARTIST_CGSMODULEIOHELPER_FILE
