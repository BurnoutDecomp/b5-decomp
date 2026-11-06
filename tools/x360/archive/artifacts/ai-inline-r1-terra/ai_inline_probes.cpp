#include "GameSource/World/AI/Route/BrnRacingLine.h"
typedef char map_size[(sizeof(BrnAI::HardNoGoMap)==0x50)?1:-1];
typedef char section_size[(sizeof(BrnAI::SectionData)==0xB0)?1:-1];
typedef char map_align[(__alignof(BrnAI::HardNoGoMap)==16)?1:-1];
typedef char section_align[(__alignof(BrnAI::SectionData)==16)?1:-1];
__declspec(noinline) Vector2 ai_prev_left(BrnAI::HardNoGoMap* p) { return p->GetPreviousLeft(); }
__declspec(noinline) Vector2 ai_prev_right(BrnAI::HardNoGoMap* p) { return p->GetPreviousRight(); }
__declspec(noinline) Vector2 ai_current_left(BrnAI::HardNoGoMap* p) { return p->GetCurrentLeft(); }
__declspec(noinline) Vector2 ai_current_right(BrnAI::HardNoGoMap* p) { return p->GetCurrentRight(); }
__declspec(noinline) Vector2 ai_entrance(const BrnAI::SectionData* p) { return p->GetSectionEntrance(); }
__declspec(noinline) Vector2 ai_exit(const BrnAI::SectionData* p) { return p->GetSectionExit(); }
__declspec(noinline) void ai_set_entrance(BrnAI::SectionData* p, const Vector2* v) { p->SetSectionEntrance(*v); }
__declspec(noinline) void ai_set_exit(BrnAI::SectionData* p, const Vector2* v) { p->SetSectionExit(*v); }
__declspec(noinline) void ai_set_corners(BrnAI::SectionData* p, const BrnAI::AISection* a) { p->SetFastSectionCorners(a); }
