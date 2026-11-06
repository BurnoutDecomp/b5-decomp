from pathlib import Path
import json,hashlib,subprocess
r=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_lion');o=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/native_lion')
changed=subprocess.check_output(['git','diff','--name-only'],cwd=r,text=True).splitlines(); d=subprocess.check_output(['git','diff','--binary'],cwd=r)
(o/'owned.diff').write_bytes(d)
(o/'source-manifest.json').write_text(json.dumps({'base':'001028b6b82f46785037ad26f46f7cff44a6c37c','worktree':str(r),'diff_sha256':hashlib.sha256(d).hexdigest(),'files':{p:hashlib.sha256((r/p).read_bytes()).hexdigest() for p in changed}},indent=2)+'\n')
notes='''# Native Lion private postmortem

Scope: src/SDKs/Packages/Lion/Final only. Base001028b6b82f46785037ad26f46f7cff44a6c37c; isolated checkout C:/Users/adri1/.codex/worktrees/bp360/native_lion. No production ledger, vendor setup, canonical source, push, or other-worker edits.

Changes and evidence:
- ParticleRender.h keeps the Microsoft C++ bool8_t=bool branch of current EABase (eabase.h515-522), with the same BOOL8_T_DEFINED guard. GNU/PS3 int8_t alias deliberately does not arbitrate the Xbox ABI. Actual XDK probe asserts type+size and compiles identical base/concrete BeginRendering member signatures (_N).
- cTime C++03 default constructor leaves mTicks untouched. ARTIST82913228 Init owns zero stores at mLastTime/+19C and mUpdateLastTime/+1A0. Native constructor/default-placement/value-placement streams are single blr, BuildZero is li0/stw0/blr. There are no zero-argument cTime() value-init uses in tracked src apart from the constructor itself.
- Serialised slot safe-bool uses pointer-to-member (no data field/layout change), preserving contextual !, conditional and explicit cast tests in native probe. Slots remain4bytes/alignment4; Get and Relocate actual native streams recorded.
- Native compiler rejects C++11 aggregate assignment/return syntax and conflicting extern-template suppress+explicit instantiation. Named C++03 aggregates preserve lanes. Template definition stays in ParticleEmitter.cpp; actual PPC object emits all three original helper variants.
- Restored missing interpolation side effect: ARTIST8290F9B8..9C0 passes temp/lower/upper/fraction to cParticleBehaviour::Lerp then reloads temp. Actual owned ParticleEmitter object now calls the same named Lerp.
- Replaced nearbyintf with genuine native __vrfin; ARTIST82910540..56C uses vrfin for angle range reduction. Actual owned native object emits vrfin1000020A. No scalar approximation.
- Removed host bring-up logging in LionFX/Emitter/Render rather than substitute CRT print APIs. Original culling exits restored (no dangling sbNoCull), and per-flush world-index draw condition restored from ARTIST82913A44..A50/B1C..B28/BE0..BEC rather than cached undefined lbDrawThisWorld.
- Native Dispatch includes actual XDK d3d9.h: SetStreamSource's sixth arg is UINT64, DrawVertices takes D3DPRIMITIVETYPE and the literal13 is D3DPT_QUADLIST. Native stream buffer ABI is used directly at boundary.
- Render-state owner identified ARTIST83010F60 as CgsGraphics::ImRendererBase::mgStateLibrary.mpSamplerState_Linear_MipNearest_ClampUV (ConstructOnceOnly827F1D90). Use actual owning field/header; no replacement global, no new initialization.

Validation before external dependency integration:
- All26 owned CPP baseline10pass/16fail; modified24pass/2fail, preserving all10 baseline passes.
- Header/interface probe1pass/0fail with original XDK14.00.6502, big-endian PPC COFF01F2/pointer4.
- Remaining owned build blockers are foreign headers: ParticleRandomSeed.cpp -> vendor/renderware/.../vector4_operation.h lambda; ParticleRender.cpp -> shadowingdevice.h missing renderengine::VertexProgramState declaration (other owner has correction).
-83 affected native src CPPs selected by reverse literal includes and complete native compile attempted; separate result pending at time this note was prepared.

Persistent evidence: canonical child build/x360/parallel/native_lion/{baseline,initial,native,final-owned,native-dispatch,header-probe,affected}; notes/probe/scripts/native-evidence.json/source-manifest.json/owned.diff in this directory. Native-evidence includes actual ARTIST export hashes/windows and emitted instruction streams.

Limits: no full-game link/boot, runtime particle witness, or byte-match claim. Existing unreconstructed EmitterCubeRender and BinSave remain untouched; unchanged fog singleton stand-in and broad Lion math/native ABI recovery remain outside this compile-blocker batch. Root handles combined review/integration.
'''
(o/'POSTMORTEM.md').write_text(notes,encoding='utf8')
print('Saved source manifest, exact diff, and private postmortem.')
