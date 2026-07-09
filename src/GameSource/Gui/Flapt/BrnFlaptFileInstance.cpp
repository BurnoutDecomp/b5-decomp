#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"

#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"   // BrnFlapt::MovieClipInstance (Update/Render forward targets)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // gpDebugPrint / gxMessageFilterFlags (SetData usage print)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"    // CgsMemory::LinearMalloc (root clip allocation)

// BrnFlapt::FlaptFileInstance member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   GetRootMovieClip @ 0x8246B360
//   SetData          @ 0x82471620
//   Update           @ 0x82471820
//   Render           @ 0x82472480
//   Construct / Prepare / Destruct (inlined into FlaptManager::Construct
//     @0x82472530 / Prepare @0x8246D548 / Destruct @0x8246E298)
//
// GetRootMovieClip guards three conditions then constructs a MovieClipRef in the
// caller-provided out buffer:
//   1. IsActive()                 (lbz 0(this))          -- BrnFlaptFileInstance.h:135
//   2. mpRootMovieClipInstance     (lwz 0x24(this))       -- BrnFlaptFileInstance.h:136
//   3. lpMovieClipInst (the same   (re-read 0x24(this))   -- BrnFlaptMovieClipRef.h:217
//      pointer, asserted again by the inlined MovieClipRef constructor)
// then writes the MovieClipRef: *out = mpRootMovieClipInstance; out[1] = 0.
// The X360-baked file/line cites are discarded per project convention.

namespace BrnFlapt
{

namespace
{
    // The file-local invalid-file handle mpFile is (re)bound to (the X360 .data
    // static @dword_82FB3C6C, referenced by the inlined Construct/Destruct in
    // FlaptManager @0x82472530/@0x8246E298). BaseResourcePtr::CreateFromHandle
    // dereferences the handle's owner slot unconditionally to copy its identity,
    // so the handle references a static zero-identity sentinel node: binding to it
    // leaves mpFile with no resource memory and no alias-ring link -- exactly the
    // "invalid handle" result.
    CgsResource::BaseResourcePtr sInvalidFlaptFileOwner;
    const CgsResource::ResourceHandle skInvalidHandle = { &sInvalidFlaptFileOwner, 0 };
}

void FlaptFileInstance::Construct(const RGBA* lpAlternateTextColours,
                                  s32 liNumAlternateColours)
{
    CGS_ASSERT(lpAlternateTextColours != 0, "lpAlternateTextColours");

    mbIsActive = false;
    mpFile = skInvalidHandle;
    mpRootMovieClipInstance = 0;
    mpAlternateTextColours = lpAlternateTextColours;
    miNumAlternateColours = liNumAlternateColours;
    mpLinearAlloc = 0;
}

void FlaptFileInstance::Prepare(CgsMemory::LinearMalloc* lpFlaptAllocator)
{
    mpLinearAlloc = lpFlaptAllocator;
}

void FlaptFileInstance::Destruct()
{
    // The X360 tears an instance down only if it is live (lbz/beq guard
    // @0x8246E2AC): deactivate, unbind the file, drop the allocator.
    if (mbIsActive)
    {
        mbIsActive = false;
        mpFile = skInvalidHandle;
        mpLinearAlloc = 0;
    }
}

// ---- SetData @ 0x82471620 --------------------------------------------------
// Bind a loaded FlaptFile resource to this (inactive) instance: rebind mpFile from
// the handle, allocate the root MovieClipInstance from the flapt allocator,
// construct it over the file's first serialised MovieClip as "_root" (no parent),
// rewind it to frame 0, and mark the instance active.
void FlaptFileInstance::SetData(CgsResource::ResourceHandle lHandle,
                                FlaptRenderer* lpRenderer)
{
    CGS_ASSERT(!mbIsActive, "!IsActive()");
    CGS_ASSERT(lpRenderer != 0, "lpRenderer");

    mpFile = lHandle;

    CGS_ASSERT(mpFile->muNumMovieClips > 0,
               "Error - invalid FLApt file with mo movieclips");

    // The root clip instance. The console allocates the literal sizeof
    // (MovieClipInstance) == 0x38. FLAG: replace the literal with
    // sizeof(MovieClipInstance) once the timeline type's x64 layout lands (its
    // bodies are still un-homed no-ops, so nothing writes past the console extent
    // yet and the constant only under-sizes a region nothing dereferences).
    mpRootMovieClipInstance =
        static_cast<MovieClipInstance*>(mpLinearAlloc->Malloc(0x38));
    CGS_ASSERT(mpRootMovieClipInstance != 0, "mpRootMovieClipInstance");

    mpRootMovieClipInstance->Construct(mpFile->mpaMovieClips, "_root", 0,
                                       mpLinearAlloc, lpRenderer,
                                       mpAlternateTextColours,
                                       miNumAlternateColours);
    mpRootMovieClipInstance->GotoFrame(0);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "FLApt: allocated "
                                   << static_cast<s32>(mpLinearAlloc->GetUsage())
                                   << "b runtime data for file\n";
    }

    mbIsActive = true;
}

// ---- Update @ 0x82471820 ---------------------------------------------------
// Per-frame tick: guard the instance is live with a bound movie, then advance the
// root clip tree by the time step.
void FlaptFileInstance::Update(f32 lfTimeStep)
{
    CGS_ASSERT(mbIsActive, "IsActive()");
    CGS_ASSERT(mpFile->muNumMovieClips > 0, "mpFile->muNumMovieClips > 0");
    CGS_ASSERT(mpRootMovieClipInstance != 0, "mpRootMovieClipInstance");

    mpRootMovieClipInstance->Update(lfTimeStep);
}

// ---- Render @ 0x82472480 ---------------------------------------------------
// Per-frame draw: same three guards, then draw the root clip tree through the
// manager's renderer.
void FlaptFileInstance::Render(FlaptRenderer* lpRenderer)
{
    CGS_ASSERT(mbIsActive, "IsActive()");
    CGS_ASSERT(mpFile->muNumMovieClips > 0, "mpFile->muNumMovieClips > 0");
    CGS_ASSERT(mpRootMovieClipInstance != 0, "mpRootMovieClipInstance");

    mpRootMovieClipInstance->Render(lpRenderer);
}

// ---- GetRootMovieClip @ 0x8246B360 ---------------------------------------
MovieClipRef* FlaptFileInstance::GetRootMovieClip(MovieClipRef* lpOutRef) const
{
    CGS_ASSERT(mbIsActive, "IsActive()");
    CGS_ASSERT(mpRootMovieClipInstance != 0, "mpRootMovieClipInstance");

    // Inlined MovieClipRef construction: re-asserts the clip pointer is non-null
    // ("lpMovieClipInst") and seeds the transform slot to 0 (a root clip has no
    // Im2dTransform). The root instance is held opaquely here (void*); the Ref
    // names it as a MovieClipInstance*, so bridge the pointer type.
    void* lpMovieClipInst = mpRootMovieClipInstance;
    CGS_ASSERT(lpMovieClipInst != 0, "lpMovieClipInst");

    lpOutRef->mpMovieClipInst =
        static_cast<BrnFlapt::MovieClipInstance*>(lpMovieClipInst);
    lpOutRef->mpTransform = 0;
    return lpOutRef;
}

}
