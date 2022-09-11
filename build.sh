gcc -mwindows -Wall -Wpedantic -Werror `pkg-config --cflags glib-2.0 gobject-2.0 gtk+-3.0` test.c `pkg-config --libs glib-2.0 gobject-2.0 gtk+-3.0`
#-mwindows
