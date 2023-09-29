#define macros
DIR_OBJ = obj/release
DIR_BIN = bin/release
CFLAGS = -O3 -Wall -std=c++17 -fexceptions -c #-m32
RESFLAGS = -J rc -O coff -i
create_dir = @mkdir -p $(@D)
OBJ = $(DIR_OBJ)/dart.o $(DIR_OBJ)/ascii2dirart.o $(DIR_OBJ)/petscii2dirart.o $(DIR_OBJ)/pixelcnttab.o $(DIR_OBJ)/charsettab.o $(DIR_OBJ)/thirdparty/lodepng.o

ifeq ($(OS),Windows_NT)
	OBJ += $(DIR_OBJ)/dart.res
	EXEC = $(DIR_BIN)/dart.exe
	LINKFLAGS = -static -static-libgcc -static-libstdc++ -s -std=c++17 -o
else
	EXEC = $(DIR_BIN)/dart

	UNAME = $(shell uname)
    ifeq ($(UNAME),Linux)
		LINKFLAGS = -static -static-libgcc -static-libstdc++ -s -std=c++17 -o #-m32
    endif
    ifeq ($(UNAME),Darwin)
		LINKFLAGS = -std=c++17 -o
    endif
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
