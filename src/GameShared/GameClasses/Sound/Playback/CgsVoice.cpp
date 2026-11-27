// ============================================================================
// CgsVoice.cpp -- CgsSound::Playback::Slot control surface.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Slot::Attach        @ 0x826C77F8
//   Slot::Play          @ 0x82693428
//   Slot::Stop          @ 0x826934F8
//   Slot::HandleAttach  @ 0x826935D0
//   Slot::HandleDetach  @ 0x82693690
//   Slot::Release       @ 0x826C0810
//
// A Slot binds a Content to a Voice and drives its type-specific play/stop/attach
// behaviour through the pluggable ISlotImplementation. Every operation asserts the
// impl is present, and (when content is bound) that the handle owns a Content.
// ============================================================================
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "rw/rwcore_structs.h"

#include "rw/math/fpu/scalar_operation.h"

#include <limits>
#include <new>

namespace CgsSound
{
namespace Playback
{

const ISlotFactory* ISlotFactory::spHead = 0;
const Name PlayerVoice::SK_PLAYER_SLOT_NAME("~PlayerVoice::SK_PLAYER_SLOT_NAME~");

// CgsVoice.h:144. Static slot factories register by prepending themselves to
// the process-wide list; SetupConf performs the corresponding name lookup.
ISlotFactory::ISlotFactory(Name aName)
    : mName(aName), mpNext(spHead)
{
    spHead = this;
}

const ISlotFactory* ISlotFactory::GetFactory(Name aName)
{
    const ISlotFactory* lpFactory = spHead;
    while (lpFactory && lpFactory->mName != aName)
        lpFactory = lpFactory->mpNext;
    return lpFactory;
}

void* Voice::operator new(size_t auClientSize, Factory& arFactory,
                          const VoiceSpec& arVoiceSpec)
{
    const u32 luOutputCount = arVoiceSpec.GetOutputParameterCount();
    const u32 luParameterCount = arVoiceSpec.GetParameterCount();
    const u32 luInputCount = luParameterCount - luOutputCount;
    const size_t luSize = auClientSize
        + sizeof(Slot) * arVoiceSpec.GetSlotCount()
        + sizeof(Send) * arVoiceSpec.GetSendCount()
        + sizeof(InputParameter) * luInputCount
        + sizeof(OutputParameter) * luOutputCount;

    CgsResource::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
    for (u32 luIndex = 1; luIndex < 5; ++luIndex)
    {
        lDescriptor.m_baseResourceDescriptors[luIndex].m_size = 0;
        lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
    }

    rw::Resource lResource = arFactory.GetEnvironment().GetAllocator()->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), "Voice");
    return lResource.m_baseResources[0];
}

void Voice::operator delete(void* /*apMemory*/, Factory& /*arFactory*/,
                            const VoiceSpec& /*arVoiceSpec*/)
{
}

