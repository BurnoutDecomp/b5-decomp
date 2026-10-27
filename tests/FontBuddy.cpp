// Production dispatchers/helpers and real queues; explicit boundaries are in
// evidence/font_buddy/BOUNDARIES.md. This translation unit does not load the game.
#undef private
#undef protected
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiFontCollection.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"
#include "GameSource/GameState/BrnCgsPlayerName.h"
#include "SDKs/EATech/include/Apt/Apt.h"

static unsigned guChecks, guFailures, guAssertions, guUnexpected;
static void Check(bool pass, const std::string& id)
{
    ++guChecks;
    guFailures += !pass;
    std::printf("CHECK %s %s\n", pass ? "PASS" : "FAIL", id.c_str());
}
static void Unexpected(const char* label)
{
    ++guUnexpected;
    std::printf("UNEXPECTED %s\n", label);
}
static u32 Bits(f32 value) { u32 bits; std::memcpy(&bits, &value, 4); return bits; }
static u32 Bits(s32 value) { u32 bits; std::memcpy(&bits, &value, 4); return bits; }
static f32 FloatBits(u32 bits) { f32 value; std::memcpy(&value, &bits, 4); return value; }
static s32 IntBits(u32 bits) { s32 value; std::memcpy(&value, &bits, 4); return value; }

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int)
{
    ++guAssertions; std::printf("ASSERT %s\n", message); return 0;
}
void* EndAssert() { return nullptr; }
} namespace Log {
void WriteToLog(const char* message) { std::printf("ENGINE_LOG %s", message); }
} }
static char gaLogText[4096];
static CgsDev::StrStream gLogStream(gaLogText, sizeof(gaLogText));
namespace CgsDev { namespace Log { StrStreamBase* gpDebugPrint = &gLogStream; }
namespace Message { u64 gxMessageFilterFlags = 0; } }

// These class declarations project only storage and declarations needed by the
// complete source bodies. Generated members/getters/virtual declarations come
// from the pinned production headers. They are not native layout/vtable tests.
#include "font_buddy_context.inc"
#include "font_buddy_cases.inc"

struct ViewTail
{
    s32 type;
    const CgsModule::Event* event;
    u32 effect, spacing;
};
static std::vector<ViewTail> gViewTail;
struct ViewRecipient : CgsGui::CustomRendererManager
{
    CgsGui::AptRenderHandler* handler = nullptr;
    void RecvEvent(const CgsModule::Event* event, s32 type) override
    {
        gViewTail.push_back({type, event, Bits(handler->GetTextEffect()), Bits(handler->GetFontSizeScale())});
    }
};

// String-pool and ZID lifetime boundary: only opaque pointer identity/storage is
// observed. No CgsAptString object, layout engine, glyph or renderer is fabricated.
alignas(std::max_align_t) static unsigned char gaOpaqueSlot[1];
static CgsUnicode::CgsUtf8 gaStringBuffer[256];
static unsigned char gaPreviousStringToken[8];
static unsigned guPoolCalls, guDeallocateCalls, guZidCalls;
static AptAssetString gpDeallocated;
static const char* gpPoolText;
namespace CgsGui {
AptAux* AptAuxPointer::mpAptAuxInst = nullptr;
CgsAptString* AptRenderHandler::GetUnusedAptString(CgsUnicode::CgsUtf8** output, const char* text)
{
    ++guPoolCalls; gpPoolText = text; *output = gaStringBuffer;
    return reinterpret_cast<CgsAptString*>(gaOpaqueSlot);
}
int AptRenderHandler::AptStringToZID(const CgsAptString* slot) const
{
    ++guZidCalls;
    if (slot != reinterpret_cast<const CgsAptString*>(gaOpaqueSlot)) Unexpected("ZID boundary received another pool slot");
    return 17;
}
void AptCallbackRender::DeallocateString(AptAssetString slot, u32 flags)
{
    ++guDeallocateCalls; gpDeallocated = slot;
    if (flags != 0) Unexpected("deallocate boundary flags");
}
void ViewModule::ProcessIncomingLanguageEvent(const void*, s32, s32, ViewIO::OutputBuffer*) { Unexpected("unselected language case"); }
void ViewModule::ProcessIncomingLoadNotification(const CgsModule::Event*) { Unexpected("unselected load case"); }
void ViewModule::ProcessIncomingUnloadRequestNotification(const void*) { Unexpected("unselected unload case"); }
void ViewModule::ProcessIncomingAptEvent(const void*, s32) { Unexpected("unselected Apt case"); }
void ViewModule::SetClearScreenAlpha(f32) { Unexpected("unselected clear-screen case"); }
}

