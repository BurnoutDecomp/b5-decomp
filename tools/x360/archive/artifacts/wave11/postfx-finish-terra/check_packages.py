import pathlib,struct,json,re,hashlib
root=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave11/postfx_native_sol'); out=root.parent/'postfx-finish-terra'
src=pathlib.Path('src/GameSource/Graphics/PostFx/BrnPostFxShader.cpp').read_text(); rows=[]
for spec in json.loads((root/'shader_packages.json').read_text()):
 name=spec['name']; b=(root/(name+'.bin')).read_bytes(); m=re.search(r'const u32 '+name+r'\[\]\s*=\s*\{(.*?)\};',src,re.S); assert m,name
 words=[int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',m[1])]; assert b==b''.join(struct.pack('>I',x) for x in words)
 u=lambda o:struct.unpack_from('>I',b,o)[0]
 shader_off,shader_size,table_off=u(4),u(8),u(16); assert shader_off+shader_size<=len(b)
 # XGGetMicrocodeShaderParts: base+word[1], size word[2]. XGMicrocodeGetConstantTable: base+word[4]+4.
 size=u(table_off); base=table_off+4; assert table_off+4+size<=len(b)
 count=u(base+12); entries=base+u(base+16); assert entries+20*count<=table_off+4+size
 names=[]
 for i in range(count):
  o=base+u(entries+20*i); assert base<=o<table_off+4+size
  end=b.index(b'\0',o,table_off+4+size); names.append(b[o:end].decode('ascii'))
 rows.append(dict(name=name,size=len(b),microcode_offset=shader_off,microcode_size=shader_size,table_offset=table_off,table_size=size,constant_names=names,source_bytes_match=True))
(out/'package-relocation-check.json').write_text(json.dumps({'evidence':['829435D0','82943F58','82B608A8'], 'method':'big-endian offsets relative to package or constant table; bounds and NUL terminated names, source bytes match original','packages':rows},indent=2))
print('validated',len(rows),'packages')
