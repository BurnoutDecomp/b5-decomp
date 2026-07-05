#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptComponentList.h"  // AptComponentList (complete type)

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT, Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (dynamic assert messages)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Containers/CgsHash.h"         // CgsContainers::CgsHash::CalculateHash
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"        // AptString::Create
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"       // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"         // AptFloat::Create
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"       // AptBoolean::Create
#include "SDKs/EATech/include/Apt/AptValueFactory.h"           // AptValueFactory::CreateArray
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"         // AptNativeFunction (Initialize's psMethod_* wrappers)
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h" // AptIsDeferredVectorFull
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"       // gAptActionInterpreter (getVariable/callFunction)
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"      // Push/PopStaticData (the call register frame)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"         // AptGetAnimationAtLevel (root-scope object resolve)
#include "SDKs/EATech/include/Apt/AptCIH.h"                     // AptGetAnimationAtLevel's AptCIH -> AptValue

extern AptActionInterpreter gAptActionInterpreter;   // AptGlobals.cpp (&dword_8324E760)

#include <cstring>   // strncpy
#include <cstdlib>   // atof

// ============================================================================
// CgsGui::AptCommunicator - reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// De-optimization notes (applied throughout):
//   * The Begin/Fire/EndAssert + BasePriorityQueue::Clear + indirect-call triplet the
//     X360 emits for each assert is the inlined CGS_ASSERT machinery (the indirect
//     call through off_82000D08 is the StrStreamBase::operator<< the buffered message
//     flows through). Plain-string asserts fold to CGS_ASSERT(cond,"msg"); the asserts
//     that stream a value are kept as an explicit CgsDev::StrStream build, matching the
//     X360 dynamic-message form.
//   * `EAStringC lScratch;  pValue->toString(&lScratch);  ...  /*dtor*/` is the codegen
//     the X360 shows as `v[0]=&unk_82F72FF8 (the empty-string sentinel); toString(v);
//     ...; EAStringC::DecreaseInternalRefCount(v[0])`. Reconstructed as a plain local.
//   * The `while(*p++) ; len = p-s-1; CalculateHash(s,len)` strlen-then-hash idiom is
//     kept as CalculateHash with an explicit length (CalculateHash takes char*,len).
//   * The packed AptValue type/flag tests `(w & 0x7F)==N`, `((w<<25)>>25)==N`,
//     `(w>>27)&1` read getVtblIndex()/getIsDefined() through the named bitfield, not
//     raw offsets.
//   * Per-frame debug logging gated on `CgsDev::Message::gxMessageFilterFlags & 1`
//     streams through CgsDev::Log::gpDebugPrint (`*gpDebugPrint << ...`).
//
// Callee homes (2026-07-05: the whole TU is now self-contained bar one debug gate):
//   AptExtObject::GetParam(int)                  - HOMED (AptExtObject.cpp @0x82ADC270).
//   FindAptComponent(AptValue*)                  - HOMED below (X360 sub_8284A0B8).
//   AddNewAptComponent(AptValue*, const char*)   - HOMED below (X360 0x82849B88).
//   Initialize / CalculateReservedVariableHashes - HOMED below (0x8285F0D8 / 0x8284A1F8).
//   AptCallFunctionOpti(...)                     - HOMED below (anonymous namespace).
// FLAG - gbLogGuiAudioTriggers (X360 byte_82FB5098) - GUI-audio debug log gate; the
// owning debug TU is un-homed, so it is DEFINED here (default false == the X360
// cold-boot .data value) rather than extern'd, keeping the TU link-clean.
// ============================================================================

class AptArray;
bool gbLogGuiAudioTriggers = false;   // X360 byte_82FB5098 (cold-boot default 0)

namespace
{
    // The C++->AS call chain the X360 reaches as
    // AptCallFunctionOpti("UpdateAll", 0, "gAptCommunicator", 1, lpArray). HOMED 2026-07-04
    // from the console chain Opti @0x82B07DB8 -> VOpti @0x82B07D58 -> Internal @0x82B07CC0 ->
    // MemberFunctionInternal @0x82B07B80, now that AptActionInterpreter::callFunction executes
    // real script functions (AptInterp_ExecuteScriptFunction). Opti/VOpti are the console
    // varargs wrappers; this build takes the single AptArray directly (the only call is
    // numArgs=1), so they collapse into the forward to Internal.

    // AptCallMemberFunctionInternal @0x82B07B80 -- push the numArgs args onto the interpreter's
    // operand stack, resolve lpacFunction as a member of lpObject, invoke it via callFunction,
    // pop the register frame + discard the result.
    void AptCallMemberFunctionInternal(const char* lpacFunction, int liThisArg,
                                       AptValue* lpObject, int liNumArgs, AptValue** lppArgs)
    {
        if (lpObject == 0)
            lpObject = static_cast<AptValue*>(AptGetAnimationAtLevel(0));

        // Push the call arguments (console loops i = numArgs-1 .. 0, AddRef'ing each).
        for (int liArg = liNumArgs - 1; liArg >= 0; --liArg)
        {
            gAptActionInterpreter.mpStack[gAptActionInterpreter.mnStackTop] = lppArgs[liArg];
            ++gAptActionInterpreter.mnStackTop;
            lppArgs[liArg]->AddRef();
        }

        // Resolve the named method on the object, run it through a fresh register frame.
        EAStringC   lFuncName(lpacFunction);   // InitFromBuffer; dtor = DecreaseInternalRefCount
        AptValue*   lpFunc      = gAptActionInterpreter.getVariable(lpObject, 0, &lFuncName, 1, 1, 0);
        AptValue**  lpSavedBase = AptScriptFunctionBase::PushStaticData();
        gAptActionInterpreter.callFunction(lpObject, lpFunc, liNumArgs, 0, 0);
        AptScriptFunctionBase::PopStaticData(lpSavedBase);
        // (liThisArg != 0 -> the console sub_82AFCFC8 this-arg cleanup; not taken for the
        //  UpdateAll call, whose thisArg is 0 -- left un-homed until a thisArg!=0 caller needs it.)
        (void)liThisArg;
        gAptActionInterpreter.stackSafePop(1);   // AptValue::_Pop -- discard the call result
    }

