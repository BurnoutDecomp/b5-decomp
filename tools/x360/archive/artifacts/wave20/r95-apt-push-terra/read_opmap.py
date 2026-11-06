import ida_auto,ida_bytes,ida_name,ida_pro
ida_auto.auto_wait();f=open('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r95-apt-push-terra/artist-op-map.txt','w')
for i in range(0xb9):
 ea=ida_bytes.get_dword(0x82f73068+4*i);f.write('%02x %08x %s\n'%(i,ea,ida_name.get_name(ea)))
f.close();ida_pro.qexit(0)
