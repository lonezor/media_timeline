all:
	g++ -g3 -O0 main.cpp image.cpp fs_scanner.cpp cairo_jpg.c cairo_png_writer.cpp surface.cpp `pkg-config --cflags --libs cairo libjpeg gtk+-3.0` -lpng -lmhash && ./a.out
	g++ viewer.cpp fs_scanner.cpp  -o viewer -lmhash -lSDL2_image -lSDL2 && ./viewer
	g++ update_info.cpp fs_scanner.cpp -o /usr/bin/update_info
	g++ find_mismatches.cpp fs_scanner.cpp -o find_mismatches -lmhash && ./find_mismatches
