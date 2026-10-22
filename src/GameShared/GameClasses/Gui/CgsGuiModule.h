#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Gui/Model/CgsModelModule.h"
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/Language/CgsSku.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsModule { struct IOBufferStack; }
namespace CgsMemory { class HeapMalloc; class LinearMalloc; }
namespace rw { class IResourceAllocator; namespace core { struct GeneralResourceAllocator; } }

namespace CgsGui
{
    class ViewModule;
    namespace ViewIO { struct InputBuffer; struct OutputBuffer; }

    // DecFIGS CgsGuiModule.h:55..74; all seven bank arguments and the four
    // View.Prepare allocator seats are attested by ARTIST82857070.
    struct GuiModuleConfig
    {
        s32 miAptPersistentBunldePoolId;
        s32 miAptStreamedbundlePoolId;
        s32 miFontBundlePoolId;
        s32 miFSMBunldePoolId;
        s32 miLanguageBundlePoolId;
        s32 miTexturesBundlePoolId;
        s32 miGlobalTexturePoolId;
        CgsMemory::HeapMalloc* mpAptAllocator;
        CgsMemory::HeapMalloc* mpFSMAllocator;
        CgsMemory::HeapMalloc* mpLanguageAllocator;
        rw::core::GeneralResourceAllocator* mpRendererAllocator;
        CgsMemory::LinearMalloc* mpGuiStateAllocator;
        rw::IResourceAllocator* mpTextureAllocator;
        CgsMemory::LinearMalloc* mpFlaptAllocator;
    };

    // ARTIST827E54B0 and DecFIGS h:86/190..235: real base and contained Model,
    // followed by View, Sku, stored events and lifecycle state. C++ owns host
    // layout and vptrs; no console-sized owner shell or placement writes.
    class GuiModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        typedef CgsModule::VariableEventQueue<18432, 16> GuiNotificationQueue;

        // Original monitor handles, published by the real registration owner
        // BrnGui::GuiPerfmons::Initialise (ARTIST824EF050).
        static s32 miGuiPM;
        static s32 miModelUpdatePM;
        static s32 miModelViewBridgePM;
        static s32 miViewUpdatePM;
        static s32 miGuiRenderPM;

        enum GuiPrepareStage
        {
            E_PREPARE_START = 0,
            E_PREPARE_MANAGER = 1,
            E_PREPARE_MODEL = 2,
            E_PREPARE_SKU = 3,
            E_PREPARE_CHECK_BUTTON_MAPPING = 4,
            E_PREPARE_VIEW = 5,
            E_PREPARE_DONE = 6,
        };
        enum GuiReleaseStage
        {
            E_RELEASE_START = 0,
            E_RELEASE_VIEW = 1,
            E_RELEASE_MODEL = 2,
            E_RELEASE_MANAGER = 3,
            E_RELEASE_DONE = 4,
        };

