import ida_hexrays,ida_pro
f=ida_hexrays.decompile(0x82ba0068)
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/sine-event-original.txt','w').write(str(f));ida_pro.qexit(0)