enum Destination { JOIN = 1, SEND, CANCEL, ACCEPT, REVOKE, DECLINE };
struct BuddyCall
{
    Destination destination;
    const CgsNetwork::PlayerName* pointer;
    std::array<u8, 16> name;
};
static std::vector<BuddyCall> gBuddyCalls;
static unsigned guOutgoingCalls;
static bool gbOutgoingAfterClear, gbOutgoingQueueIdentity;
static BrnNetwork::BrnNetworkModuleIO::NetworkEventQueue* gpExpectedOutgoing;
static BrnNetwork::BuddyManagerDebugComponent* gpExpectedDebug;
static void Record(Destination destination, const CgsNetwork::PlayerName* name)
{
    BuddyCall call{destination, name, {}};
    if (name != nullptr) std::memcpy(call.name.data(), name, call.name.size());
    gBuddyCalls.push_back(call);
}
namespace CgsNetwork {
s32 BuddyManagerBase::GetNumBuddies() { Unexpected("unselected count-buddies case"); return 0; }
u32 BuddyManagerX360::ShowProfile(const PlayerName*) { Unexpected("unselected SDK profile endpoint"); return 0; }
}
namespace BrnNetwork {
void BuddyManagerBase::SendEmptyBuddyInformation(BrnNetworkModuleIO::NetworkEventQueue*) { Unexpected("unselected buddy-information response"); }
void BuddyManagerDebugComponent::ProcessOutgoingEvents(BrnNetworkModuleIO::NetworkEventQueue* queue)
{
    // Logger/server-response boundary, deliberately not a production logger body.
    ++guOutgoingCalls;
    gbOutgoingAfterClear = gpExpectedDebug != nullptr && gpExpectedDebug->GetEventQueue()->GetLength() == 0;
    gbOutgoingQueueIdentity = queue == gpExpectedOutgoing;
}
}
struct ObservedBuddy : BrnNetwork::BuddyManagerX360
{
    void JoinBuddy(const CgsNetwork::PlayerName* name) override { Record(JOIN, name); }
    void SendInvite(const CgsNetwork::PlayerName* name) override { Record(SEND, name); }
    void CancelInvites() override { Record(CANCEL, nullptr); }
    void AcceptInvite(const CgsNetwork::PlayerName* name) override { Record(ACCEPT, name); }
    void RevokeInvite(const CgsNetwork::PlayerName* name) override { Record(REVOKE, name); }
    void DeclineInvite(const CgsNetwork::PlayerName* name) override { Record(DECLINE, name); }
};

// Complete extracted bodies, including all original switch arms and traversal.
// The old controls replace only one complete dispatcher with its real old body.
#include "font_buddy_production.inc"

