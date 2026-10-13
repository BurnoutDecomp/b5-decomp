#define NOMINMAX
#include <windows.h>
#include <d3d9.h>

#include "pc/gcm/renderengine/OcclusionQueryProviderPC.h"

#include <cstdio>

// This fixture exercises the real PC D3D9 provider with native queries and indexed draws.
// It does not link the inactive WorldDraw_IndexedUP observer, activate the renderer path, or
// establish Xenos predicate polarity. D3D9 nonzero-sample drawing is the explicit host policy.
// It does not validate the original nested-scope protocol, gameplay performance, or ordinary
// shutdown ownership; the source lane keeps those limits separate from this native leaf fixture.
namespace
{
    struct NativeContext
    {
        HWND mhWindow = nullptr;
        IDirect3D9Ex* mpApi = nullptr;
        IDirect3DDevice9Ex* mpDevice = nullptr;
        D3DPRESENT_PARAMETERS mParameters = {};
    };

    struct TestVertex
    {
        float mfX;
        float mfY;
        float mfZ;
        DWORD muColour;
    };
    static_assert(sizeof(TestVertex) == 16, "D3D9 XYZ/diffuse test vertex");

    struct TriangleResult
    {
        HRESULT mahDrawResults[2] = {E_FAIL, E_FAIL};
        bool mabHasSubmissionToken[2] = {false, false};
        bool mbAllowed = true;
        bool mbAllowedAgain = true;
    };

    int siChecks = 0;
    int siFailures = 0;

    void Check(bool lbGood, const char* lpcLabel)
    {
        ++siChecks;
        if (!lbGood)
        {
            ++siFailures;
            std::printf("FAIL %s\n", lpcLabel);
        }
    }

    void Step(const char* lpcLabel)
    {
        std::printf("STEP: %s\n", lpcLabel);
        std::fflush(stdout);
    }

    void SetReason(char* lpReason, size_t luCapacity, const char* lpcWhat, HRESULT lResult = S_OK)
    {
        if (lpReason == nullptr || luCapacity == 0)
            return;
        if (lResult == S_OK)
            std::snprintf(lpReason, luCapacity, "%s", lpcWhat);
        else
            std::snprintf(lpReason, luCapacity, "%s (HRESULT 0x%08X)", lpcWhat,
                          static_cast<unsigned>(lResult));
    }

    void ReleaseContext(NativeContext& lrContext)
    {
        if (lrContext.mpDevice != nullptr)
        {
            // The provider stores native query references and does not own the device.
            renderengine::OcclusionQueryProviderPCInvalidate(lrContext.mpDevice);
            lrContext.mpDevice->Release();
            lrContext.mpDevice = nullptr;
        }
        if (lrContext.mpApi != nullptr)
        {
            lrContext.mpApi->Release();
            lrContext.mpApi = nullptr;
        }
        if (lrContext.mhWindow != nullptr)
        {
            DestroyWindow(lrContext.mhWindow);
            lrContext.mhWindow = nullptr;
        }
    }

    bool CreateNativeContext(NativeContext& lrContext, char* lpReason, size_t luCapacity)
    {
        lrContext.mhWindow = CreateWindowA("STATIC", "PC occlusion query fixture", WS_POPUP,
                                           0, 0, 64, 64, nullptr, nullptr,
                                           GetModuleHandleA(nullptr), nullptr);
        if (lrContext.mhWindow == nullptr)
        {
            SetReason(lpReason, luCapacity, "CreateWindowA failed");
            return false;
        }

        HRESULT lResult = Direct3DCreate9Ex(D3D_SDK_VERSION, &lrContext.mpApi);
        if (FAILED(lResult) || lrContext.mpApi == nullptr)
        {
            SetReason(lpReason, luCapacity, "Direct3DCreate9Ex unavailable", lResult);
            return false;
        }

        lrContext.mParameters.Windowed = TRUE;
        lrContext.mParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
        lrContext.mParameters.BackBufferFormat = D3DFMT_X8R8G8B8;
        lrContext.mParameters.BackBufferWidth = 64;
        lrContext.mParameters.BackBufferHeight = 64;
        lrContext.mParameters.BackBufferCount = 1;
        lrContext.mParameters.hDeviceWindow = lrContext.mhWindow;
        lrContext.mParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        const DWORD lauVertexProcessing[] = {
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            D3DCREATE_MIXED_VERTEXPROCESSING,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING
        };
        for (DWORD luFlags : lauVertexProcessing)
        {
            D3DPRESENT_PARAMETERS lParameters = lrContext.mParameters;
            lResult = lrContext.mpApi->CreateDeviceEx(
                D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, lrContext.mhWindow, luFlags,
                &lParameters, nullptr, &lrContext.mpDevice);
            if (SUCCEEDED(lResult) && lrContext.mpDevice != nullptr)
            {
                lrContext.mParameters = lParameters;
                ShowWindow(lrContext.mhWindow, SW_HIDE);
                return true;
            }
            if (lrContext.mpDevice != nullptr)
            {
                lrContext.mpDevice->Release();
                lrContext.mpDevice = nullptr;
            }
        }

        SetReason(lpReason, luCapacity, "no D3D9Ex HAL device could be created", lResult);
        return false;
    }

