#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (CgsIOBufferStack.h:139)
#include <new>
#include <cstring>                                   // memset (BRN_IOBUF_ZERO diagnostic only)

// CgsModule::IOBufferStack - a linear stack allocator that hands out typed IO buffers (the
// per-frame input/output payloads modules exchange). CreateIOBuffer<T> pushes a T and
// DestroyIOBuffer<T> pops it; both are member templates instantiated per buffer type at the
// call sites (e.g. BrnGameModule tears its GUI/director buffers down at end of frame).
// Layout + API recovered from the DecFIGS DWARF (Module/CgsIOBufferStack.h); the allocator
// bodies + per-type template instantiations are their own TU.
namespace CgsModule
{
    // FLAG PC (host-policy diagnostic knob, not console behaviour): returns true when the
    // environment variable BRN_IOBUF_ZERO=1 is set, in which case CreateIOBuffer<T> zero-fills
    // the buffer before constructing it -- the behaviour the PC template used to have
    // unconditionally (`new (lpMem) T()` value-initialises, memset-ing ~18-20 MB EVERY frame:
    // 10.8% of the main thread in the 2026-08-15 driving profile). The console does NOT zero
    // (every one of the 134 X360 CreateIOBuffer<T> instantiations is `Alloc(...)` followed by
    // the buffer's own Construct, never a memset), so this is OFF by default and exists only so
    // a future "it broke when the zeroing went away" regression can be A/B'd in one boot.
    // Read ONCE into a function-local static -- never per create.
    bool IOBufferHostZeroFillEnabled();

    struct IOBufferStack
    {
        void Construct(const char* lpcDebugName);
        bool Prepare(void* lpMemory, u32 luSize, u32 luAlignment);
        bool Release();
        void Destruct();
        void Clear();

        // Push a T onto the stack (LIFO); DestroyIOBuffer pops it. Defined inline so each
        // buffer type instantiates at the call site (the per-type bodies are these templates).
        //
        // CgsIOBufferStack.h:139-160. Shape taken from the 134 X360 instantiations, every one of
        // which is: assert(lpOutBuffer) -> Alloc(sizeof(T), name) -> if the alloc succeeded, run
        // the buffer's OWN Construct, statically bound to T (e.g.
        // BrnResource::GameDataIO::InputBuffer::Construct @0x823AC540 for the 65,576-byte GameData
        // input buffer; BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::Construct
        // @0x827B65B8). Where T::Construct is small the X360 compiler inlined it and the first
        // thing it does is the base IOBuffer status store -- e.g. @0x8228E4F0
        // (ParticleIO::PrepareOutputBuffer) is `*p = 1; VariableEventQueue<4096,16>::Construct(p+4);
        // Clear(p+4)`, where `*p = 1` IS IOBuffer::Construct (Clear + SetBit(eStatusConstructed)).
        // Three types (CgsPhysics::IslandGenerator @0x8289E0D0,
        // CgsSceneManager::ContactGenerator::QueryAccumulator @0x828AE7F0,
        // CgsSound::Playback::Module::Io::InputBuffer @0x826C5B68) have NO call at all in their
        // instantiation -- the instantiation is a bare Alloc. The binary cannot distinguish an
        // EMPTY Construct from NO Construct call, so "empty Construct folded away" is an
        // inference, not an observation. What IS observable: for these three the console writes
        // NOTHING into the buffer, whereas the PC (which reaches CgsModule::IOBuffer::Construct
        // through inheritance) emits IOBuffer::Construct's status byte -- one store the console
        // never makes. Documented on the types themselves:
        //   CgsPhysics::IslandGenerator                       -- CgsIslandGenerator.h (base-only,
        //                                                        no own Construct; PC-only status store)
        //   CgsSceneManager::ContactGenerator::QueryAccumulator -- CgsContactGenerationIO.h
        //                                                        (base-only, same PC-only store)
        //   CgsSound::Playback::Module::Io::InputBuffer       -- NOT reconstructed in this tree
        //                                                        yet, so there is no PC Construct
        //                                                        to annotate; the note lives here.
        //
        // ---- POLICY: a T whose Construct is the inherited base-only one -------------------
        // Such a T is FAITHFUL AS-IS when its payload has no PC readers -- the extra status
        // store is the only divergence and nothing reads the payload. A T whose modelled members
        // the console Construct would clear MUST clear them itself; where its console Construct
        // body has not been recovered, a memset stands in and is marked with a FLAG until it is.
        // Do NOT blanket-memset base-only types: with the zero-fill removed (2026-08-15) a
        // gratuitous memset is exactly the ~18-20 MB/frame the removal was for.
        //
        // NOTE the `T` with NO parentheses: DEFAULT-initialisation, matching the console
        // (`*lpOutBuffer = new (Alloc(sizeof(T), lpcDebugName)) T;`, Feb-2007
        // GameShared/GameClasses/Module/CgsIOBufferStack.h:146). `T()` would VALUE-initialise,
        // which for these POD-ish buffers (no user-provided default ctor) zero-fills the whole
        // object -- that is what put ~18-20 MB/frame of memset into WorldModule::Update and
        // PhysicsModule::Update. No X360 instantiation zeroes the buffer.
        template <typename T>
        bool CreateIOBuffer(T** lpOutBuffer, const char* lpcDebugName)
        {
            CGS_ASSERT(lpOutBuffer, "Must pass in pointer to pointer to buffer\n");   // :139

            void* lpMem = Alloc(sizeof(T), lpcDebugName);
            if (lpMem == 0)
            {
                *lpOutBuffer = 0;
                return false;
            }

            // FLAG PC: opt-in host diagnostic only (BRN_IOBUF_ZERO=1) -- see the note on
            // IOBufferHostZeroFillEnabled above. The console never does this.
            if (IOBufferHostZeroFillEnabled())
                memset(lpMem, 0, sizeof(T));

            *lpOutBuffer = new (lpMem) T;
            (*lpOutBuffer)->Construct();
            return true;
        }