    // AptCallFunctionInternal @0x82B07CC0 -- resolve lpacObject (a name) to an AS value in the
    // level-0 root scope, then invoke lpacFunction on it.
    void AptCallFunctionInternal(const char* lpacFunction, int liThisArg,
                                 const char* lpacObject, int liNumArgs, AptValue** lppArgs)
    {
        AptValue* lpObject = 0;
        if (lpacObject != 0)
        {
            EAStringC lObjName(lpacObject);
            AptValue* lpRoot = static_cast<AptValue*>(AptGetAnimationAtLevel(0));
            lpObject = gAptActionInterpreter.getVariable(lpRoot, 0, &lObjName, 1, 1, 0);
        }
        AptCallMemberFunctionInternal(lpacFunction, liThisArg, lpObject, liNumArgs, lppArgs);
    }

    // AptCallFunctionOpti @0x82B07DB8 (+ VOpti) -- the single-array-arg entry the reconstruction
    // uses (numArgs==1: the one vararg is lpArgs).
    void AptCallFunctionOpti(const char* lpacFunction, int liThisArg,
                             const char* lpacObject, int liNumArgs, AptArray* lpArgs)
    {
        AptValue* lapArgSlot[1];
        lapArgSlot[0] = reinterpret_cast<AptValue*>(lpArgs);
        AptCallFunctionInternal(lpacFunction, liThisArg, lpacObject, liNumArgs, lapArgSlot);
    }
}

namespace CgsGui
{
    // ----- static-data definitions (the X360 file-static class members) -----
    AptComponentList AptCommunicator::mAptComponentList;
    s32              AptCommunicator::miNumUsedKeyValues    = 0;
    u32              AptCommunicator::muNumActivecomponents = 0;
    s32              AptCommunicator::miMaxNumUsedKeyValues = 0;
    KeyValue         AptCommunicator::maKeyValuePool[AptCommunicator::KI_MAX_KEYVALUES];
    AptValue*        AptCommunicator::mpAptInternalCommunicator = 0;
    bool             AptCommunicator::mbCircleButtonAsSelect    = false;
    CgsModule::VariableEventQueue<18432, 16> AptCommunicator::mOutAptTriggerEvents;
    // FLAG: the literal reserved-variable names live in un-exported X360 rodata (see
    // the header note); null entries here -> zero hashes -> "not reserved" for every
    // key, which is the correct off-menu-path behaviour until the names are recovered.
    const char* AptCommunicator::mpacReservedVariableNames[AptCommunicator::KI_NUM_RESERVED_VARIABLES] = { 0 };
    u32         AptCommunicator::mauReservedVariablesHashes[AptCommunicator::KI_NUM_RESERVED_VARIABLES] = { 0 };

    // @ dword_8305A6E0..8305A6F4 - the cached native-function wrappers Initialize fills.
    AptValue* AptCommunicator::psMethod_SendAptEvent            = 0;
    AptValue* AptCommunicator::psMethod_SendAptSoundEvent       = 0;
    AptValue* AptCommunicator::psMethod_SetCommunicationObject  = 0;
    AptValue* AptCommunicator::psMethod_GetComponentData        = 0;
    AptValue* AptCommunicator::psMethod_GetPlatformString       = 0;
    AptValue* AptCommunicator::psMethod_GetCircleButtonAsSelect = 0;

    // The packed AptValue type/flag word bit the apt value tests use (mbIsDefined,
    // bit 4 from the MSB of mValueBitfield == `>> 27 & 1`).
    static const u32 KU_APT_DEFINED_FLAG_MASK = 1u << 27;

    // ========================================================================
    // GetName  @ 0x82849860
    // ========================================================================
    const char* AptCommunicator::GetName()
    {
        return "CAptCommunicator";
    }

