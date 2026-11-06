import ida_bytes,ida_pro
f=open('D:/Reverse/BP-X360-Experimental-Artifacts/wave79/gui-preload-terra/artist-tables.txt','w')
for label,base,count in [('main',0x82F25F3C,1),('western',0x82F25F48,3),('cjk',0x82F25F64,2),('flapthud',0x82F25F30,1)]:
 f.write(label+' '+hex(base)+'\n')
 for i in range(count):f.write('%08X %08X\n'%(ida_bytes.get_dword(base+8*i),ida_bytes.get_dword(base+8*i+4)))
for a in [0x82F25F38,0x82F25F44,0x82F25F60,0x82F25F74]:f.write('%08X count %08X\n'%(a,ida_bytes.get_dword(a)))
f.close();ida_pro.qexit(0)
