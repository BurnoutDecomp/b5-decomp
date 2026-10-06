#pragma once

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// FLAG PC-platform leaf: bounded, opt-in host diagnostics for authored AS prompt
// variables. Observe the existing resolver and native hash; never supply values.
class AptPromptDiagnosticsPC
{
public:
    AptPromptDiagnosticsPC(const char* pOperation, AptValue* pScope,
                           AptValue* pTarget, const EAStringC* pName,
                           AptValue* pValue = nullptr)
        : mpOperation(pOperation), mpScope(pScope), mpTarget(pTarget),
          mpContext(pScope), mpName(pName), mpValue(pValue) {}

    void SetContext(AptValue* pContext) { mpContext = pContext; }
    void SetValue(AptValue* pValue) { mpValue = pValue; }

    ~AptPromptDiagnosticsPC()
    {
        static const bool sbEnabled = []() {
            const char* pEnv = std::getenv("BRN_APT_PROMPT_DIAG");
            return pEnv != nullptr && pEnv[0] == '1';
        }();
        if (!sbEnabled || mpName == nullptr)
            return;
        const char* pName = mpName->GetBuffer();
        if (std::strcmp(pName, "mAlignment") != 0 &&
            std::strcmp(pName, "mPromptType") != 0 &&
            std::strcmp(pName, "mText") != 0 &&
            std::strcmp(pName, "mCustomText") != 0)
            return;

        static unsigned int suLines = 0;
        if (suLines >= 4096)
        {
            if (suLines == 4096)
            {
                ++suLines;
                CgsDev::Log::WriteToLog("[apt-prompt] BUDGET EXHAUSTED; later silence is unobserved.\n");
            }
            return;
        }
        ++suLines;

        AptNativeHash* const pHash = (mpContext != nullptr && mpContext->getIsDefined())
            ? mpContext->GetNativeHashVirtual() : nullptr;
        AptValue* const pStored = pHash != nullptr ? pHash->Lookup(*mpName) : nullptr;
        EAStringC valueText;
        EAStringC storedText;
        StringValue(mpValue, &valueText);
        StringValue(pStored, &storedText);
        char output[768];
        std::snprintf(output, sizeof(output),
            "[apt-prompt] %s name='%s' scope=%p('%s') target=%p context=%p('%s') parent='%s' "
            "value=%p type=%d defined=%d text='%s' hash=%p storedType=%d storedDefined=%d stored='%s'\n",
            mpOperation, pName, static_cast<void*>(mpScope), ContextName(mpScope),
            static_cast<void*>(mpTarget), static_cast<void*>(mpContext), ContextName(mpContext),
            ParentName(mpContext), static_cast<void*>(mpValue), ValueType(mpValue),
            mpValue != nullptr && mpValue->getIsDefined() ? 1 : 0, valueText.GetBuffer(),
            static_cast<void*>(pHash), ValueType(pStored),
            pStored != nullptr && pStored->getIsDefined() ? 1 : 0, storedText.GetBuffer());
        CgsDev::Log::WriteToLog(output);
    }

private:
    static int ValueType(AptValue* pValue)
    {
        return pValue != nullptr ? static_cast<int>(pValue->getVtblIndex()) : -1;
    }
    static AptCIH* ContextNode(AptValue* pValue)
    {
        if (pValue != nullptr &&
            (pValue->getVtblIndex() == AptVFT_CharacterInstHandle ||
             pValue->getVtblIndex() == AptVFT_CIHNone))
            return static_cast<AptCIH*>(pValue);
        return nullptr;
    }
    static const char* ContextName(AptValue* pValue)
    {
        AptCIH* const pNode = ContextNode(pValue);
        return pNode != nullptr ? pNode->GetInstanceName().GetBuffer() : "";
    }
    static const char* ParentName(AptValue* pValue)
    {
        AptCIH* const pNode = ContextNode(pValue);
        return pNode != nullptr && pNode->GetDisplayListParent() != nullptr
            ? pNode->GetDisplayListParent()->GetInstanceName().GetBuffer() : "";
    }
    static void StringValue(AptValue* pValue, EAStringC* pText)
    {
        if (pValue != nullptr && pValue->getIsDefined() &&
            (pValue->getVtblIndex() == AptVFT_StringValue ||
             pValue->getVtblIndex() == AptVFT_StringObject))
            pValue->toString(pText);
    }

    const char* mpOperation;
    AptValue* mpScope;
    AptValue* mpTarget;
    AptValue* mpContext;
    const EAStringC* mpName;
    AptValue* mpValue;
};