        GuiModule();
        virtual void Construct(ViewModule* lpViewModule, bool lbHighDef);
        virtual bool Prepare(GuiModuleConfig& lrConfig);
        bool Release() override;
        void Destruct() override;
        virtual void PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            CgsGuiModuleIO::OutputBuffer* lpOutput, ModelIO::OutputBuffer* lpModelOutput);
        virtual void Update(CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            CgsGuiModuleIO::InputBuffer* lpInput, CgsGuiModuleIO::OutputBuffer* lpOutput,
            ModelIO::InputBuffer* lpModelInput, ModelIO::OutputBuffer* lpModelOutput,
            ViewIO::InputBuffer* lpViewInput, ViewIO::OutputBuffer* lpViewOutput);
        virtual void Render(ViewIO::InputBuffer* lpViewInput,
            CgsGuiModuleIO::InputBuffer* lpInput);

        void BridgeFromInputToView(ViewIO::InputBuffer* lpViewInput,
            const CgsGuiModuleIO::InputBuffer* lpInput);
        void BridgeFromModelToOutput(CgsGuiModuleIO::OutputBuffer* lpOutput,
            const ModelIO::OutputBuffer* lpModelOutput);

        // ---- GUI input-event publisher template (270 X360-attested instantiations) -------
        // X360 0x823CEB70..0x823DAE68 -- one out-of-line body per event type T (e.g.
        // AddGuiEvent<BrnGui::GuiAftertouchEvent> @0x823D4490). A producer that has
        // write-locked the module input buffer pushes one typed GUI event onto that buffer's
        // inbound VariableEventQueue<32768,16> (mInputQueue @+4, reached through the buffer's
        // GetGuiEvents() write handle == the X360 sub_8284F238 lock+queue accessor). The
        // queued record's event-type id and byte size are lpEvent->GetEventType() (the
        // GuiEvent<N> id, inlined to a per-T constant) and sizeof(T), so ONE body reproduces
        // all 270 AddEvent(&event, id, size) stores. The null-checked argument is the buffer
        // (X360 FireAssert message "Input hasn't been locked for write", CgsGuiModule.h:286;
        // non-gating). Bodied + explicitly instantiated in CgsGuiModule_AddGuiEvent_Inst.cpp.
        template <class T>
        int AddGuiEvent(const T* lpEvent, CgsGuiModuleIO::InputBuffer* lpBuffer);

        // ---- single-event inbound publisher template (X360-attested instances) ----------
        // The producer-side twin of OutputBuffer::AddGuiOutEvent<T>: push one GUI event onto
        // the module INPUT buffer's inbound queue. X360 emits one out-of-line body per event
        // type (??$AddGuiEvent@V...@GuiModule@CgsGui@@QAAXAAV...@PAVInputBuffer@CgsGuiModuleIO@2@@Z);
        // every instance is the same three steps, recovered from
        // AddGuiEvent<GuiEventControllerInputPressed> @0x823DA8A8 (siblings 0x823DA960
        // ActiveUserIndex / 0x823DAAD0 Axis / 0x823DAB88 Down / 0x823DAC40 Released /
        // 0x823DAA18 ToggleChangeCar / 0x823DADB0 SetLanguage):
        //   1. assert the input-buffer pointer ("Input hasn't been locked for write",
        //      CgsGuiModule.h:286);
        //   2. fetch the buffer's inbound queue via InputBuffer::GetGuiEvents() @0x8284F238
        //      (which asserts the write lock);
        //   3. AddEvent(payload, T's event-type id, payload size) -- the on-queue record is
        //      the PAYLOAD ONLY (the bytes past the 12-byte GuiEvent<N> header; the id rides
        //      the queue entry). X360 payload sizes: Pressed/Down/Released 8, Axis 12,
        //      ActiveUserIndex/SetLanguage 4, and a 1-byte marker for payload-less events
        //      (GuiEvent<296> and ToggleChangeCar(540) both push size 1).
        //
        // The X360 emits these as non-static members of the module embedded in BrnGameModule
        // (@+7252512); the body never reads `this`, so this reconstruction keeps it callable
        // as a static until that embed is constructed on PC (the model-module ctor chain is a
        // GuiFsmController-flow follow-on).
        template <class T>
        static void AddGuiEvent(T& lrEvent, CgsGuiModuleIO::InputBuffer* lpInput)
        {
            CGS_ASSERT(lpInput != 0, "Input hasn't been locked for write");
            // GuiEvent<N> header = 3 x u32 (CgsGuiEventTypeDefs.h static_asserts pin +0x0C).
            const s32 KI_GUI_EVENT_HEADER_SIZE = 12;
            s32 liPayloadSize = static_cast<s32>(sizeof(T)) - KI_GUI_EVENT_HEADER_SIZE;
            const void* lpPayload;
            if (liPayloadSize <= 0)
            {
                liPayloadSize = 1;      // payload-less event: the X360 pushes a 1-byte marker
                lpPayload     = &lrEvent;
            }
            else
            {
                lpPayload = reinterpret_cast<const char*>(&lrEvent) + KI_GUI_EVENT_HEADER_SIZE;
            }
            lpInput->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lpPayload),
                lrEvent.GetEventType(), liPayloadSize);
        }


    protected:
        ModelModule mGuiModel;
        ViewModule* mpGuiView;
        CgsLanguage::Sku mSku;
        GuiNotificationQueue mStoredGuiEvents;

    private:
        void BridgeFromInputToModel(ModelIO::InputBuffer* lpModelInput,
            const CgsGuiModuleIO::InputBuffer* lpInput);
        void BridgeFromModelToView(ViewIO::InputBuffer* lpViewInput,
            const ModelIO::OutputBuffer* lpModelOutput);
        void BridgeFromViewToOutput(CgsGuiModuleIO::OutputBuffer* lpOutput,
            const ViewIO::OutputBuffer* lpViewOutput);
        void BridgeFromOutputToGui(const CgsGuiModuleIO::OutputBuffer* lpOutput);

        GuiPrepareStage mePrepareStage;
        GuiReleaseStage meReleaseStage;
        bool mbCircleButtonMappingChanged;
    };
}
