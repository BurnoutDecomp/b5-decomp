#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

// [stub] CgsLanguage::LanguageManager::IsUsingMetricUnits -- pulled in transitively by
// CgsGui::StateInterface::IsUsingMetricUnits (not used on the boot-video path). The real impl reads the
// SKU/locale; returning false (imperial) is a safe placeholder until the language manager is reconstructed.

namespace CgsLanguage
{
    bool LanguageManager::IsUsingMetricUnits() const
    {
        return false;
    }
}
