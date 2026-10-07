// ARTIST event-interpreter IO: real capacities, unusual mutable read handle,
// and the deliberately different InputBuffer/OutputBuffer destruction.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <new>

#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO.h"

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
} namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }

#include "gui_interpreter_io.inc"

using Input = CgsGui::EventInterpreterModuleIO::InputBuffer;
using Output = CgsGui::EventInterpreterModuleIO::OutputBuffer;

template<class Queue>
static bool CheckEmpty(Queue& lrQueue)
{
    unsigned char luConstructed = 0;
    std::memcpy(&luConstructed, &lrQueue.mbIsConstructed, 1);
    const s32 liExpected = 16 - static_cast<s32>(
        reinterpret_cast<uintptr_t>(lrQueue.macData) & 15);
    const bool lbValid = luConstructed == 1 && lrQueue.miLength == 0
        && lrQueue.miFirstEventOffset == liExpected
        && lrQueue.miBufferWritePos == liExpected;
    Check(lbValid, "Construct initializes actual queue alignment and empty state");
    return lbValid;
}

template<class Queue>
static void CheckPoison(Queue& lrQueue)
{
    bool lbPreserved = true;
    for (char lcByte : lrQueue.macData)
        lbPreserved = lbPreserved && static_cast<unsigned char>(lcByte) == 0xA5;
    Check(lbPreserved, "Construct preserves the poisoned payload bytes");
}

template<class Queue>
static void Fill(Queue& lrQueue, s32 liId)
{
    const u32 lauPayload[4] = {0xDEADBEEF, 0x12345678, 0x87654321, 0xFEEDCAFE};
    Check(lrQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(lauPayload),
                          liId, sizeof(lauPayload)), "real queue accepts a small record");
}

static void ExerciseInput(Input& lrInput)
{
    lrInput.Construct();
    Check(lrInput.mxStatusFlags.GetFlags() == 1, "input Construct sets original base status");
    const bool lbInitialized = CheckEmpty(lrInput.mGuiEvents);
    CheckPoison(lrInput.mGuiEvents);
    if (!lbInitialized) return;

    for (unsigned luPass = 0; luPass < 2; ++luPass)
    {
        Input::GuiEventInputQueue lSource;
        lSource.Construct();
        Fill(lSource, 64);
        lrInput.LockForWrite();
        Check(lrInput.AddGuiEvents(lSource) != 0, "input publisher appends the real source queue");
        lrInput.UnlockForWrite();

        lrInput.LockForRead();
        Input::GuiEventInputQueue* lpReadHandle = lrInput.GetEventQueue();
        Check(lpReadHandle == &lrInput.mGuiEvents, "read-locked mutable handle is the actual input queue");
        CgsModule::VariableEventQueue<18432, 16> lInternal;
        lInternal.Construct();
        Fill(lInternal, 42);
        lpReadHandle->Append(lInternal);
        Check(lpReadHandle->GetLength() == 2, "read handle accepts the interpreter's internal-event append");
        lrInput.UnlockForRead();
        const Input lBefore = lrInput;
        lrInput.Destruct();
        Check(lrInput.mxStatusFlags.GetFlags() == 0, "input Destruct clears only the base status");
        Check(std::memcmp(&lrInput.mGuiEvents, &lBefore.mGuiEvents, sizeof(lrInput.mGuiEvents)) == 0,
              "input Destruct leaves its complete populated queue unchanged");
        lrInput.Construct();
        CheckEmpty(lrInput.mGuiEvents);
    }

    lrInput.LockForWrite();
    const unsigned luBefore = suAsserts;
    Check(lrInput.GetEventQueue() == &lrInput.mGuiEvents, "wrong-lock accessor still returns the original handle");
    Check(suAsserts == luBefore + 1, "mutable input accessor diagnoses a write lock instead of a read lock");
    lrInput.UnlockForWrite();
    lrInput.Destruct();
}

static void ExerciseOutput(Output& lrOutput)
{
    lrOutput.Construct();
    Check(lrOutput.mxStatusFlags.GetFlags() == 1, "output Construct sets original base status");
    const bool lbEvents = CheckEmpty(lrOutput.mGuiOutEvents);
    const bool lbResources = CheckEmpty(lrOutput.mGuiOutResource);
    const bool lbView = CheckEmpty(lrOutput.mGuiOutView);
    CheckPoison(lrOutput.mGuiOutEvents);
    CheckPoison(lrOutput.mGuiOutResource);
    CheckPoison(lrOutput.mGuiOutView);
    Check(lrOutput.mGuiOutEvents.GetMaxLength() == 18432, "ARTIST output event capacity is 18432 bytes");
    if (!lbEvents || !lbResources || !lbView) return;

    lrOutput.LockForWrite();
    Output::GuiEventQueue* lpEvents = lrOutput.GetOutEventQueue();
    Output::GuiEventQueueSmall* lpResources = lrOutput.GetResourceEventQueue();
    Output::GuiEventQueueLarge* lpView = lrOutput.GetViewEventQueue();
    Check(lpEvents == &lrOutput.mGuiOutEvents && lpResources == &lrOutput.mGuiOutResource
          && lpView == &lrOutput.mGuiOutView, "all three write accessors return their actual named queues");
    static const unsigned char lauLargePayload[17000] = {0x5A, 0xC3};
    Check(lpEvents->AddEventSafe(reinterpret_cast<const CgsModule::Event*>(lauLargePayload),
                                599, sizeof(lauLargePayload)),
          "event record beyond the old 16384-byte capacity fits the original queue");
    Fill(*lpResources, 39);
    Fill(*lpView, 26);
    lrOutput.UnlockForWrite();

    lrOutput.LockForRead();
    const Output& lrConst = lrOutput;
    Check(lrConst.GetOutEventQueue() == lpEvents && lrConst.GetResourceEventQueue() == lpResources
          && lrConst.GetViewEventQueue() == lpView, "const read accessors return the same three queues");
    lrOutput.UnlockForRead();
    const Output lBefore = lrOutput;
    lrOutput.Destruct();
    Check(lrOutput.mxStatusFlags.GetFlags() == 0, "output Destruct clears the base status");
    CheckEmpty(lrOutput.mGuiOutEvents);
    CheckEmpty(lrOutput.mGuiOutResource);
    CheckEmpty(lrOutput.mGuiOutView);
    Check(std::memcmp(lrOutput.mGuiOutEvents.macData, lBefore.mGuiOutEvents.macData,
                      sizeof(lrOutput.mGuiOutEvents.macData)) == 0
          && std::memcmp(lrOutput.mGuiOutResource.macData, lBefore.mGuiOutResource.macData,
                         sizeof(lrOutput.mGuiOutResource.macData)) == 0
          && std::memcmp(lrOutput.mGuiOutView.macData, lBefore.mGuiOutView.macData,
                         sizeof(lrOutput.mGuiOutView.macData)) == 0,
          "output queue destruction retains payload bytes while emptying all channels");
    lrOutput.Construct();
    CheckEmpty(lrOutput.mGuiOutEvents);
    CheckEmpty(lrOutput.mGuiOutResource);
    CheckEmpty(lrOutput.mGuiOutView);
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
    Check(suAsserts == 4, "only the four deliberate wrong-lock calls assert");
    std::printf("GuiInterpreterIO: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
