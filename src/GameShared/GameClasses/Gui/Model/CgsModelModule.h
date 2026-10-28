#pragma once

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"
#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.h"

namespace CgsModule { struct IOBufferStack; }

namespace CgsGui
{
    // DecFIGS CgsModelModule.h:54/189..194, with the ARTIST-owned subobjects
    // proven by 827E4F10 and 82847760. Host access is by member name.
    class ModelModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // Original CPU monitor handles (DecFIGS h:182..184), published by
        // BrnGui::GuiPerfmons::Initialise824EF050 into these actual owners.
        static s32 miUpdateInputPM;
        static s32 miUpdateMainPM;
        static s32 miUpdateOutputPM;

        enum ModelPrepareStage
        {
            E_PREPARE_START = 0,
            E_PREPARE_MANAGER = 1,
            E_PREPARE_EVENT_INTERPRETER = 2,
            E_PREPARE_RESOURCE_MANAGER = 3,
            E_PREPARE_DONE = 4,
        };
        enum ModelReleaseStage
        {
            E_RELEASE_START = 0,
            E_RELEASE_MANAGER = 1,
            E_RELEASE_EVENT_INTERPRETER = 2,
            E_RELEASE_RESOURCE_MANAGER = 3,
            E_RELEASE_DONE = 4,
        };

        ModelModule();
        virtual void Construct(bool lbHighDef);
        virtual bool Prepare(s32 liAptPersistentBundleBank, s32 liAptStreamedBundleBank,
            s32 liFontBundleBank, s32 liFSMBundleBank, s32 liLanguageBundleBank,
            s32 liTexturesBundlePoolId, s32 liGlobalTexturePoolId);
        bool Release() ;
        void Destruct() ;

        virtual void PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack, ModelIO::OutputBuffer* lpOutput);
        virtual void Update(CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            ModelIO::InputBuffer* lpInput, ModelIO::OutputBuffer* lpOutput);

        // ARTIST82852CF8; DecFIGS cpp:516. The four native BrnGui
        // Construct registrations use this real Interpreter owner.
        void AddGuiEventObserver(EventObserver* lpEventObserver, s32 liEventType);

        // ARTIST8285DF50; DecFIGS cpp:371. Original 18432-byte director
        // queue overload, distinct from the 32768-byte shared input overload.
        void AddGuiEvents(const ModelIO::InputBuffer::GuiEventQueue* lpEvents,
            ModelIO::InputBuffer* lpInput);

        // ARTIST8285DFF8; DecFIGS CgsModelModule.cpp:390. The original
        // publisher is void and asserts the locked-input handle, then continues.
        void AddGuiEvents(const ModelIO::InputBuffer::GuiEventInputQueue* lpEvents,
            ModelIO::InputBuffer* lpInput);

    private:
        void BridgeInEventInterpreter(EventInterpreterModuleIO::InputBuffer* lpInterpreterInput,
            const ModelIO::InputBuffer* lpInput);
        void BridgeInGuiResource(GuiResourceModuleIO::InputBuffer* lpResourceInput,
            const ModelIO::InputBuffer* lpInput);
        void BridgeEventInterpreterToGuiResource(GuiResourceModuleIO::InputBuffer* lpResourceInput,
            const EventInterpreterModuleIO::OutputBuffer* lpInterpreterOutput);
        void BridgeOutEventInterpreter(ModelIO::OutputBuffer* lpOutput,
            const EventInterpreterModuleIO::OutputBuffer* lpInterpreterOutput);
        void BridgeOutGuiResource(ModelIO::OutputBuffer* lpOutput,
            const GuiResourceModuleIO::OutputBuffer* lpResourceOutput);

        ModelPrepareStage mePrepareStage;
        ModelReleaseStage meReleaseStage;
        EventInterpreterModule mEventInterpreter;
        GuiResourceModule mGuiResource;
    };
}
