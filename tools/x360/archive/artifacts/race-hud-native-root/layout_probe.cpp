#define private public
#define protected public
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/Events/BrnGuiMugshotControlEvent.h"
#undef protected
#undef private
#define PIN(name,expr) typedef char name[(expr)?1:-1]
PIN(CacheRoadId, offsetof(BrnGui::GuiCache,mRoadRuleShotRoadId)==0xAC50);
PIN(CacheTime, offsetof(BrnGui::GuiCache,mbIsRoadRuleShotTime)==0xAC58);
PIN(CacheActive, offsetof(BrnGui::GuiCache,mbRoadRuleShotActive)==0xAC59);
PIN(CacheRuler, offsetof(BrnGui::GuiCache,mbRoadRuleShotLocalPlayerNewRuler)==0xAC5A);
PIN(CacheTaken, offsetof(BrnGui::GuiCache,mbRoadRuleShotTaken)==0xAC5B);
PIN(CacheStunt, offsetof(BrnGui::GuiCache,maStuntToDisplay)==0xAC5C);
PIN(CachePlayers, offsetof(BrnGui::GuiCache,maPlayerInfo)==0xAC80);
PIN(HudCache, offsetof(BrnGui::RaceMainHudState,mpCache)==0x140);
PIN(HudMugshot, offsetof(BrnGui::RaceMainHudState,mMugShotComponent)==0x649C);
PIN(HudGamertag, offsetof(BrnGui::RaceMainHudState,mMugshotOpponentGamertag)==0x64E8);
PIN(HudRoadShot, offsetof(BrnGui::RaceMainHudState,mRoadRuleShotComponent)==0x657C);
PIN(HudSize,sizeof(BrnGui::RaceMainHudState)==0x7940);
extern "C" unsigned HudNativeOffsets[] = {
 sizeof(BrnGui::RaceMainHudState),
 offsetof(BrnGui::RaceMainHudState,mpCache),
 offsetof(BrnGui::RaceMainHudState,mMugShotComponent),
 offsetof(BrnGui::RaceMainHudState,mMugshotOpponentGamertag),
 offsetof(BrnGui::RaceMainHudState,mRoadRuleShotComponent),
 offsetof(BrnGui::GuiCache,mRoadRuleShotRoadId),
 offsetof(BrnGui::GuiCache,mbRoadRuleShotActive),
 offsetof(BrnGui::GuiCache,maPlayerInfo)
};