using ViewQueue = CgsGui::GuiEventQueueBase<65536, 16>;
using ViewQueueBase = CgsModule::VariableEventQueue<65536, 16>;
using NetworkQueue = BrnNetwork::BrnNetworkModuleIO::NetworkEventQueue;
static void Init(ViewQueue& queue) { queue.MarkUnconstructed(); static_cast<ViewQueueBase&>(queue).Construct(); static_cast<ViewQueueBase&>(queue).Prepare(); }
static void Init(NetworkQueue& queue) { queue.MarkUnconstructed(); queue.Construct(); queue.Prepare(); }
static bool Append(ViewQueue& queue, const void* body, s32 type, s32 size)
{
    return static_cast<ViewQueueBase&>(queue).AddEvent(reinterpret_cast<const CgsModule::Event*>(body), type, size);
}
static bool Append(NetworkQueue& queue, const void* body, s32 type, s32 size)
{
    return queue.AddEvent(reinterpret_cast<const CgsModule::Event*>(body), type, size);
}
static s32 Length(const ViewQueue& queue) { return static_cast<const ViewQueueBase&>(queue).GetLength(); }
struct EventSnapshot { s32 type, size; const CgsModule::Event* pointer; std::vector<u8> bytes; };
template <typename Queue> static std::vector<EventSnapshot> Snapshot(const Queue& queue)
{
    std::vector<EventSnapshot> records;
    const CgsModule::Event* event = nullptr; s32 size = 0;
    for (s32 type = queue.GetFirstEvent(&event, &size); event != nullptr; type = queue.GetNextEvent(event, &event, &size))
    {
        EventSnapshot record{type, size, event, {}};
        // Tag11/size1 is an originally unwritten notification byte. Inspect only
        // its tag/size and never invent or assert its value.
        if (!(type == 11 && size == 1))
        {
            const u8* bytes = reinterpret_cast<const u8*>(event);
            record.bytes.assign(bytes, bytes + size);
        }
        records.push_back(record);
    }
    return records;
}
static std::array<u8, 16> NameBytes(const CgsNetwork::PlayerName& name)
{
    std::array<u8, 16> value; std::memcpy(value.data(), &name, value.size()); return value;
}
static s32 Index(const EventSnapshot& record)
{
    s32 value = 0; if (record.bytes.size() >= 20) std::memcpy(&value, record.bytes.data() + 16, 4); return value;
}
static void SeedName(CgsNetwork::PlayerName& name, unsigned index)
{
    for (unsigned byte = 0; byte < 16; ++byte) name.macName[byte] = static_cast<char>(0x30 + byte + 17 * index);
}

struct LayoutInputs
{
    void* slot; CgsUnicode::CgsUtf8* buffer; const void* fonts; s32 effect, zid; f32 spacing;
    bool endpoints;
};
static LayoutInputs Acquire(CgsGui::ViewModule& view, unsigned flags = 0x406)
{
    CgsGui::AptAuxPointer::mpAptAuxInst = &view.mAptAux;
    AptAllocateStringParameters parameters{};
    parameters.szString = "finite fixture text"; parameters.eFlags = flags;
    parameters.pCurrString = gaPreviousStringToken;
    const auto pool = guPoolCalls, zid = guZidCalls, dealloc = guDeallocateCalls;
    LayoutInputs inputs{};
    inputs.slot = CgsGui::AptCallbackRender::AcquireStringSlot(&parameters, &inputs.buffer, &inputs.fonts,
                                                            &inputs.effect, &inputs.spacing, &inputs.zid);
    inputs.endpoints = inputs.slot == gaOpaqueSlot && inputs.buffer == gaStringBuffer && inputs.zid == 17
        && guPoolCalls == pool + 1 && guZidCalls == zid + 1 && gpPoolText == parameters.szString
        && guDeallocateCalls == dealloc + ((flags & 0x406u) == 0 ? 1u : 0u);
    return inputs;
}

