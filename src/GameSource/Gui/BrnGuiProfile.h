#pragma once

// BrnGui::ProfileManager -- the GUI profile/save-load manager (X360 GuiModule member at
// +681696; Prepare @GuiModulePrepare 0x82518D68 hands it the module's heap/linear
// allocators). DWARF home: this header + BrnGuiProfile.cpp (36 out-of-line functions, 13
// header-homed incl. the MessageDisplay::OptionHandler base thunks). The real
// reconstruction is a pending campaign; the flows' Prepare signatures thread a
// ProfileManager& through to their states (X360 mangled forms of BrnHudFlow::Prepare /
// BrnScreenFlow::Prepare both end ...RNS_14ProfileManagerE).
namespace BrnGui
{
    // FLAG PC-platform leaf: empty shell so the flow Prepare chain can carry the real
    // ProfileManager& parameter; every committed state models its manager calls as
    // FLAG'd boundary no-ops until the real class lands (see BrnBootProfile.cpp).
    class ProfileManager
    {
    };
}
