all: boot-nec.exe boot-ibm.exe

boot-nec.exe: elfboot.c
	wcl -l=dos -3 -fe=boot-nec.exe -dARCH_PC98 elfboot.c

boot-ibm.exe: elfboot.c
	wcl -l=dos -3 -fe=boot-ibm.exe -dARCH_PCAT elfboot.c
