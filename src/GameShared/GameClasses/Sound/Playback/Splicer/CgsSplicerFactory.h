#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERFACTORY_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"   // Factory (base)
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"  // Registry
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"    // Handle<GenericRwacFactory>

struct SpliceManager;

// ============================================================================
// CgsSplicerFactory.h  (MINIMAL home for the SplicerFactory destructor TU).
//
//   CgsSound::Playback::SplicerFactory::`scalar deleting destructor'  @ 0x826DB0E0
//
// DWARF (CgsSplicerFactory.h:58): SplicerFactory : public Factory, with three
// trivially-destructible members. The class destructor body is empty -- the Factory
// base dtor does the teardown; MSVC re-synthesises the scalar-deleting thunk from the
// out-of-line dtor. Members pinned BY NAME (host-width FLAG).
// ============================================================================

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;
struct SplicerContent;

// The sizing spec Create/ctor consume (console spec words +0 the retained RWAC
// factory handle / +4 entityCount / +8 dataBytes / +0xC stringBytes -- the same
// ref-spec r5 lowering as AEMS).
struct SplicerFactorySpec
{
    Factory*  mpRwacFactory;        // +0x00 (Handle<GenericRwacFactory> raw pointer)
    u32       mu32EntityCount;      // +0x04
    u32       mu32DataSize;         // +0x08
    u32       mu32StringTableSize;  // +0x0C
};

struct SplicerFactory : public Factory
{
    // @ 0x826DB130 (AEMS-cascade slice 3; full decode progress/scratch_dossiers/
    // aems_factory_cascade_codex.md). Carve (console 4*(entities+0x1C0)+data+
    // strings at host widths: the fixed head + the in-place Registry + the
    // trailing SpliceManager arena) through the ENVIRONMENT's allocator tagged
    // "SplicerFactory"; construct; return the handle with one explicit Acquire.
    static Handle<SplicerFactory> Create(Environment& arEnvironment,
                                         const SplicerFactorySpec& akrSpec);

    // @ 0x826DB010. Base (Name = the "~SplicerFactory::SK_NAME~" intern ==
    // console dword_83008404, writer sub_82C65938), the RWAC retain, the
    // in-place Registry (+0x1C), the trailing SpliceManager (mono 0x40 / stereo
    // 0x18 pool counts) and the manager's assert-sink install (+0x610 :=
    // &SplicerAssertFunc). Body in CgsSplicerFactory.cpp.
    SplicerFactory(Environment& arEnvironment, const SplicerFactorySpec& akrSpec);

    // @ 0x826DB0E0. Empty out-of-line dtor (Factory base dtor runs implicitly).
    virtual ~SplicerFactory();

    // @ 0x8268ABA0. The splice factory's assertion sink (always fires; returns
    // the assert front-end's leave result). Bodied in CgsSplicerFactory.cpp.
    // ⭐ STATIC (ABI corrected, slice 3): the ctor stores this function's RAW
    // ADDRESS into the manager's +0x610 one-argument callback slot and the
    // manager call sites pass only the message -- a hidden-this member ABI
    // cannot match.
    static void* SplicerAssertFunc(const char* lpcExpression);

    // The nested registry (console +0x10), by name.
    Registry* GetRegistry() { return mpRegistry; }

    SpliceManager* GetManager() const { return mpManager; }
    GenericRwacFactory& GetRwacFactory() const;

protected:
    virtual bool DoCreateVoice(const VoiceSpec& akrSpec,
                               Handle<Voice>& arHandleOut, u32 au32Ident);
    virtual bool DoCreateContent(const ContentSpec& akrSpec,
                                 Handle<Content>& arHandleOut, u32 au32Ident);
    virtual void DoUpdate(f32 af32DeltaTime);

private:
    Registry*      mpRegistry;     // CgsSplicerFactory.h:128 (console +0x10 -> the in-place Registry @ +0x1C)
    Factory*       mpRwacFactory;  // CgsSplicerFactory.h:129 (console +0x14, retained raw handle)
    SpliceManager* mpManager;      // CgsSplicerFactory.h:130 (console +0x18 -> the trailing arena)
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERFACTORY_H
