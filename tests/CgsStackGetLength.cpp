#include <cstdio>
#include <cstring>

#include "GameShared/GameClasses/Containers/CgsStack.h"

static int gChecks;
static int gFailures;
static int gBeginCount;
static int gFireCount;
static int gEndCount;
static int gCallOrder[3];
static int gCallOrderCount;
static const char* gExpression;
static const char* gFile;
static int gLine;
static s32* gpLengthToMutateAtEnd;
static s32 gLengthAtEnd;

static void RecordAssertCall(int liCall)
{
    if (gCallOrderCount < 3)
    {
        gCallOrder[gCallOrderCount] = liCall;
    }
    ++gCallOrderCount;
}

static void Check(bool lbCondition, const char* lpcName)
{
    ++gChecks;
    if (!lbCondition)
    {
        ++gFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

static void ResetAssertCapture()
{
    gBeginCount = 0;
    gFireCount = 0;
    gEndCount = 0;
    gCallOrderCount = 0;
    gExpression = nullptr;
    gFile = nullptr;
    gLine = -1;
    gpLengthToMutateAtEnd = nullptr;
    gLengthAtEnd = 0;
}

namespace CgsDev
{
namespace Assert
{
int BeginAssert()
{
    ++gBeginCount;
    RecordAssertCall(1);
    return 0;
}

int FireAssert(const char* lpcExpression, const char* lpcFile, int liLine)
{
    ++gFireCount;
    RecordAssertCall(2);
    gExpression = lpcExpression;
    gFile = lpcFile;
    gLine = liLine;
    return 0;
}

void* EndAssert()
{
    ++gEndCount;
    RecordAssertCall(3);
    if (gpLengthToMutateAtEnd != nullptr)
    {
        *gpLengthToMutateAtEnd = gLengthAtEnd;
        gpLengthToMutateAtEnd = nullptr;
    }
    return nullptr;
}
}
}

int main()
{
    const char* const kExpectedMessage = "Stack used before Construct/Clear was called";
    const char* const kExpectedFile = "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsStack.h";

    CgsContainers::Stack<u16, 400> lEmpty;
    lEmpty.Construct();
    Check(lEmpty.GetLength() == 0, "constructed empty stack reports length zero");
    Check(gBeginCount == 0 && gFireCount == 0 && gEndCount == 0,
          "constructed empty stack does not assert");

    CgsContainers::Stack<u16, 400> lPartiallyFull;
    lPartiallyFull.Construct();
    lPartiallyFull.Push(static_cast<u16>(17));
    Check(lPartiallyFull.GetLength() == 1, "nonempty stack reports its live count");
    Check(gBeginCount == 0 && gFireCount == 0 && gEndCount == 0,
          "nonempty stack does not assert");

    CgsContainers::Stack<u16, 400> lAtCapacity;
    lAtCapacity.Construct();
    for (u32 luIndex = 0; luIndex < 400; ++luIndex)
    {
        lAtCapacity.Push(static_cast<u16>(luIndex));
    }
    Check(lAtCapacity.GetLength() == 400, "full stack reports its capacity-sized live count");
    Check(gBeginCount == 0 && gFireCount == 0 && gEndCount == 0,
          "full stack does not assert");

    CgsContainers::Stack<u16, 400> lUnconstructed;
    lUnconstructed.miLength = CgsContainers::KI_STACK_UNCONSTRUCTED;
    ResetAssertCapture();
    Check(lUnconstructed.GetLength() == CgsContainers::KI_STACK_UNCONSTRUCTED,
          "sentinel stack continues and returns the length when EndAssert leaves it unchanged");
    Check(gBeginCount == 1 && gFireCount == 1 && gEndCount == 1,
          "sentinel stack runs one complete assert sequence");
    Check(gCallOrderCount == 3 && gCallOrder[0] == 1 && gCallOrder[1] == 2 && gCallOrder[2] == 3,
          "sentinel assertion order is BeginAssert, FireAssert, EndAssert");
    Check(gExpression != nullptr && std::strcmp(gExpression, kExpectedMessage) == 0 &&
          gFile != nullptr && std::strcmp(gFile, kExpectedFile) == 0 && gLine == 160,
          "sentinel assertion preserves the original message, path, and line");

    lUnconstructed.miLength = CgsContainers::KI_STACK_UNCONSTRUCTED;
    ResetAssertCapture();
    gpLengthToMutateAtEnd = &lUnconstructed.miLength;
    gLengthAtEnd = 2;
    Check(lUnconstructed.GetLength() == 2 && lUnconstructed.miLength == 2,
          "GetLength rereads miLength after EndAssert changes it");
    Check(gBeginCount == 1 && gFireCount == 1 && gEndCount == 1,
          "post-EndAssert reread still follows one complete assertion");

    std::printf("CgsStackGetLength: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