Voice::Voice(size_t auClientSize, Factory& arFactory, const VoiceSpec& arVoiceSpec,
             u32 au32Ident)
    : Object(),
      mFactory(arFactory),
      mIdent(au32Ident),
      mu8PlaybackState(E_PLAYBACK_STATE_INVALID),
      mu8RemoveState(E_VOICE_REMOVE_ALIVE),
      mu32SlotCount(arVoiceSpec.GetSlotCount()),
      mu32SendCount(arVoiceSpec.GetSendCount()),
      mu32InputParameterCount(arVoiceSpec.GetParameterCount()
                              - arVoiceSpec.GetOutputParameterCount()),
      mu32OutputParameterCount(arVoiceSpec.GetOutputParameterCount())
{
    size_t luOffset = auClientSize;
    mOffsets.muSlotOffset = static_cast<u16>(luOffset);
    luOffset += sizeof(Slot) * mu32SlotCount;
    mOffsets.muSendOffset = static_cast<u16>(luOffset);
    luOffset += sizeof(Send) * mu32SendCount;
    mOffsets.muInputParameterOffset = static_cast<u16>(luOffset);
    luOffset += sizeof(InputParameter) * mu32InputParameterCount;
    mOffsets.muOutputParameterOffset = static_cast<u16>(luOffset);

    for (u32 luSend = 0; luSend < mu32SendCount; ++luSend)
        ::new (&GetSend(luSend)) Send(arVoiceSpec.mapSendName[luSend]);

    u32 luSlot = 0;
    u32 luInput = 0;
    u32 luOutput = 0;
    const VoiceSchema& lrVoiceSchema = arVoiceSpec.GetVoiceSchema();
    for (u32 luFeature = 0; luFeature < lrVoiceSchema.GetFeatureSchemaCount();
         ++luFeature)
    {
        const FeatureSchema& lrFeature = lrVoiceSchema.GetFeatureSchema(luFeature);
        for (u32 lu = 0; lu < lrFeature.GetSlotSchemaCount(); ++lu)
            ::new (&GetSlot(luSlot++)) Slot(*lrFeature.GetSlotSchema(lu));

        for (u32 lu = 0; lu < lrFeature.GetParameterSchemaCount(); ++lu)
        {
            const ParameterSchema& lrParameter = *lrFeature.GetParameterSchema(lu);
            if (lrParameter.GetDirection() == E_PARAMETER_OUTPUT)
                ::new (&GetOutputParameter(luOutput++)) OutputParameter(lrParameter);
            else
                ::new (&GetInputParameter(luInput++)) InputParameter(lrParameter);
        }
    }
}

Slot::Slot()
    : mName(), mpContentClass(0), mhContent(), mpImpl(0), mu8Attach(0),
      mu8Playing(0), mu16PluginOffset(0)
{
}

Slot::Slot(const SlotSchema& arSchema)
    : mName(arSchema.mName), mpContentClass(arSchema.mpContentClass), mhContent(),
      mpImpl(0), mu8Attach(0), mu8Playing(0), mu16PluginOffset(0)
{
}

// @ 0x826C77F8. Bind ahContent iff its ContentClass matches this slot's authored
// mpContentClass (Entity mName + mTypeName word-for-word). The incoming
// Handle<Content> arrives by value; its ref is released on every return path by the
// Handle destructor.
bool Slot::Attach(Voice& arVoice, Handle<Content> ahContent)
{
    if (ahContent)                              // non-empty incoming handle
    {
        Content& lrContent = *ahContent;        // *a3 (non-null here)

        // content.mContentSpec -> ContentType -> ContentClass (the class the incoming
        // content belongs to).
        const ContentType&  lkrType  = lrContent.GetContentSpec().GetContentType();
        const ContentClass& lkrClass = lkrType.GetContentClass();

        // Inlined ContentClass::operator== : Entity mName + mTypeName match against
        // the class this slot was authored to accept.
        const u32* lpuThis  = reinterpret_cast<const u32*>(mpContentClass);
        const u32* lpuOther = reinterpret_cast<const u32*>(&lkrClass);
        bool lbMatch = (lpuThis[0] == lpuOther[0]) && (lpuThis[1] == lpuOther[1]);

        if (lbMatch)
        {
            Detach(arVoice);          // drop any content currently bound

            // Handle<Content>::operator= @ 0x826A7070 (ARTIST 0x826C7878).
            mhContent = ahContent;

            mhContent->OnAttach(arVoice, *this);
            return true;
        }
    }

    // ARTIST 0x826C7890 / 0x826C78AC: the by-value input handle is destroyed on
    // both paths.
    return false;
}

// ARTIST @ 0x826C78C8. Resolve the authored slot name and let that slot enforce
// its ContentClass contract before binding the content handle.
bool Voice::Attach(Name akName, Handle<Content>& arhContent)
{
    Slot* lpSlot = FindNamedSlot(akName);
    if (lpSlot)
        return lpSlot->Attach(*this, arhContent);
    return false;
}

