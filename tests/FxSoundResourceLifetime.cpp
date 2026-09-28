// ARTIST Ready 82696794 and Detach 826EBFA0 refer to one EffectBase latch.
// Distinct interface bases ensure the registrar gets the requester identity.
#include <cstdio>
static int assertions;
#define CGS_ASSERT(condition, message) do { if (!(condition)) ++assertions; } while (0)
namespace CgsSound { namespace Logic {
struct EffectBase {
    enum { E_ATTACH_STATE_NONE, E_ATTACH_STATE_WAITING_FOR_DATA, E_ATTACH_STATE_PREPARING };
    enum { E_DETACH_STATE_NONE, E_DETACH_STATE_FINISHED = 3 };
    virtual ~EffectBase() = default;
    bool mbHasLoadedData = false;
    int meAttachState = E_ATTACH_STATE_WAITING_FOR_DATA, meDetachState = E_DETACH_STATE_NONE;
    int GetAttachState() const { return meAttachState; }
    bool Detach() { mbHasLoadedData = false; meAttachState = E_ATTACH_STATE_NONE; meDetachState = E_DETACH_STATE_FINISHED; return true; }
};
}}
namespace BrnSound { namespace Logic {
struct IResourceRequester { virtual void ResourcesAreReady() = 0; virtual ~IResourceRequester() = default; };
struct ResourceRegistrar {
    IResourceRequester* removed = nullptr;
    int calls = 0;
    bool loadedAtRemoval = false;
    CgsSound::Logic::EffectBase* effect = nullptr;
    void RemoveRequests(IResourceRequester* requester) {
        removed = requester; ++calls; loadedAtRemoval = effect->mbHasLoadedData;
    }
};
#define EFFECT_CLASS(Name) struct Name : CgsSound::Logic::EffectBase, IResourceRequester { \
    /* Legacy shadow fields let the OLD production bodies compile and fail. */ \
    bool mbResourceRequestActive = false, mbResourcesReady = false; \
    ResourceRegistrar registrar; \
    Name() { registrar.effect = this; } \
    ResourceRegistrar& GetResourceRegistrar() { return registrar; } \
    void ResourcesAreReady() override; bool Detach(); \
};
EFFECT_CLASS(BrnEffectObject)
EFFECT_CLASS(BrnEffectControl)
#include "fx_sound_resource_lifetime.inc"
}}
static int checks, failures;
static void Check(bool good, const char* what) { ++checks; if (!good) { ++failures; std::printf("FAIL %s\n", what); } }
template<class T> void Exercise() {
    T effect;
    auto* requester = static_cast<BrnSound::Logic::IResourceRequester*>(&effect);
    Check(static_cast<void*>(requester) != static_cast<void*>(&effect), "fixture has distinct requester base");
    Check(effect.Detach() && effect.registrar.calls == 0, "never-loaded effect does not remove requests");
    Check(effect.meAttachState == T::E_ATTACH_STATE_NONE && effect.meDetachState == T::E_DETACH_STATE_FINISHED, "empty detach resets states");
    for (int cycle = 0; cycle < 2; ++cycle) {
        effect.meAttachState = T::E_ATTACH_STATE_WAITING_FOR_DATA;
        effect.meDetachState = T::E_DETACH_STATE_NONE;
        // Loading via IResourceRequester bypasses the old hidden LoadAsset overloads.
        requester->ResourcesAreReady();
        Check(effect.mbHasLoadedData, "completion sets inherited loaded-data latch");
        Check(effect.meAttachState == T::E_ATTACH_STATE_PREPARING, "completion advances attach state");
        Check(effect.Detach(), "loaded detach succeeds");
        Check(effect.registrar.calls == cycle + 1 && effect.registrar.removed == requester && effect.registrar.loadedAtRemoval,
              "each completed load releases exactly its requester before clearing latch");
        Check(!effect.mbHasLoadedData && effect.meAttachState == T::E_ATTACH_STATE_NONE && effect.meDetachState == T::E_DETACH_STATE_FINISHED,
              "detach clears latch and permits reattach");
        effect.Detach();
        Check(effect.registrar.calls == cycle + 1, "repeated detach does not release twice");
    }
}
int main() {
    Exercise<BrnSound::Logic::BrnEffectObject>();
    Exercise<BrnSound::Logic::BrnEffectControl>();
    if (assertions) { ++failures; std::printf("FAIL unexpected assertions %d\n", assertions); }
    std::printf("FxSoundResourceLifetime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