        // Pop the T this stack handed out. CgsIOBufferStack.h:176-186, verified verbatim on
        // three X360 instantiations that agree instruction-for-instruction:
        //   0x8228E5D8 DestroyIOBuffer<BrnParticle::ParticleIO::PrepareOutputBuffer> (Free 0x1014)
        //   0x823AEB98 DestroyIOBuffer<BrnWorldIO::DispatchOutputBuffer>             (Free 0xF0)
        //   0x823AD1D8 DestroyIOBuffer<BrnWorldIO::UpdateOutputBuffer>               (Free 0x35250)
        // Every one is: assert(lpInOutBuffer && *lpInOutBuffer) with the SAME message the create
        // side uses but the :180 line -> T::Destruct(*lpInOutBuffer) statically bound to T (the
        // ICF representative is whichever body it folds with -- e.g. the DispatchOutputBuffer
        // instantiation calls PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0, which is
        // a bare `b CgsModule::IOBuffer::Destruct`) -> Free(*lpInOutBuffer, sizeof(T)) -> null the
        // caller's pointer -> return the Free result. Note the console does NOT null-guard around
        // the work: it asserts and then dereferences anyway, and it returns Free's result (not an
        // unconditional true), which is what CgsModuleIOHelper.h:57 asserts on.
        template <typename T>
        bool DestroyIOBuffer(T** lpInOutBuffer)
        {
            CGS_ASSERT(lpInOutBuffer && *lpInOutBuffer, "Must pass in pointer to pointer to buffer\n");   // :180

            (*lpInOutBuffer)->Destruct();
            const bool lbFreed = Free(*lpInOutBuffer, sizeof(T));
            *lpInOutBuffer = 0;
            return lbFreed;
        }

    private:
        u8* mpData;
        u32 muSize;
        u32 muAlignment;
        u32 muAllocated;
        u32 muMaxAllocated;
        u32 muNumAllocated;

        void* Alloc(u32 luSize, const char* lpcDebugName);
        bool  Free(void* lpMemory, u32 luSize);
    };
}
