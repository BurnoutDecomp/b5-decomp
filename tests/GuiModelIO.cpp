// ARTIST model IO lifecycle on poisoned and reused native objects.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <new>

#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"

static unsigned suChecks = 0, suFailures = 0, suAsserts = 0;
static void Check(bool lbPass, const char* lpcMessage)
{
    ++suChecks;
    if (!lbPass) { ++suFailures; std::printf("FAIL %s\n", lpcMessage); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++suAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }

#include "gui_model_io.inc"

using Input = CgsGui::ModelIO::InputBuffer;
using Output = CgsGui::ModelIO::OutputBuffer;

template<class Queue>
static bool CheckEmpty(Queue& lrQueue)
{
    unsigned char luConstructed = 0;
    std::memcpy(&luConstructed, &lrQueue.mbIsConstructed, 1);
    const s32 liExpected = 16 - static_cast<s32>(
        reinterpret_cast<uintptr_t>(lrQueue.macData) & 15);
    const bool lbValid = luConstructed == 1
        && lrQueue.miFirstEventOffset == liExpected
        && lrQueue.miBufferWritePos == liExpected && lrQueue.miLength == 0;
    Check(lbValid, "queue is constructed, empty and aligned for its actual address");
    return lbValid;
}

template<class Queue>
static void CheckPoison(Queue& lrQueue)
{
    bool lbPreserved = true;
    for (char lcByte : lrQueue.macData)
        lbPreserved = lbPreserved && static_cast<unsigned char>(lcByte) == 0xA5;
    Check(lbPreserved, "Construct leaves queue payload bytes untouched");
}

template<class Queue>
static void Fill(Queue& lrQueue, s32 liId)
{
    const u32 lauPayload[4] = {0xDEADBEEF, 0x12345678, 0x87654321, 0xFEEDCAFE};
    Check(lrQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(lauPayload),
                          liId, sizeof(lauPayload)), "constructed queue accepts a record");
}

template<class Queue>
static void CheckPayload(Queue& lrActual, const Queue& lrBefore)
{
    Check(std::memcmp(lrActual.macData, lrBefore.macData, sizeof(lrActual.macData)) == 0,
          "Clear and Destruct retain queue payload bytes");
}

static void ExerciseInput(Input& lrInput)
{
    lrInput.Construct();
    Check(lrInput.mxStatusFlags.GetFlags() == 1, "input Construct resets only the base status");
    const bool lbEvents = CheckEmpty(lrInput.mGuiEvents);
    const bool lbRequests = CheckEmpty(lrInput.mLoadRequests);
    CheckPoison(lrInput.mGuiEvents);
    CheckPoison(lrInput.mLoadRequests);
    Check(lrInput.maStatusPad[0] == 0xA5 && lrInput.maStatusPad[1] == 0xA5
          && lrInput.maStatusPad[2] == 0xA5, "input Construct preserves status padding");
    if (!lbEvents || !lbRequests) return;

    for (unsigned luPass = 0; luPass < 2; ++luPass)
    {
        lrInput.LockForWrite();
        Fill(lrInput.mGuiEvents, 64);
        Fill(*lrInput.GetLoadRequests(), 39);
        lrInput.UnlockForWrite();
        const Input lBefore = lrInput;
        lrInput.Destruct();
        Check(lrInput.mxStatusFlags.GetFlags() == 0, "input Destruct clears the base status");
        CheckEmpty(lrInput.mGuiEvents);
        CheckEmpty(lrInput.mLoadRequests);
        CheckPayload(lrInput.mGuiEvents, lBefore.mGuiEvents);
        CheckPayload(lrInput.mLoadRequests, lBefore.mLoadRequests);
        lrInput.Construct();
        CheckEmpty(lrInput.mGuiEvents);
        CheckEmpty(lrInput.mLoadRequests);
    }
    lrInput.Destruct();
}

