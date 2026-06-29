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
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h" // AptIsDeferredVectorFull

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
// FLAG - un-homed callees named (their bodies live in separate TUs; not invented):
//   AptExtObject::GetParam(int)                       - native-method arg fetch (apt VM).
//   AptCommunicator::FindAptComponent(AptValue*)      - the movieclip-ref overload
//        (X360 sub_8284A0B8, DWARF cpp:808); distinct from the char* overload homed here.
//   AptCommunicator::AddNewAptComponent(AptValue*, const char*) - DWARF cpp:721.
//   AptCallFunctionOpti(const char*,int,const char*,int,AptArray*) - the apt call helper.
//   gbLogGuiAudioTriggers (X360 byte_82FB5098)        - GUI-audio debug log gate.
// These are declared (not defined) below so the bodies compile and call them by name.
// ============================================================================

// ---- FLAG: un-homed externs/callees referenced by this TU (declared, not defined) ----
class AptArray;
extern bool gbLogGuiAudioTriggers;   // X360 byte_82FB5098

namespace
{
    // The apt "call ActionScript function" optimised helper the X360 reaches as
    // AptCallFunctionOpti("UpdateAll", 0, "gAptCommunicator", 1, lpArray). FLAG: body
    // is a separate apt TU; declared here so UpdateAllComponents can name it.
    void AptCallFunctionOpti(const char* lpacFunction, int liThisArg,
                             const char* lpacObject, int liNumArgs, AptArray* lpArgs);
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
    // FLAG: name strings + hashes are installed by CalculateReservedVariableHashes
    // (un-homed cpp:929); zero-initialised here.
    const char* AptCommunicator::mpacReservedVariableNames[AptCommunicator::KI_NUM_RESERVED_VARIABLES] = { 0 };
    u32         AptCommunicator::mauReservedVariablesHashes[AptCommunicator::KI_NUM_RESERVED_VARIABLES] = { 0 };

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