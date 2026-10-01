#pragma once

#include <d3d9.h>
#include <new>
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include "pc/gcm/renderengine/TextureStagingCachePCLeaf.h"

// FLAG PC-platform leaf: D3D9Ex has no MANAGED pool. Keep a SYSTEMMEM upload
// texture attached to the DEFAULT texture through COM private data. This keeps
// the engine's serialized texture header unchanged and ties both lifetimes.
// https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-updatetexture
namespace renderengine::TextureUploadPC
{
    inline const GUID KU_SHADOW_ID = {0xa2de315a,0xb29d,0x4d4c,{0x98,0x4e,0xb5,0x10,0x09,0xe1,0x77,0x04}};
    inline bool IsExtended(IDirect3DDevice9* lpDevice)
    {
        IDirect3DDevice9Ex* lpExtended = nullptr;
        const bool lbResult = lpDevice && SUCCEEDED(lpDevice->QueryInterface(__uuidof(IDirect3DDevice9Ex),
            reinterpret_cast<void**>(&lpExtended)));
        if (lpExtended) lpExtended->Release();
        return lbResult;
    }
    inline D3DPOOL StaticBufferPool(IDirect3DDevice9* lpDevice)
    { return IsExtended(lpDevice) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED; }

    class Shadow final : public IUnknown
    {
        volatile LONG miReferences = 1;
    public:
        IDirect3DBaseTexture9* mpTexture;
        StagingKey mKey;
        const UINT muLevels, muFaces;
        DWORD* mpLockFlags = nullptr;
        UINT muActiveLocks = 0;
        bool mbDirty = false;
        bool mbReusable = true;
        explicit Shadow(IDirect3DBaseTexture9* lpSource, const StagingKey& lrKey)
            : mpTexture(lpSource), mKey(lrKey), muLevels(lpSource->GetLevelCount()), muFaces(lpSource->GetType()==D3DRTYPE_CUBETEXTURE?6:1)
        {
            mpLockFlags = new (std::nothrow) DWORD[muLevels*muFaces];
            if (mpLockFlags) for (UINT luIndex=0;luIndex<muLevels*muFaces;++luIndex) mpLockFlags[luIndex]=~DWORD(0);
        }
        ~Shadow() {
            delete[] mpLockFlags;
            if (!mbReusable || muActiveLocks || mbDirty) mpTexture->Release();
            else gStagingCache.Retire(mpTexture, mKey);
        }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID lrIID,void** lppValue) override
        {
            if (!lppValue) return E_POINTER;
            *lppValue=nullptr;
            if (lrIID!=__uuidof(IUnknown)) return E_NOINTERFACE;
            *lppValue=static_cast<IUnknown*>(this); AddRef(); return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&miReferences); }
        ULONG STDMETHODCALLTYPE Release() override
        { const ULONG luCount=InterlockedDecrement(&miReferences); if(!luCount)delete this; return luCount; }
        UINT Index(UINT luLevel,UINT luFace) const
        { return luLevel<muLevels && (muFaces==1 || luFace<muFaces) ? (muFaces==1?0:luFace)*muLevels+luLevel : ~UINT(0); }
        void MarkDirty(UINT luFace)
        {
            if (muFaces==6) static_cast<IDirect3DCubeTexture9*>(mpTexture)->AddDirtyRect(static_cast<D3DCUBEMAP_FACES>(luFace),nullptr);
            else if(mpTexture->GetType()==D3DRTYPE_VOLUMETEXTURE) static_cast<IDirect3DVolumeTexture9*>(mpTexture)->AddDirtyBox(nullptr);
            else static_cast<IDirect3DTexture9*>(mpTexture)->AddDirtyRect(nullptr);
            mbDirty=true;
        }
        HRESULT Upload(IDirect3DBaseTexture9* lpDestination)
        {
            if (!mbDirty || muActiveLocks) return S_OK;
            FrameProfile::CycleScope lProfile(FrameProfile::TEXTURE_UPLOAD);
            IDirect3DDevice9* lpDevice=nullptr;
            HRESULT lhResult=lpDestination->GetDevice(&lpDevice);
            if(SUCCEEDED(lhResult)) { lhResult=lpDevice->UpdateTexture(mpTexture,lpDestination); lpDevice->Release(); }
            if(SUCCEEDED(lhResult))mbDirty=false;
            return lhResult;
        }
    };
    inline Shadow* Acquire(IDirect3DBaseTexture9* lpTexture)
    {
        Shadow* lpShadow=nullptr; DWORD luBytes=sizeof(lpShadow);
        if (!lpTexture || FAILED(lpTexture->GetPrivateData(KU_SHADOW_ID,&lpShadow,&luBytes))) return nullptr;
        return lpShadow; // GetPrivateData adds a reference for D3DSPD_IUNKNOWN.
    }
    template<class Texture,class Create> HRESULT CreatePair(IDirect3DDevice9* lpDevice,Texture** lppTexture,StagingKey lKey,Create lCreate)
    {
        if (!lppTexture) return E_POINTER;
        *lppTexture=nullptr;
        if (!lpDevice) return D3DERR_INVALIDCALL;
        if (!IsExtended(lpDevice)) {
            FrameProfile::CycleScope lProfile(FrameProfile::TEXTURE_GPU_CREATE);
            return lCreate(D3DPOOL_MANAGED,lppTexture);
        }
        Texture *lpGpuTexture=nullptr,*lpCpuTexture=nullptr;
        HRESULT lhResult;
        { FrameProfile::CycleScope lProfile(FrameProfile::TEXTURE_GPU_CREATE);
          lhResult=lCreate(D3DPOOL_DEFAULT,&lpGpuTexture); }
        if(SUCCEEDED(lhResult)) {
            FrameProfile::CycleScope lProfile(FrameProfile::TEXTURE_CPU_CREATE);
            lKey.mpDevice = lpDevice;
            lKey.muLevels = lpGpuTexture->GetLevelCount();
            lpCpuTexture = static_cast<Texture*>(gStagingCache.Take(lKey));
            FrameProfile::TextureStaging(lpCpuTexture != nullptr);
            if (!lpCpuTexture) lhResult=lCreate(D3DPOOL_SYSTEMMEM,&lpCpuTexture);
        }
        if(FAILED(lhResult)) { if(lpCpuTexture)lpCpuTexture->Release(); if(lpGpuTexture)lpGpuTexture->Release(); return lhResult; }
        Shadow* lpShadow=new(std::nothrow) Shadow(lpCpuTexture, lKey);
        if(!lpShadow) { lpCpuTexture->Release(); lpGpuTexture->Release(); return E_OUTOFMEMORY; }
        if(!lpShadow->mpLockFlags || lpCpuTexture->GetLevelCount()!=lpGpuTexture->GetLevelCount()) {
            lpShadow->mbReusable=false;
            lhResult=E_OUTOFMEMORY;
        }
        else lhResult=lpGpuTexture->SetPrivateData(KU_SHADOW_ID,static_cast<IUnknown*>(lpShadow),sizeof(IUnknown*),D3DSPD_IUNKNOWN);
        lpShadow->Release();
        if(FAILED(lhResult)) { lpGpuTexture->Release(); return lhResult; }
        *lppTexture=lpGpuTexture; return S_OK;
    }
    inline HRESULT Create2D(IDirect3DDevice9* lpDevice,UINT luWidth,UINT luHeight,UINT luNumLevels,D3DFORMAT leFormat,IDirect3DTexture9** lppTexture)
    {
        return CreatePair(lpDevice,lppTexture,{lpDevice,D3DRTYPE_TEXTURE,leFormat,luWidth,luHeight,1,luNumLevels},[&](D3DPOOL lePool,IDirect3DTexture9** lppValue) {
            return lpDevice->CreateTexture(luWidth,luHeight,luNumLevels,0,leFormat,lePool,lppValue,nullptr); });
    }
    inline HRESULT CreateCube(IDirect3DDevice9* lpDevice,UINT luEdge,UINT luNumLevels,D3DFORMAT leFormat,IDirect3DCubeTexture9** lppTexture)
    {
        return CreatePair(lpDevice,lppTexture,{lpDevice,D3DRTYPE_CUBETEXTURE,leFormat,luEdge,luEdge,6,luNumLevels},[&](D3DPOOL lePool,IDirect3DCubeTexture9** lppValue) {
            return lpDevice->CreateCubeTexture(luEdge,luNumLevels,0,leFormat,lePool,lppValue,nullptr); });
    }
    inline HRESULT CreateVolume(IDirect3DDevice9* lpDevice,UINT luWidth,UINT luHeight,UINT luDepth,UINT luNumLevels,D3DFORMAT leFormat,IDirect3DVolumeTexture9** lppTexture)
    {
        return CreatePair(lpDevice,lppTexture,{lpDevice,D3DRTYPE_VOLUMETEXTURE,leFormat,luWidth,luHeight,luDepth,luNumLevels},[&](D3DPOOL lePool,IDirect3DVolumeTexture9** lppValue) {
            return lpDevice->CreateVolumeTexture(luWidth,luHeight,luDepth,luNumLevels,0,leFormat,lePool,lppValue,nullptr); });
    }
    class Upload
    {
        IDirect3DBaseTexture9* mpDestination;
        Shadow* mpShadow;
        HRESULT mhResult = S_OK;
    public:
        explicit Upload(IDirect3DBaseTexture9* lpDestination) : mpDestination(lpDestination),mpShadow(Acquire(lpDestination))
        {
            // Bulk uploads lock native storage directly. Only a completed upload
            // proves that all native locks were released and the storage is reusable.
            if(mpShadow)mpShadow->mbReusable=false;
        }
        ~Upload() { if(mpShadow)mpShadow->Release(); }
        Upload(const Upload&)=delete;
        Upload& operator=(const Upload&)=delete;
        IDirect3DBaseTexture9* Storage() const { return mpShadow?mpShadow->mpTexture:mpDestination; }
        bool Accept(HRESULT lResult)
        { if (FAILED(lResult) && SUCCEEDED(mhResult)) mhResult=lResult; return SUCCEEDED(lResult); }
        HRESULT Finish()
        {
            if (FAILED(mhResult)) return mhResult;
            if(!mpShadow)return S_OK;
            for(UINT luFace=0;luFace<mpShadow->muFaces;++luFace)mpShadow->MarkDirty(luFace);
            const HRESULT lhResult=mpShadow->Upload(mpDestination);
            mpShadow->mbReusable=SUCCEEDED(lhResult);
            return lhResult;
        }
    };
    inline HRESULT Lock(IDirect3DBaseTexture9* lpTexture,UINT luLevel,UINT luFace,DWORD luFlags,D3DLOCKED_BOX& lrBox)
    {
        lrBox={}; if(!lpTexture)return D3DERR_INVALIDCALL;
        Shadow* lpShadow=Acquire(lpTexture);
        const UINT luLockIndex=lpShadow?lpShadow->Index(luLevel,luFace):0;
        if(lpShadow && (luLockIndex==~UINT(0) || lpShadow->mpLockFlags[luLockIndex]!=~DWORD(0))) {lpShadow->Release();return D3DERR_INVALIDCALL;}
        IDirect3DBaseTexture9* lpStorage=lpShadow?lpShadow->mpTexture:lpTexture;
        HRESULT lhResult;
        if(lpStorage->GetType()==D3DRTYPE_VOLUMETEXTURE)
            lhResult=static_cast<IDirect3DVolumeTexture9*>(lpStorage)->LockBox(luLevel,&lrBox,nullptr,luFlags);
        else {
            D3DLOCKED_RECT lRect={};
            lhResult=lpStorage->GetType()==D3DRTYPE_CUBETEXTURE
                ?static_cast<IDirect3DCubeTexture9*>(lpStorage)->LockRect(static_cast<D3DCUBEMAP_FACES>(luFace),luLevel,&lRect,nullptr,luFlags)
                :static_cast<IDirect3DTexture9*>(lpStorage)->LockRect(luLevel,&lRect,nullptr,luFlags);
            if(SUCCEEDED(lhResult)) {lrBox.pBits=lRect.pBits;lrBox.RowPitch=lRect.Pitch;}
        }
        // D3D9 records dirty regions only on level zero, including for MANAGED
        // textures. A writable sublevel must dirty its corresponding top level.
        if(SUCCEEDED(lhResult) && luLevel && !(luFlags&(D3DLOCK_READONLY|D3DLOCK_NO_DIRTY_UPDATE)))
        {
            if(lpStorage->GetType()==D3DRTYPE_CUBETEXTURE)static_cast<IDirect3DCubeTexture9*>(lpStorage)->AddDirtyRect(static_cast<D3DCUBEMAP_FACES>(luFace),nullptr);
            else if(lpStorage->GetType()==D3DRTYPE_VOLUMETEXTURE)static_cast<IDirect3DVolumeTexture9*>(lpStorage)->AddDirtyBox(nullptr);
            else static_cast<IDirect3DTexture9*>(lpStorage)->AddDirtyRect(nullptr);
        }
        if(lpShadow) { if(SUCCEEDED(lhResult)) {lpShadow->mpLockFlags[luLockIndex]=luFlags;++lpShadow->muActiveLocks;} lpShadow->Release(); }
        return lhResult;
    }
    inline HRESULT Unlock(IDirect3DBaseTexture9* lpTexture,UINT luLevel,UINT luFace)
    {
        if(!lpTexture)return D3DERR_INVALIDCALL;
        Shadow* lpShadow=Acquire(lpTexture);
        const UINT luLockIndex=lpShadow?lpShadow->Index(luLevel,luFace):0;
        if(lpShadow && (luLockIndex==~UINT(0) || lpShadow->mpLockFlags[luLockIndex]==~DWORD(0))) {lpShadow->Release();return D3DERR_INVALIDCALL;}
        IDirect3DBaseTexture9* lpStorage=lpShadow?lpShadow->mpTexture:lpTexture;
        HRESULT lhResult=lpStorage->GetType()==D3DRTYPE_VOLUMETEXTURE
            ?static_cast<IDirect3DVolumeTexture9*>(lpStorage)->UnlockBox(luLevel)
            :lpStorage->GetType()==D3DRTYPE_CUBETEXTURE
                ?static_cast<IDirect3DCubeTexture9*>(lpStorage)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(luFace),luLevel)
                :static_cast<IDirect3DTexture9*>(lpStorage)->UnlockRect(luLevel);
        if(lpShadow) {
            if(SUCCEEDED(lhResult)) {
                if(!(lpShadow->mpLockFlags[luLockIndex]&(D3DLOCK_READONLY|D3DLOCK_NO_DIRTY_UPDATE)))lpShadow->MarkDirty(luFace);
                lpShadow->mpLockFlags[luLockIndex]=~DWORD(0);--lpShadow->muActiveLocks;
                lhResult=lpShadow->Upload(lpTexture);
            }
            lpShadow->Release();
        }
        return lhResult;
    }
}
