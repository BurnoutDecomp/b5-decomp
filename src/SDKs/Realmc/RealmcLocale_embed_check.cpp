// Tiny embed/compile check for the RealmcCore::Locale home: includes the header
// and touches each reconstructed entry point so the gate exercises the full TU.
#include "SDKs/Realmc/RealmcLocale.h"

namespace
{
u16* gpLocaleTemplate = nullptr;
u16* LocaleTemplateGetter() { return gpLocaleTemplate; }

void RealmcLocaleEmbedCheck()
{
    using namespace RealmcCore::Locale;

    LocaleGetStrCallback pfn = SetLocaleGetStrCallback(&LocaleTemplateGetter);
    (void)pfn;

    u16* pResult = GetString("ds", 42, static_cast<u16*>(nullptr));
    (void)pResult;
}
} // namespace
