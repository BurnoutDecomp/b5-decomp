#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Gui/CustomRenderer/Renderers/BrnInGameMessageRenderer.h
//
// ⭐⭐ [tut-ticker] BrnGui::InGameMessageRenderer -- THE BOTTOM-OF-SCREEN TICKER
// (manager slot 8, E_INGAME_MESSAGE). The scrolling text ribbon that carries the
// training tips ("Okay, let's just check this thing still starts..."), the custom
// ticker messages (GUI event 537), the road-rules feed, the autosave notice and
// the controller-disconnect notice. Reconstructed whole 2026-08-24 from the X360
// ARTIST function set (addresses below) against the DecFIGS DWARF layout
// (references/DecFIGS/dwarfdump/.../BrnInGameMessageRenderer.h) -- every member
// name is the DWARF's, every offset was gated against the X360 bodies' loads and
// stores (console this+<n> map in the comments).
//
// The X360 function set (progress ledger names):
//   Construct 0x82455308   Prepare 0x82455558      Release 0x82455688
//   Destruct 0x82455758    Update 0x82446F30       GetID 0x82446F58
//   ResetYPos 0x82446F70   ClearAllMessages 0x8244B4E0
//   UpdateTickerMode 0x8244B570                    InGameMessage::SetupMessage 0x8244B7A0
//   AddNewMessage 0x82455790                       DrawMessages 0x82455B10
//   BufferMessagesForGameMode 0x82455E98           DrawBackground 0x8245CC20
//   RecvEvent 0x82468170   RequestNewRoadRulesScore 0x82468E20
//   RenderComponent 0x82469C68                     SetLanguageManager 0x82443FB0
//
// LAYOUT (console byte map, all X360-gated):
//   +8     maMessages[8]        (InGameMessage stride 532: text[512] + next + 7 flags
//                                + f32 width @+524 + u32 hash @+528)
//   +4264  mAutoSaveMessage     +4796  mpCurrentMessage    +4800  mePrepareStage
//   +4804  meUpdateStage        +4808  meTickerMode        +4812  meReleaseStage
//   +4816  mpHeapAllocator      +4824  mRoadRulesToShow (BitArray<64>)
//   +4832  mpOutputEventQueue   +4836  miLastRoadRuleScoreReceived
//   +4840  mfTextStartPosX      +4844  mfTextPosY
//   +4848  mBlendStateResource  +4868  mpBlendState        +4872  mTextObject (124B)
//   +4996  mpTextRenderer       +5000  mpLanguageManager   +5004  mpGuiCache
//   +5008  mfTimeRemainingInState  +5012..5014 the three alpha bytes
//   +5015..5017 the road-rules counters  +5018..5023 the six state bools
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"   // CustomRenderComponentInterface base
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"               // TextObject / TextRenderer
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                      // BitArray<64> (mRoadRulesToShow)
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"                            // CgsUnicode::CgsUtf8

namespace CgsLanguage { class LanguageManager; }
namespace CgsGraphics { struct Im2d; }

namespace BrnGui
{
    class GuiCache;

    class InGameMessageRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // DWARF BrnInGameMessageRenderer.h:51/:57/:184/:198.
        enum EPrepareStage { E_PREPARESTAGE_START = 0, E_PREPARESTAGE_DONE = 1 };
        enum EReleaseStage { E_RELEASESTAGE_START = 0, E_RELEASESTAGE_DONE = 1 };
        enum ETickerMode
        {
            E_TICKERMODE_NONE                         = 0,
            E_TICKERMODE_STARTENGINE                  = 1,
            E_TICKERMODE_OFFLINE                      = 2,
            E_TICKERMODE_SIGNEDIN                     = 3,
            E_TICKERMODE_ROADRULES                    = 4,
            E_TICKERMODE_CUSTOMMESSAGES               = 5,
            E_TICKERMODE_AUTOSAVE_MESSAGE             = 6,
            E_TICKERMODE_RECONNECT_CONTROLLER_MESSAGE = 7,
            E_TICKERMODE_NUM                          = 8
        };
        enum EUpdateStage
        {
            E_UPDATESTAGE_NOTDISPLAYED                = 0,
            E_UPDATESTAGE_FADINGIN                    = 1,
            E_UPDATESTAGE_DISPLAYING_MESSAGES         = 2,
            E_UPDATESTAGE_DISPLAYING_ROADRULES        = 3,
            E_UPDATESTAGE_DISPLAYING_AUTOSAVE_MESSAGE = 4,
            E_UPDATESTAGE_RECONNECT_CONTROLLER_MESSAGE= 5,
            E_UPDATESTAGE_FADINGOUT                   = 6,
            E_UPDATESTAGE_NUM                         = 7
        };

