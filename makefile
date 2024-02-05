
#---------------------------------------------------
# This makefile requires GNU make, mkdir, and g++.
# On Windows please use MinGW.
#---------------------------------------------------

ifeq ($(OS),Windows_NT)
	DIR_OBJ = obj/windows
	DIR_BIN = bin/windows
	EXEC = $(DIR_BIN)/DART.exe
	LINKFLAGS = -static -static-libgcc -static-libstdc++ -s -std=c++17 -o
else
	UNAME = $(shell uname)
	ifeq ($(UNAME),Linux)
		DIR_OBJ = obj/linux
		DIR_BIN = bin/linux
		LINKFLAGS = -static -static-libgcc -static-libstdc++ -s -std=c++17 -o
	endif
	ifeq ($(UNAME),Darwin)
		DIR_OBJ = obj/macos
		DIR_BIN = bin/macos
		LINKFLAGS = -std=c++17 -o
	endif
	EXEC = $(DIR_BIN)/dart
endif

# "mkdir" (with quotes) will force Windows to search for the executable (GNU) mkdir instead of the CMD command.
# Just make sure that its folder is in the system PATH. It also works on Linux and macOS.
create_dir = @"mkdir" -p $(@D)

CFLAGS = -O3 -Wall -std=c++17 -fexceptions -c #-m32
RESFLAGS = -J rc -O coff -i
OBJ = $(DIR_OBJ)/ascii2dirart.o $(DIR_OBJ)/c64palettes.o $(DIR_OBJ)/charsettab.o $(DIR_OBJ)/char2petscii.o $(DIR_OBJ)/dart.o $(DIR_OBJ)/petscii2dirart.o $(DIR_OBJ)/pixelcnttab.o $(DIR_OBJ)/thirdparty/lodepng.o

ifeq ($(OS),Windows_NT)
	OBJ += $(DIR_OBJ)/dart.res
endif

$(EXEC): $(OBJ)
	$(create_dir)
	g++ $(LINKFLAGS) $(EXEC) $(OBJ)

$(DIR_OBJ)/%.o: %.cpp common.h
	$(create_dir)
	g++ $(CFLAGS) $< -o $@

$(DIR_OBJ)/thirdparty/%.o: thirdparty/%.cpp thirdparty/%.h
	$(create_dir)
	g++ $(CFLAGS) $< -o $@

$(DIR_OBJ)/dart.res: dart.rc resource.h
	$(create_dir)
	windres $(RESFLAGS) dart.rc -o $(DIR_OBJ)/dart.res
