windres.exe my.rc -O coff holoicon.res
gcc -mwindows -Wall -Wpedantic \
  `pkg-config --cflags glib-2.0 gobject-2.0 gtk+-3.0` \
  main.c holoicon.res\
  `pkg-config --libs glib-2.0 gobject-2.0 gtk+-3.0` \
  -o HoloCureKR.exe
gcc -mwindows exec_bin.c holoicon.res -o HoloCureKRLauncher.exe
#-mwindows
