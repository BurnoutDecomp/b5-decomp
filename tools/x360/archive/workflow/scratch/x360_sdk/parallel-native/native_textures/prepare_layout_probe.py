from pathlib import Path
p=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_textures/src/GameSource/Game/BrnLoadingScreenRenderer.cpp')
s=p.read_text(encoding='utf-8').replace('namespace BrnResource { LinearResourceAllocator* GetEmbeddedTextureAllocator(); }\n','').replace('ARTIST 823E8718:', 'ARTIST 823E871C:')
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
root=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_textures')
p=root/'scratch/native_texture_layout.cpp';p.parent.mkdir(exist_ok=True)
p.write_text('''#include <xtl.h>
#define private public
#include "GameSource/Game/BrnLoadingScreenRenderer.h"
#include "GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h"
#undef private
using BrnGame::LoadingScreenRenderer;
using BrnGui::NetworkPlayerImageRenderer;
extern "C" const u32 NativeTextureLayout[] = {
sizeof(renderengine::Texture),sizeof(renderengine::Texture2D),
sizeof(renderengine::Texture::Locked),sizeof(renderengine::Texture::Parameters),sizeof(renderengine::Texture2D::Parameters),
offsetof(renderengine::Texture,Format),
sizeof(LoadingScreenRenderer),offsetof(LoadingScreenRenderer,mbVisible),offsetof(LoadingScreenRenderer,muLastTime),offsetof(LoadingScreenRenderer,mfArrowRotation),offsetof(LoadingScreenRenderer,mfFade),offsetof(LoadingScreenRenderer,mbRenderInBackground),offsetof(LoadingScreenRenderer,mfBlackOverlayFade),offsetof(LoadingScreenRenderer,mbKillBlackOverlayWhenDone),offsetof(LoadingScreenRenderer,mReusableDataBuffer),offsetof(LoadingScreenRenderer,mfTimeStep),offsetof(LoadingScreenRenderer,mfRotateSpeedInterp),
sizeof(NetworkPlayerImageRenderer),offsetof(NetworkPlayerImageRenderer,mpDefaultTexture),offsetof(NetworkPlayerImageRenderer,maaLockedTextures),offsetof(NetworkPlayerImageRenderer,mapCompressedTextureBuffer),offsetof(NetworkPlayerImageRenderer,maLockedCompressedTextures),offsetof(NetworkPlayerImageRenderer,maapYUY2TextureBuffer),offsetof(NetworkPlayerImageRenderer,maaLockedYUY2Textures),offsetof(NetworkPlayerImageRenderer,maabRenderTexture),offsetof(NetworkPlayerImageRenderer,mePrepareStage),offsetof(NetworkPlayerImageRenderer,mpTextureAllocator),offsetof(NetworkPlayerImageRenderer,miCurrentRenderTexture),offsetof(NetworkPlayerImageRenderer,mbUseDefaultTexture)
};
''',encoding='utf-8')
