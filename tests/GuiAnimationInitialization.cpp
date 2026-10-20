// Hosted regression for the real CgsGui animation owners. The runner extracts
// production bodies and uses the production headers/Array implementation.
// Only ObjectController/GuiComponent, assert reporting, and heap ownership at
// the APT VM boundary are fixtures. This does not execute a movie or a game.
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <new>
#include "types.hpp"

namespace CgsGui
{
    struct GuiComponent {};

    // Recording boundary for calls which otherwise enter APT/ActionScript.
    // The real Animator owns this pointer and executes all calls under test.
    class ObjectController
    {
    public:
        GuiComponent* target = nullptr;
        int registrations = 0;
        int unregistrations = 0;
        int reads = 0;
        int writes = 0;
        const char* lastRead = nullptr;
        const char* lastWrite = nullptr;
        f32 lastValue = 0.0f;

        void SetControlledObject(GuiComponent* value) { target = value; }
        void Register() { ++registrations; }
        void UnRegister() { ++unregistrations; }
        f32 GetObjectVariableFloat(const char* name)
        {
            ++reads;
            lastRead = name;
            return 0.0f;
        }
        void SetObjectVariableFloat(const char* name, f32 value)
        {
            ++writes;
            lastWrite = name;
            lastValue = value;
        }
    };
}

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimator.h"

static int gChecks = 0;
static int gFailures = 0;
static int gAsserts = 0;
static int gTemplatePoisons = 0;
static CgsGui::Interpolator gPoisonInterpolator;

namespace CgsDev { namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char* expression, const char*, int)
    {
        ++gAsserts;
        std::printf("ASSERT %s\n", expression);
        return 0;
    }
    void* EndAssert() { return nullptr; }
} }

// This is an adversarial input hook, not an initializer replacement. It runs
// after the real C++ constructor and before the real two-phase Construct.
// The vptr stays installed by the real Interpolator constructor.
static void FixturePoisonDefaultChannel(CgsGui::AnimChannel& channel)
{
    ++gTemplatePoisons;
    channel.mpCurrentInterpolator = &gPoisonInterpolator;
    channel.mbActive = true;
    channel.mfCurrentValue = 123.0f;
    channel.mLinearInterpolator.mfStartLambda = 11.0f;
    channel.mLinearInterpolator.mfEndLambda = 12.0f;
    channel.mLinearInterpolator.mfStartValue = 13.0f;
    channel.mLinearInterpolator.mfEndValue = 14.0f;
    channel.mLinearInterpolator.mfCurrentValue = 15.0f;
}

#include "gui_animation_initialization.inc"

static void Check(bool passed, const char* label)
{
    ++gChecks;
    if (!passed)
    {
        ++gFailures;
        std::printf("FAIL %s\n", label);
    }
}

static bool ZeroInterpolator(const CgsGui::Interpolator& interpolator)
{
    return interpolator.mfStartLambda == 0.0f && interpolator.mfEndLambda == 0.0f &&
           interpolator.mfStartValue == 0.0f && interpolator.mfEndValue == 0.0f &&
           interpolator.mfCurrentValue == 0.0f;
}

// Inspect untrusted construction results as bytes. This also keeps the
// omission control independent of evaluating any noncanonical pointer/bool
// representation left in the poisoned owner backing.
static bool RawNullBinding(const CgsGui::AnimChannel& channel)
{
    unsigned char actual[sizeof(CgsGui::Interpolator*)];
    unsigned char expected[sizeof(CgsGui::Interpolator*)];
    CgsGui::Interpolator* nullBinding = nullptr;
    std::memcpy(actual, &channel.mpCurrentInterpolator, sizeof(actual));
    std::memcpy(expected, &nullBinding, sizeof(expected));
    return std::memcmp(actual, expected, sizeof(actual)) == 0;
}

static bool RawInactive(const CgsGui::AnimChannel& channel)
{
    unsigned char actual[sizeof(bool)];
    unsigned char expected[sizeof(bool)];
    const bool inactive = false;
    std::memcpy(actual, &channel.mbActive, sizeof(actual));
    std::memcpy(expected, &inactive, sizeof(expected));
    return std::memcmp(actual, expected, sizeof(actual)) == 0;
}

static bool UnboundChannel(const CgsGui::AnimChannel& channel)
{
    return channel.mpCurrentInterpolator == nullptr && !channel.mbActive &&
           channel.mfCurrentValue == -FLT_MAX;
}