    // ========================================================================
    // Initialize  @ 0x8285F0D8  (AptExtObject::Initialize override)
    //
    // AptRegisterExtension @0x82AF7330 virtual-calls this right after GetName():
    // wrap each of the 6 sMethod_* natives in an AptNativeFunction (cached in the
    // psMethod_* globals, dword_8305A6E0..F4) and SetFunction it onto this
    // extension's member hash under its ActionScript name; then Construct the
    // out-trigger queue (unk_8305A960) and precompute the reserved-name hashes.
    // ========================================================================
    void AptCommunicator::Initialize()
    {
        psMethod_SendAptEvent = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&sMethod_SendAptEvent));
        SetFunction("SendAptEvent", psMethod_SendAptEvent);

        psMethod_SendAptSoundEvent = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&sMethod_SendAptSoundEvent));
        SetFunction("SendAptSoundEvent", psMethod_SendAptSoundEvent);

        psMethod_SetCommunicationObject = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&sMethod_SetCommunicationObject));
        SetFunction("SetCommunicationObject", psMethod_SetCommunicationObject);

        psMethod_GetComponentData = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&sMethod_GetComponentData));
        SetFunction("GetComponentData", psMethod_GetComponentData);

        psMethod_GetPlatformString = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&sMethod_GetPlatformString));
        SetFunction("GetPlatformString", psMethod_GetPlatformString);

        psMethod_GetCircleButtonAsSelect = AptExtObject::CreateNewAptFunction(
            reinterpret_cast<AptExtFunctionPtr>(&sMethod_GetCircleButtonAsSelect));
        SetFunction("GetCircleButtonAsSelect", psMethod_GetCircleButtonAsSelect);

        // Pin the permanent runtime objects against the Apt GC sweep: this extension
        // and its 6 native-function wrappers live for the process (the console pins
        // its permanent native singletons the same way -- AptValueInitialize's
        // setGCRoot(1) on each). Without the pin the GC reaped them seconds into the
        // title (the boot AV'd in the FADE_IN window; bisect-verified 2026-07-05).
        setGCRoot(1);
        if (psMethod_SendAptEvent)            psMethod_SendAptEvent->setGCRoot(1);
        if (psMethod_SendAptSoundEvent)       psMethod_SendAptSoundEvent->setGCRoot(1);
        if (psMethod_SetCommunicationObject)  psMethod_SetCommunicationObject->setGCRoot(1);
        if (psMethod_GetComponentData)        psMethod_GetComponentData->setGCRoot(1);
        if (psMethod_GetPlatformString)       psMethod_GetPlatformString->setGCRoot(1);
        if (psMethod_GetCircleButtonAsSelect) psMethod_GetCircleButtonAsSelect->setGCRoot(1);

        mOutAptTriggerEvents.Construct();
        CalculateReservedVariableHashes();
    }

    // ========================================================================
    // CalculateReservedVariableHashes  @ 0x8284A1F8
    //
    // Precompute the reserved-variable name hashes. FLAG: the literal name table is
    // un-exported X360 rodata (see the header); with null entries the hashes stay 0
    // and UpdateComponentReserved treats every key as "not reserved".
    // ========================================================================
    void AptCommunicator::CalculateReservedVariableHashes()
    {
        for (s32 li = 0; li < KI_NUM_RESERVED_VARIABLES; ++li)
        {
            const char* lpacName = mpacReservedVariableNames[li];
            if (lpacName == 0)
            {
                mauReservedVariablesHashes[li] = 0;
                continue;
            }
            const char* lpcEnd = lpacName;
            while (*lpcEnd++)
            {
            }
            mauReservedVariablesHashes[li] = CgsContainers::CgsHash::CalculateHash(
                const_cast<char*>(lpacName), static_cast<int>(lpcEnd - lpacName - 1));
        }
    }

    // ========================================================================
    // FindAptComponent(AptValue*)  @ sub_8284A0B8  (DWARF cpp:808)
    //
    // The movieclip-ref overload: linear-scan the registered components' bound
    // references (mapComponentReference) for POINTER equality; -1 on miss.
    // ========================================================================
    s32 AptCommunicator::FindAptComponent(AptValue* lpMovieClip)
    {
        CGS_ASSERT(lpMovieClip != 0,
                   "Invalid component reference sent to AptCommunicator::FindAptComponent");

        if (muNumActivecomponents != 0)
        {
            for (s32 liComponent = 0; liComponent < static_cast<s32>(muNumActivecomponents); ++liComponent)
            {
                CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
                CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

                if (mAptComponentList.GetAptValue(liComponent) == lpMovieClip)
                {
                    return liComponent;
                }
            }
        }
        return -1;
    }

    // ========================================================================
    // AddNewAptComponent  @ 0x82849B88  (DWARF cpp:721)
    //
    // Register a new component (an ONLOAD event's movieclip + name): seed the next
    // slot's hashed name (from the component name) + hashed reference name (from the
    // movieclip's toString rendering), clear its used-data count, bind the reference,
    // duplicate-check the hashed-name table (debug log on a match), store the name
    // text, and bump the active count.
    // ========================================================================
    void AptCommunicator::AddNewAptComponent(AptValue* lpMovieClip, const char* lpacName)
    {
        CGS_ASSERT(muNumActivecomponents < static_cast<u32>(AptComponentList::KU_MAX_COMPONENTS),
                   "Trying to add too many components in AptCommunicator::AddNewAptComponent");
        CGS_ASSERT(lpMovieClip != 0, "Invalid component reference in AptCommunicator::AddNewAptComponent");
        CGS_ASSERT(lpacName != 0, "Invalid name sent to AptCommunicator::AddNewAptComponent");

        // Render the movieclip reference to text (its AS path) for the reference hash.
        EAStringC lRefText;
        lpMovieClip->toString(&lRefText);

        const char* lpcNameEnd = lpacName;
        while (*lpcNameEnd++)
        {
        }
        const u32 luNameHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacName), static_cast<int>(lpcNameEnd - lpacName - 1));

        const char* lpacRef = lRefText.GetBuffer();
        const char* lpcRefEnd = lpacRef;
        while (*lpcRefEnd++)
        {
        }
        const u32 luRefHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacRef), static_cast<int>(lpcRefEnd - lpacRef - 1));

        const s32 liNew = static_cast<s32>(muNumActivecomponents);
        mAptComponentList.SetHashedName(liNew, luNameHash);
        mAptComponentList.SetHashedReferenceName(liNew, luRefHash);
        mAptComponentList.SetUsedData(liNew, 0);
        mAptComponentList.SetAptValue(liNew, lpMovieClip);

        // Duplicate-name debug check against the already-registered components.
        for (s32 liComponent = 0; liComponent < liNew; ++liComponent)
        {
            CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
            CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

            if (mAptComponentList.GetHashedName(liComponent) == mAptComponentList.GetHashedName(liNew)
                && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                *CgsDev::Log::gpDebugPrint
                    << "Component "
                    << (lpacName ? lpacName : "<NULLSTRING>")
                    << " is being added but already exists in our List\n";
            }
        }

        mAptComponentList.SetName(liNew, lpacName);
        ++muNumActivecomponents;
        // lRefText's internal refcount drops here (X360 DecreaseInternalRefCount).
    }

    // ========================================================================
    // FindAptComponent(const char*)  @ 0x82849F48
    //
    // Hash the name and scan the registered components' hashed-name table for a match.
    // ========================================================================
    s32 AptCommunicator::FindAptComponent(const char* lpacName)
    {
        CGS_ASSERT(lpacName != 0, "Invalid name sent to AptCommunicator::FindAptComponent");

        const char* lpcEnd = lpacName;
        while (*lpcEnd++)
        {
        }
        const u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacName), static_cast<int>(lpcEnd - lpacName - 1));

        if (muNumActivecomponents != 0)
        {
            for (s32 liComponent = 0; liComponent < static_cast<s32>(muNumActivecomponents); ++liComponent)
            {
                CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
                CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

                if (mAptComponentList.GetHashedName(liComponent) == luHash)
                {
                    return liComponent;
                }
            }
        }
        return -1;
    }

    // ========================================================================
    // sMethod_GetPlatformString  @ 0x82849950
    // ========================================================================
    AptValue* AptCommunicator::sMethod_GetPlatformString(AptValue* /*pContext*/, int /*iNumParams*/)
    {
        return AptString::Create("xbox");
    }

    // ========================================================================
    // sMethod_GetCircleButtonAsSelect  @ 0x82849960
    // ========================================================================
    AptValue* AptCommunicator::sMethod_GetCircleButtonAsSelect(AptValue* /*pContext*/, int /*iNumParams*/)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "Calling Get Circle Button as Select\n";
        }
        return AptBoolean::Create(mbCircleButtonAsSelect == true);
    }

    // ========================================================================
    // sMethod_SetCommunicationObject  @ 0x82849870
    //
    // Param 0 must be an apt Object value (getVtblIndex()==AptVFT_Object==19) and be
    // defined; bind it as the internal communicator.
    // ========================================================================
    AptValue* AptCommunicator::sMethod_SetCommunicationObject(AptValue* /*pContext*/, int /*iNumParams*/)
    {
        AptValue* lpCommunicator = AptExtObject::GetParam(0);   // FLAG: GetParam un-homed

        const bool lbValid = (lpCommunicator != 0)
            && (lpCommunicator->getVtblIndex() == AptVFT_Object)
            && lpCommunicator->getIsDefined();
        CGS_ASSERT(lbValid, "Invalid Communicator sent to SetCommunicationObject");

        mpAptInternalCommunicator = lpCommunicator;
        return AptInteger::Create(0);
    }

    // ========================================================================
    // UpdateAllComponents  @ 0x828499D0
    //
    // Collect each dirty component's bound reference into a temporary vector, hand it
    // to Flash via AptCallFunctionOpti("UpdateAll"), then clear the per-frame dirty
    // flags and reset the key/value pool counter.
    // ========================================================================
    void AptCommunicator::UpdateAllComponents()
    {
        AptValue* lapDirtyRefs[AptComponentList::KU_MAX_COMPONENTS];
        s32       liNumDirty = 0;

        if (muNumActivecomponents != 0)
        {
            for (s32 liComponent = 0; liComponent < static_cast<s32>(muNumActivecomponents); ++liComponent)
            {
                CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
                CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

                // maiNumUsedData doubles as the per-frame "dirty" flag (non-zero == dirty).
                if (mAptComponentList.GetUsedData(liComponent) != 0)
                {
                    CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
                    CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

                    lapDirtyRefs[liNumDirty++] = mAptComponentList.GetAptValue(liComponent);
                }
            }

            if (liNumDirty != 0)
            {
                AptArray* lpArray = AptValueFactory::CreateArray(liNumDirty, lapDirtyRefs);
                // FLAG: AptCallFunctionOpti body is a separate apt TU.
                AptCallFunctionOpti("UpdateAll", 0, "gAptCommunicator", 1, lpArray);
            }
        }

        // Clear the per-frame dirty flags on every active component.
        for (s32 liComponent = 0; liComponent < static_cast<s32>(muNumActivecomponents); ++liComponent)
        {
            CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
            CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");
            mAptComponentList.SetUsedData(liComponent, 0);
        }

        miNumUsedKeyValues = 0;
    }

    // ========================================================================
    // UpdateComponent  @ 0x82850958
    //
    // Mirror one (key,value) text pair onto the named component's bound reference:
    //   - find the component by name; require its bound AptValue to be defined.
    //   - hash the key; if the component already stores that key, overwrite its value.
    //   - otherwise allocate a fresh KeyValue from the pool (asserting capacity).
    // The large gxMessageFilterFlags-gated block is the X360 debug dump emitted when
    // the pool is exhausted; reconstructed faithfully as logging + the capacity assert.
    // ========================================================================
    void AptCommunicator::UpdateComponent(const char* lpacName, const char* lpacKey, const char* lpacValue)
    {
        CGS_ASSERT(lpacName  != 0, "Invalid name sent to AptCommunicator::UpdateComponent");
        CGS_ASSERT(lpacKey   != 0, "Invalid key sent to AptCommunicator::UpdateComponent");
        CGS_ASSERT(lpacValue != 0, "Invalid value sent to AptCommunicator::UpdateComponent");

        const s32 liComponent = FindAptComponent(lpacName);
        if (liComponent < 0)
        {
            return;
        }

        AptValue* lpReference = mAptComponentList.GetAptValue(liComponent);
        if ((lpReference->mnValueData & KU_APT_DEFINED_FLAG_MASK) == 0)
        {
            return;
        }

        // Hash the key string.
        const char* lpcKeyEnd = lpacKey;
        while (*lpcKeyEnd++)
        {
        }
        const u32 luKeyHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacKey), static_cast<int>(lpcKeyEnd - lpacKey - 1));

        // Search this component's existing keys for a hash match.
        KeyValue* lpKeyValue = 0;
        const u8  lu8Used = mAptComponentList.GetUsedData(liComponent);
        if (lu8Used != 0)
        {
            for (s32 liKey = 0; liKey < mAptComponentList.GetUsedData(liComponent); ++liKey)
            {
                KeyValue* lpCandidate = mAptComponentList.GetKeyValue(liComponent, liKey);
                if (lpCandidate->mHashedKey == luKeyHash)
                {
                    lpKeyValue = lpCandidate;
                    break;
                }
            }
        }

        if (lpKeyValue != 0)
        {
            // Overwrite the existing value (CgsStringUtils copy guard: <128 chars).
            const char* lpcValEnd = lpacValue;
            while (*lpcValEnd++)
            {
            }
            CGS_ASSERT((lpcValEnd - lpacValue - 1) < KeyValue::KI_MAX_VALUE_LENGTH,
                       "String too long: ");
            strncpy(lpKeyValue->macValue, lpacValue, KeyValue::KI_MAX_VALUE_LENGTH);
            lpKeyValue->macValue[KeyValue::KI_MAX_VALUE_LENGTH - 1] = 0;
            return;
        }

        // No existing key -> need to add one.
        CGS_ASSERT(mAptComponentList.GetUsedData(liComponent) < AptComponentList::KI_MAX_DATA_PER_COMPONENT,
                   "Trying to add too much data to a component in AptCommunicator::UpdateComponent");

        if (miNumUsedKeyValues < KI_MAX_KEYVALUES)
        {
            const u8 lu8Slot = mAptComponentList.GetUsedData(liComponent);
            if (lu8Slot < AptComponentList::KI_MAX_DATA_PER_COMPONENT)
            {
                KeyValue* lpNew = &maKeyValuePool[miNumUsedKeyValues++];
                mAptComponentList.SetKeyValue(liComponent, lu8Slot, lpNew);
                mAptComponentList.SetUsedData(liComponent, static_cast<u8>(lu8Slot + 1));
                lpNew->mHashedKey = luKeyHash;

                const char* lpcValEnd = lpacValue;
                while (*lpcValEnd++)
                {
                }
                CGS_ASSERT((lpcValEnd - lpacValue - 1) < KeyValue::KI_MAX_VALUE_LENGTH,
                           "String too long: ");
                strncpy(lpNew->macValue, lpacValue, KeyValue::KI_MAX_VALUE_LENGTH);
                lpNew->macValue[KeyValue::KI_MAX_VALUE_LENGTH - 1] = 0;

                if (miNumUsedKeyValues > miMaxNumUsedKeyValues)
                {
                    miMaxNumUsedKeyValues = miNumUsedKeyValues;
                }
            }
        }
        else
        {
            // Pool exhausted: dump the whole component/key table (debug), then assert.
            if (muNumActivecomponents != 0)
            {
                for (s32 liComp = 0; liComp < static_cast<s32>(muNumActivecomponents); ++liComp)
                {
                    CGS_ASSERT(liComp >= 0, "Invalid Component Index");
                    CGS_ASSERT(liComp < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

                    if (mAptComponentList.GetUsedData(liComp) != 0)
                    {
                        if (CgsDev::Message::gxMessageFilterFlags & 1)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "Component -> \""
                                << mAptComponentList.GetName(liComp)
                                << "\"\n";
                        }

                        for (s32 liKey = 0; liKey < mAptComponentList.GetUsedData(liComp); ++liKey)
                        {
                            if (CgsDev::Message::gxMessageFilterFlags & 1)
                            {
                                *CgsDev::Log::gpDebugPrint
                                    << "  KeyValue["
                                    << liKey
                                    << "]";
                            }
                            if (CgsDev::Message::gxMessageFilterFlags & 1)
                            {
                                KeyValue* lpKv = mAptComponentList.GetKeyValue(liComp, liKey);
                                *CgsDev::Log::gpDebugPrint
                                    << " : "
                                    << lpKv->mHashedKey;
                            }
                            if (CgsDev::Message::gxMessageFilterFlags & 1)
                            {
                                KeyValue* lpKv = mAptComponentList.GetKeyValue(liComp, liKey);
                                *CgsDev::Log::gpDebugPrint
                                    << " : "
                                    << lpKv->macValue
                                    << "\n";
                            }
                        }
                    }
                }
            }

            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "NEW Data to send -> \"" << lpacName << "\"\n";
            }
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "  KeyValue";
            }
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << " : " << lpacKey;
            }
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << " : " << lpacValue << "\n";
            }

            CGS_ASSERT(false, "Sending too many update messages to the AptCommunicator");
        }
    }

    // ========================================================================
    // UpdateComponentReserved  @ 0x82851408
    //
    // Mirror a reserved (typed) variable onto a component's bound reference: look the
    // key up in the reserved-variable hash table, then set the apt variable as a float
    // (most reserved keys) or as a boolean (the one boolean reserved key, table index 4).
    // ========================================================================
    void AptCommunicator::UpdateComponentReserved(const char* lpacName, const char* lpacKey, const char* lpacValue)
    {
        CGS_ASSERT(lpacName  != 0, "Invalid name sent to AptCommunicator::UpdateComponent");
        CGS_ASSERT(lpacKey   != 0, "Invalid key sent to AptCommunicator::UpdateComponent");
        CGS_ASSERT(lpacValue != 0, "Invalid value sent to AptCommunicator::UpdateComponent");

        const s32 liComponent = FindAptComponent(lpacName);
        if (liComponent < 0)
        {
            return;
        }

        AptValue* lpReference = mAptComponentList.GetAptValue(liComponent);
        if ((lpReference->mnValueData & KU_APT_DEFINED_FLAG_MASK) == 0)
        {
            return;
        }

        if (AptIsDeferredVectorFull())
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Trying to add too many reserved values to the AptCommunicator. "
                          "This has been known to cause memory leaks. Increase the "
                          "iDeferedVectorSize value in CgsAptAux to stop this problem.";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        // Hash the key and find it in the reserved-variable table.
        const char* lpcKeyEnd = lpacKey;
        while (*lpcKeyEnd++)
        {
        }
        const u32 luKeyHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacKey), static_cast<int>(lpcKeyEnd - lpacKey - 1));

        s32 liReserved = 0;
        while (luKeyHash != mauReservedVariablesHashes[liReserved])
        {
            ++liReserved;
            if (liReserved >= KI_NUM_RESERVED_VARIABLES)
            {
                return;   // not a reserved variable.
            }
        }

        // Build the key EAStringC (its lifetime is the X360's InitFromBuffer/Decrease pair).
        EAStringC lKey(lpacKey);

        switch (liReserved)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
            {
                AptFloat* lpFloat = AptFloat::Create(static_cast<float>(atof(lpacValue)));
                AptExtObject::SetVariable(mAptComponentList.GetAptValue(liComponent), &lKey, lpFloat);
            }
            break;

        case 4:
            {
                AptInteger* lpBool;
                if (strcmp(lpacValue, "true") == 0)
                {
                    lpBool = AptInteger::Create(1);
                }
                else
                {
                    lpBool = AptInteger::Create(0);
                }
                AptExtObject::SetVariable(mAptComponentList.GetAptValue(liComponent), &lKey, lpBool);
            }
            break;

        default:
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled reserved variable type: " << (lpacKey ? lpacKey : "<NULLSTRING>");
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            break;
        }
        // lKey's internal refcount is dropped by its destructor here (X360 DecreaseInternalRefCount).
    }

    // ========================================================================
    // sMethod_GetComponentData  @ 0x82850618
    //
    // Param 0: the requesting movieclip; Param 1: the data key (a string value).
    // Resolve the movieclip's registered component index, hash the key, scan that
    // component's stored keys and return the matching value text as a new AptString
    // (or the empty-string sentinel if not found).
    // ========================================================================
    AptValue* AptCommunicator::sMethod_GetComponentData(AptValue* /*pContext*/, int /*iNumParams*/)
    {
        AptValue* lpMovieClip = AptExtObject::GetParam(0);   // FLAG: GetParam un-homed
        AptValue* lpKeyValue  = AptExtObject::GetParam(1);
        s32       liData      = 0;

        CGS_ASSERT(lpMovieClip != 0, "Invalid Movieclip sent to AptCommunicator::GetComponentData");

        const bool lbKeyOk = (lpKeyValue != 0)
            && ((lpKeyValue->getVtblIndex() == AptVFT_StringValue)
                || (lpKeyValue->getVtblIndex() == AptVFT_StringObject))
            && lpKeyValue->getIsDefined();
        CGS_ASSERT(lbKeyOk, "Invalid data key sent to AptCommunicator::SendAptEvent");

        // FLAG: FindAptComponent(AptValue*) is the movieclip-ref overload (un-homed,
        // X360 sub_8284A0B8 / DWARF cpp:808); distinct from FindAptComponent(const char*).
        const s32 liComponent = FindAptComponent(lpMovieClip);
        CGS_ASSERT(liComponent >= 0,
                   "An unregistered component is looking for data in AptCommunicator::GetComponentData");

        // The default result is the empty string (X360 &unk_820046A7).
        const char* lpacResult = "";

        // Render the key value to text and hash it.
        EAStringC lKeyText;
        lpKeyValue->toString(&lKeyText);
        const char* lpacKey = lKeyText.GetBuffer();
        const char* lpcKeyEnd = lpacKey;
        while (*lpcKeyEnd++)
        {
        }
        const u32 luKeyHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacKey), static_cast<int>(lpcKeyEnd - lpacKey - 1));

        while (true)
        {
            CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
            CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

            if (liData >= mAptComponentList.GetUsedData(liComponent))
            {
                break;
            }

            CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
            CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");

            KeyValue* lpStored = mAptComponentList.GetKeyValue(liComponent, liData);
            if (luKeyHash == lpStored->mHashedKey)
            {
                CGS_ASSERT(liComponent >= 0, "Invalid Component Index");
                CGS_ASSERT(liComponent < AptComponentList::KU_MAX_COMPONENTS, "Invalid Component Index");
                lpacResult = mAptComponentList.GetKeyValue(liComponent, liData)->macValue;
            }
            ++liData;
        }

        AptValue* lpReturn = AptString::Create(lpacResult);
        // lKeyText's internal refcount drops here (X360 DecreaseInternalRefCount).
        return lpReturn;
    }

    // ========================================================================
    // sMethod_SendAptEvent  @ 0x8285C360
    //
    // Params: 0 = event id (int), 1 = unique id (int), 2 = component name (string).
    // For an ONLOAD event (id 1) also fetches Param 3 (the movieclip) and registers a
    // new component. Pushes a GuiEventAptTrigger for the start/onload/transition kinds.
    // ========================================================================
    AptValue* AptCommunicator::sMethod_SendAptEvent(AptValue* /*pContext*/, int /*iNumParams*/)
    {
        AptValue* lpEventId  = AptExtObject::GetParam(0);   // FLAG: GetParam un-homed
        AptValue* lpUniqueId = AptExtObject::GetParam(1);
        AptValue* lpNameVal  = AptExtObject::GetParam(2);

        const bool lbEventOk = (lpEventId != 0)
            && ((lpEventId->mnValueData & 0x7F) == AptVFT_Integer)
            && lpEventId->getIsDefined();
        CGS_ASSERT(lbEventOk, "Invalid event id sent to AptCommunicator::SendAptEvent");

        const bool lbUniqueOk = (lpUniqueId != 0)
            && ((lpUniqueId->mnValueData & 0x7F) == AptVFT_Integer)
            && lpUniqueId->getIsDefined();
        CGS_ASSERT(lbUniqueOk, "Invalid unique id sent to AptCommunicator::SendAptEvent");

        const bool lbNameOk = (lpNameVal != 0)
            && ((lpNameVal->getVtblIndex() == AptVFT_StringValue)
                || (lpNameVal->getVtblIndex() == AptVFT_StringObject))
            && lpNameVal->getIsDefined();
        CGS_ASSERT(lbNameOk, "Invalid component name sent to AptCommunicator::SendAptEvent");

        const s32 liEventId  = lpEventId->toInteger();
        const s32 liUniqueId = lpUniqueId->toInteger();
        EAStringC lName;
        lpNameVal->toString(&lName);

        CGS_ASSERT((liEventId > 0) && (liEventId < GuiEventAptTrigger::E_APT_EVENT_NUM),
                   "Event id sent to AptCommunicator::SendAptEvent is out of range");

        AptValue* lpMovieClip = 0;
        if (liEventId == GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        {
            lpMovieClip = AptExtObject::GetParam(3);

            if (muNumActivecomponents >= static_cast<u32>(AptComponentList::KU_MAX_COMPONENTS))
            {
                // Capacity overflow: dump the component table (debug) then assert.
                for (s32 liComp = 0; liComp < static_cast<s32>(muNumActivecomponents); ++liComp)
                {
                    if (mAptComponentList.GetUsedData(liComp) != 0
                        && (CgsDev::Message::gxMessageFilterFlags & 1))
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "Component -> \""
                            << mAptComponentList.GetName(liComp)
                            << "\"\n";
                    }
                }

                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    const char* lpacBuffer = lName.GetBuffer();
                    *CgsDev::Log::gpDebugPrint
                        << "NEW Component to add -> \""
                        << (lpacBuffer ? lpacBuffer : "<NULLSTRING>")
                        << "\"\n";
                }

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Invalid number of components in AptCommunicator::SendAptEvent: "
                           << static_cast<u32>(muNumActivecomponents)
                           << " max: "
                           << static_cast<s32>(AptComponentList::KU_MAX_COMPONENTS)
                           << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }

            CGS_ASSERT(lpMovieClip != 0, "Invalid movieclip sent to AptCommunicator::SendAptEvent");

            // FLAG: AddNewAptComponent (DWARF cpp:721) is un-homed.
            AddNewAptComponent(lpMovieClip, lName.GetBuffer());
        }

        // Build and (for the queued kinds) push the trigger record.
        GuiEventAptTrigger lTrigger;
        lTrigger.meEventType       = static_cast<GuiEventAptTrigger::AptEventType>(liEventId);
        lTrigger.miUniqueId        = liUniqueId;
        lTrigger.mpacComponentName = lName.GetBuffer();
        lTrigger.mpComponentRef    = lpMovieClip;
        {
            const char* lpacName = lName.GetBuffer();
            const char* lpcNameEnd = lpacName;
            while (*lpcNameEnd++)
            {
            }
            lTrigger.muComponentNameHash = CgsContainers::CgsHash::CalculateHash(
                const_cast<char*>(lpacName), static_cast<int>(lpcNameEnd - lpacName - 1));
        }

        if (liEventId == GuiEventAptTrigger::E_APT_EVENT_ONLOAD
            || liEventId == GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE
            || liEventId == GuiEventAptTrigger::E_APT_EVENT_FRAME_TRIGGER)
        {
            mOutAptTriggerEvents.AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lTrigger.meEventType), 21, 20);
        }

        AptValue* lpReturn = AptInteger::Create(0);
        // lName's internal refcount drops here.
        return lpReturn;
    }

    // ========================================================================
    // sMethod_SendAptSoundEvent  @ 0x8285C9B8
    //
    // Params: 0 = type name, 1 = action name, 2 = label, 3 = component name (all string
    // values). Builds a GuiEventSoundTrigger (three 32-char fields + the layer derived
    // from the component name's 7th character) and pushes it onto the out queue.
    // ========================================================================
    AptValue* AptCommunicator::sMethod_SendAptSoundEvent(AptValue* /*pContext*/, int /*iNumParams*/)
    {
        AptValue* lpTypeVal   = AptExtObject::GetParam(0);   // FLAG: GetParam un-homed
        AptValue* lpActionVal = AptExtObject::GetParam(1);
        AptValue* lpLabelVal  = AptExtObject::GetParam(2);
        AptValue* lpNameVal   = AptExtObject::GetParam(3);

        const bool lbTypeOk = (lpTypeVal != 0)
            && ((lpTypeVal->getVtblIndex() == AptVFT_StringValue)
                || (lpTypeVal->getVtblIndex() == AptVFT_StringObject))
            && lpTypeVal->getIsDefined();
        CGS_ASSERT(lbTypeOk, "Invalid sound event id sent to AptCommunicator::SendAptSoundEvent");

        const bool lbActionOk = (lpActionVal != 0)
            && ((lpActionVal->getVtblIndex() == AptVFT_StringValue)
                || (lpActionVal->getVtblIndex() == AptVFT_StringObject))
            && lpActionVal->getIsDefined();
        CGS_ASSERT(lbActionOk, "Invalid unique id sent to AptCommunicator::SendAptSoundEvent");

        const bool lbLabelOk = (lpLabelVal != 0)
            && ((lpLabelVal->getVtblIndex() == AptVFT_StringValue)
                || (lpLabelVal->getVtblIndex() == AptVFT_StringObject))
            && lpLabelVal->getIsDefined();
        CGS_ASSERT(lbLabelOk, "Invalid description label sent to AptCommunicator::SendAptSoundEvent");

        CGS_ASSERT(lpNameVal != 0, "Invalid component name sent to AptCommunicator::SendAptEvent");

        EAStringC lTypeText;
        EAStringC lActionText;
        EAStringC lLabelText;
        EAStringC lNameText;
        lpTypeVal->toString(&lTypeText);
        lpActionVal->toString(&lActionText);
        lpLabelVal->toString(&lLabelText);
        lpNameVal->toString(&lNameText);

        const char* lpacName = lNameText.GetBuffer();

        if (gbLogGuiAudioTriggers && (CgsDev::Message::gxMessageFilterFlags & 1))   // FLAG: gbLogGuiAudioTriggers un-homed
        {
            const char* lpacNameLog = lNameText.GetBuffer();
            *CgsDev::Log::gpDebugPrint
                << "\nGUI AUDIO TRIGGERS: "
                << "AptCommunicator::SendAptSoundEvent() - received msg from "
                << lpacNameLog
                << "\n";
        }

        if (lpacName != 0 && *lpacName != 0)
        {
            // The layer is the digit at name[6] ('0'..'9'); name[7] must be '.' or NUL.
            const s32 liLevel = static_cast<s32>(static_cast<signed char>(lpacName[6])) - '0';
            const signed char lcNext = static_cast<signed char>(lpacName[7]);
            if (lcNext != '.' && lcNext != 0)
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Currently only up to 10 layers are handled" << lpacName << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            CGS_ASSERT((liLevel >= 0) && (liLevel <= 9), "(liLevel >= 0) && (liLevel <= 9)");

            GuiEventSoundTrigger lSound;

            // Copy the three names into the fixed fields (CgsStringUtils <32 guard each).
            {
                const char* lpacType = lTypeText.GetBuffer();
                const char* lpcEnd = lpacType;
                while (*lpcEnd++)
                {
                }
                CGS_ASSERT((lpcEnd - lpacType - 1) < GuiEventSoundTrigger::KI_EVENT_NAME_LENGTH,
                           "String too long: ");
                strncpy(lSound.macTypeName, lpacType, GuiEventSoundTrigger::KI_EVENT_NAME_LENGTH);
            }
            {
                const char* lpacAction = lActionText.GetBuffer();
                const char* lpcEnd = lpacAction;
                while (*lpcEnd++)
                {
                }
                CGS_ASSERT((lpcEnd - lpacAction - 1) < GuiEventSoundTrigger::KI_EVENT_NAME_LENGTH,
                           "String too long: ");
                strncpy(lSound.macActionName, lpacAction, GuiEventSoundTrigger::KI_EVENT_NAME_LENGTH);
            }
            {
                const char* lpacLabel = lLabelText.GetBuffer();
                const char* lpcEnd = lpacLabel;
                while (*lpcEnd++)
                {
                }
                CGS_ASSERT((lpcEnd - lpacLabel - 1) < GuiEventSoundTrigger::KI_EVENT_NAME_LENGTH,
                           "String too long: ");
                strncpy(lSound.macLabel, lpacLabel, GuiEventSoundTrigger::KI_EVENT_NAME_LENGTH);
            }
            lSound.miLayer = liLevel;

            mOutAptTriggerEvents.AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lSound.macTypeName), 22, 100);

            if (gbLogGuiAudioTriggers && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                *CgsDev::Log::gpDebugPrint
                    << "\nGUI AUDIO TRIGGERS: "
                    << "Received sound call from flash. Triggered message to gui.\n Component: "
                    << lpacName
                    << "\n Action: "
                    << lSound.macActionName
                    << "\n Label: "
                    << lSound.macLabel
                    << "\n"
                    << "\n";
            }
        }
        else
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Should never receive audio trigger without a component name defined.\n Came with action:"
                       << lActionText.GetBuffer()
                       << "\n Label: "
                       << lLabelText.GetBuffer()
                       << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        AptValue* lpReturn = AptInteger::Create(0);
        // lNameText / lLabelText / lActionText / lTypeText drop their refcounts here
        // (X360 DecreaseInternalRefCount order: name, label, action, type).
        return lpReturn;
    }
}