static void FillOutput(Output& lrOutput)
{
    lrOutput.LockForWrite();
    Fill(lrOutput.mLoadNotifications, 14);
    Fill(lrOutput.mResourceRequestQueue, 4);
    Fill(lrOutput.mGuiOutEvents, 508);
    Fill(lrOutput.mViewOutEvents, 26);
    lrOutput.UnlockForWrite();
}

static void CheckOutput(Output& lrOutput, const Output& lrBefore)
{
    CheckEmpty(lrOutput.mLoadNotifications);
    CheckEmpty(lrOutput.mResourceRequestQueue);
    CheckEmpty(lrOutput.mGuiOutEvents);
    CheckEmpty(lrOutput.mViewOutEvents);
    CheckPayload(lrOutput.mLoadNotifications, lrBefore.mLoadNotifications);
    CheckPayload(lrOutput.mResourceRequestQueue, lrBefore.mResourceRequestQueue);
    CheckPayload(lrOutput.mGuiOutEvents, lrBefore.mGuiOutEvents);
    CheckPayload(lrOutput.mViewOutEvents, lrBefore.mViewOutEvents);
}

static void ExerciseOutput(Output& lrOutput)
{
    lrOutput.Construct();
    Check(lrOutput.mxStatusFlags.GetFlags() == 1, "output Construct resets only the base status");
    const bool lbLoad = CheckEmpty(lrOutput.mLoadNotifications);
    const bool lbResource = CheckEmpty(lrOutput.mResourceRequestQueue);
    const bool lbGui = CheckEmpty(lrOutput.mGuiOutEvents);
    const bool lbView = CheckEmpty(lrOutput.mViewOutEvents);
    CheckPoison(lrOutput.mLoadNotifications);
    CheckPoison(lrOutput.mResourceRequestQueue);
    CheckPoison(lrOutput.mGuiOutEvents);
    CheckPoison(lrOutput.mViewOutEvents);
    Check(lrOutput.maStatusPad[0] == 0xA5 && lrOutput.maStatusPad[1] == 0xA5
          && lrOutput.maStatusPad[2] == 0xA5, "output Construct preserves status padding");
    if (!lbLoad || !lbResource || !lbGui || !lbView) return;

    for (unsigned luPass = 0; luPass < 3; ++luPass)
    {
        FillOutput(lrOutput);
        const Output lBefore = lrOutput;
        if (luPass == 1) lrOutput.LockForRead();
        if (luPass == 2) lrOutput.LockForWrite();
        const s8 liStatus = lrOutput.mxStatusFlags.GetFlags();
        lrOutput.Clear();
        Check(lrOutput.mxStatusFlags.GetFlags() == liStatus,
              "Clear preserves unlocked, read-locked and write-locked IO status");
        CheckOutput(lrOutput, lBefore);
        if (luPass == 1) lrOutput.UnlockForRead();
        if (luPass == 2) lrOutput.UnlockForWrite();
    }
    FillOutput(lrOutput);
    const Output lBefore = lrOutput;
    lrOutput.Destruct();
    Check(lrOutput.mxStatusFlags.GetFlags() == 0, "output Destruct clears the base status");
    CheckOutput(lrOutput, lBefore);
    lrOutput.Construct();
    CheckOutput(lrOutput, lBefore);
    lrOutput.Destruct();
}

int main()
{
    alignas(16) static unsigned char lauInput[sizeof(Input) + 16];
    alignas(16) static unsigned char lauOutput[sizeof(Output) + 16];
    for (unsigned luOffset = 0; luOffset < 16; luOffset += 4)
    {
        std::memset(lauInput, 0xA5, sizeof(lauInput));
        std::memset(lauOutput, 0xA5, sizeof(lauOutput));
        Input* lpInput = new (lauInput + luOffset) Input;
        Output* lpOutput = new (lauOutput + luOffset) Output;
        ExerciseInput(*lpInput);
        ExerciseOutput(*lpOutput);
    }
    Check(suAsserts == 0, "poison, lock-preserving Clear and reuse produce no assertions");
    std::printf("GuiModelIO: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
