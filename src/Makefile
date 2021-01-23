all:
	g++ -g3 -O0 main.cpp image.cpp fs_scanner.cpp cairo_jpg.c cairo_png_writer.cpp surface.cpp `pkg-config --cflags --libs cairo libjpeg gtk+-3.0` -lpng -lmhash && ./a.out
	g++ -g3 -O0 generate_high_res_collage.cpp image.cpp fs_scanner.cpp cairo_jpg.c cairo_png_writer.cpp surface.cpp `pkg-config --cflags --libs cairo libjpeg gtk+-3.0` -lpng -lmhash &&  ./a.out "/home/lonezor/Media/media_timeline/1995 - 2000"
	g++ viewer.cpp fs_scanner.cpp  -o viewer -lmhash -lSDL2_image -lSDL2 && ./viewer
	g++ update_info.cpp fs_scanner.cpp -o /usr/bin/update_info
	g++ find_mismatches.cpp fs_scanner.cpp -o find_mismatches -lmhash && ./find_mismatches
	g++ -g3 -O0 tag_image.cpp image.cpp fs_scanner.cpp cairo_jpg.c cairo_png_writer.cpp surface.cpp -o tag_image `pkg-config --cflags --libs cairo libjpeg gtk+-3.0` -lpng -lmhash && ./tag_image "Gran Canaria, Spain" /tmp/2019-02-08\ 11.55.28.jpg 1550