// @ 0x826ACA78. Notify the bound content, release the handle, and clear the
// pending/attached latch. The handle assignment performs the Object ref drop.
void Slot::Detach(Voice& arVoice)
{
    if (mhContent)
    {
        mhContent->OnDetach(arVoice, *this);

        // The null Handle assignment routes through Handle::operator= @ 0x826A7070
        // (ARTIST 0x826ACAE8), releasing this slot's owned reference.
        mhContent = Handle<Content>();
        mu8Attach = 0;
    }
}

// @ 0x82693428.
bool Slot::Play(PlayerVoice& arVoice, u32 au32Param)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (!mu8Playing)
    {
        CGS_ASSERT(mhContent, "mpObject");
        if (mpImpl->DoPlay(*this, arVoice, *mhContent, au32Param))
            mu8Playing = 1;
    }
    return mu8Playing != 0;
}

// @ 0x826934F8.
bool Slot::Stop(PlayerVoice& arVoice)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (mu8Playing)
    {
        CGS_ASSERT(mhContent, "mpObject");
        if (mpImpl->DoStop(*this, arVoice, *mhContent))
            mu8Playing = 0;
    }
    return mu8Playing == 0;
}

// @ 0x826935D0.
void Slot::HandleAttach(Voice& arVoice)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (mu8Attach != 2)
    {
        CGS_ASSERT(mhContent, "mpObject");
        mpImpl->DoPostAttach(*this, arVoice, *mhContent);
        mu8Attach = 2;
    }
}

// @ 0x82693690.
void Slot::HandleDetach(Voice& arVoice)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (mu8Attach == 2)
    {
        CGS_ASSERT(mhContent, "mpObject");
        mpImpl->DoPreDetach(*this, arVoice, *mhContent);
        mu8Attach = 0;
    }
}

// @ 0x82693B10. Commit an asynchronous attachment once its content reaches
// LOADED, then let a playing player slot advance its type-specific engine. A
// false update result stops the slot and publishes the Voice STOPPED state.
void Slot::Update(System* apSystem, Voice& arVoice, PlayerVoice* apPlayerVoice,
                  f32 af32DeltaTime)
{
    if (mu8Attach == 1)
    {
        CGS_ASSERT(mhContent, "mpObject");
        if (mhContent->GetContentState() == E_CONTENT_STATE_LOADED)
            HandleAttach(arVoice);
    }

    if (apPlayerVoice && mu8Playing)
    {
        CGS_ASSERT(mpImpl, "mpImpl");
        CGS_ASSERT(mhContent, "mpObject");
        if (!mpImpl->DoUpdatePlaying(apSystem, *this, *apPlayerVoice,
                                     *mhContent, af32DeltaTime))
        {
            arVoice.SetPlaybackState(E_PLAYBACK_STATE_STOPPED);
            mu8Playing = 0;
        }
    }
}

// @ 0x826C0810. Detach current content, then return this slot implementation's
// allocation through Voice -> Factory -> Environment -> IResourceAllocator. The
// ARTIST code reaches the same objects at +8/+0xC/+0x30 and sends a Resource whose
// first lane is mpImpl; named access preserves that route across x64 widening.
void Slot::Release(Voice& arVoice)
{
    Detach(arVoice);

    CGS_ASSERT(mpImpl, "mpImpl");
    Factory& lrFactory = const_cast<Factory&>(arVoice.GetFactory());
    rw::Resource lResource;
    lResource.m_baseResources[0] = mpImpl;
    lrFactory.GetEnvironment().GetAllocator()->DoFree(lResource);
    mpImpl = 0;
}

// ============================================================================
// CgsSound::Playback::Voice -- name-lookup / parameter / lifecycle surface (wave 11).
//
//   Voice::Connect                    @ 0x826ACC90
//   Voice::FindNamedSlot/Send/...     @ 0x826ACAF8 / B68 / BD8 / CC30
//   Voice::GetIndexOfSlot/...         @ 0x826ACE60 / EC8 / F30
//   Voice::GetOutputParameter         @ 0x82693A20
//   Voice::GetParameter               @ 0x826ACDC8
//   Voice::SetParameter               @ 0x826ACD38
//   Voice::SetPlaybackState           @ 0x82680E18
//   Voice::Update                     @ 0x826A24F0
//   Voice::~Voice                     @ 0x826D7AB0 (scalar deleting destructor home)
//
// The find/index loops linear-scan the tail tables; GetOutputParameter random-accesses
// the output-parameter table at mOffsets.muOutputParameterOffset (stride 12).
// ============================================================================

