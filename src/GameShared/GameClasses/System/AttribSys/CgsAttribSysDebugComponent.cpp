#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysDebugComponent.h"

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"  // CgsDev::DebugUI::LogWindow (Prepare + SetSize)

// CgsAttribSys::AttribSysDebugComponent member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. GetName/GetPath return TU-local const char* rodata pointers
// (off_82F30E88 / off_82F30E8C), homed here as KPAC_DEBUG_COMPONENT_NAME / KPAC_DEBUG_COMPONENT_PATH.

namespace CgsAttribSys
{
    const char* KPAC_DEBUG_COMPONENT_NAME = "AttribSys";
    const char* KPAC_DEBUG_COMPONENT_PATH = "Core";

    // The component's scrolling log window (off_82F31050 == &sLogWindow); a TU-static instance
    // shared by every AttribSysDebugComponent member. OnActivate is the only member committed so
    // far, so it lives file-local here (the un-committed Construct/Destruct/Update siblings that
    // share it home to this same .cpp when their turn comes).
    static CgsDev::DebugUI::LogWindow sLogWindow;

    // The log window's caption + debug-menu path, as passed positionally to LogWindow::Prepare by
    // the X360 asm (r4 == off_82F31048 -> "Core/AttribSys" == caption; r5 == off_82F31044 -> "Log"
    // == menu path). Names follow the Prepare parameter positions, not the intuitive reading.
    const char* KPAC_LOG_WINDOW_CAPTION = "Core/AttribSys";
    const char* KPAC_LOG_WINDOW_PATH    = "Log";

    // The size passed to Window::SetSize (flt_820D87EC / flt_820D87F0).
    const f32 KF_LOG_WINDOW_WIDTH  = 320.0f;
    const f32 KF_LOG_WINDOW_HEIGHT = 60.0f;

    // X360 0x828025F8 -- chain up to the (empty) base activation (the bl folds onto the shared empty
    // thunk the disassembler mislabels BaseCollisionGenerator::Destruct), then prepare + size the
    // log window. Prepare's third arg (flags) is 0.
    void AttribSysDebugComponent::OnActivate()
    {
        DebugComponent::OnActivate();

        sLogWindow.Prepare( KPAC_LOG_WINDOW_CAPTION, KPAC_LOG_WINDOW_PATH, 0 );
        sLogWindow.SetSize( KF_LOG_WINDOW_WIDTH, KF_LOG_WINDOW_HEIGHT );
    }

    // X360 0x827DB668 -- lwz r3, off_82F30E88 -> "AttribSys".
    const char* AttribSysDebugComponent::GetName() const
    {
        return KPAC_DEBUG_COMPONENT_NAME;
    }

    // X360 0x827DB678 -- lwz r3, off_82F30E8C -> "Core".
    const char* AttribSysDebugComponent::GetPath() const
    {
        return KPAC_DEBUG_COMPONENT_PATH;
    }
}
