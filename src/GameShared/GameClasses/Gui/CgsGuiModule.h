#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Gui/Model/CgsModelModule.h"
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"
#include "GameShared/GameClasses/Language/CgsSku.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"

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
        bool Release() ;
        void Destruct() ;
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
        // ARTIST 0x823CEB70..0x823DAE68 emits one out-of-line body per event type, decorated
        // ??$AddGuiEvent@V<T>@...@GuiModule@CgsGui@@QAAXAAV<T>@...@PAVInputBuffer@CgsGuiModuleIO@...
        // == void GuiModule::AddGuiEvent<T>(T&, CgsGuiModuleIO::InputBuffer*): a public
        // non-static member (DecFIGS CgsGuiModule.h:284; PS3 parameter names lEvent/lpInput).
        // The header-inline body follows the class.
        template <class T>
        void AddGuiEvent(T& lEvent, CgsGuiModuleIO::InputBuffer* lpInput);

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

    // DecFIGS CgsGuiModule.h:284, assert on h:286 (ARTIST li r5,0x11E before FireAssert in every
    // instance, e.g. AddGuiEvent<BrnGui::GuiAftertouchEvent> @0x823D4490). The queue's inline
    // AddGuiEvent<T> (CgsGuiEvent.h:378) records the event object itself:
    // AddEvent(&lEvent, GuiEvent<N> id, sizeof(T)) (0x823D4490: id 403, size 32;
    // 0x823D1520 GuiEnteredJunkyard: id 79, size 1, an empty event).
    template <class T>
    void GuiModule::AddGuiEvent(T& lEvent, CgsGuiModuleIO::InputBuffer* lpInput)
    {
        CGS_ASSERT(lpInput, CGS_ASSERT_STREAM_MESSAGE_AT("Input hasn't been locked for write",
            "..\\..\\..\\GameShared\\GameClasses\\Gui/CgsGuiModule.h", 286));
        lpInput->GetGuiEvents()->AddGuiEvent(lEvent);
    }
}
