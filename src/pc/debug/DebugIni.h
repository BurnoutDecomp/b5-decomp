#pragma once

// FLAG PC-platform leaf: startup INI values for the engine's registered debug
// variables. The sidecar owns text and pending registrations, never engine values.
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <Windows.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace CgsDev { namespace DebugUI
{
    struct VariableManager;

    struct DebugIniEntryPC
    {
        std::string mPath;
        std::string mValue;
        bool mbMatched = false;
    };

    struct DebugIniPendingPC
    {
        const VariableManager* mpOwner;
        Variable* mpVariable;
        std::string mPath;
        std::string mValue;
    };

    struct DebugIniStatePC
    {
        std::mutex mMutex;
        std::vector<DebugIniEntryPC> maEntries;
        std::vector<DebugIniPendingPC> maPending;
    };

    inline DebugIniStatePC& GetDebugIniStatePC()
    {
        static DebugIniStatePC sState;
        return sState;
    }

    inline std::string TrimDebugIniTextPC(const std::string& lrText)
    {
        const auto liBegin = lrText.find_first_not_of(" \t\r\n");
        return liBegin == std::string::npos ? std::string() :
            lrText.substr(liBegin, lrText.find_last_not_of(" \t\r\n") - liBegin + 1);
    }

    inline std::string DebugIniPathPC(const std::string& lrText)
    {
        std::string lPath;
        for (unsigned char lcChar : TrimDebugIniTextPC(lrText))
        {
            if (lcChar == '\\') lcChar = '/';
            if (lcChar == '/' && (lPath.empty() || lPath.back() == '/')) continue;
            lPath.push_back(static_cast<char>(std::tolower(lcChar)));
        }
        while (!lPath.empty() && lPath.back() == '/') lPath.pop_back();
        return lPath;
    }

    inline void DebugIniLogPC(const char* lpcAction, const std::string& lrPath,
                              const std::string& lrValue)
    {
        char lacLog[1024];
        std::snprintf(lacLog, sizeof(lacLog), "[debug-ini] %s %.*s=%.*s\n",
            lpcAction, 640, lrPath.c_str(), 256, lrValue.c_str());
        CgsDev::Log::WriteToLog(lacLog);
    }

    inline void LoadDebugIniPC(const char* lpcPath)
    {
        auto& lrState = GetDebugIniStatePC();
        std::lock_guard<std::mutex> lLock(lrState.mMutex);
        lrState.maEntries.clear();
        lrState.maPending.clear();
        std::vector<char> laSection(65536u, '\0');
        const DWORD luCount = GetPrivateProfileSectionA("Debug", laSection.data(),
            static_cast<DWORD>(laSection.size()), lpcPath);
        if (luCount >= laSection.size() - 2u)
        {
            DebugIniLogPC("ignored truncated section", "Debug", "");
            return;
        }
        for (const char* lpcEntry = laSection.data(); *lpcEntry;
             lpcEntry += std::strlen(lpcEntry) + 1u)
        {
            const std::string lEntry(lpcEntry);
            const auto liEquals = lEntry.find('=');
            if (liEquals == std::string::npos) continue;
            const std::string lPath = DebugIniPathPC(lEntry.substr(0, liEquals));
            if (lPath.empty()) continue;
            const std::string lValue = TrimDebugIniTextPC(lEntry.substr(liEquals + 1u));
            auto lExisting = std::find_if(lrState.maEntries.begin(), lrState.maEntries.end(),
                [&](const DebugIniEntryPC& lrEntry) { return lrEntry.mPath == lPath; });
            if (lExisting != lrState.maEntries.end()) lExisting->mValue = lValue;
            else lrState.maEntries.push_back({ lPath, lValue, false });
        }
    }

    inline bool DebugIniNeedsComponentPC(const char* lpcPath)
    {
        const std::string lPrefix = DebugIniPathPC(lpcPath ? lpcPath : "") + '/';
        auto& lrState = GetDebugIniStatePC();
        std::lock_guard<std::mutex> lLock(lrState.mMutex);
        return std::any_of(lrState.maEntries.begin(), lrState.maEntries.end(),
            [&](const DebugIniEntryPC& lrEntry) { return lrEntry.mPath.compare(0, lPrefix.size(), lPrefix) == 0; });
    }

    inline void QueueDebugIniPC(const VariableManager* lpOwner, Variable* lpVariable,
                                const char* lpcPath, const char* lpcName)
    {
        const std::string lPath = DebugIniPathPC(std::string(lpcPath ? lpcPath : "") + '/' +
                                               (lpcName ? lpcName : ""));
        auto& lrState = GetDebugIniStatePC();
        std::lock_guard<std::mutex> lLock(lrState.mMutex);
        for (auto& lrEntry : lrState.maEntries)
        {
            if (lrEntry.mPath != lPath) continue;
            lrEntry.mbMatched = true;
            lrState.maPending.push_back({ lpOwner, lpVariable, lPath, lrEntry.mValue });
            break;
        }
    }

    inline void ForgetDebugIniPC(const VariableManager* lpOwner, const Variable* lpVariable = nullptr)
    {
        auto& lrState = GetDebugIniStatePC();
        std::lock_guard<std::mutex> lLock(lrState.mMutex);
        lrState.maPending.erase(std::remove_if(lrState.maPending.begin(), lrState.maPending.end(),
            [&](const DebugIniPendingPC& lrPending) { return lrPending.mpOwner == lpOwner &&
                (!lpVariable || lrPending.mpVariable == lpVariable); }), lrState.maPending.end());
    }

    inline std::size_t PendingDebugIniCountPC(const VariableManager* lpOwner)
    {
        auto& lrState = GetDebugIniStatePC();
        std::lock_guard<std::mutex> lLock(lrState.mMutex);
        return static_cast<std::size_t>(std::count_if(lrState.maPending.begin(), lrState.maPending.end(),
            [&](const DebugIniPendingPC& lrPending) { return lrPending.mpOwner == lpOwner; }));
    }

    inline bool TakePendingDebugIniPC(const VariableManager* lpOwner, DebugIniPendingPC& lrPending)
    {
        auto& lrState = GetDebugIniStatePC();
        std::lock_guard<std::mutex> lLock(lrState.mMutex);
        auto lFound = std::find_if(lrState.maPending.begin(), lrState.maPending.end(),
            [&](const DebugIniPendingPC& lrEntry) { return lrEntry.mpOwner == lpOwner; });
        if (lFound == lrState.maPending.end()) return false;
        lrPending = std::move(*lFound);
        lrState.maPending.erase(lFound);
        return true;
    }

    inline bool PrepareDebugIniValuePC(Variable& lrVariable, const std::string& lrText, Variant& lrValue)
    {
        if (lrVariable.IsReadOnly()) return false;
        const auto leType = lrVariable.GetValue().meType;
        if (lrVariable.GetValue().GetDereferenceType() == Variant::E_TYPE_NONE ||
            !lrVariable.GetValue().mValue.mpValue) return false;
        const std::string lText = TrimDebugIniTextPC(lrText);
        if (lText.empty()) return false;
        char* lpcEnd = nullptr;
        errno = 0;
        switch (leType)
        {
        case Variant::E_TYPE_PTR_FLOAT:
        {
            const f32 lfValue = std::strtof(lText.c_str(), &lpcEnd);
            if (lpcEnd == lText.c_str() || *lpcEnd || errno == ERANGE || !std::isfinite(lfValue)) return false;
            lrValue = Variant(lfValue);
            return true;
        }
        case Variant::E_TYPE_PTR_INT32:
        {
            const StringList* lpOptions = lrVariable.GetStringList();
            for (const StringList* lpOption = lpOptions; lpOption && lpOption->mpcName; ++lpOption)
            {
                if (_stricmp(lpOption->mpcName, lText.c_str()) == 0)
                { lrValue = Variant(lpOption->miValue); return true; }
            }
            const long long liValue = std::strtoll(lText.c_str(), &lpcEnd, 10);
            if (lpcEnd == lText.c_str() || *lpcEnd || errno == ERANGE ||
                liValue < (std::numeric_limits<s32>::min)() || liValue > (std::numeric_limits<s32>::max)()) return false;
            if (lpOptions)
            {
                bool lbFound = false;
                for (; lpOptions->mpcName; ++lpOptions) lbFound |= lpOptions->miValue == liValue;
                if (!lbFound) return false;
            }
            lrValue = Variant(static_cast<s32>(liValue));
            return true;
        }
        case Variant::E_TYPE_PTR_UINT32:
        {
            if (lText.front() == '-') return false;
            const unsigned long long luValue = std::strtoull(lText.c_str(), &lpcEnd, 10);
            if (lpcEnd == lText.c_str() || *lpcEnd || errno == ERANGE ||
                luValue > (std::numeric_limits<u32>::max)()) return false;
            lrValue = Variant(static_cast<u32>(luValue));
            return true;
        }
        case Variant::E_TYPE_PTR_BOOL:
            if (_stricmp(lText.c_str(), "true") == 0 || lText == "1") lrValue = Variant(true);
            else if (_stricmp(lText.c_str(), "false") == 0 || lText == "0") lrValue = Variant(false);
            else return false;
            return true;
        default: return false; // Functions, opaque pointers and non-writable values are not settings.
        }
    }

    inline u32& DebugIniRegistrationDepthPC() { static thread_local u32 suDepth = 0; return suDepth; }
    inline bool& DebugIniApplyingPC() { static thread_local bool sbApplying = false; return sbApplying; }

    inline void ReportUnmatchedDebugIniPC()
    {
        auto& lrState = GetDebugIniStatePC();
        std::lock_guard<std::mutex> lLock(lrState.mMutex);
        for (const auto& lrEntry : lrState.maEntries)
            if (!lrEntry.mbMatched) DebugIniLogPC("unmatched", lrEntry.mPath, lrEntry.mValue);
    }
} }
