// b5-decomp/src/GameSource/Graphics/BrnCoronaTypeParams.cpp
// =============================================================================
// BrnCoronaTypeParams.cpp -- THE CORONA ARCHETYPE TABLE (25 records) and its accessor.
//
// Reconstructed 2026-08-17 (coronas step 1, group `coronadata`) from
// BURNOUT_X360_ARTIST.XEX. Ground truth, cited per record below:
//
//   * The table lives at X360 `unk_82F24310` (== the corona tuning block 0x82F242C0 + 0x50):
//     25 records of 48 bytes = 1200 bytes.  PROOF OF THE 48-BYTE STRIDE: `sub_823FD428`
//     (the `AddCorona(..., const BrnCoronaType&)` overload; xrefs from SubmitCoronasForRaceCar
//     0x822D1600 / SubmitCoronasForVehicle 0x82727BB0 / RenderCoronasForInstance 0x827571B8) is
//         0x823FD434  lwz   r11, 0(r6)      ; leCoronaType (passed BY REFERENCE)
//         0x823FD43C  slwi  r9,  r11, 1     ; type*2
//         0x823FD444  add   r11, r11, r9    ; type*3
//         0x823FD448  slwi  r11, r11, 4     ; type*48
//         0x823FD44C  add   r6,  r11, r10   ; &mParams[type]
//         0x823FD450  bl    BrnCoronaManager::BrnSubmissionInterface::AddCorona
//     i.e. GetCoronaTypeParams is INLINED there as a plain unchecked `&mParams[type]`.
//
//   * THE CONSOLE RECORD IS 48 BYTES, and its field offsets are pinned by the *reader*,
//     `BrnCoronaManager::BrnSubmissionInterface::AddCorona` @0x823FD270 (r30 = the params ptr):
//         0x823FD314  addi   r3, r30, 0x18  ; &mScaleCurve.mCurveParams  -> Evaluate arg 1
//         0x823FD318  addi   r4, r3,  0x0C  ; &mScaleCurve.mScaleFactors -> Evaluate arg 2 (+0x24)
//         0x823FD3EC  lvx128 v0, r0, r30    ; mvSize, a FULL 16-byte VMX lane at +0x00
//         0x823FD3F0  lwz    r6, 0x10(r30)  ; miTextureID    (+0x10)
//         0x823FD3F4  lfs    f1, 0x14(r30)  ; mfBiasDistance (+0x14)
//     =>  +0x00 mvSize (16-byte lane, x/y used) | +0x10 miTextureID | +0x14 mfBiasDistance
//         +0x18 mScaleCurve.mCurveParams  (THREE PACKED f32: +0x18/+0x1C/+0x20)
//         +0x24 mScaleCurve.mScaleFactors (TWO   PACKED f32: +0x24/+0x28)
//         +0x2C 4 bytes of tail padding (0x00000000 in the image for all 25 records).
//     DWARF (references/DecFIGS/dwarfdump/GameSource/Effects/Curves.h) confirms SmoothStep's
//     members are `Vector3Template<float>` / `Vector2Template<float>` -- the PACKED templates --
//     which is why the console curve is 20 bytes and not two 16-byte lanes.
//
//   * ON THIS x64 HOST THE RECORD IS 64 BYTES, because BrnCommonTypes.h aliases Vector2/Vector3
//     to the 16-byte `rw::math::vpu` lanes: mScaleCurve moves +0x18 -> +0x20 and its Vector3 /
//     Vector2 become 16-byte lanes.  *** THE TABLE BELOW IS THEREFORE TRANSCRIBED FIELD BY FIELD
//     INTO C++ INITIALISERS.  NEVER memcpy CONSOLE BYTES OVER IT. ***  (AGENTS.md rule 1; this is
//     the campaign's single most repeated defect.)
//
//   * WHERE EACH WORD COMES FROM.  The .data image ships miTextureID, mfBiasDistance and MOST of
//     the mScaleCurve rows; the 16-byte mvSize lane of every record and the mScaleCurve of exactly
//     eleven records (4,5,6,7,10,11,12,17,18,19,20) are written at run time by the C++ dynamic
//     initialiser `sub_82C4F418` (0x82C4F418-0x82C4F8D8), whose 25 `stvx128 v0, r11, <0x000, 0x030,
//     0x060, ... , 0x480>` stores land on exactly the 25 record bases and whose 55 `stfs` stores
//     land on exactly +0x18..+0x28 of those eleven records.  The two halves are an EXACT
//     COMPLEMENT -- every word the initialiser writes reads 0x00000000 in the image, and every
//     word the image ships non-zero is one the initialiser does not touch -- which is an
//     independent cross-validation of both the 48-byte layout and the initialiser decode.
//     Per-record provenance is cited inline ("RUNTIME INIT ..." vs "IMAGE @...").
//     Decode transcripts: scratch/coronas_step1/coronadata/work/{asm_tuning_init.txt,
//     decode_tuning_init.py, DECODE_LOG.txt}.
//
//   * ENUM ORDER.  The 25 rows are indexed by the DWARF `BrnCoronaType` order already committed in
//     BrnCoronaManager.h:39.  Independent plausibility evidence that the order is right (the table
//     was decoded WITHOUT reference to the names, then compared):
//       - all three *RearLight*  archetypes (1, 9, 17) use atlas page 1;
//       - all three *BrakeLight* archetypes (2, 10, 18) use atlas page 7;
//       - all three *Indicator*  archetypes (3, 11, 19) use atlas page 10;
//       - BluesTwosRed (13, 21) both use page 3; BluesTwosBlue (14, 22) both use page 0;
//       - the three traffic-signal archetypes (5 green / 6 amber / 7 red) are the ONLY records
//         with mfBiasDistance 0.35 instead of 0.5, share one falloff curve, and are the largest
//         (0.9 x 1.3 / 0.9 x 0.9) -- real traffic-signal lamps;
//       - head lamp > tail lamp in every family: 0.7 > 0.4 (traffic), 0.656 > 0.55 (race car),
//         0.525 > 0.3 (player car);
//       - every miTextureID is in [0,11] -- exactly the 12 rows of the corona atlas UV table.
//
//   * PC FILE SPLIT.  On the console this data + accessor live in BrnCoronaManager.cpp (the assert
//     strings in AddCorona/AddPropCorona name that file).  They are split into this TU so the data
//     landing is file-disjoint from the renderer landing; the header home is unchanged
//     (GameSource/Graphics/BrnCoronaManager.h).
//
//   * NAMING NOTE (deliberately NOT changed here): DWARF spells the table `mParams`
//     (BrnCoronaManager.h:94); the committed tree header spells it `smParams`.  This TU defines
//     the name the committed header declares.  Reconciling the spelling touches a header with a
//     52-TU includer closure, so it is a named follow-up, not part of a data landing.
// =============================================================================

