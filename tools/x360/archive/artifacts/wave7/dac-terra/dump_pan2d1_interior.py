import ida_hexrays,ida_pro
f=ida_hexrays.decompile(0x82b98db8)
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/pan2d1-interior-original.txt','w').write(str(f));ida_pro.qexit(0)
