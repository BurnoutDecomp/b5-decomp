// Production GS GUI/scoring initialization blocks and accessors. This fixture
// isolates these prerequisites; other GameState interfaces are not initialized.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"

static unsigned checks, failures, assertions;
static void Check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }
#include "game_state_gui_io.inc"

using namespace BrnGameState::GameStateModuleIO;
int main()
{
    alignas(16) static unsigned char storage[sizeof(OutputBuffer) + 16];
    for (unsigned offset = 0; offset < 16; offset += 4)
    {
        std::memset(storage, 0xA5, sizeof(storage));
        auto* output = reinterpret_cast<OutputBuffer*>(storage + offset);
        output->CgsModule::IOBuffer::Construct();
        InitializeGuiScoring(output);
        Check(output->mxStatusFlags.GetFlags() == 1, "initialization preserves constructed IO status");
        output->LockForRead();
        const OutputBuffer* source = output;
        const auto* scoring = source->GetScoringOutputInterface();
        Check(scoring->meGameModeType == E_MODE_NONE, "original mode sentinel is minus one");
        Check(sizeof(*scoring) == 2736, "full original scoring snapshot is 2736 bytes");
        Check(reinterpret_cast<const unsigned char*>(&scoring->maCarCheckpointData)
              - reinterpret_cast<const unsigned char*>(scoring) == 0x940,
              "all eight checkpoint bitmaps start at the original offset");
        Check(reinterpret_cast<const unsigned char*>(&scoring->maiCumulativeScoreData)
              - reinterpret_cast<const unsigned char*>(scoring) == 0x980,
              "cumulative scores follow the complete checkpoint array");
        const auto* readQueue = source->GetGuiEventQueue();
        unsigned char constructed = 0;
        std::memcpy(&constructed, &readQueue->mbIsConstructed, 1);
        const s32 firstOffset = 16 - static_cast<s32>(reinterpret_cast<uintptr_t>(readQueue->macData) & 15);
        const bool initialized = constructed == 1 && readQueue->miLength == 0
            && readQueue->miFirstEventOffset == firstOffset && readQueue->miBufferWritePos == firstOffset;
        Check(initialized, "actual GUI queue is constructed and aligned at its native address");
        output->UnlockForRead();
        if (!initialized) continue;

        output->LockForWrite();
        auto* queue = output->GetGuiEventQueue();
        Check(queue == readQueue, "read and write accessors name the same real queue");
        Check(queue->GetMaxLength() == 18432, "GUI queue has original 18432-byte capacity");
        const u64 token = UINT64_C(0x1234567887654321);
        queue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&token), 4321, sizeof(token));
        static const unsigned char large[17000] = {0x5A};
        Check(queue->AddEventSafe(reinterpret_cast<const CgsModule::Event*>(large), 7654, sizeof(large)),
              "a record beyond the discarded 1008-byte placeholder fits the real queue");
        auto* writeScoring = output->GetScoringOutputInterface();
        for (s32 car = 0; car < 8; ++car)
        {
            writeScoring->maCarCheckpointData[car].SetupCheckpoints(3 + car % 3);
            writeScoring->maCarCheckpointData[car].MarkCheckpointAsHit(1);
            s32 indexes[16] = {};
            const s32 count = writeScoring->maCarCheckpointData[car].GetAllRemainingCheckpointIndexes(indexes);
            Check(count == 2 + car % 3 && indexes[0] == 0 && indexes[1] == 2,
                  "each original bitmap independently produces remaining checkpoints");
        }
        writeScoring->maCarScoreData[5].miOnlineStandingsPosition = -1234;
        writeScoring->maCarScoreData[5].muOnlinePostEventValueC0 = 0xFEDCBA98;
        Check(writeScoring->maCarScoreData[5].GetOnlineStandingsPosition() == -1234,
              "existing standings getter preserves signed 32-bit value");
        Check(writeScoring->maCarScoreData[5].GetOnlinePostEventValueC0() == 0xFEDCBA98,
              "attested C0 getter preserves all 32 bits without assigning arithmetic semantics");
        output->UnlockForWrite();

        CgsModule::VariableEventQueue<32768, 16> destination;
        destination.Construct();
        output->LockForRead();
        destination.Append(*source->GetGuiEventQueue());
        output->UnlockForRead();
        const CgsModule::Event* event = nullptr; s32 size = 0;
        const s32 type = destination.GetFirstEvent(&event, &size);
        Check(destination.GetLength() == 2 && type == 4321 && size == 8
              && *reinterpret_cast<const u64*>(event) == token,
              "original source append preserves full native payload and both records");
        unsigned char savedPayload[18432];
        std::memcpy(savedPayload, queue->macData, sizeof(savedPayload));
        InitializeGuiScoring(output);
        Check(queue->GetLength() == 0, "reuse retires earlier GUI events");
        Check(std::memcmp(savedPayload, queue->macData, sizeof(savedPayload)) == 0,
              "queue initialization leaves payload storage untouched");
        output->LockForRead();
        Check(source->GetScoringOutputInterface()->meGameModeType == E_MODE_NONE,
              "reuse restores original game mode sentinel");
        for (s32 car = 0; car < 8; ++car)
        {
            s32 indexes[16];
            Check(source->GetScoringOutputInterface()->maCarCheckpointData[car]
                  .GetAllRemainingCheckpointIndexes(indexes) == 0,
                  "original scoring clear resets every checkpoint bitmap");
        }
        output->UnlockForRead();
    }
    Check(assertions == 0, "original IO and queue operations raise no assertion");
    std::printf("GameStateGuiIO: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
