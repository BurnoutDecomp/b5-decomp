#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGameState::TakedownManagerDebugComponent::ForceTakedownCallback @ 0x823597F8
//   BrnGameState::TakedownManagerDebugComponent::GetName               @ 0x823596C8
//   BrnGameState::TakedownManagerDebugComponent::OnActivate            @ 0x82366378
//
// A debug component that exposes the takedown manager's debug toggles/buttons.
// OnActivate registers three bool toggles (pointing at the show-flag members at
// byte offsets 28/29/30 of the component) and one action button bound to
// ForceTakedownCallback. ForceTakedownCallback clears the active race-car takedown
// state and resets its tracking fields. It is invoked as a plain callback with the
// component passed back as user data, so it is modelled as a static function.
//
// The widget-registration helpers (sub_8282D800 = add-bool-toggle, sub_8282F720 =
// add-action-button) and BrnGameState::TakedownManager::RaceCarData::Clear live in
// other not-yet-reconstructed TUs; they are declared here with trap-stub bodies for
// the registration helpers and a forward declaration for Clear (compile-only gate).

namespace BrnGameState
{
    namespace TakedownManager
    {
        // Forward-declared; reconstructed in its own TU.
        struct RaceCarData
        {
            void* Clear();
        };
    }

    class TakedownManagerDebugComponent
    {
    public:
        const char* GetName() { return "TakedownManager"; }
        static void* ForceTakedownCallback(void* pContext);
        void OnActivate();
    };

    namespace
    {
        typedef void* (*TakedownButtonCallback)(void*);

        // Unresolved debug-menu helpers (other TUs). Trap stubs satisfy the
        // compile-only gate; replaced at link time.
        void DebugAddToggle(void* /*pComponent*/, bool* /*pFlag*/, const char* /*pLabel*/)
        {
            __debugbreak();
        }

        void* DebugAddButton(void* /*pComponent*/, TakedownButtonCallback /*pCallback*/,
                             void* /*pUserData*/, const char* /*pLabel*/)
        {
            __debugbreak();
            return nullptr;
        }
    }

    void* TakedownManagerDebugComponent::ForceTakedownCallback(void* pContext)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pContext);
        TakedownManager::RaceCarData* lpRaceCar =
            *reinterpret_cast<TakedownManager::RaceCarData**>(lBase + 12);

        void* lpResult = lpRaceCar->Clear();

        uintptr_t lCar = reinterpret_cast<uintptr_t>(lpRaceCar);
        *reinterpret_cast<f32*>(lCar + 12) = 0.0f;
        *reinterpret_cast<int*>(lCar + 16) = 1;
        *reinterpret_cast<int*>(lCar + 24) = 0;
        *reinterpret_cast<int*>(lCar + 28) = 1;
        *reinterpret_cast<int*>(lCar + 48) = 0;

        return lpResult;
    }

    void TakedownManagerDebugComponent::OnActivate()
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(this);

        DebugAddToggle(this, reinterpret_cast<bool*>(lBase + 29), "Show last takedown info");
        DebugAddToggle(this, reinterpret_cast<bool*>(lBase + 28), "Show vulnerability");
        DebugAddToggle(this, reinterpret_cast<bool*>(lBase + 30), "Show revenge takedown info");

        DebugAddButton(this, &TakedownManagerDebugComponent::ForceTakedownCallback,
                       this, "Force takedown");
    }
}