        // DWARF :104 -- one queued ticker line. Console stride 532.
        struct InGameMessage
        {
            static const s32 K_MESSAGE_LENGTH = 256;   // DWARF :170 (CgsUtf8[256] == 512 bytes: the
                                                       // console text block is 512B and SetupMessage
                                                       // CopyN/Print caps at 512)

            CgsUnicode::CgsUtf8 macMessageText[512];   // +0    (DWARF :171)
            InGameMessage*      mpNextMessage;         // +512  (DWARF :172)
            bool                mbInUse;               // +516  (DWARF :173)
            bool                mbIsPriorityMessage;   // +517  (DWARF :174)
            bool                mbIsRoadRuleMessage;   // +518  (DWARF :175)
            bool                mbIsCustomMessage;     // +519  (DWARF :176)
            bool                mbIsTrainingMessage;   // +520  (DWARF :177)
            bool                mbIsChallengeMessage;  // +521  (DWARF :178)
            bool                mbRepeats;             // +522  (DWARF :179)
            f32                 mfStringWidth;         // +524  (DWARF :180)
            u32                 muStringHash;          // +528  (DWARF :181)

            void Construct();   // DWARF :107 -- the zero-seed Construct() inlines into the owner

            // X360 0x8244B7A0. Format the text (0..3 params through the CgsUnicode print
            // family), hash it, and latch the seven state flags. The bool order is the
            // X360 store map (+517 priority, +518 roadRule, +519 custom, +520 training,
            // +521 challenge, +522 repeats).
            void SetupMessage(bool lbPriority, bool lbRoadRule, bool lbCustom,
                              bool lbTraining, bool lbChallenge, bool lbRepeats,
                              const CgsUnicode::CgsUtf8* lpMessageText, s32 liNumParams,
                              const CgsUnicode::CgsUtf8* lpParam1,
                              const CgsUnicode::CgsUtf8* lpParam2,
                              const CgsUnicode::CgsUtf8* lpParam3);
        };