// @ 0x826ACC90. Resolve akName to a named Send, then forward the submix handle to
// the type-specific DoConnectSend (vtable byte 0x18). The Send index is recovered from
// the descriptor's offset into the send table (stride sizeof(Send)==12).
bool Voice::Connect(Name akName, Handle<SubmixVoice>& arhSubmix)
{
    // ARTIST 0x826ACC90: the found Send's element index (pointer difference, divw by 12) and the
    // submix through Handle::operator* (its assert on CgsHandle.h:305 at 0x826ACCF0).
    Send* lpSend = FindNamedSend(akName);
    if (lpSend)
    {
        const u32 lu32Index = static_cast<u32>(lpSend - &GetSend(0));
        return DoConnectSend(lu32Index, &*arhSubmix);
    }
    return false;
}

// @ 0x826ACAF8. Linear search the slot table for a name match.
// DWARF CgsVoice.h:985/1005 (header-inline in the original, lu32I h:987/1007): a plain for loop,
// the name compared as `entry.GetName() == lName` (Name::operator== tests its argument first: ARTIST
// cmplw lName, entry at 0x826ACB30 / 0x826ACBA0).
Slot* Voice::FindNamedSlot(Name akName)
{
    for (uint32_t lu32I = 0; lu32I < mu32SlotCount; ++lu32I)   // *(this+0x14)
    {
        if (GetSlot(lu32I).GetName() == akName)
            return &GetSlot(lu32I);
    }
    return 0;
}

// @ 0x826ACB68. Linear search the send table for a name match.
// DWARF CgsVoice.h:985/1005 (header-inline in the original, lu32I h:987/1007): a plain for loop,
// the name compared as `entry.GetName() == lName` (Name::operator== tests its argument first: ARTIST
// cmplw lName, entry at 0x826ACB30 / 0x826ACBA0).
Send* Voice::FindNamedSend(Name akName)
{
    for (uint32_t lu32I = 0; lu32I < mu32SendCount; ++lu32I)   // *(this+0x18)
    {
        if (GetSend(lu32I).GetName() == akName)
            return &GetSend(lu32I);
    }
    return 0;
}

// @ 0x826ACBD8. Linear search the input-parameter table for a name match.
InputParameter* Voice::FindNamedInputParameter(Name akName)
{
    u32 lu32Count = mu32InputParameterCount;   // *(this+0x1C)
    for (u32 lu32I = 0; lu32I < lu32Count; ++lu32I)
    {
        InputParameter* lpParam = &GetInputParameter(lu32I);
        if (lpParam->GetName() == akName)      // DWARF h:1030 lParam; Name::operator== compares the argument first (cmplw akName, param 0x826ACC10)
            return lpParam;
    }
    return 0;
}

// @ 0x826ACC30. Linear search the output-parameter table for a name match.
OutputParameter* Voice::FindNamedOutputParameter(Name akName)
{
    // DWARF CgsVoice.h:1040 (lu32I h:1042): the same loop as FindNamedInputParameter.
    for (uint32_t lu32I = 0; lu32I < mu32OutputParameterCount; ++lu32I)   // *(this+0x20)
    {
        OutputParameter* lpParam = &GetOutputParameter(lu32I);
        if (lpParam->GetName() == akName)
            return lpParam;
    }
    return 0;
}

// @ 0x826ACE60. Return the index of the named slot, or (u32)-1 if absent.
u32 Voice::GetIndexOfSlot(Name akName) const
{
    for (uint32_t lu32I = 0; lu32I < mu32SlotCount; ++lu32I)                      // *(this+0x14)
    {
        if (GetSlot(lu32I).GetName() == akName)
            return lu32I;
    }
    return static_cast<u32>(-1);
}