#include "GameSource/Graphics/BrnCoronaManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// The archetype table.  Non-const, matching the DWARF declaration (`extern BrnCoronaTypeParams[25]
// mParams` -- no const) and the console, where it is a WRITABLE .data object finished off by a
// dynamic initialiser.  Nothing in the recovered call graph writes it after start-up.
//
// Field order per row:  { mvSize{x,y,0,0}, miTextureID, mfBiasDistance,
//                         mScaleCurve{ mCurveParams{x,y,z,0}, mScaleFactors{x,y,0,0} } }
//
// The mvSize z/w lanes are 0: the console's `lvx128` at +0x00 loads all four, but the stack lanes
// the initialiser copies from have their upper 8 bytes zeroed by its paired `std r10, 0(rX)`
// stores, the image words at +0x08/+0x0C read 0x00000000 for all 25 records, and the lane's only
// consumer (AddCorona's `vmulfp128 v3, v0, v125` -> the Corona record's size field) is read back
// by CoronaRenderer::Dispatch as TWO floats.  Same reasoning for the SmoothStep lanes' unused
// w / z+w, which do not exist at all on the console (packed 3+2 floats).
BrnCoronaTypeParams BrnCoronaTypeParams::smParams[eCoronaTypeCount] =
{
    // [ 0] eCoronaTypeTrafficHeadLight          record @ X360 0x82F24310
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F580  (src flt_8203B710)
    //      texID/bias  <- IMAGE  @0x82F24320 / 0x82F24324
    //      mScaleCurve <- IMAGE  @0x82F24328
    { Vector2{ 0.7f, 0.7f, 0.0f, 0.0f }, 2, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 4800.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.35f, 1.4f, 0.0f, 0.0f } } },
    // [ 1] eCoronaTypeTrafficRearLight          record @ X360 0x82F24340
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F5B8  (src flt_8200473C)
    //      texID/bias  <- IMAGE  @0x82F24350 / 0x82F24354
    //      mScaleCurve <- IMAGE  @0x82F24358
    { Vector2{ 0.4f, 0.4f, 0.0f, 0.0f }, 1, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 3600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.6f, 3.0f, 0.0f, 0.0f } } },
    // [ 2] eCoronaTypeTrafficBrakeLight         record @ X360 0x82F24370
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F5D8  (src flt_8203A470)
    //      texID/bias  <- IMAGE  @0x82F24380 / 0x82F24384
    //      mScaleCurve <- IMAGE  @0x82F24388
    { Vector2{ 0.35f, 0.35f, 0.0f, 0.0f }, 7, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.7f, 3.0f, 0.0f, 0.0f } } },
    // [ 3] eCoronaTypeTrafficIndicator          record @ X360 0x82F243A0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F5E8  (src flt_82003F40)
    //      texID/bias  <- IMAGE  @0x82F243B0 / 0x82F243B4
    //      mScaleCurve <- IMAGE  @0x82F243B8
    { Vector2{ 0.25f, 0.25f, 0.0f, 0.0f }, 10, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.5f, 0.0f, 0.0f } } },
    // [ 4] eCoronaTypeTrafficSpotlights         record @ X360 0x82F243D0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F5F8  (src flt_82048974)
    //      texID/bias  <- IMAGE  @0x82F243E0 / 0x82F243E4
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F4F4..F50C
    { Vector2{ 0.325f, 0.325f, 0.0f, 0.0f }, 0, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [ 5] eCoronaTypeTrafficLightGreen         record @ X360 0x82F24400
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F610  (src flt_82005450 / flt_8201ECC8)
    //      texID/bias  <- IMAGE  @0x82F24410 / 0x82F24414
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F514..F534
    { Vector2{ 0.9f, 1.3f, 0.0f, 0.0f }, 5, 0.35f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 10000.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 1.0f, 0.0f, 0.0f } } },
    // [ 6] eCoronaTypeTrafficLightAmber         record @ X360 0x82F24430
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F634  (src flt_82005450)
    //      texID/bias  <- IMAGE  @0x82F24440 / 0x82F24444
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F538..F548
    { Vector2{ 0.9f, 0.9f, 0.0f, 0.0f }, 4, 0.35f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 10000.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 1.0f, 0.0f, 0.0f } } },
    // [ 7] eCoronaTypeTrafficLightRed           record @ X360 0x82F24460
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F670  (src flt_82005450)
    //      texID/bias  <- IMAGE  @0x82F24470 / 0x82F24474
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F54C..F55C
    { Vector2{ 0.9f, 0.9f, 0.0f, 0.0f }, 3, 0.35f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 10000.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 1.0f, 0.0f, 0.0f } } },
    // [ 8] eCoronaTypeRaceCarHeadLight          record @ X360 0x82F24490
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F6BC  (src flt_82048970)
    //      texID/bias  <- IMAGE  @0x82F244A0 / 0x82F244A4
    //      mScaleCurve <- IMAGE  @0x82F244A8
    { Vector2{ 0.656f, 0.656f, 0.0f, 0.0f }, 0, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 6400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.3f, 2.25f, 0.0f, 0.0f } } },
    // [ 9] eCoronaTypeRaceCarRearLight          record @ X360 0x82F244C0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F70C  (src flt_8204788C)
    //      texID/bias  <- IMAGE  @0x82F244D0 / 0x82F244D4
    //      mScaleCurve <- IMAGE  @0x82F244D8
    { Vector2{ 0.55f, 0.55f, 0.0f, 0.0f }, 1, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1600.0f, 12000.0f, 0.0f },
                                      Vector2{ 0.7f, 3.0f, 0.0f, 0.0f } } },
    // [10] eCoronaTypeRaceCarBrakeLight         record @ X360 0x82F244F0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F724  (src flt_8203A470)
    //      texID/bias  <- IMAGE  @0x82F24500 / 0x82F24504
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F688 + 0x82C4F6C4..F6D4
    { Vector2{ 0.35f, 0.35f, 0.0f, 0.0f }, 7, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [11] eCoronaTypeRaceCarIndicator          record @ X360 0x82F24520
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F734  (src flt_8203A470)
    //      texID/bias  <- IMAGE  @0x82F24530 / 0x82F24534
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F6D8..F6F0
    { Vector2{ 0.35f, 0.35f, 0.0f, 0.0f }, 10, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [12] eCoronaTypeRaceCarReversingLight     record @ X360 0x82F24550
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F744  (src flt_82004010)
    //      texID/bias  <- IMAGE  @0x82F24560 / 0x82F24564
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F6F4..F704
    { Vector2{ 0.125f, 0.125f, 0.0f, 0.0f }, 7, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [13] eCoronaTypeRaceCarBluesTwosRed       record @ X360 0x82F24580
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F754  (src flt_82001DA0)
    //      texID/bias  <- IMAGE  @0x82F24590 / 0x82F24594
    //      mScaleCurve <- IMAGE  @0x82F24598
    { Vector2{ 0.5f, 0.5f, 0.0f, 0.0f }, 3, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 3600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.4f, 2.25f, 0.0f, 0.0f } } },
    // [14] eCoronaTypeRaceCarBluesTwosBlue      record @ X360 0x82F245B0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F764  (src flt_82001DA0)
    //      texID/bias  <- IMAGE  @0x82F245C0 / 0x82F245C4
    //      mScaleCurve <- IMAGE  @0x82F245C8
    { Vector2{ 0.5f, 0.5f, 0.0f, 0.0f }, 0, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 3600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.4f, 2.25f, 0.0f, 0.0f } } },
    // [15] eCoronaTypeRaceCarSpotlights         record @ X360 0x82F245E0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F774  (src flt_82048974)
    //      texID/bias  <- IMAGE  @0x82F245F0 / 0x82F245F4
    //      mScaleCurve <- IMAGE  @0x82F245F8
    { Vector2{ 0.325f, 0.325f, 0.0f, 0.0f }, 3, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 3600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.45f, 2.25f, 0.0f, 0.0f } } },
    // [16] eCoronaTypePlayerCarHeadLight        record @ X360 0x82F24610
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F784  (src flt_8204896C)
    //      texID/bias  <- IMAGE  @0x82F24620 / 0x82F24624
    //      mScaleCurve <- IMAGE  @0x82F24628
    { Vector2{ 0.525f, 0.525f, 0.0f, 0.0f }, 0, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.3f, 2.25f, 0.0f, 0.0f } } },
    // [17] eCoronaTypePlayerCarRearLight        record @ X360 0x82F24640
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F79C  (src flt_82004740)
    //      texID/bias  <- IMAGE  @0x82F24650 / 0x82F24654
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F708 + 0x82C4F7C8..F7F0
    { Vector2{ 0.3f, 0.3f, 0.0f, 0.0f }, 1, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [18] eCoronaTypePlayerCarBrakeLight       record @ X360 0x82F24670
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F870  (src flt_82004740)
    //      texID/bias  <- IMAGE  @0x82F24680 / 0x82F24684
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F7FC..F83C
    { Vector2{ 0.3f, 0.3f, 0.0f, 0.0f }, 7, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [19] eCoronaTypePlayerCarIndicator        record @ X360 0x82F246A0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F880  (src flt_82004E58)
    //      texID/bias  <- IMAGE  @0x82F246B0 / 0x82F246B4
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F840..F858
    { Vector2{ 0.15f, 0.15f, 0.0f, 0.0f }, 10, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [20] eCoronaTypePlayerCarReversingLight   record @ X360 0x82F246D0
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F890  (src flt_82004010)
    //      texID/bias  <- IMAGE  @0x82F246E0 / 0x82F246E4
    //      mScaleCurve <- RUNTIME INIT sub_82C4F418 0x82C4F85C..F86C
    { Vector2{ 0.125f, 0.125f, 0.0f, 0.0f }, 2, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.75f, 2.25f, 0.0f, 0.0f } } },
    // [21] eCoronaTypePlayerCarBluesTwosRed     record @ X360 0x82F24700
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F8A0  (src flt_82001DA0)
    //      texID/bias  <- IMAGE  @0x82F24710 / 0x82F24714
    //      mScaleCurve <- IMAGE  @0x82F24718
    { Vector2{ 0.5f, 0.5f, 0.0f, 0.0f }, 3, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 3600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.4f, 2.25f, 0.0f, 0.0f } } },
    // [22] eCoronaTypePlayerCarBluesTwosBlue    record @ X360 0x82F24730
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F8B0  (src flt_82001DA0)
    //      texID/bias  <- IMAGE  @0x82F24740 / 0x82F24744
    //      mScaleCurve <- IMAGE  @0x82F24748
    { Vector2{ 0.5f, 0.5f, 0.0f, 0.0f }, 0, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 3600.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.4f, 2.25f, 0.0f, 0.0f } } },
    // [23] eCoronaTypePlayerCarSpotlights       record @ X360 0x82F24760
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F8C0  (src flt_82048974)
    //      texID/bias  <- IMAGE  @0x82F24770 / 0x82F24774
    //      mScaleCurve <- IMAGE  @0x82F24778
    { Vector2{ 0.325f, 0.325f, 0.0f, 0.0f }, 0, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 1400.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.325f, 2.25f, 0.0f, 0.0f } } },
    // [24] eCoronaTypePlayerCarPS3BluRayLights  record @ X360 0x82F24790
    //      mvSize      <- RUNTIME INIT sub_82C4F418 stvx128 @0x82C4F8D0  (src flt_82048968 / flt_82048964)
    //      texID/bias  <- IMAGE  @0x82F247A0 / 0x82F247A4
    //      mScaleCurve <- IMAGE  @0x82F247A8
    { Vector2{ 0.175f, 0.475f, 0.0f, 0.0f }, 11, 0.5f,
      BrnEffects::Curves::SmoothStep{ Vector3{ 400.0f, 5700.0f, 22500.0f, 0.0f },
                                      Vector2{ 0.225f, 2.0f, 0.0f, 0.0f } } },
};