    bool ConfigureFixedFunctionDraw(IDirect3DDevice9* lpDevice)
    {
        D3DMATRIX lIdentity = {};
        lIdentity._11 = 1.0f;
        lIdentity._22 = 1.0f;
        lIdentity._33 = 1.0f;
        lIdentity._44 = 1.0f;
        const D3DVIEWPORT9 lViewport = {0, 0, 64, 64, 0.0f, 1.0f};

        return SUCCEEDED(lpDevice->SetViewport(&lViewport))
            && SUCCEEDED(lpDevice->SetTransform(D3DTS_WORLD, &lIdentity))
            && SUCCEEDED(lpDevice->SetTransform(D3DTS_VIEW, &lIdentity))
            && SUCCEEDED(lpDevice->SetTransform(D3DTS_PROJECTION, &lIdentity))
            && SUCCEEDED(lpDevice->SetVertexShader(nullptr))
            && SUCCEEDED(lpDevice->SetPixelShader(nullptr))
            && SUCCEEDED(lpDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE))
            && SUCCEEDED(lpDevice->SetRenderState(D3DRS_LIGHTING, FALSE))
            && SUCCEEDED(lpDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE))
            && SUCCEEDED(lpDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE))
            && SUCCEEDED(lpDevice->SetRenderState(D3DRS_CLIPPING, TRUE))
            && SUCCEEDED(lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE))
            && SUCCEEDED(lpDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE))
            && SUCCEEDED(lpDevice->SetRenderState(
                D3DRS_COLORWRITEENABLE,
                D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN
                    | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA))
            && SUCCEEDED(lpDevice->SetTexture(0, nullptr))
            && SUCCEEDED(lpDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1))
            && SUCCEEDED(lpDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE))
            && SUCCEEDED(lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1))
            && SUCCEEDED(lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE));
    }

    bool PollQueryBounded(IDirect3DQuery9* lpQuery, DWORD* lpSampleCount,
                          char* lpReason, size_t luCapacity)
    {
        const ULONGLONG luDeadline = GetTickCount64() + 5000u;
        for (;;)
        {
            const HRESULT lResult = lpQuery->GetData(
                lpSampleCount, sizeof(*lpSampleCount), D3DGETDATA_FLUSH);
            if (lResult == S_OK)
                return true;
            if (lResult != S_FALSE)
            {
                SetReason(lpReason, luCapacity, "native occlusion GetData failed", lResult);
                return false;
            }
            if (GetTickCount64() >= luDeadline)
            {
                SetReason(lpReason, luCapacity, "native occlusion query did not finish in 5 seconds");
                return false;
            }
            Sleep(1);
        }
    }

    bool ProbeRawQuery(IDirect3DDevice9* lpDevice, IDirect3DQuery9* lpQuery,
                       bool lbDrawVisibleTriangle, DWORD* lpSampleCount,
                       char* lpReason, size_t luCapacity)
    {
        Step(lbDrawVisibleTriangle ? "raw visible-triangle query witness"
                                   : "raw empty-query zero-sample witness");
        HRESULT lResult = lpDevice->BeginScene();
        if (FAILED(lResult))
        {
            SetReason(lpReason, luCapacity, "BeginScene for native query probe failed", lResult);
            return false;
        }

        lResult = lpQuery->Issue(D3DISSUE_BEGIN);
        if (FAILED(lResult))
        {
            lpDevice->EndScene();
            SetReason(lpReason, luCapacity, "native query BEGIN issue failed", lResult);
            return false;
        }

        if (lbDrawVisibleTriangle)
        {
            static const TestVertex laVertices[3] = {
                {-0.8f, -0.8f, 0.5f, 0xFFFFFFFFu},
                { 0.8f, -0.8f, 0.5f, 0xFFFFFFFFu},
                { 0.0f,  0.8f, 0.5f, 0xFFFFFFFFu}
            };
            static const WORD lauIndices[3] = {0, 1, 2};
            const HRESULT lDrawResult = lpDevice->DrawIndexedPrimitiveUP(
                D3DPT_TRIANGLELIST, 0, 3, 1, lauIndices, D3DFMT_INDEX16,
                laVertices, sizeof(TestVertex));
            if (FAILED(lDrawResult))
            {
                lpQuery->Issue(D3DISSUE_END);
                lpDevice->EndScene();
                SetReason(lpReason, luCapacity, "visible indexed D3D9 draw probe failed", lDrawResult);
                return false;
            }
        }

        lResult = lpQuery->Issue(D3DISSUE_END);
        const HRESULT lSceneResult = lpDevice->EndScene();
        if (FAILED(lResult) || FAILED(lSceneResult))
        {
            SetReason(lpReason, luCapacity, "native query END or EndScene failed",
                      FAILED(lResult) ? lResult : lSceneResult);
            return false;
        }
        return PollQueryBounded(lpQuery, lpSampleCount, lpReason, luCapacity);
    }

    bool NativeQuerySetup(NativeContext& lrContext, DWORD* lpEmptyCount,
                          char* lpReason, size_t luCapacity)
    {
        IDirect3DQuery9* lpQuery = nullptr;
        HRESULT lResult = lrContext.mpDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION, &lpQuery);
        if (FAILED(lResult) || lpQuery == nullptr)
        {
            if (lpQuery != nullptr)
                lpQuery->Release();
            SetReason(lpReason, luCapacity, "D3D9 occlusion queries are unsupported", lResult);
            return false;
        }

        const bool lbEmptyReady = ProbeRawQuery(lrContext.mpDevice, lpQuery, false,
                                                lpEmptyCount, lpReason, luCapacity);
        DWORD luVisibleCount = 0;
        const bool lbVisibleReady = lbEmptyReady
            && ProbeRawQuery(lrContext.mpDevice, lpQuery, true, &luVisibleCount,
                             lpReason, luCapacity);
        lpQuery->Release();
        if (!lbVisibleReady)
            return false;
        if (luVisibleCount == 0)
        {
            SetReason(lpReason, luCapacity,
                      "native visible-triangle query returned zero; fixture draw witness unavailable");
            return false;
        }
        return true;
    }

    bool BeginSurveyScene(IDirect3DDevice9* lpDevice)
    {
        const HRESULT lResult = lpDevice->BeginScene();
        Check(SUCCEEDED(lResult), "native D3D9 survey scene begins");
        return SUCCEEDED(lResult);
    }

    void EndSurveyScene(IDirect3DDevice9* lpDevice)
    {
        const HRESULT lResult = lpDevice->EndScene();
        Check(SUCCEEDED(lResult), "native D3D9 survey scene ends before result readback");
    }

    TriangleResult RunTriangleSurvey(IDirect3DDevice9* lpDevice, u32 luIdentifier,
                                     const TestVertex* lpVertices, u32 luPrimitiveCount,
                                     u32 luDrawAttempts, bool lbReadTwice)
    {
        static const WORD lauIndices[3] = {0, 1, 2};
        TriangleResult lResult;
        if (!BeginSurveyScene(lpDevice))
            return lResult;

        renderengine::OcclusionQueryProviderPCBeginSurvey(lpDevice, luIdentifier);
        for (u32 luAttempt = 0; luAttempt < luDrawAttempts && luAttempt < 2u; ++luAttempt)
        {
            const renderengine::OcclusionSurveySubmissionTokenPC luToken =
                renderengine::OcclusionQueryProviderPCBeginSurveySubmission();
            lResult.mabHasSubmissionToken[luAttempt] = luToken != 0;
            lResult.mahDrawResults[luAttempt] = lpDevice->DrawIndexedPrimitiveUP(
                D3DPT_TRIANGLELIST, 0, 3, luPrimitiveCount, lauIndices, D3DFMT_INDEX16,
                lpVertices, sizeof(TestVertex));
            renderengine::OcclusionQueryProviderPCFinishSurveySubmission(
                luToken, SUCCEEDED(lResult.mahDrawResults[luAttempt]) && luPrimitiveCount != 0);
        }
        renderengine::OcclusionQueryProviderPCEndSurvey();
        EndSurveyScene(lpDevice);

        renderengine::OcclusionQueryProviderPCBeginConditional(lpDevice, luIdentifier);
        lResult.mbAllowed = renderengine::OcclusionQueryProviderPCShouldDraw(lpDevice);
        if (lbReadTwice)
            lResult.mbAllowedAgain = renderengine::OcclusionQueryProviderPCShouldDraw(lpDevice);
        renderengine::OcclusionQueryProviderPCEndConditional();
        return lResult;
    }

    bool RunNoSubmissionSurvey(IDirect3DDevice9* lpDevice, u32 luIdentifier)
    {
        if (!BeginSurveyScene(lpDevice))
            return false;
        renderengine::OcclusionQueryProviderPCBeginSurvey(lpDevice, luIdentifier);
        renderengine::OcclusionQueryProviderPCEndSurvey();
        EndSurveyScene(lpDevice);
        renderengine::OcclusionQueryProviderPCBeginConditional(lpDevice, luIdentifier);
        const bool lbAllowed = renderengine::OcclusionQueryProviderPCShouldDraw(lpDevice);
        renderengine::OcclusionQueryProviderPCEndConditional();
        return lbAllowed;
    }

    TriangleResult RunSurveyWithoutCondition(IDirect3DDevice9* lpDevice, u32 luIdentifier,
                                              const TestVertex* lpVertices)
    {
        static const WORD lauIndices[3] = {0, 1, 2};
        TriangleResult lResult;
        if (!BeginSurveyScene(lpDevice))
            return lResult;
        renderengine::OcclusionQueryProviderPCBeginSurvey(lpDevice, luIdentifier);
        const renderengine::OcclusionSurveySubmissionTokenPC luToken =
            renderengine::OcclusionQueryProviderPCBeginSurveySubmission();
        lResult.mabHasSubmissionToken[0] = luToken != 0;
        lResult.mahDrawResults[0] = lpDevice->DrawIndexedPrimitiveUP(
            D3DPT_TRIANGLELIST, 0, 3, 1, lauIndices, D3DFMT_INDEX16,
            lpVertices, sizeof(TestVertex));
        renderengine::OcclusionQueryProviderPCFinishSurveySubmission(
            luToken, SUCCEEDED(lResult.mahDrawResults[0]));
        renderengine::OcclusionQueryProviderPCEndSurvey();
        EndSurveyScene(lpDevice);
        return lResult;
    }

    bool InvalidIdentifierFailsOpen(IDirect3DDevice9* lpDevice, u32 luIdentifier)
    {
        renderengine::OcclusionQueryProviderPCBeginSurvey(lpDevice, luIdentifier);
        renderengine::OcclusionQueryProviderPCEndSurvey();
        renderengine::OcclusionQueryProviderPCBeginConditional(lpDevice, luIdentifier);
        const bool lbAllowed = renderengine::OcclusionQueryProviderPCShouldDraw(lpDevice);
        renderengine::OcclusionQueryProviderPCEndConditional();
        return lbAllowed;
    }

    struct FailedSubmissionWitness
    {
        bool mbWitnessAvailable = false;
        bool mbProviderToken = false;
        bool mbFailedDraw = false;
        bool mbAllowed = true;
        HRESULT mhrDraw = E_FAIL;
    };

    void RestoreVertexInput(IDirect3DDevice9* lpDevice, DWORD luSavedFvf,
                            IDirect3DVertexDeclaration9* lpSavedDeclaration)
    {
        HRESULT lResult = S_OK;
        if (luSavedFvf != 0)
            lResult = lpDevice->SetFVF(luSavedFvf);
        else if (lpSavedDeclaration != nullptr)
            lResult = lpDevice->SetVertexDeclaration(lpSavedDeclaration);
        Check(SUCCEEDED(lResult), "original D3D9 vertex input state restored");
    }

    FailedSubmissionWitness RunMissingVertexInputFailure(
        IDirect3DDevice9* lpDevice, u32 luIdentifier, const TestVertex* lpVertices)
    {
        FailedSubmissionWitness lWitness;
        DWORD luSavedFvf = 0;
        IDirect3DVertexDeclaration9* lpSavedDeclaration = nullptr;
        const HRESULT lGetFvf = lpDevice->GetFVF(&luSavedFvf);
        const HRESULT lGetDeclaration = lpDevice->GetVertexDeclaration(&lpSavedDeclaration);
        if (FAILED(lGetFvf) || FAILED(lGetDeclaration))
        {
            if (lpSavedDeclaration != nullptr)
                lpSavedDeclaration->Release();
            std::printf("SUBCASE_UNAVAILABLE: could not read vertex input state (FVF 0x%08X, decl 0x%08X)\n",
                        static_cast<unsigned>(lGetFvf), static_cast<unsigned>(lGetDeclaration));
            return lWitness;
        }

        Step("attempting documented no-FVF/no-declaration precondition with valid CPU spans");
        const HRESULT lClearDeclaration = lpDevice->SetVertexDeclaration(nullptr);
        const HRESULT lClearFvf = lpDevice->SetFVF(0);
        DWORD luCurrentFvf = 0;
        IDirect3DVertexDeclaration9* lpCurrentDeclaration = nullptr;
        const HRESULT lReadFvf = lpDevice->GetFVF(&luCurrentFvf);
        const HRESULT lReadDeclaration = lpDevice->GetVertexDeclaration(&lpCurrentDeclaration);
        const bool lbNoVertexInput = SUCCEEDED(lReadFvf) && SUCCEEDED(lReadDeclaration)
            && luCurrentFvf == 0 && lpCurrentDeclaration == nullptr;
        if (lpCurrentDeclaration != nullptr)
            lpCurrentDeclaration->Release();
        if (!lbNoVertexInput)
        {
            RestoreVertexInput(lpDevice, luSavedFvf, lpSavedDeclaration);
            if (lpSavedDeclaration != nullptr)
                lpSavedDeclaration->Release();
            std::printf("SUBCASE_UNAVAILABLE: driver did not expose an absent FVF/declaration "
                        "(SetVertexDeclaration hr=0x%08X, SetFVF hr=0x%08X, GetFVF hr=0x%08X, "
                        "GetVertexDeclaration hr=0x%08X, current FVF=0x%08X, current decl=%p)\n",
                        static_cast<unsigned>(lClearDeclaration), static_cast<unsigned>(lClearFvf),
                        static_cast<unsigned>(lReadFvf), static_cast<unsigned>(lReadDeclaration),
                        static_cast<unsigned>(luCurrentFvf), static_cast<void*>(lpCurrentDeclaration));
            return lWitness;
        }

        static const WORD lauValidIndices[3] = {0, 1, 2};
        if (!BeginSurveyScene(lpDevice))
        {
            RestoreVertexInput(lpDevice, luSavedFvf, lpSavedDeclaration);
            if (lpSavedDeclaration != nullptr)
                lpSavedDeclaration->Release();
            std::printf("SUBCASE_UNAVAILABLE: could not begin a native scene for missing-input draw\n");
            return lWitness;
        }

        renderengine::OcclusionQueryProviderPCBeginSurvey(lpDevice, luIdentifier);
        const renderengine::OcclusionSurveySubmissionTokenPC luToken =
            renderengine::OcclusionQueryProviderPCBeginSurveySubmission();
        lWitness.mbProviderToken = luToken != 0;
        Step("issuing indexed draw with valid spans and verified absent vertex input");
        lWitness.mhrDraw = lpDevice->DrawIndexedPrimitiveUP(
            D3DPT_TRIANGLELIST, 0, 3, 1, lauValidIndices, D3DFMT_INDEX16,
            lpVertices, sizeof(TestVertex));
        lWitness.mbFailedDraw = FAILED(lWitness.mhrDraw);
        renderengine::OcclusionQueryProviderPCFinishSurveySubmission(
            luToken, SUCCEEDED(lWitness.mhrDraw));
        renderengine::OcclusionQueryProviderPCEndSurvey();
        EndSurveyScene(lpDevice);

        if (lWitness.mbFailedDraw)
        {
            renderengine::OcclusionQueryProviderPCBeginConditional(lpDevice, luIdentifier);
            lWitness.mbAllowed = renderengine::OcclusionQueryProviderPCShouldDraw(lpDevice);
            renderengine::OcclusionQueryProviderPCEndConditional();
            lWitness.mbWitnessAvailable = true;
        }
        else
        {
            // The driver accepted the draw, so this is not a failed-submission witness. Do not
            // turn a successful result into a synthetic failure or an asserted zero count.
            renderengine::OcclusionQueryProviderPCBeginConditional(lpDevice, luIdentifier);
            (void)renderengine::OcclusionQueryProviderPCShouldDraw(lpDevice);
            renderengine::OcclusionQueryProviderPCEndConditional();
            std::printf("SUBCASE_UNAVAILABLE: D3D9 accepted the draw without vertex input "
                        "(HRESULT 0x%08X)\n", static_cast<unsigned>(lWitness.mhrDraw));
        }

        RestoreVertexInput(lpDevice, luSavedFvf, lpSavedDeclaration);
        if (lpSavedDeclaration != nullptr)
            lpSavedDeclaration->Release();
        return lWitness;
    }

    void TestProvider(NativeContext& lrContext, DWORD luEmptyQuerySamples)
    {
        IDirect3DDevice9* const lpDevice = lrContext.mpDevice;
        static const TestVertex laVisible[3] = {
            {-0.8f, -0.8f, 0.5f, 0xFFFFFFFFu},
            { 0.8f, -0.8f, 0.5f, 0xFFFFFFFFu},
            { 0.0f,  0.8f, 0.5f, 0xFFFFFFFFu}
        };
        static const TestVertex laClipped[3] = {
            {2.0f, -0.5f, 0.5f, 0xFFFFFFFFu},
            {3.0f, -0.5f, 0.5f, 0xFFFFFFFFu},
            {2.5f,  0.5f, 0.5f, 0xFFFFFFFFu}
        };

        Check(luEmptyQuerySamples == 0, "real empty D3D9 occlusion query returns zero samples");

        Check(RunNoSubmissionSurvey(lpDevice, 0u),
              "a begun and ended survey with no indexed submission fails open");

        Step("safe failed-draw subcase");
        const FailedSubmissionWitness lFailed =
            RunMissingVertexInputFailure(lpDevice, 1u, laVisible);
        if (lFailed.mbWitnessAvailable)
        {
            Check(lFailed.mbProviderToken, "failed-draw survey opened a real provider query");
            Check(lFailed.mbFailedDraw, "D3D9 rejected a draw with valid spans and no vertex input");
            Check(lFailed.mbAllowed, "a failed indexed submission cannot suppress the later mesh");
        }
        else
        {
            std::printf("SUBCASE_UNAVAILABLE: no documented HRESULT failure witness was available\n");
        }

        Step("zero-primitive survey");
        const TriangleResult lZero = RunTriangleSurvey(lpDevice, 2u, laVisible, 0u,
                                                       1u, false);
        Check(lZero.mabHasSubmissionToken[0], "zero-primitive survey opened a real provider query");
        Check(lZero.mbAllowed, "a successful or rejected zero-primitive draw fails open");

        Step("visible indexed survey");
        const TriangleResult lVisible = RunTriangleSurvey(lpDevice, 3u, laVisible, 1u,
                                                          1u, true);
        Check(lVisible.mabHasSubmissionToken[0], "visible survey opened a real provider query");
        Check(SUCCEEDED(lVisible.mahDrawResults[0]), "visible indexed triangle was submitted to D3D9");
        Check(lVisible.mbAllowed, "a real visible triangle passes the native nonzero-sample policy");
        Check(lVisible.mbAllowedAgain, "same-scope repeated check uses the cached native query result");

        Step("fully clipped indexed survey");
        const TriangleResult lClipped = RunTriangleSurvey(lpDevice, 4u, laClipped, 1u,
                                                          1u, false);
        Check(lClipped.mabHasSubmissionToken[0], "clipped survey opened a real provider query");
        Check(SUCCEEDED(lClipped.mahDrawResults[0]), "fully clipped indexed triangle was submitted");
        Check(!lClipped.mbAllowed, "a real zero-sample clipped triangle is rejected by the native policy");

        Step("overwritten and reused query ID");
        const TriangleResult lFirstUse = RunTriangleSurvey(lpDevice, 5u, laVisible, 1u,
                                                           1u, false);
        Check(lFirstUse.mbAllowed, "reused-ID setup first records the visible query result");
        const TriangleResult lOverwrite = RunTriangleSurvey(lpDevice, 5u, laClipped, 1u,
                                                             1u, false);
        Check(!lOverwrite.mbAllowed, "overwriting an ID does not reuse its previous visible result");
        const TriangleResult lReused = RunTriangleSurvey(lpDevice, 5u, laVisible, 1u,
                                                          1u, false);
        Check(lReused.mbAllowed, "a consumed native query ID can be reused for a new visible survey");

        Step("conservative multiple-submission survey");
        const TriangleResult lMultiple = RunTriangleSurvey(lpDevice, 6u, laVisible, 1u,
                                                            2u, false);
        Check(lMultiple.mabHasSubmissionToken[0] && lMultiple.mabHasSubmissionToken[1],
              "multiple-submission case made two real indexed draw attempts");
        Check(SUCCEEDED(lMultiple.mahDrawResults[0]) && SUCCEEDED(lMultiple.mahDrawResults[1]),
              "both conservative multiple-submission draws reached D3D9");
        Check(lMultiple.mbAllowed, "multiple box submissions are unavailable and fail open");

        Step("invalid and manager-sentinel IDs");
        Check(InvalidIdentifierFailsOpen(lpDevice, 64u), "out-of-range query ID fails open");
        Check(InvalidIdentifierFailsOpen(lpDevice, 0xFFFFu), "original manager sentinel ID fails open");

        Step("provider invalidation and device reset identity");
        const TriangleResult lBeforeReset = RunSurveyWithoutCondition(lpDevice, 9u, laVisible);
        Check(lBeforeReset.mabHasSubmissionToken[0] && SUCCEEDED(lBeforeReset.mahDrawResults[0]),
              "reset-identity case first records a real ended query");

        renderengine::OcclusionQueryProviderPCInvalidate(lpDevice);
        renderengine::OcclusionQueryProviderPCBeginConditional(lpDevice, 9u);
        const bool lbAfterInvalidate = renderengine::OcclusionQueryProviderPCShouldDraw(lpDevice);
        renderengine::OcclusionQueryProviderPCEndConditional();
        Check(lbAfterInvalidate, "provider invalidation discards the ended query identity");

        // The engine invalidates query objects before each ResetEx attempt. This fixture mirrors
        // that boundary; a driver that refuses ResetEx still leaves the provider safely detached.
        renderengine::OcclusionQueryProviderPCInvalidate(lpDevice);
        D3DPRESENT_PARAMETERS lResetParameters = lrContext.mParameters;
        const HRESULT lReset = lrContext.mpDevice->ResetEx(&lResetParameters, nullptr);
        if (FAILED(lReset))
        {
            std::printf("SKIP_RESET_RECOVERY: ResetEx unavailable (HRESULT 0x%08X); invalidation was tested\n",
                        static_cast<unsigned>(lReset));
            return;
        }

        renderengine::OcclusionQueryProviderPCResume(lpDevice);
        Check(ConfigureFixedFunctionDraw(lpDevice), "D3D9 fixed-function state restored after ResetEx");
        const TriangleResult lAfterReset = RunTriangleSurvey(lpDevice, 9u, laVisible, 1u,
                                                              1u, false);
        Check(lAfterReset.mabHasSubmissionToken[0] && SUCCEEDED(lAfterReset.mahDrawResults[0]),
              "a new same-ID query was created after successful ResetEx recovery");
        Check(lAfterReset.mbAllowed, "post-reset query uses the new device generation");
    }
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Step("create hidden D3D9Ex device");
    NativeContext lContext;
    char lacReason[256] = {};
    if (!CreateNativeContext(lContext, lacReason, sizeof(lacReason)))
    {
        ReleaseContext(lContext);
        std::printf("UNAVAILABLE: %s\n", lacReason);
        return 77;
    }

    Step("configure valid fixed-function indexed draw state");
    if (!ConfigureFixedFunctionDraw(lContext.mpDevice))
    {
        ReleaseContext(lContext);
        std::printf("UNAVAILABLE: D3D9 fixed-function indexed draw setup failed\n");
        return 77;
    }

    Step("probe real empty and visible occlusion-query results");
    DWORD luEmptyQuerySamples = 0;
    if (!NativeQuerySetup(lContext, &luEmptyQuerySamples, lacReason, sizeof(lacReason)))
    {
        ReleaseContext(lContext);
        std::printf("UNAVAILABLE: %s\n", lacReason);
        return 77;
    }

    Step("exercise production provider state and draw outcomes");
    TestProvider(lContext, luEmptyQuerySamples);
    ReleaseContext(lContext);
    std::printf("PCOcclusionQueries: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures == 0 ? 0 : 1;
}
