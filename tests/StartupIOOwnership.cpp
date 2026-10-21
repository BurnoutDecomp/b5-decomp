// Hosted regression for the real InputIO lifecycle/queue/stack bodies and the
// selected PC GUI ownership wrapper/calibration call. The full GUI core, VM,
// calibration resource behavior and GameData payload are explicit boundaries.
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"

static int gChecks = 0;
static int gFailures = 0;
static int gAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char* expression, const char*, int)
    {
        ++gAsserts;
        std::printf("ASSERT %s\n", expression);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Message { u32 gxMessageFilterFlags = 0; }
namespace Log
{
    // Only the allocator's out-of-space diagnostic is outside the fixture.
    struct DiagnosticBoundary
    {
        template <class T> DiagnosticBoundary& operator<<(const T&) { return *this; }
    };
    DiagnosticBoundary* gpDebugPrint = nullptr;
}
}

namespace BrnResource { namespace GameDataIO
{
    // Opaque payload boundaries. Their actual IOBuffer lock state machine is
    // production code; no GameData request or resource protocol is simulated.
    struct InputBuffer : CgsModule::IOBuffer {};
    struct OutputBuffer : CgsModule::IOBuffer {};
} }

static BrnResource::GameDataIO::InputBuffer gScriptedInput;
static BrnResource::GameDataIO::OutputBuffer gScriptedOutput;
static int gInputGets = 0;
static int gOutputGets = 0;

namespace BrnGameMainFlowController
{
    BrnResource::GameDataIO::InputBuffer* GetScriptedLoadGameDataInput()
    {
        ++gInputGets;
        return &gScriptedInput;
    }
    BrnResource::GameDataIO::OutputBuffer* GetScriptedLoadGameDataOutput()
    {
        ++gOutputGets;
        return &gScriptedOutput;
    }
}

namespace BrnGui
{
    struct CalibrationBoundary
    {
        int calls = 0;
        BrnResource::GameDataIO::InputBuffer* input = nullptr;
        const BrnResource::GameDataIO::OutputBuffer* output = nullptr;
        void* events = nullptr;
        void* allocator = nullptr;
        bool inputWasWriting = false;
        bool outputWasReading = false;

        void Update(BrnResource::GameDataIO::InputBuffer* lpInput,
                    const BrnResource::GameDataIO::OutputBuffer* lpOutput,
                    void* lpEvents, void* lpAllocator)
        {
            ++calls;
            input = lpInput;
            output = lpOutput;
            events = lpEvents;
            allocator = lpAllocator;
            inputWasWriting = lpInput->IsBufferLockedForWriting();
            outputWasReading = lpOutput->IsBufferLockedForReading();
        }
    };

    // The two ownership methods below are extracted from production. This
    // fixture declaration is not the canonical GuiModule ABI. The large GUI
    // core overload is a recording boundary that reaches the real extracted
    // calibration-call method; source wiring checks cover its production seat.
    struct GuiModule
    {
        CalibrationBoundary mColourCalibrationScreen;
        int mGuiOutQueue = 0;
        struct { void* mpTextureAllocator = nullptr; } mGuiConfig;
        int coreCalls = 0;
        void Update();
        void Update(BrnResource::GameDataIO::InputBuffer* lpInput,
                    const BrnResource::GameDataIO::OutputBuffer* lpOutput)
        {
            ++coreCalls;
            UpdateCalibrationScreen(lpInput, lpOutput);
        }
        void UpdateCalibrationScreen(BrnResource::GameDataIO::InputBuffer* lpInput,
                                     const BrnResource::GameDataIO::OutputBuffer* lpOutput);
    };
}

#include "startup_io_ownership.inc"

static void Check(bool passed, const char* label)
{
    ++gChecks;
    if (!passed)
    {
        ++gFailures;
        std::printf("FAIL %s\n", label);
    }
}

template <class T> static bool SameRepresentation(const T& actual, const T& expected)
{
    unsigned char actualBytes[sizeof(T)];
    unsigned char expectedBytes[sizeof(T)];
    std::memcpy(actualBytes, &actual, sizeof(T));
    std::memcpy(expectedBytes, &expected, sizeof(T));
    return std::memcmp(actualBytes, expectedBytes, sizeof(T)) == 0;
}

template <class T, s32 N>
static bool OwnQueue(const CgsModule::EventQueue<T, N>& queue)
{
    T* expected = const_cast<T*>(&queue.maEvents[0]);
    return SameRepresentation(queue.mpEvents, expected);
}

template <class T, s32 N>
static bool CheckQueue(const CgsModule::EventQueue<T, N>& queue, const char* name)
{
    char label[128];
    std::snprintf(label, sizeof(label), "postworld %s queue owns inline backing", name);
    Check(OwnQueue(queue), label);
    const s32 capacity = N;
    const s32 empty = 0;
    std::snprintf(label, sizeof(label), "postworld %s queue has original capacity", name);
    Check(SameRepresentation(queue.miMaxLength, capacity), label);
    std::snprintf(label, sizeof(label), "postworld %s queue starts empty", name);
    Check(SameRepresentation(queue.miLength, empty), label);
    return OwnQueue(queue) && SameRepresentation(queue.miMaxLength, capacity) &&
           SameRepresentation(queue.miLength, empty);
}