// ---------------------------------------------------------------------------------------------
// BrnCoronaTypeParams::GetCoronaTypeParams (DWARF BrnCoronaManager.h:80)
//
// INLINED on the console -- it has no standalone address; it is recovered from the only call site
// we hold, `sub_823FD428` (see the banner), which computes `&mParams[type]` with a bare type*48
// and NO bounds check.  The reconstruction is that expression.
// ---------------------------------------------------------------------------------------------
const BrnCoronaTypeParams& BrnCoronaTypeParams::GetCoronaTypeParams(BrnCoronaType leType)
{
    // [FLAG PC guard -- NOT PRESENT IN THE CONSOLE BINARY] sub_823FD428 indexes the table with a
    // raw type*48 and no range check, so an out-of-range BrnCoronaType reads 48 bytes past the
    // table there too.  This CGS_ASSERT costs one predictable compare per lookup in EVERY build
    // (CGS_ASSERT is not configuration-gated in this tree) and does NOT change the value returned
    // -- like the console, an out-of-range type still reads past the table.  It exists so a bad producer
    // (SubmitCoronasForRaceCar's per-tag-type switch is the one about to be written) fails loudly
    // instead of drawing a corona with a garbage texture id -- which would index the 12-row atlas
    // UV table out of bounds and still *render*, the exact "it draws, so it works" failure mode
    // this subsystem is prone to.  Delete it if a reviewer prefers strict binary parity.
    CGS_ASSERT(leType >= 0 && leType < eCoronaTypeCount,
               "leType >= 0 && leType < eCoronaTypeCount");

    return smParams[leType];
}

// =============================================================================================
// THE CORONA ATLAS UV TABLE (SEAM S2) -- X360 unk_82FAFC10, 12 pages x 4 corners x a 16-byte
// VMX Vector2 lane (x=u, y=v, z=w=0), built at run time on the console by sub_82C4EDD8; the rows
// live in SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronaatlasuvs.inl (with the
// per-lane store cites). BrnCoronaManager::Construct copies this table into the DWARF's non-const
// `BrnCoronaManager::s_atlasUVs[12][4]` -- the console's own static-initialiser effect, done at the
// earliest point every reader is guaranteed to follow.
//
// `extern` is REQUIRED: a namespace-scope `const` object has INTERNAL linkage in C++, and
// BrnCoronaManager.cpp declares `extern const Vector2 KA_CORONA_ATLAS_UVS[12][4];`.
// =============================================================================================
extern const Vector2 KA_CORONA_ATLAS_UVS[12][4] =
{
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronaatlasuvs.inl"
};
