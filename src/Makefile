all:
	g++ -g3 -O0 main.cpp image.cpp fs_scanner.cpp cairo_jpg.c cairo_png_writer.cpp surface.cpp `pkg-config --cflags --libs cairo libjpeg gtk+-3.0` -lpng -lmhash
	g++ viewer.cpp fs_scanner.cpp  -o viewer -lmhash -lSDL2_image -lSDL2
