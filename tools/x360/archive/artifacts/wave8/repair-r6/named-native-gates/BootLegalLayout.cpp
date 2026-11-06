#define private public
#define protected public
#include "GameSource/Gui/Flow/HUD/States/BrnBootLegal.h"
#undef private
#undef protected
#include <stddef.h>
typedef BrnGui::BootLegal B;
extern "C" const unsigned long boot_layout[] = {sizeof(B), __alignof(B), offsetof(B,mpGuiCache), offsetof(B,muCurrentAttractMovieIdx), offsetof(B,mbWaitForStartPressed), offsetof(B,meUpdateStage), offsetof(B,mfStageStartTime), offsetof(B,mbSignOutPending), offsetof(B,mfAcceptTime), offsetof(B,mHDCompAnimator), offsetof(B,mEsrbAnimator), offsetof(B,mStartMessageAnimator), offsetof(B,mBackgroundAnimator), offsetof(B,mSelectionMenuAnimator), offsetof(B,mSelectionMenu), offsetof(B,mSelectionMenu) + offsetof(BrnGui::SelectableGroup,miHighlightedIndex)};

typedef char boot_cache[(offsetof(B,mpGuiCache)==0x38)?1:-1];
typedef char boot_count[(offsetof(B,miNumFilesToLoad)==0x3c)?1:-1];
typedef char boot_attract[(offsetof(B,muCurrentAttractMovieIdx)==0x40)?1:-1];
typedef char boot_stage[(offsetof(B,meUpdateStage)==0x48)?1:-1];
typedef char boot_hd[(offsetof(B,mHDCompAnimator)==0x58)?1:-1];
typedef char boot_esrb[(offsetof(B,mEsrbAnimator)==0xe4)?1:-1];
typedef char boot_start[(offsetof(B,mStartMessageAnimator)==0x170)?1:-1];
typedef char boot_background[(offsetof(B,mBackgroundAnimator)==0x1fc)?1:-1];
typedef char boot_animation[(offsetof(B,mSelectionMenuAnimator)==0x288)?1:-1];
typedef char boot_menu[(offsetof(B,mSelectionMenu)==0x318)?1:-1];
typedef char boot_selection[((offsetof(B,mSelectionMenu)+offsetof(BrnGui::SelectableGroup,miHighlightedIndex))==0x3bd)?1:-1];