        // ---- the CustomRenderComponentInterface virtuals -------------------------------
        virtual void Construct();                                                   // 0x82455308
        virtual bool Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                             rw::IResourceAllocator* lpHeapAllocator,
                             rw::IResourceAllocator* lpTextureAllocator);           // 0x82455558
        virtual bool Release();                                                     // 0x82455688
        virtual void Destruct();                                                    // 0x82455758
        virtual void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType);   // 0x82468170
        virtual void Update();                                                      // 0x82446F30
        virtual CgsID GetID() const;                                                // 0x82446F58
        virtual void RenderComponent(CgsGui::ImRendererSet* lpRendererSet);         // 0x82469C68

        // The component vtable @0x820CF950 (read from the image): slot 8 (GetRenderLayer)
        // is the ICF'd `return 2` body -- the ticker draws in LAYER 2, over the movie --
        // and slot 10 (GetNumTextures) the ICF'd `return 1`.
        virtual CgsGui::eCustomRenderLayer GetRenderLayer() const { return CgsGui::E_CUSTOMRENDERLAYER_2; }
        virtual s32 GetNumTextures() const { return 1; }

        // ---- the manager hand-downs ----------------------------------------------------
        // X360 0x82443FB0 (already reconstructed 2026-08-16; re-homed onto the real class).
        // Stores the language manager and, for the wide-glyph language (16), moves the
        // shared ticker layout globals (font height 28 / text Y 629).
        void* SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager);
        // The manager's SetTextRenderer third store (+0x1F474 == this renderer's +4996).
        void SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer) { mpTextRenderer = lpTextRenderer; }

    private:
        // ---- internals -----------------------------------------------------------------
        void ResetYPos();                                        // 0x82446F70
        void ClearAllMessages(bool lbClearTraining, bool lbClearChallenge);   // 0x8244B4E0
        void UpdateTickerMode();                                 // 0x8244B570
        bool AddNewMessage(bool lbPriority, bool lbRoadRule, bool lbCustom,
                           bool lbRepeats, bool lbTraining, bool lbAllowDuplicates,
                           bool lbChallenge,
                           const CgsUnicode::CgsUtf8* lpMessageText, s32 liNumParams,
                           const CgsUnicode::CgsUtf8* lpParam1,
                           const CgsUnicode::CgsUtf8* lpParam2,
                           const CgsUnicode::CgsUtf8* lpParam3);  // 0x82455790
        void DrawMessages(CgsGraphics::Im2dRenderBuffer* lpBuffer);    // 0x82455B10
        void DrawBackground(CgsGraphics::Im2dRenderBuffer* lpBuffer);  // 0x8245CC20
        void BufferMessagesForGameMode();                        // 0x82455E98
        bool RequestNewRoadRulesScore();                         // 0x82468E20 (parked -- see .cpp)

        static const s32 K_MAX_INGAME_MESSAGES = 8;              // DWARF :290

        // ---- members (DWARF order; console offsets in the header banner) ---------------
        InGameMessage               maMessages[K_MAX_INGAME_MESSAGES];   // +8
        InGameMessage               mAutoSaveMessage;            // +4264
        InGameMessage*              mpCurrentMessage;            // +4796
        EPrepareStage               mePrepareStage;              // +4800
        EUpdateStage                meUpdateStage;               // +4804
        ETickerMode                 meTickerMode;                // +4808
        EReleaseStage               meReleaseStage;              // +4812
        rw::IResourceAllocator*     mpHeapAllocator;             // +4816
        CgsContainers::BitArray<64u> mRoadRulesToShow;           // +4824
        CgsGui::GuiEventQueueSmall* mpOutputEventQueue;          // +4832
        s32                         miLastRoadRuleScoreReceived; // +4836 (Road::ChallengeIndex)
        f32                         mfTextStartPosX;             // +4840
        f32                         mfTextPosY;                  // +4844
        // +4848 mBlendStateResource / +4868 mpBlendState: the console acquires a BlendState
        // resource through the heap allocator (Release's `(**alloc + 20)(alloc, &res)` free).
        // [FLAG PC fold] the PC Im2d dispatch binds its own standard-blend frame state, so the
        // per-component BlendState resource has no PC consumer; the pair is carried as named
        // storage (never acquired) and Release's conditional free is a no-op on the null.
        void*                       mpBlendStateResourceStorage[5]; // +4848..4867 (Resource, 20B console)
        void*                       mpBlendState;                // +4868
        CgsGraphics::TextObject     mTextObject;                 // +4872
        CgsGraphics::TextRenderer*  mpTextRenderer;              // +4996
        CgsLanguage::LanguageManager* mpLanguageManager;         // +5000
        GuiCache*                   mpGuiCache;                  // +5004
        f32                         mfTimeRemainingInState;      // +5008
        u8                          mu8TextAlpha;                // +5012
        u8                          mu8BackgroundAlpha;          // +5013
        u8                          mu8BackgroundAlphaPeak;      // +5014
        u8                          mu8NextRoadRulesMessage;     // +5015
        u8                          mu8NextIntervalMessage;      // +5016
        u8                          mu8NumRoadRuleScoresShown;   // +5017
        bool                        mbEnabled;                   // +5018
        bool                        mbGamePaused;                // +5019
        bool                        mbGamePausedForDisconnect;   // +5020
        bool                        mbShowingBreakingNews;       // +5021
        bool                        mbShownStartEngineTip;       // +5022
        bool                        mbShowAutoSaveMessage;       // +5023
    };
}
