# MesaFX-6.4.2

Mesa3d for 3dfx

Changes ported from DRI tdfx driver (*ish).

## Tools needed to build:
(what I used)
- Win10
- MinGw

## Build instructions
make -f Makefile.mgw clean 		(clean)
make -f Makefile.mgw FX=1		(Driver)
make -f Makefile.mgw X86=1 FX=1 (Driver with mmx,3dnow,sse... optimizations)

## Readme.3dfx
Legacy readme, in the project, with more informations.