// @ 0x826ACEC8. Return the index of the named input parameter, or (u32)-1 if absent.
u32 Voice::GetIndexOfInputParameter(Name akName) const
{
    for (uint32_t lu32I = 0; lu32I < mu32InputParameterCount; ++lu32I)           // *(this+0x1C)
    {
        if (GetInputParameter(lu32I).GetName() == akName)
            return lu32I;
    }
    return static_cast<u32>(-1);
}

// @ 0x826ACF30. Return the index of the named output parameter, or (u32)-1.
u32 Voice::GetIndexOfOutputParameter(Name akName) const
{
    for (uint32_t lu32I = 0; lu32I < mu32OutputParameterCount; ++lu32I)          // *(this+0x20)
    {
        if (GetOutputParameter(lu32I).GetName() == akName)
            return lu32I;
    }
    return static_cast<u32>(-1);
}

// @ 0x82693A20. Random-access the output-parameter table (bounds-checked). The table
// is embedded in the Voice tail allocation at mOffsets.muOutputParameterOffset; element
// stride is sizeof(OutputParameter)==12 (Name + 2 floats).
OutputParameter& Voice::GetOutputParameter(u32 au32Index) const
{
    // ARTIST streams both operands (CgsVoice.h:1539, strings 0x820AE51C / 0x820AE554).
    CGS_ASSERT_STREAM_AT(au32Index < mu32OutputParameterCount,
        "lu32Index" << au32Index << " mu32OutputParameterCount" << mu32OutputParameterCount,
        CGS_ARTIST_FILE, 1539);

    u8* lpu8Table = const_cast<u8*>(reinterpret_cast<const u8*>(this))
                    + mOffsets.muOutputParameterOffset;
    return reinterpret_cast<OutputParameter*>(lpu8Table)[au32Index];
}

// The three sibling tail-table accessors (bodied 2026-08-25, faithful-audio-
// engine phase B5): the identical GetOffsetObject pattern over the Offsets
// words the header pins (muSlotOffset/muSendOffset/muInputParameterOffset --
// Voice +0x24/+0x26/+0x28), each against its own count word, exactly as
// GetOutputParameter above.
Slot& Voice::GetSlot(u32 au32Index) const
{
    // ARTIST streams both operands (CgsVoice.h:1509, strings 0x820AE51C / 0x820AE50C).
    CGS_ASSERT_STREAM_AT(au32Index < mu32SlotCount,
        "lu32Index" << au32Index << " mu32SlotCount" << mu32SlotCount, CGS_ARTIST_FILE, 1509);
    u8* lpu8Table = const_cast<u8*>(reinterpret_cast<const u8*>(this))
                    + mOffsets.muSlotOffset;
    return reinterpret_cast<Slot*>(lpu8Table)[au32Index];
}

Send& Voice::GetSend(u32 au32Index) const
{
    // ARTIST streams both operands (CgsVoice.h:1519, strings 0x820AE51C / 0x820AE528).
    CGS_ASSERT_STREAM_AT(au32Index < mu32SendCount,
        "lu32Index" << au32Index << " mu32SendCount" << mu32SendCount, CGS_ARTIST_FILE, 1519);
    u8* lpu8Table = const_cast<u8*>(reinterpret_cast<const u8*>(this))
                    + mOffsets.muSendOffset;
    return reinterpret_cast<Send*>(lpu8Table)[au32Index];
}

InputParameter& Voice::GetInputParameter(u32 au32Index) const
{
    // ARTIST streams both operands (CgsVoice.h:1529, strings 0x820AE51C / 0x820AE538).
    CGS_ASSERT_STREAM_AT(au32Index < mu32InputParameterCount,
        "lu32Index" << au32Index << " mu32InputParameterCount" << mu32InputParameterCount,
        CGS_ARTIST_FILE, 1529);
    u8* lpu8Table = const_cast<u8*>(reinterpret_cast<const u8*>(this))
                    + mOffsets.muInputParameterOffset;
    return reinterpret_cast<InputParameter*>(lpu8Table)[au32Index];
}

