// CgsLanguageManager_wS34_00.cpp -- the two-argument FormatAndAddText (CgsLanguageManager.cpp
// family): add a string-table entry under a new id, copying the text of an existing id.

#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsLanguage
{
    // Look the source id up and add its text under lpcStringId. Returns AddString's result.
    bool LanguageManager::FormatAndAddText(const char* lpcStringId, const char* lpcSourceText)
    {
        const u8* lpUtf8 = FindString(lpcSourceText);
        CGS_ASSERT(lpUtf8 != 0, "lpUtf8");
        return AddString(lpcStringId, lpUtf8);
    }
}
