CC=g++
CXX_FLAGS=-std=c++11 -O0 -g3 -Wall
LIBS=-lpthread -lfreeimage -limagequant -lmhash -lpng

HDR=	general.h \
	Common.h \
	Directory.h \
	Html.h \
	Options.h \
	SharedThreadData.h \
	File.h \
	FileSystem.h \
	Image.h \
	Video.h \
	Media.h \
	ShellCmd.h \
	Task.h \
	GenerateSrcFileHashTask.h \
	GenerateImageTask.h \
	GenerateAudioTask.h \
	GenerateVideoTask.h \
	ReadImageMetaDataTask.h \
	ReadVideoMetaDataTask.h \
        ReadAudioMetaDataTask.h \
	SyncWithFsTask.h \
	PngReader.h \
	PngWriter.h

OBJ=	main.o \
	Common.o \
	Directory.o \
	Html.o \
	Options.o \
	SharedThreadData.o \
	File.o \
	FileSystem.o \
	Image.o \
	Video.o \
	Media.o \
	ShellCmd.o \
	Task.o \
	GenerateSrcFileHashTask.o \
	GenerateImageTask.o \
	GenerateAudioTask.o \
	GenerateVideoTask.o \
	ReadImageMetaDataTask.o \
	ReadVideoMetaDataTask.o \
        ReadAudioMetaDataTask.o \
	SyncWithFsTask.o \
	PngReader.o \
	PngWriter.o

%.o: %.cpp $(HDR)
	$(CC) $(CXX_FLAGS) -c $<

mtransc: $(OBJ)
	$(CC) -o $@  *.o $(LIBS)

all: mtransc

install:
	cp -f mtransc /usr/bin

clean:
	rm -f *.o




