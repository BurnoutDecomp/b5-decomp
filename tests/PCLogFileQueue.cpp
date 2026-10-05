#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include "GameShared/GameClasses/Development/Log/CgsLogFileQueuePC.h"

static int siChecks, siFailures;
static std::string sOutput;
static HANDLE shEntered, shRelease;
static std::atomic<bool> sbBlock{false}, sbFail{false};
static void Check(bool lbValue, const char* lpcName)
{ ++siChecks; if (!lbValue) { ++siFailures; std::printf("FAIL %s\n", lpcName); } }
static bool Sink(const char* lpcText, size_t luBytes)
{
    if (sbBlock.exchange(false))
    {
        SetEvent(shEntered);
        WaitForSingleObject(shRelease, 5000);
    }
    sOutput.append(lpcText, luBytes);
    return !sbFail.exchange(false);
}
int main()
{
    using CgsDev::Log::LogFileQueuePC;
    shEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    shRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    {
        LogFileQueuePC<256, 64> lQueue;
        Check(!lQueue.Start(nullptr), "invalid sink does not start a worker");
        Check(lQueue.Start(Sink), "native worker starts");
        Check(!lQueue.Start(Sink), "running writer cannot be started twice");
        sbBlock = true;
        Check(lQueue.TryWrite("hold\n", 5), "initial record accepted");
        Check(WaitForSingleObject(shEntered, 2000) == WAIT_OBJECT_0, "file sink deliberately blocked");
        HANDLE lhDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        bool lbAccepted = false, lbOverflow = false;
        const std::string lPayload(256, 'a');
        std::thread lProducer([&] {
            lbAccepted = lQueue.TryWrite(lPayload.data(), lPayload.size());
            lbOverflow = !lQueue.TryWrite("lost\n", 5);
            SetEvent(lhDone);
        });
        Check(WaitForSingleObject(lhDone, 200) == WAIT_OBJECT_0,
              "producer and full-queue rejection finish while file IO remains blocked");
        SetEvent(shRelease);
        lProducer.join();
        Check(lbAccepted && lbOverflow, "full queue rejects a complete write without replacing retained bytes");
        Check(lQueue.Flush(), "flush waits for actual sink completion");
        Check(sOutput == "hold\n" + lPayload, "queued bytes drain in order without loss or duplication");
        lQueue.Stop();
        const auto lStats = lQueue.GetStatistics();
        Check(lStats.muDroppedWrites == 1 && lStats.muDroppedBytes == 5,
              "overflow loss is counted exactly");
        Check(sOutput.find("dropped 1 queued writes (5 bytes)") != std::string::npos,
              "shutdown reports overflow in the log");
        Check(!lQueue.TryWrite("late", 4) && !lQueue.Start(Sink), "stopped queue cannot accept writes or restart");
        lQueue.Stop();
        CloseHandle(lhDone);
    }
    {
        sOutput.clear();
        LogFileQueuePC<97, 31> lQueue;
        Check(lQueue.Start(Sink), "independent queue starts");
        std::string lExpected;
        bool lbAllAccepted = true;
        for (unsigned lu = 0; lu < 200; ++lu)
        {
            const std::string lPart(7 + lu % 83, static_cast<char>('a' + lu % 26));
            lExpected += lPart;
            lbAllAccepted &= lQueue.TryWrite(lPart.data(), lPart.size());
            lbAllAccepted &= lQueue.Flush();
        }
        Check(lbAllAccepted && sOutput == lExpected,
              "variable writes wrap both ring boundaries and preserve every byte");
        sbFail = true;
        lQueue.TryWrite("failure", 7);
        lQueue.Stop();
        Check(lQueue.GetStatistics().muWriteErrors == 1, "sink failure is reported without wedging shutdown");
        Check(sOutput == lExpected + "failure", "stop drains the last pending write");
    }
    CloseHandle(shEntered); CloseHandle(shRelease);
    std::printf("PCLogFileQueue: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
