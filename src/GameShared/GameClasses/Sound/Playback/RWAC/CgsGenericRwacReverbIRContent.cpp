#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacSubmixVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>
#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826D7EE0
//   (CgsSound::Playback::GenericRwacReverbIRContent::`scalar deleting destructor')
//
// The X360 dtor is the compiler synthesis over this class's layout:
//   v4 = *(this+44); if (v4) *(v4+16) = *(this+48);   // unlink alias-list node
//   v5 = *(this+48); if (v5) *(v5+12) = *(this+44);
//   *(this+44) = this+32; *(this+48) = this+32;        // re-self-link the ring
//   CgsSound::Playback::Content::~Content(this);        // base dtor
//   if (a2 & 1) operator delete(this);                  // scalar-deleting tail
//
// this+32 (0x20) is mLoader.mpResource (a ResourcePtr<AlignedBinaryFileResource>),
// whose BaseResourcePtr alias links live at +0x0C/+0x10 == object +44/+48. The
// unlink + re-self-link IS CgsResource::BaseResourcePtr::~BaseResourcePtr()
// (@0x821F1E18). Defining the class destructor out-of-line makes the compiler emit
// that exact sequence: destroy mLoader (-> ~ResourcePtr -> ~BaseResourcePtr unlink)
// then run the Content base dtor. The body is empty -- the members and base do all
// the work the asm performs.

namespace CgsSound
{
namespace Playback
{
    const Name GenericRwacReverbIRSlot::SK_SLOT_CLASSNAME("~GenericRwacReverbIRSlot~");

    namespace
    {
        // [FLAG PC witness] NOT IN THE X360 BINARY. BRN_AEMS_DIAG=1. Slots are few.
        void ReverbSlotDiag(const char* lpcMessage)
        {
            if (std::getenv("BRN_AEMS_DIAG"))
                CgsDev::Log::WriteToLog(lpcMessage);
        }

        // SlotFactory<GenericRwacReverbIRSlot>::DoCreateSlot (DecFIGS, the same template
        // instantiation as GenericRwacContentSlot's @0x8EF094): allocate through
        // Voice -> Factory -> Environment, alignment 4, tag "SlotImplementation".
        class GenericRwacReverbIRSlotFactory : public ISlotFactory
        {
        public:
            GenericRwacReverbIRSlotFactory()
                : ISlotFactory(GenericRwacReverbIRSlot::SK_SLOT_CLASSNAME) {}

            virtual ISlotImplementation* DoCreateSlot(Voice& arVoice) const
            {
                Factory& lrFactory = const_cast<Factory&>(arVoice.GetFactory());
                void* lpMemory = lrFactory.GetEnvironment().Allocate(
                    static_cast<u32>(sizeof(GenericRwacReverbIRSlot)), 4,
                    "SlotImplementation");
                CGS_ASSERT(lpMemory != 0, "lpvMem");
                ReverbSlotDiag("[FLAG PC witness] [reverbir] slot created\n");
                return lpMemory ? ::new (lpMemory) GenericRwacReverbIRSlot() : 0;
            }
        };

        const GenericRwacReverbIRSlotFactory sGenericRwacReverbIRSlotFactory;
    }

    // ARTIST @0x826C1C00. Plug-in event 0 with one parameter record: the loaded IR file.
    void GenericRwacReverbIRSlot::DoPostAttach(const Slot& arSlot, Voice& arVoice,
                                               Content& arContent)
    {
        GenericRwacSubmixVoice& lrRwacVoice = static_cast<GenericRwacSubmixVoice&>(arVoice);
        void* lpvReverbIRFile = arContent.GetData(E_CONTENT_STATE_LOADED);

        RwacCommandPluginEvent lReverbCommand(
            reinterpret_cast<uintptr_t>(lrRwacVoice.GetPlugin(arSlot.GetPluginOffset())),
            0u, 1u);
        RwacCommandApplyReverbIRFile lReverbParameters(lpvReverbIRFile);

        RwacCommandQueue& lrQueue = lrRwacVoice.GetRwacFactory().GetCommandQueue();
        lrQueue.Post(lReverbCommand);
        lrQueue.Post(lReverbParameters);
        ReverbSlotDiag("[FLAG PC witness] [reverbir] post attach\n");
    }

    // ARTIST @0x826C1D08. Plug-in event 1, no parameter record (the StopReverbHandler arm of
    // ReverbIR1::EventEvent @0x82BA56B8).
    void GenericRwacReverbIRSlot::DoPreDetach(const Slot& arSlot, Voice& arVoice,
                                              Content& /*arContent*/)
    {
        GenericRwacSubmixVoice& lrRwacVoice = static_cast<GenericRwacSubmixVoice&>(arVoice);

        RwacCommandPluginEvent lReverbCommand(
            reinterpret_cast<uintptr_t>(lrRwacVoice.GetPlugin(arSlot.GetPluginOffset())),
            1u, 0u);
        lrRwacVoice.GetRwacFactory().GetCommandQueue().Post(lReverbCommand);
        ReverbSlotDiag("[FLAG PC witness] [reverbir] pre detach\n");
    }

    GenericRwacReverbIRContent::GenericRwacReverbIRContent(Factory& arFactory,
                                                           const ContentSpec& akrSpec,
                                                           u32 au32Ident)
        : Content(arFactory, akrSpec, au32Ident),
          mLoader()
    {
    }

    bool GenericRwacReverbIRContent::DoLoad()
    {
        return mLoader.Load(*this, GetContentSpec());
    }

    bool GenericRwacReverbIRContent::DoUnload()
    {
        return mLoader.Unload(*this, GetContentSpec());
    }

    void GenericRwacReverbIRContent::DoUpdate(f32 /*af32Dt*/)
    {
        mLoader.Update(*this, GetContentSpec());
    }

    void* GenericRwacReverbIRContent::DoGetData()
    {
        return mLoader.GetData();
    }

    GenericRwacReverbIRContent::~GenericRwacReverbIRContent()
    {
        // mLoader (-> ResourcePtr<AlignedBinaryFileResource> -> ~BaseResourcePtr
        // alias-list unlink) and the Content base dtor are run implicitly here.
    }
}
}