int main()
{
    using namespace CgsGui;

    // Poison the complete owner backing before its actual C++ construction.
    // In particular, Array's element constructors must install real vptrs.
    alignas(Animator) unsigned char backing[sizeof(Animator)];
    std::memset(backing, 0xA5, sizeof(backing));
    Animator* animator = new (backing) Animator;
    GuiComponent target;
    animator->Construct(&target, Animator::maDefaultChannelTypeNames,
                        AnimData::ANIMATOR_CHANNEL_MAX);
    ObjectController* controller = animator->GetObjectController();

    Check(gTemplatePoisons == 1, "template poison executed once");
    Check(animator->mpTarget == &target, "animator target binding");
    Check(animator->miNumChannels == 6, "six declared channels");
    Check(animator->maAnimChannels.GetLength() == 6, "six constructed channels");
    Check(controller != nullptr && controller->target == &target, "controller target binding");
    Check(controller != nullptr && controller->registrations == 1, "controller registered once");

    bool initialized = animator->maAnimChannels.GetLength() == 6;
    for (u32 index = 0; index < 6; ++index)
    {
        const AnimChannel& channel = animator->maAnimChannels.GetItem(index);
        char label[96];
        std::snprintf(label, sizeof(label), "channel[%u] null binding", index);
        Check(RawNullBinding(channel), label);
        std::snprintf(label, sizeof(label), "channel[%u] inactive", index);
        Check(RawInactive(channel), label);
        std::snprintf(label, sizeof(label), "channel[%u] current -FLT_MAX", index);
        Check(channel.GetCurrentValue() == -FLT_MAX, label);
        std::snprintf(label, sizeof(label), "channel[%u] five interpolator fields zero", index);
        Check(ZeroInterpolator(channel.mLinearInterpolator), label);
        const Interpolator* embedded = &channel.mLinearInterpolator;
        std::snprintf(label, sizeof(label), "channel[%u] real virtual getter zero", index);
        Check(embedded->GetCurrentValue() == 0.0f, label);
        initialized = initialized && RawNullBinding(channel) && RawInactive(channel) &&
                      channel.GetCurrentValue() == -FLT_MAX && ZeroInterpolator(*embedded);
    }

    // A broken construction must produce deterministic failed witnesses without
    // dereferencing its poisoned binding. The positive path executes real Update.
    if (initialized && controller != nullptr)
    {
        const int assertsBefore = gAsserts;
        animator->Update(0.25f);
        Check(gAsserts == assertsBefore, "clean update has no assertion");
        Check(controller->writes == 0, "clean update publishes no inactive values");
        bool unchanged = true;
        for (u32 index = 0; index < 6; ++index)
            unchanged = unchanged && UnboundChannel(animator->maAnimChannels.GetItem(index));
        Check(unchanged, "clean update preserves six inactive channels");
    }
    else
    {
        Check(false, "clean update prerequisite: initialized channel binding");
        Check(false, "clean update prerequisite: inactive channels");
        Check(false, "clean update prerequisite: initialized channel values");
    }

    // Exercise the actual Interpolator initializer, virtual getter, reset and
    // finite keyframe boundaries independently of the aggregate template copy.
    Interpolator interpolator;
    interpolator.Construct();
    Check(ZeroInterpolator(interpolator), "interpolator Construct zeroes five fields");
    const Interpolator* virtualInterpolator = &interpolator;
    Check(virtualInterpolator->GetCurrentValue() == 0.0f, "interpolator virtual getter starts zero");
    interpolator.SetInterpolator(1.0f, 3.0f, 10.0f, 20.0f);
    Check(interpolator.GetStartLambda() == 1.0f && interpolator.GetEndLambda() == 3.0f &&
          interpolator.GetStartValue() == 10.0f && interpolator.GetEndValue() == 20.0f,
          "interpolator keyframe ranges");
    Check(virtualInterpolator->GetCurrentValue() == -FLT_MAX, "SetInterpolator resets current -FLT_MAX");
    Check(interpolator.Update(0.0f) == 10.0f, "interpolator before start");
    Check(interpolator.Update(1.0f) == 10.0f, "interpolator at start");
    Check(interpolator.Update(2.0f) == 15.0f, "interpolator midpoint");
    Check(interpolator.Update(3.0f) == 20.0f, "interpolator at end");
    Check(interpolator.Update(4.0f) == 20.0f, "interpolator past end");
    interpolator.Reset();
    Check(virtualInterpolator->GetCurrentValue() == -FLT_MAX, "interpolator Reset current -FLT_MAX");

    AnimChannelData data;
    data.Construct();
    data.SetData(1.0f, 3.0f, 10.0f, 20.0f,
                 AnimChannelData::E_INTERPOLATE_LINEAR,
                 AnimChannelData::E_ANIMATION_SCREEN_TO_SCREEN_SPACE);
    AnimChannel channel;
    channel.Construct();
    Check(RawNullBinding(channel) && RawInactive(channel), "channel Construct null and inactive");
    Check(channel.GetCurrentValue() == -FLT_MAX && channel.mLinearInterpolator.GetCurrentValue() == 0.0f,
          "channel Construct current and embedded current differ");
    channel.SetAnimation(data);
    Check(channel.IsActive(), "channel SetAnimation active");
    Check(channel.mpCurrentInterpolator == &channel.mLinearInterpolator, "channel binds its own embedded interpolator");
    Check(channel.mLinearInterpolator.GetStartLambda() == 1.0f &&
          channel.mLinearInterpolator.GetEndLambda() == 3.0f &&
          channel.mLinearInterpolator.GetStartValue() == 10.0f &&
          channel.mLinearInterpolator.GetEndValue() == 20.0f &&
          channel.mLinearInterpolator.GetCurrentValue() == -FLT_MAX,
          "channel forwards keyframes and resets interpolator");
    channel.Update(1.0f);
    Check(channel.GetCurrentValue() == 10.0f && channel.IsActive() &&
          channel.mpCurrentInterpolator == &channel.mLinearInterpolator, "channel at start active and bound");
    channel.Update(2.0f);
    Check(channel.GetCurrentValue() == 15.0f, "channel midpoint value");
    channel.Update(3.0f);
    Check(channel.GetCurrentValue() == 20.0f && channel.mpCurrentInterpolator != nullptr,
          "channel at end retains binding");
    channel.Update(4.0f);
    Check(channel.GetCurrentValue() == 20.0f && channel.mpCurrentInterpolator == nullptr && channel.IsActive(),
          "channel past end drops binding before active flag");
    channel.Update(4.0f);
    Check(!channel.IsActive(), "channel following update clears active");

    // Use the production AnimData builder and virtual Animator dispatch. Other
    // channels must remain inactive; only _x may cross the recording VM boundary.
    if (initialized && controller != nullptr)
    {
        AnimData animation;
        animation.Construct();
        animation.AddAnimationChannel(AnimData::ANIMATOR_CHANNEL_X, data);
        animator->SetAnimation(&animation, 0.0f);
        AnimChannel& active = animator->maAnimChannels.GetItem(0);
        bool onlyX = active.IsActive() && active.mpCurrentInterpolator == &active.mLinearInterpolator;
        for (u32 index = 1; index < 6; ++index)
            onlyX = onlyX && UnboundChannel(animator->maAnimChannels.GetItem(index));
        Check(onlyX, "animator SetAnimation binds only owned X interpolator");
        Check(controller->reads == 1 && controller->lastRead != nullptr && std::strcmp(controller->lastRead, "_x") == 0,
              "animator reads original X property name");
        animator->Update(2.0f);
        Check(active.GetCurrentValue() == 15.0f, "animator midpoint value");
        Check(controller->writes == 1 && controller->lastWrite != nullptr && std::strcmp(controller->lastWrite, "_x") == 0 &&
              controller->lastValue == 15.0f, "animator publishes only X midpoint value");
        animator->Update(4.0f);
        Check(active.mpCurrentInterpolator == nullptr && active.IsActive() && active.GetCurrentValue() == 20.0f &&
              controller->writes == 2 && controller->lastValue == 20.0f, "animator publishes final value while dropping binding");
        animator->Update(4.0f);
        Check(!active.IsActive() && controller->writes == 2, "animator following update stops publishing");
    }
    else
    {
        for (int index = 0; index < 6; ++index)
            Check(false, "active animator prerequisite: initialized channel binding");
    }

    animator->Destruct();
    Check(controller != nullptr && controller->unregistrations == 1, "controller unregistered once");
    Check(animator->GetObjectController() == nullptr, "animator drops controller binding");
    Check(gAsserts == 0, "all production valid paths preserve assertions");
    delete controller; // fixture boundary only; real UnRegister ownership is out of scope.
    animator->~Animator();

    std::printf("GUI_ANIMATION_INITIALIZATION: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