// ARTIST 0x826ACDC8. Inputs use RenderWare's ordered floating-point min/max
// Clamp helper. Outputs return the stored value. A missing name yields the CRT NaN.
f32 Voice::GetParameter(Name akName)
{
    if (InputParameter* lpInput = FindNamedInputParameter(akName))
    {
        return rw::math::fpu::Clamp(lpInput->GetValueRaw(), lpInput->GetMin(), lpInput->GetMax());
    }

    if (OutputParameter* lpOutput = FindNamedOutputParameter(akName))
        return lpOutput->Get();

    return std::numeric_limits<f32>::quiet_NaN();
}

// @ 0x826ACD38. Set the value of the input parameter at as32Index (the caller passes its
// name for an integrity assert). Marks the parameter changed iff the value moved.
void Voice::SetParameter(s32 as32Index, f32 af32Value, Name akParamName)
{
    InputParameter& lrParam = GetInputParameter(static_cast<u32>(as32Index));

    // *a5 == *result : the caller's name must equal the resolved parameter's name.
    CGS_ASSERT_AT(lrParam.GetName() == akParamName,
        "lParam.GetName() == lParamName",
            CGS_ARTIST_FILE, 1288 /* ARTIST 0x826ACD78 */);


    bool lbChanged = (lrParam.GetValueRaw() != af32Value);   // fcmpu mf32Value(@0xC) vs value
    lrParam.SetValueRaw(af32Value);                          // stfs @0xC
    lrParam.SetChanged(lbChanged);                           // stb  @0x10
}

// @ 0x826A24F0. Per-frame tick. While ALIVE, update each slot then run the type-specific
// DoUpdate; while REMOVING, run DoRemove and advance to REMOVED when it reports done.
// (mu8RemoveState @+0x11: 1=ALIVE, 2=REMOVING, 3=REMOVED.)
// DWARF (_compile/BrnSoundUnity.cpp, CgsVoice.cpp:217) lists only the loop index lu32I: the remove
// state is tested on the member itself.
void Voice::Update(System* apSystem, f32 af32DeltaTime)
{
    if (mu8RemoveState == E_VOICE_REMOVE_ALIVE)   // *(this+0x11)
    {
        for (u32 lu32I = 0; lu32I < mu32SlotCount; ++lu32I)   // *(this+0x14)
        {
            // asm: r4=System*, r5=this(Voice&), r6=this(PlayerVoice&), f1=dt.
            GetSlot(lu32I).Update(apSystem, *this, static_cast<PlayerVoice*>(this), af32DeltaTime);
        }
        DoUpdate(apSystem, af32DeltaTime);      // vtable byte 0x14
    }
    else if (mu8RemoveState == E_VOICE_REMOVE_REMOVING)
    {
        if (DoRemove())                         // vtable byte 0x1C
            mu8RemoveState = E_VOICE_REMOVE_REMOVED;
    }
}

// Base removal poll: done while the playback-state byte is non-zero.
bool Voice::DoRemove()
{
    return mu8PlaybackState != E_PLAYBACK_STATE_INVALID;
}

// @ 0x826D7AB0. Compiler-synthesised `scalar deleting destructor' for Voice, expressed
// as the plain virtual destructor. ~Voice() member/base teardown is compiler-generated;
// the X360 custom-allocator tail (operator delete) folds into the host delete-expression
// at the call site, so no member teardown is emitted here.
Voice::~Voice()
{
    for (u32 luSlot = 0; luSlot < mu32SlotCount; ++luSlot)
        GetSlot(luSlot).Release(*this);
}

// @ 0x826C0D08. Snapshot the factory, run the most-derived non-deleting
// destructor, then return the shared Voice/tail-table carve to the factory's
// environment allocator as a one-lane Resource.
void Voice::DoDispose()
{
    Factory& lrFactory = const_cast<Factory&>(mFactory);
    this->~Voice();

    lrFactory.GetEnvironment().Free(this);
}

} // namespace Playback
} // namespace CgsSound

#undef CGS_ARTIST_FILE
