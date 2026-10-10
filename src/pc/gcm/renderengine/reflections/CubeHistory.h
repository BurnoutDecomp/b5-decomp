#pragma once

#include <d3d9.h>

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: reflected vehicle materials sample the completed
    // previous cube, never a face of the texture currently being written.
    class CubeHistory
    {
    public:
        ~CubeHistory() { Release(); }
        void Release()
        {
            if (mpSnapshot) mpSnapshot->Release();
            mpSnapshot = nullptr; mpSource = nullptr; mpDevice = nullptr; mbActive = false;
        }
        bool Begin(IDirect3DDevice9* lpDevice, IDirect3DBaseTexture9* lpSource, bool lbComplete)
        {
            const bool lbReady = Prepare(lpDevice, lpSource, lbComplete);
            mpSource = lpSource;
            mbActive = lpDevice && lpSource && lpSource->GetType() == D3DRTYPE_CUBETEXTURE;
            mbSampleValid = lbReady;
            if (mbActive) lpDevice->SetTexture(13, lbReady ? mpSnapshot : nullptr);
            return lbReady;
        }
        bool Prepare(IDirect3DDevice9* lpDevice, IDirect3DBaseTexture9* lpSource, bool lbComplete)
        {
            mbActive = false;
            if (!lpDevice || !lpSource || lpSource->GetType() != D3DRTYPE_CUBETEXTURE) return false;
            auto* lpCube = static_cast<IDirect3DCubeTexture9*>(lpSource);
            D3DSURFACE_DESC lDescription = {};
            if (FAILED(lpCube->GetLevelDesc(0, &lDescription))) return false;
            if (mpDevice != lpDevice || muSize != lDescription.Width || meFormat != lDescription.Format)
            {
                Release();
                if (FAILED(lpDevice->CreateCubeTexture(lDescription.Width, 1, D3DUSAGE_RENDERTARGET,
                    lDescription.Format, D3DPOOL_DEFAULT, &mpSnapshot, nullptr))) return false;
                mpDevice = lpDevice; muSize = lDescription.Width; meFormat = lDescription.Format;
            }
            for (unsigned luFace = 0; luFace < 6; ++luFace)
            {
                IDirect3DSurface9 *lpRead = nullptr, *lpWrite = nullptr;
                const auto leFace = static_cast<D3DCUBEMAP_FACES>(luFace);
                HRESULT lhResult = mpSnapshot->GetCubeMapSurface(leFace, 0, &lpWrite);
                if (SUCCEEDED(lhResult) && lbComplete)
                    lhResult = lpCube->GetCubeMapSurface(leFace, 0, &lpRead);
                if (SUCCEEDED(lhResult))
                    lhResult = lbComplete ? lpDevice->StretchRect(lpRead, nullptr, lpWrite, nullptr, D3DTEXF_NONE)
                                          : lpDevice->ColorFill(lpWrite, nullptr, 0xFF808080u);
                if (lpRead) lpRead->Release();
                if (lpWrite) lpWrite->Release();
                if (FAILED(lhResult)) return false;
            }
            mpSource = lpSource;
            mbActive = true;
            return true;
        }
        IDirect3DBaseTexture9* Resolve(IDirect3DBaseTexture9* lpTexture) const
        {
            return mbActive && lpTexture == mpSource ? (mbSampleValid ? mpSnapshot : nullptr) : lpTexture;
        }
        IDirect3DBaseTexture9* GetSnapshot() const { return mbActive && mbSampleValid ? mpSnapshot : nullptr; }
        bool IsSource(const IDirect3DBaseTexture9* lpTexture) const { return mpSource == lpTexture; }
        bool End() { const bool lbWasActive = mbActive; mbActive = false; return lbWasActive; }
    private:
        IDirect3DDevice9* mpDevice = nullptr;
        IDirect3DBaseTexture9* mpSource = nullptr;
        IDirect3DCubeTexture9* mpSnapshot = nullptr;
        unsigned muSize = 0;
        D3DFORMAT meFormat = D3DFMT_UNKNOWN;
        bool mbActive = false;
        bool mbSampleValid = false;
    };
    inline CubeHistory& GetCubeHistory() { static CubeHistory sHistory; return sHistory; }
    inline IDirect3DBaseTexture9* ResolveCaptureTexture(IDirect3DBaseTexture9* lpTexture)
    {
        return GetCubeHistory().Resolve(lpTexture);
    }
}