static void FontChecks()
{
    CgsGui::ViewModule view{};
    auto& handler = view.mAptAux.mRenderHandler;
    CgsGui::FontCollection fonts{}; fonts.Construct();
    int opaqueResource = 0;
    fonts.maFonts[0].mpResourceMemory = &opaqueResource; // IsNull only; no Font dereference.
    handler.mpFontCollection = &fonts;
    ViewRecipient recipient; recipient.handler = &handler; view.mpCustomRendererManager = &recipient;

    for (const auto& item : kEffectCases)
    {
        handler.miTextEffect = IntBits(item.before); handler.mfFontSizeScale = FloatBits(0x40A00000u);
        view.mAptAux.DebugRotateToNextFontEffect();
        const std::string id = std::string("helper.effect.") + item.id;
        Check(Bits(handler.GetTextEffect()) == item.after, id + ".word");
        Check(Bits(handler.GetFontSizeScale()) == 0x40A00000u, id + ".spacing_unchanged");
    }
    for (const auto& item : kSpacingCases)
    {
        handler.miTextEffect = 3; handler.mfFontSizeScale = 12.0f;
        view.mAptAux.DebugChangeFontSpacing(FloatBits(item.bits));
        const std::string id = std::string("helper.spacing.") + item.id;
        Check(Bits(handler.GetFontSizeScale()) == item.bits, id + ".bits");
        Check(handler.GetTextEffect() == 3, id + ".effect_unchanged");
    }
    for (const auto& item : kEffectCases)
    {
        handler.miTextEffect = IntBits(item.before); handler.mfFontSizeScale = FloatBits(0x40A00000u);
        ViewQueue queue; Init(queue); const u8 payload = 0x5A;
        const bool appended = Append(queue, &payload, 61, 1);
        gViewTail.clear(); view.ProcessIncomingViewEvents(&queue, nullptr);
        const auto inputs = Acquire(view);
        const std::string id = std::string("view.effect.") + item.id;
        Check(Bits(handler.GetTextEffect()) == item.after, id + ".word");
        Check(appended && Length(queue) == 1, id + ".queue_retained");
        Check(gViewTail.size() == 1 && gViewTail[0].type == 61, id + ".tail_once");
        Check(gViewTail.size() == 1 && gViewTail[0].effect == item.after, id + ".tail_after_store");
        Check(Bits(inputs.effect) == item.after, id + ".layout_effect");
        Check(Bits(inputs.spacing) == 0x40A00000u, id + ".layout_spacing");
        Check(inputs.endpoints && inputs.fonts == &fonts, id + ".layout_endpoints");
    }
    for (const auto& item : kSpacingCases)
    {
        handler.miTextEffect = 3; handler.mfFontSizeScale = 12.0f;
        ViewQueue queue; Init(queue); const f32 payload = FloatBits(item.bits);
        const bool appended = Append(queue, &payload, 62, 4);
        gViewTail.clear(); view.ProcessIncomingViewEvents(&queue, nullptr);
        const auto inputs = Acquire(view);
        const std::string id = std::string("view.spacing.") + item.id;
        Check(Bits(handler.GetFontSizeScale()) == item.bits, id + ".bits");
        Check(appended && Length(queue) == 1, id + ".queue_retained");
        Check(gViewTail.size() == 1 && gViewTail[0].type == 62, id + ".tail_once");
        Check(gViewTail.size() == 1 && gViewTail[0].spacing == item.bits, id + ".tail_after_store");
        Check(Bits(inputs.spacing) == item.bits, id + ".layout_spacing");
        Check(inputs.effect == 3, id + ".layout_effect");
        Check(inputs.endpoints && inputs.fonts == &fonts, id + ".layout_endpoints");
    }

    ViewQueue mixed; Init(mixed);
    const u8 tag = 0x6C; const f32 negativeZero = FloatBits(0x80000000u), twoAndHalf = 2.5f;
    bool added = Append(mixed, &tag, 61, 1); added &= Append(mixed, &negativeZero, 62, 4);
    added &= Append(mixed, &tag, 999, 1); added &= Append(mixed, &tag, 61, 1); added &= Append(mixed, &twoAndHalf, 62, 4);
    handler.miTextEffect = 3; handler.mfFontSizeScale = 1.0f; gViewTail.clear();
    view.ProcessIncomingViewEvents(&mixed, nullptr);
    Check(added, "view.mixed.append");
    Check(gViewTail.size() == 5 && gViewTail[0].type == 61 && gViewTail[1].type == 62 && gViewTail[2].type == 999
          && gViewTail[3].type == 61 && gViewTail[4].type == 62, "view.mixed.tail_order");
    Check(gViewTail.size() == 5, "view.mixed.tail_count");
    Check(handler.GetTextEffect() == 0, "view.mixed.effect_final");
    Check(Bits(handler.GetFontSizeScale()) == 0x40200000u, "view.mixed.spacing_final");
    const u32 effects[] = {4, 4, 4, 0, 0}, spacings[] = {0x3F800000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x40200000u};
    bool states = gViewTail.size() == 5, bytes = gViewTail.size() == 5;
    for (unsigned i = 0; i < gViewTail.size() && i < 5; ++i)
    {
        states &= gViewTail[i].effect == effects[i] && gViewTail[i].spacing == spacings[i];
        if (i == 0 || i == 2 || i == 3) bytes &= *reinterpret_cast<const u8*>(gViewTail[i].event) == tag;
        else { u32 value; std::memcpy(&value, gViewTail[i].event, 4); bytes &= value == (i == 1 ? 0x80000000u : 0x40200000u); }
    }
    Check(states, "view.mixed.tail_states"); Check(bytes, "view.mixed.input_bytes");
    Check(Length(mixed) == 5, "view.mixed.queue_length");

    view.mpCustomRendererManager = nullptr; handler.miTextEffect = 4;
    ViewQueue alone; Init(alone); const bool aloneAdded = Append(alone, &tag, 61, 1);
    view.ProcessIncomingViewEvents(&alone, nullptr);
    Check(handler.GetTextEffect() == 0, "view.no_manager.word");
    Check(aloneAdded && Length(alone) == 1, "view.no_manager.queue_retained");

    fonts.Construct(); handler.mpFontCollection = &fonts;
    Check(Acquire(view).fonts == nullptr, "layout.empty_fonts_null");
    handler.mpFontCollection = nullptr;
    Check(Acquire(view).fonts == nullptr, "layout.null_fontsptr");
    const auto before = guDeallocateCalls; const auto released = Acquire(view, 0);
    Check(released.endpoints && guDeallocateCalls == before + 1 && gpDeallocated == gaPreviousStringToken, "layout.deallocate_when_no_keep");
    const auto kept = Acquire(view, 2);
    Check(kept.endpoints && guDeallocateCalls == before + 1, "layout.keepflags_prevent_deallocate");
    CgsGui::AptAuxPointer::mpAptAuxInst = nullptr;
}

