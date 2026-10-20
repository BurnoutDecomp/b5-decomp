#include "GameShared/GameClasses/Gui/CgsGuiModule.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/rwcore_general_alloc.h"

namespace CgsGui
{
    // ARTIST827E54B0 constructs the real base/Model owner and clears only
    // the stored queue's constructed flag. Lifecycle state belongs to Construct.
    GuiModule::GuiModule()
    {
        mStoredGuiEvents.MarkUnconstructed();
    }

    // ARTIST82856FA8. The non-null assertion does not gate continuation.
    void GuiModule::Construct(ViewModule* lpViewModule, bool lbHighDef)
    {
        if (!lpViewModule)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpViewModule",
                "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\gui\CgsGuiModule.cpp", 64);
            CgsDev::Assert::EndAssert();
        }
        CgsModule::ModuleSingleBuffered::Construct();
        mGuiModel.Construct(lbHighDef);
        mpGuiView = lpViewModule;
        mStoredGuiEvents.Construct();
        mSku.Construct(this);
        mbCircleButtonMappingChanged = false;
        mePrepareStage = E_PREPARE_START;
        meReleaseStage = E_RELEASE_DONE;
        mbIsNewModule = true;
    }

    // ARTIST82857070. Completed stages fall through in this call; waiting
    // stages retain their seat. ARTIST joins CHECK_BUTTON_MAPPING at VIEW.
    bool GuiModule::Prepare(GuiModuleConfig& lrConfig)
    {
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        case E_PREPARE_MANAGER:
            mePrepareStage = E_PREPARE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            mStoredGuiEvents.Prepare();
            [[fallthrough]];
        case E_PREPARE_MODEL:
            mePrepareStage = E_PREPARE_MODEL;
            if (!mGuiModel.Prepare(lrConfig.miAptPersistentBunldePoolId,
                lrConfig.miAptStreamedbundlePoolId, lrConfig.miFontBundlePoolId,
                lrConfig.miFSMBunldePoolId, lrConfig.miLanguageBundlePoolId,
                lrConfig.miTexturesBundlePoolId, lrConfig.miGlobalTexturePoolId))
                return false;
            [[fallthrough]];
        case E_PREPARE_SKU:
            mePrepareStage = E_PREPARE_SKU;
            mSku.SetLanguage(mSku.FindLanguage());
            mSku.SetSku(mSku.FindSku());
            [[fallthrough]];
        case E_PREPARE_CHECK_BUTTON_MAPPING:
        case E_PREPARE_VIEW:
            mePrepareStage = E_PREPARE_VIEW;
            if (!mpGuiView->Prepare(lrConfig.mpAptAllocator, lrConfig.mpRendererAllocator,
                lrConfig.mpLanguageAllocator, lrConfig.mpFlaptAllocator))
                return false;
            [[fallthrough]];
        case E_PREPARE_DONE:
            mePrepareStage = E_PREPARE_DONE;
            meReleaseStage = E_RELEASE_START;
            return true;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Unknown prepare state",
                "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\gui\CgsGuiModule.cpp", 188);
            CgsDev::Assert::EndAssert();
            return false;
        }
    }

    // ARTIST82857200: View, Model/queue, then the base manager.
    bool GuiModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
        case E_RELEASE_VIEW:
            meReleaseStage = E_RELEASE_VIEW;
            if (!mpGuiView->Release())
                return false;
            [[fallthrough]];
        case E_RELEASE_MODEL:
            meReleaseStage = E_RELEASE_MODEL;
            if (!mGuiModel.Release())
                return false;
            mStoredGuiEvents.Release();
            [[fallthrough]];
        case E_RELEASE_MANAGER:
            meReleaseStage = E_RELEASE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            [[fallthrough]];
        case E_RELEASE_DONE:
            meReleaseStage = E_RELEASE_DONE;
            mePrepareStage = E_PREPARE_START;
            return true;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Unknown release state",
                "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\gui\CgsGuiModule.cpp", 261);
            CgsDev::Assert::EndAssert();
            return false;
        }
    }

    // ARTIST82857338 has no separate Model.Destruct call.
    void GuiModule::Destruct()
    {
        mStoredGuiEvents.Destruct();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // ARTIST8285F300: PreWorld has no resource/view input phase of its own.
    void GuiModule::PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        CgsGuiModuleIO::OutputBuffer* lpOutput, ModelIO::OutputBuffer* lpModelOutput)
    {
        mGuiModel.PreWorldUpdate(lpInputBufferStack, lpOutputBufferStack, lpModelOutput);
        const ModelIO::OutputBuffer* lpModelRead = lpModelOutput;
        CgsModule::LockBuffersForIO(lpOutput, lpModelRead);
        BridgeFromModelToOutput(lpOutput, lpModelRead);
        CgsModule::UnlockBuffersForIO(lpOutput, lpModelRead);
    }

    // ARTIST828602C8. lpOutput enters and leaves write-locked by the BrnGui
    // wrapper. r4/r5 are the IO stacks; the ninth total argument is View output
    // (caller stack arg_54, loaded at828604BC), not 20 anonymous arguments.
    void GuiModule::Update(CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        CgsGuiModuleIO::InputBuffer* lpInput, CgsGuiModuleIO::OutputBuffer* lpOutput,
        ModelIO::InputBuffer* lpModelInput, ModelIO::OutputBuffer* lpModelOutput,
        ViewIO::InputBuffer* lpViewInput, ViewIO::OutputBuffer* lpViewOutput)
    {
        CgsDev::PerfMonCpu::StartMonitor(miGuiPM);
        const CgsGuiModuleIO::InputBuffer* lpInputRead = lpInput;
        CgsModule::LockBuffersForIO(lpModelInput, lpInputRead);
        BridgeFromInputToModel(lpModelInput, lpInputRead);
        CgsModule::UnlockBuffersForIO(lpModelInput, lpInputRead);
        CgsModule::LockBuffersForIO(lpViewInput, lpInputRead);
        BridgeFromInputToView(lpViewInput, lpInputRead);
        CgsModule::UnlockBuffersForIO(lpViewInput, lpInputRead);

        if (!lpModelInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "..\..\..\GameShared\GameClasses\Module/CgsModuleUtils.h", 238);
            CgsDev::Assert::EndAssert();
        }
        lpModelInput->LockForWrite();
        mSku.Update(lpModelInput, lpOutput);
        if (mbCircleButtonMappingChanged == true)
        {
            GuiEventSetCircleButtonConfig lEvent;
            lpOutput->AddGuiOutEvent(lEvent);
            mbCircleButtonMappingChanged = false;
        }
        if (!lpModelInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "..\..\..\GameShared\GameClasses\Module/CgsModuleUtils.h", 248);
            CgsDev::Assert::EndAssert();
        }
        lpModelInput->UnlockForWrite();

        CgsDev::PerfMonCpu::StartMonitor(miModelUpdatePM);
        mGuiModel.Update(lpInputBufferStack, lpOutputBufferStack, lpModelInput, lpModelOutput);
        CgsDev::PerfMonCpu::StopMonitor(miModelUpdatePM);
        CgsDev::PerfMonCpu::StartMonitor(miModelViewBridgePM);
        const ModelIO::OutputBuffer* lpModelRead = lpModelOutput;
        CgsModule::LockBuffersForIO(lpViewInput, lpModelRead);
        BridgeFromModelToView(lpViewInput, lpModelRead);
        CgsModule::UnlockBuffersForIO(lpViewInput, lpModelRead);
        CgsDev::PerfMonCpu::StopMonitor(miModelViewBridgePM);
        CgsDev::PerfMonCpu::StartMonitor(miViewUpdatePM);
        mpGuiView->Update(lpInputBufferStack, lpOutputBufferStack, lpViewInput, lpViewOutput);
        CgsDev::PerfMonCpu::StopMonitor(miViewUpdatePM);

        lpModelRead->LockForRead();
        BridgeFromModelToOutput(lpOutput, lpModelRead);
        lpModelRead->UnlockForRead();
        lpViewOutput->LockForWrite();
        AptCommunicator::FlushTriggerEventsTo(lpViewOutput->GetGuiEventQueue());
        lpViewOutput->UnlockForWrite();
        const ViewIO::OutputBuffer* lpViewRead = lpViewOutput;
        lpViewRead->LockForRead();
        BridgeFromViewToOutput(lpOutput, lpViewRead);
        lpViewRead->UnlockForRead();

        lpOutput->UnlockForWrite();
        lpOutput->LockForRead();
        BridgeFromOutputToGui(lpOutput);
        lpOutput->UnlockForRead();
        lpOutput->LockForWrite();
        CgsDev::PerfMonCpu::StopMonitor(miGuiPM);
    }

    // ARTIST8285AF38. Publish the caller's renderer set under its read lock,
    // then let the real View.Render consume its input.
    void GuiModule::Render(ViewIO::InputBuffer* lpViewInput,
        CgsGuiModuleIO::InputBuffer* lpInput)
    {
        CgsDev::PerfMonCpu::StartMonitor(miGuiRenderPM);
        const CgsGuiModuleIO::InputBuffer* lpInputRead = lpInput;
        CgsModule::LockBuffersForIO(lpViewInput, lpInputRead);
        lpViewInput->SetImRenderers(lpInputRead->GetImRenderers());
        CgsModule::UnlockBuffersForIO(lpViewInput, lpInputRead);
        mpGuiView->Render(lpViewInput);
        CgsDev::PerfMonCpu::StopMonitor(miGuiRenderPM);
    }
}
