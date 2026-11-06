import sys,struct,pathlib,json,hashlib
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re'); import x360rd
u=lambda a:struct.unpack('>I',x360rd.rd(a,4))[0]
for a,w in {0x827f1cdc:0x38800002,0x827f1ce8:0x907e0024,0x827f1cec:0x38800001,0x827f1cf8:0x907e0020,0x82400480:0x807c0018,0x82400488:0x807c001c,0x82400490:0x807c002c,0x822844d8:0x809e002c,0x822844e4:0x809e0000,0x822844f4:0x809e001c}.items(): assert u(a)==w,(hex(a),hex(u(a)))
base=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r26-imstate-terra'); files=list((base/'objects').glob('*.obj')); files=[p for p in files if not p.name.startswith('BrnSimpleParticleRenderer-')]+list((base/'simple-final').glob('*.obj')); assert len(files)==6
for p in files:
 b=p.read_bytes(); assert struct.unpack_from('<H',b)[0]==498
 assert all(x not in b for x in [b'ImDeviceSetBlendState',b'ImDeviceSetDepthStencilState',b'ImDeviceSetRasterizerState',b'gpSkyDomeDepthStencilState',b'gpSkyDomeRasterizerState',b'gpSkyDomeBlendState',b'gpSkyDomeEnvMapDepthStencilState',b'gpImDebrisRasterizerState',b'gpImDebrisDepthStencilState',b'gpImStandardAlphaBlendState',b'gpImAdditiveBlendState'])
 assert b'mgStateLibrary@ImRendererBase@CgsGraphics' in b
simple=next(p for p in files if p.name.startswith('BrnSimpleParticleRenderer'))
assert b'dword_83010F20' not in simple.read_bytes()
print(json.dumps({'raw_sentinels':'pass','machine':'xbox360 ppc big-endian 0x1f2','obsolete_wrapper_and_alias_symbols':'absent','library_symbol':'present in all six','objects':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}},indent=2))
