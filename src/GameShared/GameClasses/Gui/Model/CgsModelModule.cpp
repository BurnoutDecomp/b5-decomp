#include "GameShared/GameClasses/Gui/Model/CgsModelModule.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

namespace CgsGui
{
    // ARTIST827E4F10 constructs the real base and the two embedded owners. C++
    // performs their construction and vptr installation without console offsets.
    ModelModule::ModelModule() = default;

    // ARTIST82847760; the bool remains in r4 across the Interpreter call.
    void ModelModule::Construct(bool lbHighDef)
    {
        CgsModule::ModuleSingleBuffered::Construct();
        mEventInterpreter.Construct();
        mGuiResource.Construct(lbHighDef);
        mePrepareStage = E_PREPARE_START;
        meReleaseStage = E_RELEASE_DONE;
        mbIsNewModule = true;
    }

    // ARTIST828477E0. Each successful stage falls through in the same call.
    bool ModelModule::Prepare(s32 liAptPersistentBundleBank, s32 liAptStreamedBundleBank,
        s32 liFontBundleBank, s32 liFSMBundleBank, s32 liLanguageBundleBank,
        s32 liTexturesBundlePoolId, s32 liGlobalTexturePoolId)
    {
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
            mePrepareStage = E_PREPARE_START;
            [[fallthrough]];
        case E_PREPARE_MANAGER:
            mePrepareStage = E_PREPARE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            [[fallthrough]];
        case E_PREPARE_EVENT_INTERPRETER:
            mePrepareStage = E_PREPARE_EVENT_INTERPRETER;
            if (!mEventInterpreter.Prepare())
                return false;
            [[fallthrough]];
        case E_PREPARE_RESOURCE_MANAGER:
            mePrepareStage = E_PREPARE_RESOURCE_MANAGER;
            if (!mGuiResource.Prepare(liAptPersistentBundleBank, liAptStreamedBundleBank,
                liFontBundleBank, liFSMBundleBank, liLanguageBundleBank,
                liTexturesBundlePoolId, liGlobalTexturePoolId))
                return false;
            [[fallthrough]];
        case E_PREPARE_DONE:
            meReleaseStage = E_RELEASE_START;
            mePrepareStage = E_PREPARE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Unknown prepare stage");
            return false;
        }
    }

    // ARTIST82847920 + release postincrement828471F0. Base is released first,
    // followed by Interpreter and Resource; this order is intentional.
    bool ModelModule::Release()
    {
        const auto Advance = [this]() {
            meReleaseStage = static_cast<ModelReleaseStage>(meReleaseStage + 1);
            CGS_ASSERT(meReleaseStage <= E_RELEASE_DONE,
                "leEnumIndex <= ModelModule::E_RELEASE_DONE");
        };
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
            Advance();
            [[fallthrough]];
        case E_RELEASE_MANAGER:
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            Advance();
            [[fallthrough]];
        case E_RELEASE_EVENT_INTERPRETER:
            if (!mEventInterpreter.Release())
                return false;
            Advance();
            [[fallthrough]];
        case E_RELEASE_RESOURCE_MANAGER:
            if (!mGuiResource.Release())
                return false;
            Advance();
            [[fallthrough]];
        case E_RELEASE_DONE:
            mePrepareStage = E_PREPARE_START;
            return true;
        default:
            CGS_ASSERT(false, "Unknown release stage");
            return false;
        }
    }

    // Native Model table820D11D0+0x0C points at the shared822A93A0 thunk.
    void ModelModule::Destruct()
    {
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // ARTIST8285DE98. This phase deliberately creates no Resource input/output.
    void ModelModule::PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack, ModelIO::OutputBuffer* lpOutput)
    {
        EventInterpreterModuleIO::InputBuffer* lpInterpreterInput = 0;
        EventInterpreterModuleIO::OutputBuffer* lpInterpreterOutput = 0;
        lpInputBufferStack->CreateIOBuffer(&lpInterpreterInput, "Gui EventInterpreter");
        lpOutputBufferStack->CreateIOBuffer(&lpInterpreterOutput, "Gui EventInterpreter");
        mEventInterpreter.PreWorldUpdate(lpInterpreterInput, lpInterpreterOutput);
        const EventInterpreterModuleIO::OutputBuffer* lpInterpreterRead = lpInterpreterOutput;
        CgsModule::LockBuffersForIO(lpOutput, lpInterpreterRead);
        BridgeOutEventInterpreter(lpOutput, lpInterpreterRead);
        CgsModule::UnlockBuffersForIO(lpOutput, lpInterpreterRead);
        lpOutputBufferStack->DestroyIOBuffer(&lpInterpreterOutput);
        lpInputBufferStack->DestroyIOBuffer(&lpInterpreterInput);
    }

    // ARTIST8285F360. Resource remains alive across Interpreter->Resource and
    // both output bridges, then all four fresh buffers retire in reverse order.
    void ModelModule::Update(CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        ModelIO::InputBuffer* lpInput, ModelIO::OutputBuffer* lpOutput)
    {
        CgsDev::PerfMonCpu::StartMonitor(miUpdateInputPM);
        lpInput->LockForRead();
        GuiResourceModuleIO::InputBuffer* lpResourceInput = 0;
        GuiResourceModuleIO::OutputBuffer* lpResourceOutput = 0;
        EventInterpreterModuleIO::InputBuffer* lpInterpreterInput = 0;
        EventInterpreterModuleIO::OutputBuffer* lpInterpreterOutput = 0;
        lpInputBufferStack->CreateIOBuffer(&lpResourceInput, "Gui Resource");
        lpOutputBufferStack->CreateIOBuffer(&lpResourceOutput, "Gui Resource");
        lpInputBufferStack->CreateIOBuffer(&lpInterpreterInput, "Gui EventInterpreter");
        lpOutputBufferStack->CreateIOBuffer(&lpInterpreterOutput, "Gui EventInterpreter");

        CgsModule::LockBuffersForIO(lpInterpreterInput);
        BridgeInEventInterpreter(lpInterpreterInput, lpInput);
        CgsModule::UnlockBuffersForIO(lpInterpreterInput);
        CgsModule::LockBuffersForIO(lpResourceInput);
        BridgeInGuiResource(lpResourceInput, lpInput);
        CgsModule::UnlockBuffersForIO(lpResourceInput);
        lpInput->UnlockForRead();
        CgsDev::PerfMonCpu::StopMonitor(miUpdateInputPM);

        CgsDev::PerfMonCpu::StartMonitor(miUpdateMainPM);
        mEventInterpreter.Update(lpInterpreterInput, lpInterpreterOutput);
        const EventInterpreterModuleIO::OutputBuffer* lpInterpreterRead = lpInterpreterOutput;
        CgsModule::LockBuffersForIO(lpResourceInput, lpInterpreterRead);
        BridgeEventInterpreterToGuiResource(lpResourceInput, lpInterpreterRead);
        CgsModule::UnlockBuffersForIO(lpResourceInput, lpInterpreterRead);
        mGuiResource.Update(lpResourceInput, lpResourceOutput);
        CgsDev::PerfMonCpu::StopMonitor(miUpdateMainPM);

        CgsDev::PerfMonCpu::StartMonitor(miUpdateOutputPM);
        const GuiResourceModuleIO::OutputBuffer* lpResourceRead = lpResourceOutput;
        CgsModule::LockBuffersForIO(lpOutput, lpInterpreterRead, lpResourceRead);
        BridgeOutEventInterpreter(lpOutput, lpInterpreterRead);
        BridgeOutGuiResource(lpOutput, lpResourceRead);
        CgsModule::UnlockBuffersForIO(lpOutput, lpInterpreterRead, lpResourceRead);
        lpOutputBufferStack->DestroyIOBuffer(&lpInterpreterOutput);
        lpInputBufferStack->DestroyIOBuffer(&lpInterpreterInput);
        lpOutputBufferStack->DestroyIOBuffer(&lpResourceOutput);
        lpInputBufferStack->DestroyIOBuffer(&lpResourceInput);
        CgsDev::PerfMonCpu::StopMonitor(miUpdateOutputPM);
    }

    // ARTIST82852CF8..D4C. RegisterForEvent's result is not an added
    // Model success contract: the original public method is void.
    void ModelModule::AddGuiEventObserver(EventObserver* lpEventObserver, s32 liEventType)
    {
        if (!lpEventObserver)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventObserver != NULL",
                "..\\..\\..\\GameShared\\GameClasses\\Gui/Model/CgsModelModule.cpp", 518);
            CgsDev::Assert::EndAssert();
        }
        mEventInterpreter.RegisterForEvent(lpEventObserver, liEventType);
    }

    // ARTIST8285DF50 -> 8285B210. Used by both original directors.
    void ModelModule::AddGuiEvents(const ModelIO::InputBuffer::GuiEventQueue* lpEvents,
        ModelIO::InputBuffer* lpInput)
    {
        if (!lpInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Not locked for input";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Gui/Model/CgsModelModule.cpp", 373);
            CgsDev::Assert::EndAssert();
        }
        lpInput->AddGuiEvents(lpEvents);
    }

    // ARTIST8285DFF8 -> 8285B2C0. A missing handle asserts and continues;
    // there is no invented success result or null fallback.
    void ModelModule::AddGuiEvents(const ModelIO::InputBuffer::GuiEventInputQueue* lpEvents,
        ModelIO::InputBuffer* lpInput)
    {
        if (!lpInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Not locked for input";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Gui/Model/CgsModelModule.cpp", 392);
            CgsDev::Assert::EndAssert();
        }
        lpInput->AddGuiEvents(lpEvents);
    }

    // ARTIST8285E0A0: only a nonempty GUI queue reaches Interpreter.
    void ModelModule::BridgeInEventInterpreter(
        EventInterpreterModuleIO::InputBuffer* lpInterpreterInput,
        const ModelIO::InputBuffer* lpInput)
    {
        const ModelIO::InputBuffer::GuiEventInputQueue* lpInputEventQueue = lpInput->GetEventQueue();
        CGS_ASSERT(lpInputEventQueue != 0, "lpInputEventQueue != NULL");
        if (lpInputEventQueue->CgsModule::VariableEventQueue<32768, 16>::GetLength() > 0)
            lpInterpreterInput->AddGuiEvents(*lpInputEventQueue);
    }

    // Both forwarding seats are inlined in8285F360; const input accessors
    // preserve the read lock held by the original caller.
    void ModelModule::BridgeInGuiResource(GuiResourceModuleIO::InputBuffer* lpResourceInput,
        const ModelIO::InputBuffer* lpInput)
    {
        lpResourceInput->AddResourceRequests(lpInput->GetLoadRequests());
    }

    void ModelModule::BridgeEventInterpreterToGuiResource(
        GuiResourceModuleIO::InputBuffer* lpResourceInput,
        const EventInterpreterModuleIO::OutputBuffer* lpInterpreterOutput)
    {
        lpResourceInput->AddResourceRequests(lpInterpreterOutput->GetResourceEventQueue());
    }

    // ARTIST8285B148. GUI and View are independent channels; use their actual
    // reference-taking append APIs, with no pointer reinterpretation.
    void ModelModule::BridgeOutEventInterpreter(ModelIO::OutputBuffer* lpOutput,
        const EventInterpreterModuleIO::OutputBuffer* lpInterpreterOutput)
    {
        CGS_ASSERT(lpOutput != 0,
            "Invalid pointer to the output buffer in ModelModule::BridgeOutEventInterpreter");
        lpOutput->AddGuiOutEvents(*lpInterpreterOutput->GetOutEventQueue());
        lpOutput->AddViewOutEvents(*lpInterpreterOutput->GetViewEventQueue());
    }

    // ARTIST8285E120: resource requests are conditional; notifications always append.
    void ModelModule::BridgeOutGuiResource(ModelIO::OutputBuffer* lpOutput,
        const GuiResourceModuleIO::OutputBuffer* lpResourceOutput)
    {
        CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");
        CGS_ASSERT(lpResourceOutput->GetResourceRequestQueue() != 0,
            "lpResourceOutputBuffer->GetResourceRequestQueue() != NULL");
        if (lpResourceOutput->GetResourceRequestQueue()->GetLength() > 0)
            lpOutput->SetGuiResourceRequestQueue(lpResourceOutput->GetResourceRequestQueue());
        lpOutput->AddLoadNotifications(*lpResourceOutput->GetLoadNotifications());
    }
}
