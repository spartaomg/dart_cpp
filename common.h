#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <string.h>
#include <array>
#include <vector>
#include <fstream>
#include <stdio.h>      /* printf, NULL */
#include <stdlib.h>     /* srand, rand */
#include <iomanip>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <math.h>
#include "thirdparty/lodepng.h"

using namespace std;

namespace fs = filesystem;

extern const int Ascii2DirArt_size;
extern unsigned char Ascii2DirArt[];
extern const int Petscii2DirArt_size;
extern unsigned char Petscii2DirArt[];
extern const int PixelCntTab_size;
extern unsigned char PixelCntTab[];
extern const int CharSetTab_size;
extern unsigned char CharSetTab[];
extern const int Char2Petscii_size;
extern unsigned char Char2Petscii[];
extern const int NumPalettes;	// = 63;
extern const int c64palettes_size;	// = 1008;
extern unsigned int c64palettes[];
