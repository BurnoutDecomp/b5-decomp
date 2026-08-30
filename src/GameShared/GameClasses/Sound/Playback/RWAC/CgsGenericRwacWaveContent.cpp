#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacPlayerVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"
#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DA0E0
//   (CgsSound::Playback::GenericRwacWaveContent::DoUnload)
//
// X360: `return ContentLoader<BinaryFileResource>::Unload(this + 32, this, *(this + 12))`
//   this + 32     == the embedded `mLoader` (ContentLoader<BinaryFileResource>,
//                    object +0x20 -- CgsGenericRwacContent.h:70 / DWARF)
//   *(this + 12)  == Content::mContentSpec (the const ContentSpec& member @ +0x0C)
// i.e. BY NAME: delegate the unload to the embedded loader with this content and
// its spec.
//
// (2026-08-25, audio-faithfulness wave 3: the former TU-local fabricated
// `GenericRwacWaveContent { virtual bool DoUnload(); }` + `BinaryFileResource_`
// rivals -- which ODR-collided with the real CgsGenericRwacContent.h home this TU's
// own sibling dtor uses, and returned int over raw byte offsets -- are retired.)

namespace CgsSound
{
namespace Playback
{

const Name GenericRwacContentSlot::SK_SLOT_CLASSNAME(
    "~GenericRwacContentSlot~");

namespace
{
    // SlotFactory<GenericRwacContentSlot>::DoCreateSlot, DecFIGS 0x8EF094:
    // allocate one implementation through Voice -> Factory -> Environment with
    // alignment 4 and the tag "SlotImplementation", then placement-construct it.
    class GenericRwacContentSlotFactory : public ISlotFactory
    {
    public:
        GenericRwacContentSlotFactory()
            : ISlotFactory(GenericRwacContentSlot::SK_SLOT_CLASSNAME) {}

        virtual ISlotImplementation* DoCreateSlot(Voice& arVoice) const
        {
            Factory& lrFactory = const_cast<Factory&>(arVoice.GetFactory());
            void* lpMemory = lrFactory.GetEnvironment().Allocate(
                static_cast<u32>(sizeof(GenericRwacContentSlot)), 4,
                "SlotImplementation");
            CGS_ASSERT(lpMemory != 0, "lpvMem");
            return lpMemory ? ::new (lpMemory) GenericRwacContentSlot() : 0;
        }
    };

    const GenericRwacContentSlotFactory sGenericRwacContentSlotFactory;
}

GenericRwacWaveContent::GenericRwacWaveContent(Factory& arFactory,
                                               const ContentSpec& akrSpec,
                                               u32 au32Ident)
    : Content(arFactory, akrSpec, au32Ident),
      mLoader()
{
}

bool GenericRwacWaveContent::DoLoad()
{
    return mLoader.Load(*this, GetContentSpec());
}

bool GenericRwacWaveContent::DoUnload()
{
    return mLoader.Unload(*this, GetContentSpec());
}

void GenericRwacWaveContent::DoUpdate(f32 /*af32Dt*/)
{
    mLoader.Update(*this, GetContentSpec());
}

void* GenericRwacWaveContent::DoGetData()
{
    return mLoader.GetData();
}

bool GenericRwacWaveContent::GetStreamPath(char* apBuffer, size_t aBufferSize) const
{
    return GetContentSpec().GetPathZone(1, apBuffer, aBufferSize);
}

// ARTIST @0x826C1EA8. Force a parameter refresh, reset the completion query,
// then queue SndPlayer1 event 0 followed by its wave/play parameter record.
bool GenericRwacContentSlot::DoPlay(const Slot& arSlot, PlayerVoice& arVoice,
                                    Content& arContent, u32 /*au32Param*/)
{
    GenericRwacPlayerVoice& lrVoice =
        static_cast<GenericRwacPlayerVoice&>(arVoice);
    GenericRwacWaveContent& lrContent =
        static_cast<GenericRwacWaveContent&>(arContent);

    lrVoice.ForceParameterUpdate();
    rw::audio::core::SndPlayer1::IsRequestDoneParams& lrDone =
        lrVoice.GetDoneParams();
    lrDone.isRequestDone = 0.0f;

    rw::audio::core::PlugIn* lpPlugin =
        lrVoice.GetPlugin(arSlot.GetPluginOffset());
    RwacCommandPluginEvent lEvent(
        reinterpret_cast<uintptr_t>(lpPlugin), 0u, 1u);
    RwacCommandPlayerPlayParameters lParams(
        &lrContent, &lrDone.requestHandle);

    GenericRwacFactory& lrFactory = lrVoice.GetRwacFactory();
    lrFactory.GetCommandQueue().Post(lEvent);
    lrFactory.GetCommandQueue().Post(lParams);
    return true;
}

// ARTIST @0x826C1FE8. SndPlayer1 event 1 carries no secondary parameters.
bool GenericRwacContentSlot::DoStop(const Slot& arSlot, PlayerVoice& arVoice,
                                    Content& /*arContent*/)
{
    GenericRwacPlayerVoice& lrVoice =
        static_cast<GenericRwacPlayerVoice&>(arVoice);
    rw::audio::core::PlugIn* lpPlugin =
        lrVoice.GetPlugin(arSlot.GetPluginOffset());
    RwacCommandPluginEvent lEvent(
        reinterpret_cast<uintptr_t>(lpPlugin), 1u, 0u);
    lrVoice.GetRwacFactory().GetCommandQueue().Post(lEvent);
    return true;
}

// ARTIST @0x826C20A8. A completed request stops the slot; otherwise event 2
// refreshes the two-float IsRequestDoneParams record for the next tick.
bool GenericRwacContentSlot::DoUpdatePlaying(
    System* /*apSystem*/, const Slot& arSlot, PlayerVoice& arVoice,
    Content& /*arContent*/, f32 /*af32Dt*/)
{
    GenericRwacPlayerVoice& lrVoice =
        static_cast<GenericRwacPlayerVoice&>(arVoice);
    rw::audio::core::SndPlayer1::IsRequestDoneParams& lrDone =
        lrVoice.GetDoneParams();
    if (lrDone.isRequestDone == 1.0f)
        return false;

    rw::audio::core::PlugIn* lpPlugin =
        lrVoice.GetPlugin(arSlot.GetPluginOffset());
    RwacCommandPluginEvent lEvent(
        reinterpret_cast<uintptr_t>(lpPlugin), 2u, 1u);
    RwacCommandPlayerIsRequestDoneParameters lParams(&lrDone);

    GenericRwacFactory& lrFactory = lrVoice.GetRwacFactory();
    lrFactory.GetCommandQueue().Post(lEvent);
    lrFactory.GetCommandQueue().Post(lParams);
    return true;
}

} // namespace Playback
} // namespace CgsSound
