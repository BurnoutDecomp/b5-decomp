#include "types.hpp"
#include "GameSource/Network/Messages/BrnUpdateMessage.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdio>
#include <string>

namespace
{
    unsigned gChecks = 0;
    unsigned gFailures = 0;
    unsigned gLogWrites = 0;
    std::string gLogText;

    void Check(bool lbCondition, const char* lpcLabel)
    {
        ++gChecks;
        if (!lbCondition)
        {
            ++gFailures;
            std::printf("FAIL %s\n", lpcLabel);
        }
    }
}

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    void WriteToLog(const char* lpcText)
    {
        ++gLogWrites;
        gLogText += lpcText ? lpcText : "<NULLSTRING>";
    }
}
}

CgsDev::Log::LogChannelOutput CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput;

#include "NetworkUpdateDiagnostics.inc"

int main()
{
    BrnNetwork::UpdateData lSendData{};
    lSendData.mu16FramesSinceStart = 321;
    lSendData.mMatrix.wAxis.x = 1.25f;
    lSendData.mMatrix.wAxis.y = -2.5f;
    lSendData.mMatrix.wAxis.z = 3.75f;

    FakeNetworkModule lSendDisabled{false, false};
    SendDiagnosticContext lSendOff{&lSendDisabled, 17, lSendData};
    lSendOff.Run();
    Check(lSendDisabled.miSendGetterCalls == 1, "send flag read once when disabled");
    Check(gLogText.empty() && gLogWrites == 0, "disabled send diagnostic emits nothing");

    FakeNetworkModule lSendEnabled{true, false};
    SendDiagnosticContext lSendOn{&lSendEnabled, 17, lSendData};
    lSendOn.Run();
    Check(lSendEnabled.miSendGetterCalls == 1, "send flag read once when enabled");
    Check(gLogText == "SEND MESSAGE TO #17 POS=((1.250000, -2.500000, 3.750000)) FRAMES SINCE START=321\n",
          "enabled send diagnostic matches the ARTIST outer/vector format");

    gLogText.clear();
    gLogWrites = 0;

    BrnNetwork::UpdateData lRecvData{};
    lRecvData.mu16FramesSinceStart = 654;
    lRecvData.mMatrix.wAxis.x = -4.5f;
    lRecvData.mMatrix.wAxis.y = 0.125f;
    lRecvData.mMatrix.wAxis.z = 8.0f;

    FakeNetworkModule lRecvDisabled{false, false};
    ReceiveDiagnosticContext lRecvOff{&lRecvDisabled, 29, &lRecvData};
    lRecvOff.Run();
    Check(lRecvDisabled.miRecvGetterCalls == 1, "receive flag read once when disabled");
    Check(gLogText.empty() && gLogWrites == 0, "disabled receive diagnostic emits nothing");

    FakeNetworkModule lRecvEnabled{false, true};
    ReceiveDiagnosticContext lRecvOn{&lRecvEnabled, 29, &lRecvData};
    lRecvOn.Run();
    Check(lRecvEnabled.miRecvGetterCalls == 1, "receive flag read once when enabled");
    Check(gLogText == "RECV UPDATE for #29 POS=((-4.500000, 0.125000, 8.000000)) FRAMES SINCE START=654\n",
          "enabled receive diagnostic matches the ARTIST outer/vector format");

    std::printf("NetworkUpdateDiagnostics: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