static void BuddyChecks()
{
    static_assert(sizeof(CgsNetwork::PlayerName) == 16, "Actual bare PlayerName payload must remain16 bytes");
    NetworkQueue outgoing; Init(outgoing);
    BrnNetwork::BrnNetworkModule module; module.queue = &outgoing;
    ObservedBuddy manager; manager.mpNetworkModule = &module;
    CgsNetwork::PlayerName name; SeedName(name, 1); const auto expectedName = NameBytes(name);
    const s32 tags[] = {9, 10, 11, 12}; const Destination destinations[] = {REVOKE, ACCEPT, DECLINE, JOIN};
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto before = NameBytes(name); gBuddyCalls.clear();
        const auto* event = reinterpret_cast<const CgsModule::Event*>(&name);
        manager.ProcessEvent(event, tags[i]);
        const std::string id = "buddy.direct.T" + std::to_string(tags[i]);
        Check(gBuddyCalls.size() == 1, id + ".call_count");
        Check(gBuddyCalls.size() == 1 && gBuddyCalls[0].destination == destinations[i], id + ".destination");
        Check(gBuddyCalls.size() == 1 && gBuddyCalls[0].pointer == &name, id + ".pointer_identity");
        Check(gBuddyCalls.size() == 1 && gBuddyCalls[0].pointer != nullptr && gBuddyCalls[0].name == expectedName, id + ".name16");
        Check(NameBytes(name) == before, id + ".input_unchanged");
        Check(outgoing.GetLength() == 0, id + ".base_does_not_notify");
    }

    NetworkQueue inbound; Init(inbound);
    bool appended = true;
    for (const auto type : tags) appended &= Append(inbound, &name, type, 16);
    const auto incoming = Snapshot(inbound); gBuddyCalls.clear(); outgoing.Clear();
    manager.ProcessNetworkQueue(&inbound, &outgoing);
    Check(appended && gBuddyCalls.size() == 4, "buddy.leaf.calls4");
    bool routes = gBuddyCalls.size() == 4, pointers = gBuddyCalls.size() == 4, names = gBuddyCalls.size() == 4;
    for (unsigned i = 0; i < gBuddyCalls.size() && i < 4; ++i)
    {
        routes &= gBuddyCalls[i].destination == destinations[i];
        pointers &= gBuddyCalls[i].pointer == reinterpret_cast<const CgsNetwork::PlayerName*>(incoming[i].pointer);
        names &= gBuddyCalls[i].pointer != nullptr && gBuddyCalls[i].name == expectedName;
    }
    Check(routes, "buddy.leaf.destinations"); Check(pointers, "buddy.leaf.pointer_identity"); Check(names, "buddy.leaf.names");
    Check(inbound.GetLength() == 4, "buddy.leaf.queue_not_consumed");
    const auto notifications = Snapshot(outgoing);
    bool tag11 = true; for (const auto& event : notifications) tag11 &= event.type == 11 && event.size == 1;
    Check(tag11, "buddy.leaf.notifications_11_only"); Check(notifications.size() == 2, "buddy.leaf.notifications_count2");

    NetworkQueue debug; Init(debug); outgoing.Clear();
    auto& component = manager.mDebugComponent; component.mpEventQueue = &debug;
    component.miBuddyIndex = 2; component.miMessageIndex = 0x1234567; component.miFeedbackIndex = -7;
    for (unsigned i = 0; i < 5; ++i) SeedName(component.mBuddyInformation[i].mPlayerName, i);
    const auto selectedName = NameBytes(component.mBuddyInformation[2].mPlayerName);
    gpExpectedDebug = &component; gpExpectedOutgoing = &outgoing;
    BrnNetwork::BuddyManagerDebugComponent::JoinBuddy(&component);
    const auto joined = Snapshot(debug);
    Check(joined.size() == 1 && joined[0].type == 12, "buddy.callback_join.tag12");
    Check(joined.size() == 1 && joined[0].size == 16, "buddy.callback_join.size16");
    Check(joined.size() == 1 && joined[0].bytes.size() == 16 && std::memcmp(joined[0].bytes.data(), selectedName.data(), 16) == 0, "buddy.callback_join.selected_name");
    gBuddyCalls.clear(); guOutgoingCalls = 0; gbOutgoingAfterClear = gbOutgoingQueueIdentity = false;
    manager.ProcessDebugEvents();
    Check(gBuddyCalls.size() == 1 && gBuddyCalls[0].destination == JOIN, "buddy.callback_join.drain_join");
    Check(joined.size() == 1 && gBuddyCalls.size() == 1 && gBuddyCalls[0].pointer == reinterpret_cast<const CgsNetwork::PlayerName*>(joined[0].pointer)
          && gBuddyCalls[0].name == selectedName, "buddy.callback_join.drain_pointer");
    Check(debug.GetLength() == 0, "buddy.callback_join.drain_clear");
    Check(guOutgoingCalls == 1 && gbOutgoingAfterClear && gbOutgoingQueueIdentity, "buddy.callback_join.outgoing_observer_after_clear");

    BrnNetwork::BuddyManagerDebugComponent::PrintMessage(&component);
    BrnNetwork::BuddyManagerDebugComponent::SendTestMessage(&component);
    BrnNetwork::BuddyManagerDebugComponent::PrintNextUnreadMessage(&component);
    BrnNetwork::BuddyManagerDebugComponent::LeaveFeedback(&component);
    const auto dropped = Snapshot(debug); const s32 dropTags[] = {3, 4, 5, 7}, sizes[] = {20, 216, 16, 20};
    for (unsigned i = 0; i < 4; ++i)
    {
        const bool present = dropped.size() == 4;
        const std::string id = "buddy.drop.T" + std::to_string(dropTags[i]);
        Check(present && dropped[i].type == dropTags[i], id + ".tag");
        Check(present && dropped[i].size == sizes[i], id + ".size");
        Check(present && dropped[i].bytes.size() >= 16 && std::memcmp(dropped[i].bytes.data(), selectedName.data(), 16) == 0, id + ".name16");
        bool extra = false;
        if (present && i == 0) extra = Index(dropped[i]) == component.miMessageIndex;
        if (present && i == 1)
        {
            char expected[200] = {}; const char message[] = "This is a test message sent via the debug menu";
            std::memcpy(expected, message, sizeof(message));
            extra = dropped[i].bytes.size() == 216 && std::memcmp(dropped[i].bytes.data() + 16, expected, 200) == 0;
        }
        if (present && i == 2) extra = dropped[i].bytes.size() == 16;
        if (present && i == 3) extra = Index(dropped[i]) == component.miFeedbackIndex;
        Check(extra, id + ".extra_payload");
    }
    gBuddyCalls.clear(); guOutgoingCalls = 0; outgoing.Clear(); gbOutgoingAfterClear = gbOutgoingQueueIdentity = false;
    manager.ProcessDebugEvents();
    Check(gBuddyCalls.empty(), "buddy.drop.no_virtual_actions"); Check(debug.GetLength() == 0, "buddy.drop.same_queue_cleared");
    Check(outgoing.GetLength() == 0, "buddy.drop.outgoing_empty");
    Check(guOutgoingCalls == 1 && gbOutgoingAfterClear && gbOutgoingQueueIdentity, "buddy.drop.logger_boundary_after_clear");
    Check(component.miBuddyIndex == 2 && component.miMessageIndex == 0x1234567 && component.miFeedbackIndex == -7, "buddy.drop.selection_unchanged");
    component.mpEventQueue = nullptr; const auto calls = gBuddyCalls.size();
    BrnNetwork::BuddyManagerDebugComponent::JoinBuddy(&component);
    Check(gBuddyCalls.size() == calls && outgoing.GetLength() == 0, "buddy.guard.join_null_queue");
    BrnNetwork::BuddyManagerDebugComponent::LeaveFeedback(&component);
    Check(gBuddyCalls.size() == calls && outgoing.GetLength() == 0, "buddy.guard.feedback_null_queue");
    gpExpectedDebug = nullptr; gpExpectedOutgoing = nullptr;

    unsigned char imageUserData[64], before[64]; std::memset(imageUserData, 0xA7, sizeof(imageUserData)); std::memcpy(before, imageUserData, sizeof(before));
    const auto imageCalls = gBuddyCalls.size(); const auto imagePool = guPoolCalls;
    BrnNetwork::ImageManagerDebugComponent::EncodeMugshotAtSelectedQuality(imageUserData);
    Check(std::memcmp(imageUserData, before, sizeof(before)) == 0, "image.drop.opaque_userdata_untouched");
    Check(gBuddyCalls.size() == imageCalls && guPoolCalls == imagePool && outgoing.GetLength() == 0, "image.drop.no_endpoints");
}

int main()
{
    FontChecks(); BuddyChecks();
    Check(guAssertions == 0, "global.assertions_zero"); Check(guUnexpected == 0, "global.unexpected_boundaries_zero");
    std::printf("FontBuddy: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