static bool Idle(const CgsModule::IOBuffer& buffer)
{
    return !buffer.IsBufferLockedForReading() && !buffer.IsBufferLockedForWriting();
}

int main()
{
    using CgsInput::InputIO::PostWorldInputBuffer;
    alignas(16) unsigned char backing[sizeof(PostWorldInputBuffer) + 64];
    std::memset(backing, 0xA5, sizeof(backing));
    CgsModule::IOBufferStack stack;
    stack.Construct("startup-ownership");
    Check(stack.Prepare(backing, sizeof(backing), 16), "input stack prepared");
    PostWorldInputBuffer* saved = nullptr;
    const f32 zero = 0.0f;
    {
        // Real IOHelper -> CreateIOBuffer<T> -> derived Construct, no fixture
        // initializer and no blanket zero-fill. Read bad initial pointers only
        // as bytes, and never post through them in the negative control.
        CgsModule::IOHelper<PostWorldInputBuffer> helper(&stack, "InputPostWorld");
        PostWorldInputBuffer* input = helper;
        saved = input;
        Check(input != nullptr, "postworld helper owns a buffer");
        Check(stack.muNumAllocated == 1 && stack.muAllocated >= sizeof(PostWorldInputBuffer),
              "postworld stack allocation is live");
        Check(input->mxStatusFlags.IsBitSet(CgsModule::IOBuffer::eStatusConstructed), "postworld base constructed");
        Check(!input->IsBufferLockedForReading(), "postworld starts without read lock");
        Check(!input->IsBufferLockedForWriting(), "postworld starts without write lock");
        bool initialized = CheckQueue(input->mBindRequestQueue, "bind");
        initialized = CheckQueue(input->mUnBindRequestQueue, "unbind") && initialized;
        initialized = CheckQueue(input->mPadMappingQueue, "mapping") && initialized;
        Check(SameRepresentation(input->mWheelFFSpring.mfSpringCoefficient, zero) &&
              SameRepresentation(input->mWheelFFSpring.mfSpringSaturation, zero), "postworld wheel spring starts zero");

        if (initialized)
        {
            CgsInput::InputIO::ActionMapping mapping[CgsInput::InputIO::PadMapping::KI_NUM_ACTION_MAPPINGS];
            for (s32 index = 0; index < CgsInput::InputIO::PadMapping::KI_NUM_ACTION_MAPPINGS; ++index)
                for (s32 slot = 0; slot < 4; ++slot)
                    mapping[index].maiGameIds[slot] = static_cast<s8>(index * 4 + slot - 56);
            CgsInput::Device::WheelFFSpring spring = { 3.0f, -2.0f };
            input->LockForWrite();
            Check(input->IsBufferLockedForWriting(), "postworld writer lock is live");
            input->PostBindRequest(3, 2);
            input->PostUnbindRequest(4);
            input->PostMappingRequest(mapping, -1);
            input->SetWheelFFSpring(spring);
            input->UnlockForWrite();
            Check(Idle(*input), "postworld writer releases lock");
            input->LockForRead();
            const PostWorldInputBuffer* read = input;
            Check(read->GetBindRequestQueue() == &input->mBindRequestQueue &&
                  read->GetUnBindRequestQueue() == &input->mUnBindRequestQueue &&
                  read->GetPadMappingQueue() == &input->mPadMappingQueue, "postworld accessors return owned queues");
            Check(read->GetBindRequestQueue()->GetLength() == 1 && read->GetUnBindRequestQueue()->GetLength() == 1 &&
                  read->GetPadMappingQueue()->GetLength() == 1, "postworld records queued once");
            const auto& bind = read->GetBindRequestQueue()->GetEvent(0);
            const auto& unbind = read->GetUnBindRequestQueue()->GetEvent(0);
            const auto& mapped = read->GetPadMappingQueue()->GetEvent(0);
            Check(bind.miPlayer == 3 && bind.miPort == 2, "postworld bind record preserved");
            Check(unbind.miPlayer == 4 && unbind.miPort == 0, "postworld unbind record preserved");
            Check(mapped.miPortId == -1, "postworld all-port mapping preserved");
            Check(std::memcmp(mapped.maMapping, mapping, sizeof(mapping)) == 0, "postworld complete mapping payload copied");
            Check(read->GetWheelFFSpring()->mfSpringCoefficient == 3.0f &&
                  read->GetWheelFFSpring()->mfSpringSaturation == -2.0f, "postworld wheel spring copied");
            input->UnlockForRead();
            Check(Idle(*input), "postworld reader releases lock");
        }
        else
        {
            for (int index = 0; index < 10; ++index)
                Check(false, "postworld posting prerequisite: owned initialized queues");
        }
    }
    Check(saved->mPadMappingQueue.GetLength() == 0, "postworld destruct empties mapping queue");
    Check(saved->mUnBindRequestQueue.GetLength() == 0, "postworld destruct empties unbind queue");
    Check(saved->mBindRequestQueue.GetLength() == 0, "postworld destruct empties bind queue");
    Check(saved->mxStatusFlags.IsZero(), "postworld destruct retires base status");
    Check(stack.muAllocated == 0 && stack.muNumAllocated == 0, "postworld helper restores stack ownership");
    {
        CgsModule::IOHelper<PostWorldInputBuffer> helper(&stack, "InputPostWorldAgain");
        PostWorldInputBuffer* reused = helper;
        Check(reused == saved, "postworld stack reuses retired backing");
        const s32 empty = 0;
        Check(OwnQueue(reused->mBindRequestQueue) && OwnQueue(reused->mUnBindRequestQueue) && OwnQueue(reused->mPadMappingQueue) &&
              SameRepresentation(reused->mBindRequestQueue.miLength, empty) &&
              SameRepresentation(reused->mUnBindRequestQueue.miLength, empty) &&
              SameRepresentation(reused->mPadMappingQueue.miLength, empty), "postworld reused queues are rebound and empty");
        Check(reused->mxStatusFlags.IsBitSet(CgsModule::IOBuffer::eStatusConstructed) && Idle(*reused), "postworld reused base is constructed and idle");
        Check(SameRepresentation(reused->mWheelFFSpring.mfSpringCoefficient, zero) &&
              SameRepresentation(reused->mWheelFFSpring.mfSpringSaturation, zero), "postworld reused spring reset");
    }

    gScriptedInput.Construct();
    gScriptedOutput.Construct();
    Check(Idle(gScriptedInput), "normal GUI input starts idle");
    Check(Idle(gScriptedOutput), "normal GUI output starts idle");
    BrnGui::GuiModule gui;
    int allocatorIdentity = 0;
    gui.mGuiConfig.mpTextureAllocator = &allocatorIdentity;
    int assertionsBefore = gAsserts;
    gui.Update();
    Check(gInputGets == 1, "normal GUI obtains scripted input once");
    Check(gOutputGets == 1, "normal GUI obtains scripted output once");
    Check(gui.coreCalls == 1 && gui.mColourCalibrationScreen.calls == 1, "normal GUI forwards one core calibration call");
    Check(gui.mColourCalibrationScreen.inputWasWriting, "normal GUI owns input write lock during calibration");
    Check(gui.mColourCalibrationScreen.outputWasReading, "normal GUI owns output read lock during calibration");
    Check(gui.mColourCalibrationScreen.input == &gScriptedInput && gui.mColourCalibrationScreen.output == &gScriptedOutput,
          "normal GUI forwards its actual GameData pair");
    Check(gui.mColourCalibrationScreen.events == &gui.mGuiOutQueue && gui.mColourCalibrationScreen.allocator == &allocatorIdentity,
          "normal GUI forwards output sink and allocator");
    Check(Idle(gScriptedInput), "normal GUI releases its input lock");
    Check(Idle(gScriptedOutput), "normal GUI releases its output lock");
    Check(gAsserts == assertionsBefore, "normal GUI has no lock assertion");

    for (int route = 0; route < 2; ++route)
    {
        BrnResource::GameDataIO::InputBuffer callerInput;
        BrnResource::GameDataIO::OutputBuffer callerOutput;
        callerInput.Construct();
        callerOutput.Construct();
        callerInput.LockForWrite();
        callerOutput.LockForRead();
        char label[128];
        const char* name = route == 0 ? "initial-loading" : "scripted-loading";
        std::snprintf(label, sizeof(label), "%s caller acquires GameData locks", name);
        Check(callerInput.IsBufferLockedForWriting() && callerOutput.IsBufferLockedForReading(), label);
        const int callsBefore = gui.mColourCalibrationScreen.calls;
        assertionsBefore = gAsserts;
        gui.Update(&callerInput, &callerOutput);
        std::snprintf(label, sizeof(label), "%s caller reaches calibration once", name);
        Check(gui.mColourCalibrationScreen.calls == callsBefore + 1, label);
        std::snprintf(label, sizeof(label), "%s caller pair is borrowed by identity", name);
        Check(gui.mColourCalibrationScreen.input == &callerInput && gui.mColourCalibrationScreen.output == &callerOutput, label);
        std::snprintf(label, sizeof(label), "%s calibration sees caller locks", name);
        Check(gui.mColourCalibrationScreen.inputWasWriting && gui.mColourCalibrationScreen.outputWasReading, label);
        std::snprintf(label, sizeof(label), "%s caller retains GameData locks", name);
        Check(callerInput.IsBufferLockedForWriting() && callerOutput.IsBufferLockedForReading(), label);
        std::snprintf(label, sizeof(label), "%s borrowed pair has no lock assertion", name);
        Check(gAsserts == assertionsBefore, label);
        callerOutput.UnlockForRead();
        callerInput.UnlockForWrite();
        std::snprintf(label, sizeof(label), "%s owner releases GameData locks", name);
        Check(Idle(callerInput) && Idle(callerOutput), label);
    }
    Check(gAsserts == 0, "all exercised owners preserve assertions");
    std::printf("STARTUP_IO_OWNERSHIP: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
