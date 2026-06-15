#pragma once

#include "types.hpp"

// CgsLanguage::LanguageManager - localisation/units manager. Only the metric-units
// query needed by the in-scope GUI code is declared here; the full manager (string
// tables, distance/speed formatting, etc.) is a large out-of-scope object. Members
// and the metric flag are from the DecFIGS DWARF (CgsLanguageManager.h).
namespace CgsLanguage
{
    class LanguageManager
    {
    public:
        void SetUseMetricUnits(bool lbUseMetric);
        bool IsUsingMetricUnits() const;
    };
}
