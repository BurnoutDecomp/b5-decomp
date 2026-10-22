#include "GameShared/GameClasses/Gui/CgsGuiModule.h"
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace CgsGui
{
    // ARTIST8285B088; original diagnostic and assert-and-continue behavior.
    void GuiModule::BridgeFromInputToView(ViewIO::InputBuffer* lpViewInput,
        const CgsGuiModuleIO::InputBuffer* lpInput)
    {
        if (!lpInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lMessage;
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Invalid input queue in GuiModule::BridgeFromInputToView";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\gui\\CgsGuiModuleBridges.cpp", 42);
            CgsDev::Assert::EndAssert();
        }
        mpGuiView->AddViewState<32768, 16>(lpInput->GetGuiEvents(), lpViewInput);
    }

    // Inlined in ARTIST8286030C..350; DecFIGS B9F868 gives the helper name.
    void GuiModule::BridgeFromInputToModel(ModelIO::InputBuffer* lpModelInput,
        const CgsGuiModuleIO::InputBuffer* lpInput)
    {
        const auto* lpEventQueue = lpInput->GetGuiEvents();
        if (!lpEventQueue)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventQueue != NULL",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\gui\\CgsGuiModuleBridges.cpp", 60);
            CgsDev::Assert::EndAssert();
        }
        mGuiModel.AddGuiEvents(lpEventQueue, lpModelInput);
    }

    // Inlined in ARTIST82860474..490; DecFIGS BA2628 names the helper.
    void GuiModule::BridgeFromModelToView(ViewIO::InputBuffer* lpViewInput,
        const ModelIO::OutputBuffer* lpModelOutput)
    {
        mpGuiView->AddViewState<65536, 16>(lpModelOutput->GetViewOutEvents(), lpViewInput);
    }

    // ARTIST8285DD40. Both resource queues now use their canonical owner type.
    void GuiModule::BridgeFromModelToOutput(CgsGuiModuleIO::OutputBuffer* lpOutput,
        const ModelIO::OutputBuffer* lpModelOutput)
    {
        if (!lpOutput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpOutput != NULL",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\gui\\CgsGuiModuleBridges.cpp", 94);
            CgsDev::Assert::EndAssert();
        }
        if (!lpModelOutput->GetGuiResourceRequestQueue())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpModelOutput->GetGuiResourceRequestQueue()",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\gui\\CgsGuiModuleBridges.cpp", 96);
            CgsDev::Assert::EndAssert();
        }
        if (lpModelOutput->GetGuiResourceRequestQueue()->GetLength() > 0)
            lpOutput->SetGuiResourceRequestQueue(lpModelOutput->GetGuiResourceRequestQueue());
        lpOutput->AddGuiOutEvents(lpModelOutput->GetGuiOutEvents());
        mStoredGuiEvents.Append<4096, 16>(*lpModelOutput->GetLoadNotifications());
    }

    // ARTIST8285DE10..E90, recovered from the bounded image, not a one-feed
    // approximation of the larger PS3 body. The three destinations/order match.
    void GuiModule::BridgeFromViewToOutput(CgsGuiModuleIO::OutputBuffer* lpOutput,
        const ViewIO::OutputBuffer* lpViewOutput)
    {
        mStoredGuiEvents.Append<18432, 16>(*lpViewOutput->GetGuiEventQueue());
        lpOutput->AddGuiOutEvents(mpGuiView->GetCustomRendererManager()->GetOutputEventQueue());
        lpOutput->AddGuiOutEvents(&mpGuiView->GetOutputEventQueue());
    }

    // Inlined in ARTIST82860564..578; DecFIGS BA4208 names this bridge.
    void GuiModule::BridgeFromOutputToGui(const CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        mStoredGuiEvents.Append<18432, 16>(*lpOutput->GetOutEventQueue());
    }
}
