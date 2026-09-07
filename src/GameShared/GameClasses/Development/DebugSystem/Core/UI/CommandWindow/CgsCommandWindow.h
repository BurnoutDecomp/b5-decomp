#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

namespace CgsDev
{
    namespace DebugUI
    {
        struct CommandWindow : public Window
        {
            static const s32 KI_MAX_INPUT = 100;

            CommandWindow();

            void Construct();
            virtual void Render(Debug2DImmediateRender* lpRender) override;
            virtual void Update(f32 lfTimeStep, InputEvent leEvent) override;

            void Register(const char* lpcName);
            void Unregister();
            void ToggleShow();
            bool IsVisible();

            virtual void OnGetFocus() override;
            virtual void OnLostFocus() override;

        protected:
            void Clear();
            void ProcessInput();
            void UpdatePosition();

        private:
            void GetCurrentItemString(char* lpcBuffer, s32 liBufferLen);

            static const f32 KF_CURSOR_BLINK_TIME;

            char macInput[KI_MAX_INPUT];
            f32  mfBlinkTime;
            bool mbCursorBlink;
        };
    }
}
