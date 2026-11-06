from pathlib import Path
p=Path('D:/Reverse/BP-X360-Experimental-Worktrees/world-module-native-r1/src/GameSource/World/BrnWorldModule.cpp')
s=p.read_bytes().decode('utf-8').replace('\r\n','\n')
def repl(a,b):
 global s
 assert a in s,a[:100]
 s=s.replace(a,b)
start=s.index('static __forceinline Vector4 MakeWorldVector4(');end=s.index('WorldModule::WorldModule()',start)
s=s[:start]+s[end:];s=s.replace('MakeWorldVector4(', 'Vector4(')
a=s.index('// Clamp a colour to [0, white level]');b=s.index('    // ------------------------------------------------------------------------',a)
s=s[:a]+'''// ARTIST827D15E0/15F4: clamp all four native lanes, with zeroW in the ceiling.
static void ClampColourToWhiteLevel( Vector3& lrColour, f32 lfWhiteLevel )
{
    const Vector3 lCeiling(lfWhiteLevel, lfWhiteLevel, lfWhiteLevel);
    lrColour = __vminfp(__vmaxfp(lrColour.GetVector(), __vspltisw(0)), lCeiling.GetVector());
}

// ARTIST827D2634..2720: each irradiance row is multiplied as a full VMX register.
static void ScaleIrradiance( Matrix44& lrIrradiance, f32 lfScale )
{
    VecFloat lScale;
    lScale.SetFloat(lfScale);
    lrIrradiance.xAxis = __vmulfp(lrIrradiance.xAxis.GetVector(), lScale.GetVector());
    lrIrradiance.yAxis = __vmulfp(lrIrradiance.yAxis.GetVector(), lScale.GetVector());
    lrIrradiance.zAxis = __vmulfp(lrIrradiance.zAxis.GetVector(), lScale.GetVector());
    lrIrradiance.wAxis = __vmulfp(lrIrradiance.wAxis.GetVector(), lScale.GetVector());
}

'''+s[b:]
a=s.index('void\nShaderLodInfo::Construct()');b=s.index('// ============================================================================',a)
s=s[:a]+'''void
ShaderLodInfo::Construct()
{
    mShaderLod1NearDistance = 20.0f;
    mMaxBelievableRadius = 1000.0f;
    mfShaderLod1NearDistance = 20.0f;
    miEnvMapTechnique = 0;
    miOverrideTechnique = -1;
    mbUseShaderLod = false;
}

void
ShaderLodInfo::Update()
{
    mShaderLod1NearDistance = mfShaderLod1NearDistance;
}

'''+s[b:]
a=s.index('                // ⛔ THE BAIL PREVENTS AN OUT-OF-BOUNDS WRITE.');b=s.index('                maeCarControls[ meLocalPlayerActiveRaceCarIndex ] =',a)
s=s[:a]+'''                // ARTIST827C4640..4650 continues after the original non-gating assert.
'''+s[b:]
a=s.index('                    // NOT X360. One line, once:');b=s.index('                }\n                break;',a)
s=s[:a]+s[b:]
repl('''        Vector3 lvCarKeyLight = lvKeyLightColour;
        lvCarKeyLight.x *= mfCarKeyLightMultiplier;
        lvCarKeyLight.y *= mfCarKeyLightMultiplier;
        lvCarKeyLight.z *= mfCarKeyLightMultiplier;
''','''        VecFloat lCarKeyLightScale;
        lCarKeyLightScale.SetFloat(mfCarKeyLightMultiplier);
        const Vector3 lvCarKeyLight(__vmulfp(lvKeyLightColour.GetVector(), lCarKeyLightScale.GetVector()));
''')
a=s.index('// -----------------------------------------------------------------------------\n// Lane accessors for the two rw::math::vpu idioms');b=s.index('// =============================================================================\n// GenerateShaderConstantsForQuadricIrradiance',a)
s=s[:a]+s[b:]
a=s.index('void\nGenerateShaderConstantsForQuadricIrradiance(');b=s.index('// =============================================================================\n// WorldModule::SetupShaderConstantsBeforeRendering',a)
s=s[:a]+'''void
GenerateShaderConstantsForQuadricIrradiance( const rw::math::vpu::Matrix44& lIrradianceMatrixR,
                                           const rw::math::vpu::Matrix44& lIrradianceMatrixG,
                                           const rw::math::vpu::Matrix44& lIrradianceMatrixB,
                                           Matrix44& lOutQuadricMatrix0,
                                           Matrix44& lOutQuadricMatrix1 )
{
    Vector4 lIrradiance_1;
    lIrradiance_1.SetZero();
    Matrix44 lIrradiance_x_y_z_xx;
    Matrix44 lIrradiance_xy_yz_zx_yy;
    Vector4* const lapLinearRows[3] = { &lIrradiance_x_y_z_xx.yAxis,
        &lIrradiance_x_y_z_xx.zAxis, &lIrradiance_x_y_z_xx.wAxis };
    Vector4* const lapCrossRows[3] = { &lIrradiance_xy_yz_zx_yy.xAxis,
        &lIrradiance_xy_yz_zx_yy.yAxis, &lIrradiance_xy_yz_zx_yy.zAxis };
    const Matrix44* const laIrradianceMatrices[3] =
        { &lIrradianceMatrixR, &lIrradianceMatrixG, &lIrradianceMatrixB };
    for (u32 luChannel = 0; luChannel < 3; ++luChannel)
    {
        const Matrix44& m = *laIrradianceMatrices[luChannel];
        const f32 Czz = m.zAxis.GetZ().GetFloat();
        const f32 Cxx = m.xAxis.GetX().GetFloat() - Czz;
        const f32 Cyy = m.yAxis.GetY().GetFloat() - Czz;
        const f32 C1 = m.wAxis.GetW().GetFloat() + Czz;
        const f32 Cx = 2.0f * m.xAxis.GetW().GetFloat();
        const f32 Cy = 2.0f * m.yAxis.GetW().GetFloat();
        const f32 Cz = 2.0f * m.zAxis.GetW().GetFloat();
        const f32 Cxy = 2.0f * m.xAxis.GetY().GetFloat();
        const f32 Cyz = 2.0f * m.yAxis.GetZ().GetFloat();
        const f32 Czx = 2.0f * m.zAxis.GetX().GetFloat();
        lIrradiance_1.SetComponent(luChannel, C1);
        *lapLinearRows[luChannel] = Vector4(Cx, Cy, Cz, Cxx);
        *lapCrossRows[luChannel] = Vector4(Cxy, Cyz, Czx, Cyy);
    }
    // Preserve the original row-store order, including B.w before B.xyz.
    lOutQuadricMatrix0.xAxis = lIrradiance_1;
    lOutQuadricMatrix0.yAxis = lIrradiance_x_y_z_xx.yAxis;
    lOutQuadricMatrix0.zAxis = lIrradiance_x_y_z_xx.zAxis;
    lOutQuadricMatrix0.wAxis = lIrradiance_x_y_z_xx.wAxis;
    lOutQuadricMatrix1.wAxis = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    lOutQuadricMatrix1.xAxis = lIrradiance_xy_yz_zx_yy.xAxis;
    lOutQuadricMatrix1.yAxis = lIrradiance_xy_yz_zx_yy.yAxis;
    lOutQuadricMatrix1.zAxis = lIrradiance_xy_yz_zx_yy.zAxis;
}

'''+s[b:]
repl('''    const VecFloat lSimTime  = { lfSimTime,  lfSimTime,  lfSimTime,  lfSimTime  };
    const VecFloat lGameTime = { lfGameTime, lfGameTime, lfGameTime, lfGameTime };
    Vector4 lTime;
    lTime.SetZero();
    lTime.x = lSimTime.x;
    lTime.y = lGameTime.x;
''','''    const Vector4 lTime(lfSimTime, lfGameTime, 0.0f, 0.0f);
''')
repl('    lKeyLightClampedColour.w = 0.0f;\n','')
repl('''    Vector4 lFogColourPlusWhiteLevel;
    lFogColourPlusWhiteLevel.x = lScatt_TopColourDrk.x;
    lFogColourPlusWhiteLevel.y = lScatt_TopColourDrk.y;
    lFogColourPlusWhiteLevel.z = lScatt_TopColourDrk.z;
    lFogColourPlusWhiteLevel.w = lfWhiteLevel;
''','''    Vector4 lFogColourPlusWhiteLevel = lScatt_TopColourDrk;
    lFogColourPlusWhiteLevel.SetW(lfWhiteLevel);
''')
repl('''    Vector4 lHDRConstants;
    lHDRConstants.x = lfWhiteLevel;
    lHDRConstants.y = 1.0f / lfWhiteLevel;
    lHDRConstants.z = 0.0f;
    lHDRConstants.w = 0.0f;
''','''    const Vector4 lHDRConstants(lfWhiteLevel, 1.0f / lfWhiteLevel, 0.0f, 0.0f);
''')
repl('''    lpOutputShaderConstants->SetCloudDarkColour0( Vector4( laCloudDarkColours[ 0 ].x,
                                                          laCloudDarkColours[ 0 ].y,
                                                          laCloudDarkColours[ 0 ].z, 1.0f ) );
    lpOutputShaderConstants->SetCloudLiteColour0( Vector4( laCloudLiteColours[ 0 ].x,
                                                          laCloudLiteColours[ 0 ].y,
                                                          laCloudLiteColours[ 0 ].z, 1.0f ) );
''','''    lpOutputShaderConstants->SetCloudDarkColour0(Vector4(laCloudDarkColours[0], 1.0f));
    lpOutputShaderConstants->SetCloudLiteColour0(Vector4(laCloudLiteColours[0], 1.0f));
''')
for var in ['lCloudOpacity','lCloudNegativeDensity','lCloudFeathering']:
 for lane,index in [('x',0),('y',1)]:s=s.replace(var+'.'+lane,var+f'.mV.v[{index}]')
p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
print('Owned source candidate bytes',p.stat().st_size)
