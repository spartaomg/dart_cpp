
//#define DEBUG
#define AddBlockCount

#include "common.h"
#include "thirdparty/gif.h"

const int GifWidth = 384;
const int GifHeight = 272;
static uint8_t GifImage[GifWidth * GifHeight * 4];

int GifFrameCount = 0;

const int StdDiskSize = (664 + 19) * 256;
const int ExtDiskSize = StdDiskSize + (85 * 256);

const int NumSectorsTrack18 = 19;
int NumSectorsOnTrack = NumSectorsTrack18;

int NumFreeEntries = 0;

vector <unsigned char> Disk;
vector <unsigned char> Image;   //pixels in RGBA format (4 bytes per pixel)
vector <unsigned char> ImgRaw;
vector <unsigned char> ScrRam;
vector <unsigned char> ColRam;

//command line parameters
string InFileName = "";
string OutFileName = "";
string argSkippedEntries = "0";
string argEntryType = "DEL";
string argFirstImportedEntry = "";
string argLastImportedEntry = "";
string argDiskName = "";
string argDiskID = "";
string argPalette = "16";

unsigned char FileType = 0x80;  //Default file type is DEL
int NumSkippedEntries = 0;      //We are overwriting the whole directory by default
int FirstImportedEntry = 1;     //We start import with the first entry by default
int LastImportedEntry = 0xffff; //We import the whole directory art by default

int EntryIndex = 0;             //Index of current entry, will be compared to FirstImportedentry and LastImportedEntry

string DirEntry = "";
string DirArt = "";
string DirArtType = "";
string OutputType = "";

bool DirEntryAdded = false;
bool AppendMode = false;

int DirPos = 0;
int DiskSize = 0;
int Mplr = 0;

unsigned int ImgWidth = 0;
unsigned int ImgHeight = 0;
unsigned int BGCol = 0;
unsigned int FGCol = 0;

int PaletteIdx = 16;

int ColorBlack = 0;
int ColorWhite = 1;
int ColorRed = 2;
int ColorCyan = 3;
int ColorPurple = 4;
int ColorGreen = 5;
int ColorBlue = 6;
int ColorYellow = 7;
int ColorOrange = 8;
int ColorBrown = 9;
int ColorPink = 10;
int ColorDkGrey = 11;
int ColorMdGrey = 12;
int ColorLtGreen = 13;
int ColorLtBlue = 14;
int ColorLtGrey = 15;

string C64PaletteNames[23] {
"00 (C64HQ)",
"01 (C64S)",
"02 (CCS64)",
"03 (ChristopherJam)",
"04 (Colodore)",
"05 (Community Colors)",
"06 (Deekay)",
"07 (Frodo)",
"08 (Godot)",
"09 (PALette)",
"10 (PALette 6569R1)",
"11 (PALette 6569R5)",
"12 (PALette 8565R2)",
"13 (PC64)",
"14 (Pepto NTSC Sony)",
"15 (Pepto NTSC)",
"16 (Pepto PAL)",
"17 (Pepto PAL old)",
"18 (Pixcen)",
"19 (Ptoing)",
"20 (RGB)",
"21 (VICE 3.8 Original)",
"22 (VICE 3.8 Internal)"
};

unsigned int BkgColor = 0;  // ColorBlue;       //dark blue
//unsigned int ForeColor = ColorLtBlue;      //light blue

size_t DirTrack = 18, DirSector = 1, LastDirTrack{}, LastDirSector{};
size_t Track[41]{};

int ScreenLeft = 32;
int ScreenTop = 35;
int CharX = 0;      // Virtual screen X
int CharY = 0;      // Virtual screen Y
int CursorX = 0;    // Screen X
int CursorY = 0;    // Screen Y

size_t ScrLeft = 0;
size_t ScrTop = 0;
size_t ScrWidth = 0;
size_t ScrHeight = 0;

bool HasBorders = false;

int NumDirEntries = 0;
int MaxNumDirEntries = ((0xa000 - 0x800) / 0x20) - 2;   //1214 + header + blocks free, this fills up the whole BASIC RAM

#ifdef DEBUG
    int ThisDirEntry = 0;
#endif

unsigned char CurrentColor = 0x0e;      //We start with light blue

bool QuotedText = false;
//bool HeaderText = true;
bool InvertedText = false;
int CharSet = 0;                        //Upper case = 0, lower case = 1

int NumExtraSpaces = 0;

int NumInserts = 0;

bool CharSetSwitchEnabled = true;

typedef struct tagBITMAPINFOHEADER {
    int32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    int16_t biPlanes;
    int16_t biBitCount;
    int32_t biCompression;
    int32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    int32_t biClrUsed;
    int32_t biClrImportant;
} BITMAPINFOHEADER, * LPBITMAPINFOHEADER, * PBITMAPINFOHEADER;

typedef struct tagRGBQUAD {
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[1];
} BITMAPINFO, * LPBITMAPINFO, * PBITMAPINFO;

BITMAPINFO BmpInfo;
BITMAPINFOHEADER BmpInfoHeader;

//----------------------------------------------------------------------------------------------------------------------------------------------------

string ConvertIntToHextString(const int i, const int hexlen)
{
    std::stringstream hexstream;
    hexstream << setfill('0') << setw(hexlen) << hex << i;
    return hexstream.str();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

int ConvertStringToInt(const string& s)
{
    return stoul(s, nullptr, 10);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

int ConvertHexStringToInt(const string& s)
{
    return stoul(s, nullptr, 16);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

string ConvertHexStringToDecimalString(const string& s)
{
    return to_string(ConvertHexStringToInt(s));
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool IsHexString(const string& s)
{
    return !s.empty() && all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c); });
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool IsNumeric(const string& s)
{
    return !s.empty() && find_if(s.begin(), s.end(), [](unsigned char c) { return !isdigit(c); }) == s.end();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool CreateDirectory(const string& DiskDir)
{
    if (!fs::exists(DiskDir))
    {
        cout << "Creating folder: " << DiskDir << "\n";
        fs::create_directories(DiskDir);
    }

    if (!fs::exists(DiskDir))
    {
        cerr << "***ABORT***\tUnable to create the following folder: " << DiskDir << "\n\n";
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool WriteBinaryFile(string FileName, vector <unsigned char> binfile)
{

    ofstream myFile(FileName + ".bin", ios::out | ios::binary);

    if (myFile.is_open())
    {
        //cout << "Writing " + FileName + ".bin" + "...\n";
        myFile.write((char*)&binfile[0], binfile.size());

        if (!myFile.good())
        {
            cerr << "***ABORT***\tError during writing " << FileName << ".bin\n";
            myFile.close();
            return false;
        }

        //cout << "Done!\n";
        return true;
    }
    else
    {
        cerr << "***ABORT***\tError opening file for writing disk image " << FileName << ".bin\n\n";
        return false;
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool WriteBinaryFile()
{

    ofstream myFile(OutFileName + ".bin", ios::out | ios::binary);

    if (myFile.is_open())
    {
        cout << "Writing " + OutFileName + ".bin" + "...\n";
        myFile.write((char*)&Image[0], (size_t)ImgWidth * ImgHeight * 4);

        if (!myFile.good())
        {
            cerr << "***ABORT***\tError during writing " << OutFileName << "\n";
            myFile.close();
            return false;
        }

        cout << "Done!\n";
        return true;
    }
    else
    {
        cerr << "***ABORT***\tError opening file for writing disk image " << OutFileName << "\n\n";
        return false;
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool WriteDiskImage(const string& DiskName)
{

    string DiskDir{};

    for (size_t i = 0; i <= DiskName.length() - 1; i++)
    {
#if _WIN32 
        if ((DiskName[i] == '\\') || (DiskName[i] == '/'))
        {
            if (DiskDir[DiskDir.length() - 1] != ':')   //Don't try to create root directory
            {
                if (!CreateDirectory(DiskDir))
                    return false;
            }
        }
#elif __APPLE__ || __linux__
        if ((DiskName[i] == '/') && (DiskDir.size() > 0) && (DiskDir != "~"))   //Don't try to create root directory and home directory
        {
            if (!CreateDirectory(DiskDir))
                return false;
        }
#endif
        DiskDir += DiskName[i];
    }

    ofstream myFile(DiskName, ios::out | ios::binary);

    if (myFile.is_open())
    {
        cout << "Writing " + DiskName + "...\n";
        myFile.write((char*)&Disk[0], DiskSize);

        if (!myFile.good())
        {
            cerr << "***ABORT***\tError during writing " << DiskName << "\n";
            myFile.close();
            return false;
        }
        
        cout << "Done!\n";
        return true;
    }
    else
    {
        cerr << "***ABORT***\tError opening file for writing disk image " << DiskName << "\n\n";
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

int ReadBinaryFile(const string& FileName, vector<unsigned char>& prg)
{

    if (!fs::exists(FileName))
    {
        return -1;
    }

    prg.clear();

    ifstream infile(FileName, ios_base::binary);

    if (infile.fail())
    {
        return -1;
    }

    infile.seekg(0, ios_base::end);
    int length = infile.tellg();
    infile.seekg(0, ios_base::beg);

    prg.reserve(length);

    copy(istreambuf_iterator<char>(infile), istreambuf_iterator<char>(), back_inserter(prg));

    infile.close();

    return length;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

string ReadFileToString(const string& FileName, bool CorrectFilePath = false)
{

    if (!fs::exists(FileName))
    {
        return "";
    }

    ifstream infile(FileName);

    if (infile.fail())
    {
        return "";
    }

    string str;

    infile.seekg(0, ios::end);
    str.reserve(infile.tellg());
    infile.seekg(0, ios::beg);

    str.assign((istreambuf_iterator<char>(infile)), istreambuf_iterator<char>());

    for (int i = str.length() - 1; i >= 0; i--)        //Do this backwards - we may shorten the string the string
    {
        if (str[i] == '\r')
        {
            str.replace((size_t)i, 1, "");      //Windows does this automatically, remove '\r' (0x0d) chars if string contains any
        }
        else if ((CorrectFilePath) && (str[i] == '\\'))
        {
            str.replace((size_t)i, 1, "/");     //Replace '\' with '/' in file paths, Windows can also handle this
        }
    }

    infile.close();

    return str;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

unsigned int GetPixel(size_t X, size_t Y)
{
    size_t Pos = Y * ((size_t)ImgWidth * 4) + (X * 4);

    unsigned char R = Image[Pos + 0];
    unsigned char G = Image[Pos + 1];
    unsigned char B = Image[Pos + 2];
    //unsigned char A = Image[Pos + 3];

    unsigned int Col = (R << 16) + (G << 8) + B;        //Ignore A

    return Col;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void SetPixel(size_t X, size_t Y, unsigned int Col)
{
    size_t Pos = Y * ((size_t)ImgWidth * 4) + (X * 4);

    unsigned char R = (Col >> 16) & 0xff;
    unsigned char G = (Col >> 8) & 0xff;
    unsigned char B = Col & 0xff;
    unsigned char A = 255;

    Image[Pos + 0] = R;
    Image[Pos + 1] = G;
    Image[Pos + 2] = B;
    Image[Pos + 3] = A;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void SetGifPixel(int X, int Y, unsigned int Col)
{
    int Pos = (Y * GifWidth + X) * 4;

    if (Pos < GifWidth * GifHeight * 4)
    {
        uint8_t* pixel = &GifImage[Pos];

        unsigned char R = (Col >> 16) & 0xff;
        unsigned char G = (Col >> 8) & 0xff;
        unsigned char B = Col & 0xff;
        unsigned char A = 255;

        pixel[0] = R;
        pixel[1] = G;
        pixel[2] = B;
        pixel[3] = A;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void DrawGifChar(unsigned char Char, unsigned char Col, int GifX, int GifY)
{

    int Color = c64palettes[(PaletteIdx * 16) + ColorLtBlue];  //Default = light blue
    int BkgColor = c64palettes[(PaletteIdx * 16) + ColorBlue];

    if (Col < 0x10)
    {
        Color = c64palettes[(PaletteIdx * 16) + Col];
    }

    unsigned int px = Char % 16;    //lower nibble
    unsigned int py = Char / 16;    //upper nibble

    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            unsigned int CharSetPos = (CharSet * CharSetTab_size / 2) + (py * 8 * 128) + (y * 128) + (px * 8) + x;

            if (CharSetTab[CharSetPos] == 1)
            {
                SetGifPixel((size_t)GifX + x, (size_t)GifY + y, Color);
            }
            else
            {
                SetGifPixel((size_t)GifX + x, (size_t)GifY + y, BkgColor);
            }
        }
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void DrawChar(unsigned char Char, unsigned char Col, int PngX, int PngY)
{
    int Color = c64palettes[(PaletteIdx * 16) + ColorLtBlue];  //Default = light blue
    int BkgColor = c64palettes[(PaletteIdx * 16) + ColorBlue];

    if (Col < 0x10)
    {
        Color = c64palettes[(PaletteIdx * 16) + Col];
    }
    
    unsigned int px = Char % 16;    //lower nibble
    unsigned int py = Char / 16;    //upper nibble

    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            unsigned int CharSetPos = (CharSet * CharSetTab_size / 2) + (py * 8 * 128) + (y * 128) + (px * 8) + x;

            if (CharSetTab[CharSetPos] == 1)
            {
                SetPixel((size_t)PngX + x, (size_t)PngY + y, Color);
            }
            else
            {
                SetPixel((size_t)PngX + x, (size_t)PngY + y, BkgColor);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void OutputDel()
{
    if (CharX > 0)
    {
        CharX--;
        for (size_t x = CharX; x < 78; x++)
        {
            ScrRam[(size_t)(CharY * 80) + x] = ScrRam[(size_t)(CharY * 80) + x + 1];
            ColRam[(size_t)(CharY * 80) + x] = ColRam[(size_t)(CharY * 80) + x + 1];
        }
        ScrRam[(size_t)(CharY * 80) + 79] = 0x00;
        ColRam[(size_t)(CharY * 80) + 79] = 0xff;
    }
    else if (CharY > 0)
    {
        CharY--;

        if (ScrRam[(size_t)(CharY * 80) + 40] != 0x00)
        {
            CharX = 79;
        }
        else
        {
            CharX = 39;
        }
        ScrRam[(size_t)(CharY * 80) + CharX] = 0x00;
        ColRam[(size_t)(CharY * 80) + CharX] = 0xff;
    }

    if (CursorX > 0)
    {
        CursorX--;
    }
    else if (CursorY > 0)
    {
        CursorX = 39;
        CursorY--;
    }


    return;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void OutputInsert()
{
    if ((CharX != 79) && ((ScrRam[(size_t)(CharY * 80) + 79] == 0x20) || (ScrRam[(size_t)(CharY * 80) + 79] == 0x00)))  //Last char of double line is space and we are not on the last char
    {
        NumInserts++;   //count number of inserts

        int MaxX = 38;
        if ((ScrRam[(size_t)(CharY * 80) + 40] != 0x00) || ((ScrRam[(size_t)(CharY * 80) + 39] != 0x00) && (ScrRam[(size_t)(CharY * 80) + 39] != 0x20)))
        {
            MaxX = 78;
        }

        for (int x = MaxX; x >= CharX; x--)
        {
            ScrRam[(size_t)(CharY * 80) + x + 1] = ScrRam[(size_t)(CharY * 80) + x];
            ColRam[(size_t)(CharY * 80) + x + 1] = ColRam[(size_t)(CharY * 80) + x];
        }

        ScrRam[(size_t)(CharY * 80) + CharX] = 0x20;
        ColRam[(size_t)(CharY * 80) + CharX] = CurrentColor;

        //If we just pushed the last (non-space) char of the first halfline to the first pos in the second halfline then fill the rest of the second half line with space  
        if ((ScrRam[(size_t)(CharY * 80) + 40] != 0x00) && (ScrRam[(size_t)(CharY * 80) + 41] == 0x00))
        {
            for (int i = 41; i < 80; i++)
            {
                ScrRam[(size_t)(CharY * 80) + i] = 0x20;
                ColRam[(size_t)(CharY * 80) + i] = 0x0e;    //c64palettes[(PaletteIdx * 16) + 0x0e]; //light blue
            }
        }
    }
    return;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void OutputToScreen(unsigned char ThisChar)
{

    size_t Pos = (size_t)(CharY * 80) + CharX;

    if (Pos >= ScrRam.size())
    {
        ScrRam.resize((size_t)(CharY + 1) * 80);
        ColRam.resize((size_t)(CharY + 1) * 80);

        for (size_t i = ScrRam.size() - 80; i < ScrRam.size(); i++)
        {
            ScrRam[i] = 0x00;
            ColRam[i] = 0xff;
        }
    }

    //If this will be the first char in the second halfline, then fill halfline with space to mark it
    if (CharX >= 40)
    {
        if (ScrRam[(size_t)(CharY * 80) + 40] == 0x00)
        {
            for (int i = 40; i < 80; i++)
            {
                ScrRam[(size_t)(CharY * 80) + i] = 0x20;
            }
        }
    }
    else
    {
        if (ScrRam[(size_t)(CharY * 80)] == 0x00)
        {
            for (int i = 00; i < 40; i++)
            {
                ScrRam[(size_t)(CharY * 80) + i] = 0x20;
            }
        }
    }

    ScrRam[Pos] = ThisChar;
    ColRam[Pos] = CurrentColor;

    CharX++;

    if (CharX == 80)
    {
        CharX = 0;
        CharY++;
    }

    CursorX++;
    if (CursorX == 40)
    {
        CursorX = 0;
        if (CursorY < 24)
        {
            CursorY++;
        }
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void OutputCR()
{
    NumInserts = 0;
    QuotedText = false;
    //HeaderText = false;
    InvertedText = false;
    CharX = 0;
    CharY++;
    CursorX = 0;
    if (CursorY < 24)
    {
        CursorY++;
    }
    if (((size_t)(CharY * 80) >= ScrRam.size()) || (ScrRam[(CharY * 80) + CharX] == 0x00))
    {
        OutputToScreen(0x20);
        CharX = 0;
        CursorX = 0;
    }
    
    return;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void chrout(unsigned char ThisChar)
{
    //Direct implementation of the KERNAL CHROUT routine
    
    if (ThisChar < 0x80)
    {
        //---------------------------------------------
        //UNSHIFTED
        //---------------------------------------------

        if (ThisChar == 0x0d)
        {
            OutputCR();
            return;
        }
        else if (ThisChar < 0x20)
        {
            if (NumInserts == 0)
            {
                if (ThisChar == 0x14)
                {
                    OutputDel();
                    return;
                }
                else
                {
                    if (QuotedText)
                    {
                        ThisChar |= 0x80;
                        if (NumInserts > 0)
                        {
                            NumInserts--;
                        }
                        
                        //OUTPUT TO SCREEN
                        OutputToScreen(ThisChar);
                        return;
                    }
                    else
                    {
                        //Not quoted text
                        if (ThisChar == 0x12)
                        {
                            InvertedText = true;
                            return;
                        }
                        else if (ThisChar == 0x13)
                        {
                            //Home
                            CharX = 0;
                            CharY = 0;
                            CursorX = 0;
                            CursorY = 0;
                            return;
                        }
                        else if (ThisChar == 0x1d)
                        {
                            //Crsr Right
                            CharX++;
                            if (CharX == 40)
                            {
                                if (ScrRam[(size_t)(CharY * 80) + CharX] == 0x00)
                                {
                                    CharX = 0;
                                    CharY++;
                                }
                            }
                            
                            CursorX++;
                            if (CursorX == 40)
                            {
                                CursorX = 0;
                                if (CursorY < 24)
                                {
                                    CursorY++;
                                }
                            }
                            return;
                        }
                        else if (ThisChar == 0x11)
                        {
                            //Crsr down -> check if this is a double line, if yes step to next half otherwise step to next line          
                            CharX += 40;

                            if (CharX >= 80)        //First half line
                            {
                                CharX -= 80;
                                CharY++;
                            }
                            else
                            {
                                if (ScrRam[(size_t)(CharY * 80) + 40] == 0x00)  //Second half line - check if empty and step to next first half line if it is empty
                                {
                                    CharX -= 40;
                                    CharY++;
                                }
                            }

                            if (ColRam[CharY * 80] == 0xff)
                            {
                                ScrRam[CharY * 80] = 0x20;
                                ColRam[CharY * 80] = CurrentColor;
                            }

                            if(CursorY < 24)
                            {
                                CursorY++;
                            }

                            return;
                        }
                        else if (ThisChar == 0x0e)
                        {
                            //D018 |= 0x02
                            if (CharSetSwitchEnabled)
                            {
                                CharSet = 1;
                                return;
                            }
                        }
                        else if (ThisChar == 0x08)
                        {
                            //Upper/lower case flag OFF
                            CharSetSwitchEnabled = false;
                            return;
                        }
                        else if (ThisChar == 0x09)
                        {
                            //Upper/lower case flag ON
                            CharSetSwitchEnabled = true;
                            return;
                        }
                        else
                        {
                            if (ThisChar == 0x05)
                            {
                                CurrentColor = ColorWhite;
                                return;
                            }
                            else if (ThisChar == 0x1c)
                            {
                                CurrentColor = ColorRed;
                                return;
                            }
                            else if (ThisChar == 0x1e)
                            {
                                CurrentColor = ColorGreen;
                                return;
                            }
                            else if (ThisChar == 0x1f)
                            {
                                CurrentColor = ColorBlue;
                                return;
                            }
                            //color codes
                            //0x90,0x05,0x1c,0x9f,0x9c,0x1e,0x1f,0x9e
                            //0x81,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b
                        }
                    }
                }
            }
            else
            {
                //NumInserts > 0
                ThisChar |= 0x80;
                NumInserts--;
                //OUTPUT TO SCREEN
                OutputToScreen(ThisChar);
                return;
            }
        }
        else
        {

            if (ThisChar < 0x60)
            {
                ThisChar &= 0x3f;
            }
            else
            {
                ThisChar &= 0xdf;
            }

            if (ThisChar == 0x22)
            {
                QuotedText = !QuotedText;
            }

           //OUTPUT TO SCREEN
            if (InvertedText)
            {
                ThisChar |= 0x80;
            }
            if (NumInserts > 0)
            {
                NumInserts--;
            }
            OutputToScreen(ThisChar);
            return;
        }
    }
    else
    {
        //---------------------------------------------
        //SHIFTED
        //---------------------------------------------

        ThisChar &= 0x7f;

        if (ThisChar == 0x7f)
        {
            ThisChar = 0x5e;
        }
        
        if (ThisChar < 0x20)
        {
            if (ThisChar == 0x0d)
            {
                //0x8d
                OutputCR();
                return;
            }
            else
            {
                if (!QuotedText)
                {
                    if (ThisChar == 0x14)
                    {
                        //0x94 - INSERT
                        OutputInsert();
                        return;
                    }
                    else
                    {
                        if (NumInserts > 0)
                        {
                            ThisChar |= 0xC0;

                            if (NumInserts > 0)
                            {
                                NumInserts--;
                            }

                            //OUTPUT TO SCREEN
                            OutputToScreen(ThisChar);
                            return;
                        }
                        else
                        {
                            if (ThisChar == 0x11)
                            {
                                //0x91 - cursor UP
                                if (CharX >= 40)        //Bug fix based on the dir art of Desp-AI-r.d64
                                {
                                    CharX -= 40;
                                }
                                else if (CharY > 0)
                                {
                                    CharY--;
                                    if (ScrRam[(size_t)(CharY * 80) + 40] != 00)
                                    {
                                        CharX += 40;
                                    }
                                }
                                if (CursorY > 0)
                                {
                                    CursorY--;
                                }
                                return;
                            }
                            else if (ThisChar == 0x12)
                            {
                                //0x92
                                InvertedText = false;
                                return;
                            }
                            else if (ThisChar == 0x1d)
                            {
                                //0x9d - cursor left
                                CharX--;
                                if (CharX < 0)
                                {
                                    if (CharY == 0)
                                    {
                                        CharX = 0;
                                        return;
                                    }
                                    else
                                    {
                                        CharY--;
                                        if (ScrRam[(size_t)(CharY * 80) + 40] == 0x00)
                                        {
                                            CharX = 39;
                                        }
                                        else
                                        {
                                            CharX = 79;
                                        }
                                    }
                                }
                                
                                if (CursorX > 0)
                                {
                                    CursorX--;
                                }
                                else if (CursorY > 0)
                                {
                                    CursorX = 39;
                                    CursorY--;
                                }
                                return;
                            }
                            else if (ThisChar == 0x13)
                            {
                                //0x93 - clear screen
                                CharX = 0;
                                CharY = 0;
                                CursorX = 0;
                                CursorY = 0;
                                for (size_t i = 0; i < ScrRam.size(); i++)
                                {
                                    ScrRam[i] = 0x00;
                                    ColRam[i] = 0xff;
                                }
                                return;
                            }
                            else
                            {
                                ThisChar |= 0x80;

                                if (ThisChar == 0x90)
                                {
                                    CurrentColor = ColorBlack;
                                    return;
                                }
                                else if (ThisChar == 0x9f)
                                {
                                    CurrentColor = ColorCyan;
                                    return;
                                }
                                else if (ThisChar == 0x9c)
                                {
                                    CurrentColor = ColorPurple;
                                    return;
                                }
                                else if (ThisChar == 0x9e)
                                {
                                    CurrentColor = ColorYellow;
                                    return;
                                }
                                else if (ThisChar == 0x81)
                                {
                                    CurrentColor = ColorOrange;
                                    return;
                                }
                                else if ((ThisChar >= 0x95) && (ThisChar <= 0x9b))
                                {
                                    CurrentColor = ThisChar - 0x8c;
                                    return;
                                }
                                //color codes
                                //0x90,0x05,0x1c,0x9f,0x9c,0x1e,0x1f,0x9e
                                //0x81,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b

                                if (ThisChar == 0x8e)
                                {
                                    //D018 &= 0xfd
                                    if (CharSetSwitchEnabled)
                                    {
                                        CharSet = 0;
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    //SET UP SCREEN PRINT - $E691
                    ThisChar |= 0xc0;

                    if (NumInserts > 0)
                    {
                        NumInserts--;
                    }

                    OutputToScreen(ThisChar);
                    return;

                }
            }        
        }
        else
        {
            //SET UP SCREEN PRINT - $E691
            ThisChar |= 0x40;

            if (InvertedText)
            {
                ThisChar |= 0x80;
            }

            if (NumInserts > 0)
            {
                NumInserts--;
            }

            OutputToScreen(ThisChar);
            return;
        }

    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool CreatePng()
{
    size_t LastEntry = ColRam.size();

    for (size_t i = ColRam.size(); i >= 40; i -= 40)
    {
        if (ColRam[i - 40] != 0xff)
        {
            LastEntry = i;
            break;
        }
    }

    int iNumDirEntries = 0;

    for (size_t i = 0; i < LastEntry; i += 40)
    {
        if ((ColRam[i] != 0xff) || (i % 80 == 0))
        {
            iNumDirEntries++;
        }
    }

    ImgWidth = 384;

#ifdef AddBlockCount
    ImgHeight = iNumDirEntries > 24 ? (iNumDirEntries * 8) + 72 : 272;
#elif
    ImgHeight = NumEntries > 24 ? (NumEntries * 8) + 8 + 72 : 272;
#endif

    Image.resize((size_t)ImgWidth * ImgHeight * 4);

    unsigned int BorderColor = c64palettes[(PaletteIdx * 16) + ColorLtBlue];
    BkgColor = c64palettes[(PaletteIdx * 16) + ColorBlue];

    int R0 = (BorderColor >> 16) & 0xff;
    int G0 = (BorderColor >> 8) & 0xff;
    int B0 = BorderColor & 0xff;

    int R1 = (BkgColor >> 16) & 0xff;
    int G1 = (BkgColor >> 8) & 0xff;
    int B1 = BkgColor & 0xff;

    for (size_t y = 0; y < (size_t)ImgHeight; y++)
    {
        for (size_t x = 0; x < (size_t)ImgWidth; x++)
        {
            size_t Pos = y * ((size_t)ImgWidth * 4) + (x * 4);

            if ((y < 35) || (y >= (size_t)ImgHeight - 37) || (x < 32) || (x >= (size_t)ImgWidth - 32))
            {
                Image[Pos + 0] = R0;
                Image[Pos + 1] = G0;
                Image[Pos + 2] = B0;
            }
            else
            {
                Image[Pos + 0] = R1;
                Image[Pos + 1] = G1;
                Image[Pos + 2] = B1;
            }
            Image[Pos + 3] = 255;
        }
    }

    int PngX = ScreenLeft;
    int PngY = ScreenTop;
    int i = 0;
    
    while ((size_t)(i * 80) < ScrRam.size())
    {
        for (int j = 0; j < 40; j++)
        {
            if (ScrRam[(size_t)(i * 80) + j] != 0x00)
            {
                DrawChar(ScrRam[(size_t)(i * 80) + j], ColRam[(size_t)(i * 80) + j], PngX, PngY);
            }
            PngX += 8;
        }
        
        PngX = ScreenLeft;
        int OldPngY = PngY;
        
        for (int j = 40; j < 80; j++)
        {
            if ((ScrRam[(size_t)(i * 80) + j] != 0x00) && (ScrRam[(size_t)(i * 80) + j] != 0x20))
            {
                if (OldPngY == PngY)
                {
                    PngY += 8;
                }
                DrawChar(ScrRam[(size_t)(i * 80) + j], ColRam[(size_t)(i * 80) + j], PngX, PngY);
            }
            PngX += 8;
        }

        PngX = ScreenLeft;
        PngY += 8;

        i++;
    }

    unsigned int errorpng = lodepng::encode(OutFileName, Image, ImgWidth, ImgHeight);

    if (errorpng)
    {
        cout << "Error during encoding and saving PNG.\n";
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

int CalcNumEntries()
{
    DirTrack = 18;
    DirSector = 1;
    DirPos = 2;
    int NE = 0;

    while (DirPos != 0)
    {
        if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] != 0)
        {
            NE++;
        }
        DirPos += 32;
        if (DirPos > 256)
        {
            DirPos -= 256;
            int NT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
            int NS = Disk[Track[DirTrack] + (DirSector * 256) + 1];
            DirTrack = NT;
            DirSector = NS;

            if (DirTrack == 0)
            {
                DirPos = 0;
            }
        }
    }

    return NE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void CreateGifFrame()
{
/*
    string BinName = "C:/Dart/AnimFrames/GifScr_";
    unsigned char B;
    int GFC = GifFrameCount++;
    
    B = 0x30+(GFC / 1000);
    BinName += B;
    GFC %= 1000;
    B = 0x30 + (GFC / 100);
    BinName += B;
    GFC %= 100;
    B = 0x30 + (GFC / 10);
    BinName += B;
    GFC %= 10;
    B = 0x30 + GFC;
    BinName += B;
    BinName += ".bin";

    WriteBinaryFile(BinName,ScrRam);
*/
    size_t FirstEntryPos = 0;
    size_t LastEntryPos = ScrRam.size();

    for (size_t i = ScrRam.size(); i >= 40; i -= 40)
    {
        if (ScrRam[i - 40] != 0x00)
        {
            LastEntryPos = i;
            break;
        }
    }

    int iNumDirEntries = 0;

    for (size_t i = 0; i < LastEntryPos; i += 40)
    {
        if ((ScrRam[i] != 0x00) || (i % 80 == 0))
        {
            iNumDirEntries++;
        }
    }
    int NumEntries = 0;

    for (size_t i = LastEntryPos; i >= 40; i -= 40)
    {
        if ((ScrRam[i - 40] != 0x00) || ((i - 40) % 80 == 0))
        {
            NumEntries++;
            if (NumEntries == 25)
            {
                FirstEntryPos = i;
                break;
            }
        }
    }

    unsigned int BorderColor = c64palettes[(PaletteIdx * 16) + ColorLtBlue];
    BkgColor = c64palettes[(PaletteIdx * 16) + ColorBlue];

    int R0 = (BorderColor >> 16) & 0xff;
    int G0 = (BorderColor >> 8) & 0xff;
    int B0 = BorderColor & 0xff;

    int R1 = (BkgColor >> 16) & 0xff;
    int G1 = (BkgColor >> 8) & 0xff;
    int B1 = BkgColor & 0xff;

    for (size_t y = 0; y < (size_t)GifHeight; y++)
    {
        for (size_t x = 0; x < (size_t)GifWidth; x++)
        {
            size_t Pos = y * ((size_t)GifWidth * 4) + (x * 4);

            if ((y < 35) || (y >= (size_t)GifHeight - 37) || (x < 32) || (x >= (size_t)GifWidth - 32))
            {
                GifImage[Pos + 0] = R0;
                GifImage[Pos + 1] = G0;
                GifImage[Pos + 2] = B0;
            }
            else
            {
                GifImage[Pos + 0] = R1;
                GifImage[Pos + 1] = G1;
                GifImage[Pos + 2] = B1;
            }
            GifImage[Pos + 3] = 255;
        }
    }

    int GifX = ScreenLeft;
    int GifY = ScreenTop;
    int i = FirstEntryPos / 80;

    while ((size_t)(i * 80) < ScrRam.size())
    {
        for (int j = 0; j < 40; j++)
        {
            if (ScrRam[(size_t)(i * 80) + j] != 0x00)
            {
                DrawGifChar(ScrRam[(size_t)(i * 80) + j], ColRam[(size_t)(i * 80) + j], GifX, GifY);
            }
            GifX += 8;
        }

        GifX = ScreenLeft;
        int OldPngY = GifY;

        for (int j = 40; j < 80; j++)
        {
            if ((ScrRam[(size_t)(i * 80) + j] != 0x00) && (ScrRam[(size_t)(i * 80) + j] != 0x20))
            {
                if (OldPngY == GifY)
                {
                    GifY += 8;
                }
                DrawGifChar(ScrRam[(size_t)(i * 80) + j], ColRam[(size_t)(i * 80) + j], GifX, GifY);
            }
            GifX += 8;
        }

        GifX = ScreenLeft;
        GifY += 8;

        i++;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ConvertD64ToGif()
{
    int FrameCount = 0;

    const char* GifFileName = OutFileName.c_str();

    GifWriter Gif = {};
    GifBegin(&Gif, GifFileName, GifWidth, GifHeight, 2);

    int iNumDirEntries = CalcNumEntries();

    ScrRam.resize((size_t)(iNumDirEntries + 4 + 8) * 80);    //80-char long virtual lines
    ColRam.resize((size_t)(iNumDirEntries + 4 + 8) * 80);

    for (int i = 0; i < (iNumDirEntries + 4 + 8) * 80; i++)
    {
        ScrRam[i] = 0x00;   //Unused Petscii code - indicates that the line is unused
        ColRam[i] = 0xff;   //Unused color code - no color change needed
    }
    
    unsigned int B = 0;
    CharX = 0;
    CharY = 0;

    chrout(0x0d);

    //          0123456789012345678901234567890123456789
    string S = "    **** COMMODORE 64 BASIC V2 ****";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    chrout(0x0d);
    chrout(0x0d);

    //   0123456789012345678901234567890123456789
    S = " 64K RAM SYSTEM  38911 BASIC BYTES FREE";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    chrout(0x0d);
    chrout(0x0d);

    //   0123456789012345678901234567890123456789
    S = "READY";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    chrout(0x0d);

    //   0123456789012345678901234567890123456789
    S = "LOAD\"$\",8";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    chrout(0x0d);
    chrout(0x0d);

    //   0123456789012345678901234567890123456789
    S = "SEARCHING FOR $";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    chrout(0x0d);

    //   0123456789012345678901234567890123456789
    S = "LOADING";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    chrout(0x0d);

    //   0123456789012345678901234567890123456789
    S = "READY";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    chrout(0x0d);

    //   0123456789012345678901234567890123456789
    S = "LIST";

    for (size_t i = 0; i < S.length(); i++)
    {
        B = S[i];
        chrout(B);
    }

    for (int i = 0; i < 2; i++)
    {

        InvertedText = true;
        chrout(0x20);

        CreateGifFrame();
        GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 50);

        CharX--;
        InvertedText = false;
        chrout(0x20);

        CreateGifFrame();
        GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 50);

        CharX--;
    }

    chrout(0x0d);
    chrout(0x0d);

    CreateGifFrame();
    GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 2);

    DirTrack = 18;
    DirSector = 1;
    DirPos = 2;

    chrout(0x30);
    chrout(0x20);

    InvertedText = true;
    chrout(0x22);

    for (int i = 0; i < 16; i++)
    {
        B = Disk[Track[DirTrack] + 0x90 + i];
        chrout(B);
    }

    chrout(0x22);
    chrout(0x20);

    CreateGifFrame();
    GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 2);

    for (int i = 0; i < 5; i++)
    {
        B = Disk[Track[DirTrack] + 0xa2 + i];
        if ((i == 4) && (B == 0xa0))
        {
            B = 0x31;   //if the 5th character is 0xa0 then the C64 displays a "1" instead
        }
        chrout(B);
    }

    chrout(0x0d);

    //DirTrack = 18;
    //DirSector = 1;
    //DirPos = 2;

    bool Finished = false;

    while (!Finished)
    {
        if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] != 0)
        {
            NumInserts = 0;

            unsigned char ET = Disk[Track[DirTrack] + (DirSector * 256) + DirPos];

            string EntryType = "";
            if (ET == 0x01)
            {
                EntryType = "*SEQ  ";
            }
            else if (ET == 0x02)
            {
                EntryType = "*PRG  ";
            }
            else if (ET == 0x03)
            {
                EntryType = "*USR  ";
            }
            else if (ET == 0x80)
            {
                EntryType = " DEL  ";
            }
            else if (ET == 0x81)
            {
                EntryType = " SEQ  ";
            }
            else if (ET == 0x82)
            {
                EntryType = " PRG  ";
            }
            else if (ET == 0x83)
            {
                EntryType = " USR  ";
            }
            else if (ET == 0x84)
            {
                EntryType = " REL  ";
            }
            else if (ET == 0xc0)
            {
                EntryType = " DEL< ";
            }
            else if (ET == 0xc1)
            {
                EntryType = " SEQ< ";
            }
            else if (ET == 0xc2)
            {
                EntryType = " PRG< ";
            }
            else if (ET == 0xc3)
            {
                EntryType = " USR< ";
            }
            else if (ET == 0xc4)
            {
                EntryType = " REL< ";
            }

            int NumBlocks = Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 28] + (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 29] * 256);

            //CharX = 0;

            string BlockCnt = to_string(NumBlocks);

            for (size_t i = 0; i < BlockCnt.size(); i++)
            {
                chrout(BlockCnt[i]);
            }

            for (size_t i = 5; i > BlockCnt.size(); i--)
            {
                chrout(0x20);
            }

            chrout(0x22);

            NumExtraSpaces = 0;

            for (int i = 0; i < 16; i++)
            {
                unsigned char NextChar = Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i];

                if (NextChar != 0xa0)
                {
                    chrout(NextChar);
                }
                else
                {
                    NumExtraSpaces++;
                }
            }

            chrout(0x22);

            for (int i = 0; i < NumExtraSpaces; i++)
            {
                chrout(0x20);
            }

            for (size_t i = 0; i < EntryType.length(); i++)
            {
                B = EntryType[i];
                chrout(B);
            }

            CreateGifFrame();
            GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 2);

            chrout(0x0d);
        }
        DirPos += 32;
        if (DirPos > 256)
        {
            DirPos -= 256;
            int NT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
            int NS = Disk[Track[DirTrack] + (DirSector * 256) + 1];

            if (NT != 0)
            {
                DirTrack = NT;
                DirSector = NS;
            }
            else
            {
                Finished = true;
            }
        }
        else
        {
            if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] == 0)
            {
                Finished = true;
            }
        }

        if (CursorY == 24)
        {
            CreateGifFrame();
            GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 2);
        }

        if (FrameCount % 10 == 0)
        {
            cout << ".";
        }
        FrameCount++;

        if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] == 0)
        {
            DirPos = 0;
        }
    }

#ifdef AddBlockCount
    int NumBlocksFree = 0;
    //DirTrack = 18;
    for (size_t i = 1; i < 36; i++)
    {
        if (i != 18)        //Skip track 18
        {
            NumBlocksFree += Disk[Track[18] + (i * 4)];
        }
    }

    CharX = 0;

    string BlocksFree = to_string(NumBlocksFree);

    for (size_t i = 0; i < BlocksFree.size(); i++)
    {
        chrout(BlocksFree[i]);
    }

    string BlocksFreeMsg = " BLOCKS FREE.";

    for (size_t i = 0; i < BlocksFreeMsg.length(); i++)
    {
        B = BlocksFreeMsg[i];
        chrout(B);
    }

    CreateGifFrame();
    GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 2);

    chrout(0x0d);

    if (CursorY == 24)
    {
        CreateGifFrame();
        GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 2);
    }

    string ReadyMsg = "READY.";

    for (size_t i = 0; i < ReadyMsg.length(); i++)
    {
        B = ReadyMsg[i];
        chrout(B);
    }
#endif

    chrout(0x0d);

    size_t Pos = (size_t)(CharY * 80) + CharX;
    B = ScrRam[Pos];

    if (B == 0x00)
    {
        B = 0x20;
        ColRam[Pos] = CurrentColor;
    }

    for (int i = 0; i < 6; i++)
    {
        B = B ^ 0x80;
        ScrRam[Pos] = B;

        CreateGifFrame();
        GifWriteFrame(&Gif, GifImage, GifWidth, GifHeight, 50);
    }

    GifEnd(&Gif);
    
    cout << "\n";
    
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ConvertD64ToPng()
{   

    int iNumDirEntries = CalcNumEntries();

    ScrRam.resize((size_t)(iNumDirEntries + 4) * 80);    //80-char long virtual lines
    ColRam.resize((size_t)(iNumDirEntries + 4) * 80);

    for (int i = 0; i < (iNumDirEntries + 4) * 80; i++)
    {
        ScrRam[i] = 0x00;   //Unused Petscii code - indicates that the line is unused
        ColRam[i] = 0xff;   //Unused color code - no color change needed
    }

    DirTrack = 18;
    DirSector = 1;
    DirPos = 2;

    chrout(0x30);
    chrout(0x20);

    InvertedText = true;
    chrout(0x22);

    unsigned int B = 0;
    for (int i = 0; i < 16; i++)
    {
        B = Disk[Track[DirTrack] + 0x90 + i];
        chrout(B);
    }

    chrout(0x22);
    chrout(0x20);

    for (int i = 0; i < 5; i++)
    {
        B = Disk[Track[DirTrack] + 0xa2 + i];
        if ((i==4) && (B == 0xa0))
        {
            B = 0x31;   //if the 5th character is 0xa0 then the C64 displays a "1" instead
        }
        chrout(B);
    }
    chrout(0x0d);

    DirTrack = 18;
    DirSector = 1;
    DirPos = 2;

    bool Finished = false;

    while (!Finished)
    {
        if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] != 0)
        {
            NumInserts = 0;

            unsigned char ET = Disk[Track[DirTrack] + (DirSector * 256) + DirPos];

            string EntryType = "";
            if (ET == 0x01)
            {
                EntryType = "*SEQ  ";
            }
            else if (ET == 0x02)
            {
                EntryType = "*PRG  ";
            }
            else if (ET == 0x03)
            {
                EntryType = "*USR  ";
            }
            else if (ET == 0x80)
            {
                EntryType = " DEL  ";
            }
            else if (ET == 0x81)
            {
                EntryType = " SEQ  ";
            }
            else if (ET == 0x82)
            {
                EntryType = " PRG  ";
            }
            else if (ET == 0x83)
            {
                EntryType = " USR  ";
            }
            else if (ET == 0x84)
            {
                EntryType = " REL  ";
            }
            else if (ET == 0xc0)
            {
                EntryType = " DEL< ";
            }
            else if (ET == 0xc1)
            {
                EntryType = " SEQ< ";
            }
            else if (ET == 0xc2)
            {
                EntryType = " PRG< ";
            }
            else if (ET == 0xc3)
            {
                EntryType = " USR< ";
            }
            else if (ET == 0xc4)
            {
                EntryType = " REL< ";
            }

            int NumBlocks = Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 28] + (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 29] * 256);
           
            string BlockCnt = to_string(NumBlocks);
            
            for (size_t i = 0; i < BlockCnt.size(); i++)
            {
                chrout(BlockCnt[i]);
            }

            for (size_t i = 5; i > BlockCnt.size(); i--)
            {
                chrout(0x20);
            }

            chrout(0x22);
            
            NumExtraSpaces = 0;

            for (int i = 0; i < 16; i++)
            {
                unsigned char NextChar = Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i];

                if (NextChar != 0xa0)
                {
                    chrout(NextChar);
                }
                else
                {
                    NumExtraSpaces++;       //We will need to correct the entry length after the end-qoute
                }
            }

            chrout(0x22);

            for (int i = 0; i < NumExtraSpaces; i++)
            {
                chrout(0x20);
            }

            for (size_t i = 0; i < EntryType.length(); i++)
            {
                B = EntryType[i];
                chrout(B);
            }

            chrout(0x0d);            
        }
        DirPos += 32;
        if (DirPos > 256)
        {
            DirPos -= 256;
            int NT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
            int NS = Disk[Track[DirTrack] + (DirSector * 256) + 1];
            
            if (NT != 0)
            {
                DirTrack = NT;
                DirSector = NS;
            }
            else
            {
                Finished = true;
            }
        }
        else
        {
            if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] == 0)
            {
                Finished = true;
            }
        }
#ifdef DEBUG
        //CreatePng();
        //ThisDirEntry++;
#endif
    }

#ifdef AddBlockCount
    int NumBlocksFree = 0;
    //DirTrack = 18;
    for (size_t i = 1; i < 36; i++)
    {
        if (i != 18)        //Skip track 18
        {
            NumBlocksFree += Disk[Track[18] + (i * 4)];
        }
    }

    CharX = 0;
    
    string BlocksFree = to_string(NumBlocksFree);
    
    for (size_t i = 0; i < BlocksFree.size(); i++)
    {
        chrout(BlocksFree[i]);
    }
    
    string BlocksFreeMsg = " BLOCKS FREE.";

    for (size_t i = 0; i < BlocksFreeMsg.length(); i++)
    {
        B = BlocksFreeMsg[i];
        chrout(B);
    }
    
    chrout(0x0d);

    string ReadyMsg = "READY.";

    for (size_t i = 0; i < ReadyMsg.length(); i++)
    {
        B = ReadyMsg[i];
        chrout(B);
    }

    chrout(0x0d);

#endif

    int RamSize = ScrRam.size();

    while ((ScrRam[(size_t)RamSize - 80] == 0x00) && (ScrRam[(size_t)RamSize - 40] == 0x00))
    {
        RamSize -= 80;
    }
    if ((size_t)RamSize < ScrRam.size())
    {
        ScrRam.resize(RamSize);
        ScrRam.resize(RamSize);
    }

    return CreatePng();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void MarkSectorAsUsed(vector<unsigned char>& DiskImage,size_t T, size_t S)
{
    size_t NumSectorPtr = Track[18] + (T * 4) + ((T > 35) ? 7 * 4 : 0);
    
    unsigned char NumUnusedSectors = 0;

    //Calculate number of used sectors -before- update
    for (size_t I = NumSectorPtr + 1; I <= NumSectorPtr + 3; I++)
    {
        unsigned char B = DiskImage[I];
        for (int J = 0; J < 8; J++)
        {
            if (B % 2 == 1)
            {
                NumUnusedSectors++;
            }
            B >>= 1;
        }
    }

    size_t BitPtr = NumSectorPtr + 1 + (S / 8);     //BAM Position for Bit Change
    unsigned char BitToDelete = 1 << (S % 8);       //BAM Bit to be deleted
    
    if ((DiskImage[BitPtr] & BitToDelete) != 0)
    {
        DiskImage[BitPtr] &= (BitToDelete ^ 0xff);
        NumUnusedSectors--;
    }

    DiskImage[NumSectorPtr] = NumUnusedSectors;
    
    //cout << hex << T << "\t" << S << "\t" << (int)NumUnusedSectors << "\n";
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void MarkSectorAsFree(size_t T, size_t S)
{
    size_t NumSectorPtr = Track[18] + (T * 4) + ((T > 35) ? 7 * 4 : 0);

    unsigned char NumUnusedSectors = 0;

    //Calculate number of used sectors -before- update
    for (size_t I = NumSectorPtr + 1; I <= NumSectorPtr + 3; I++)
    {
        unsigned char B = Disk[I];
        for (int J = 0; J < 8; J++)
        {
            if (B % 2 == 1)
            {
                NumUnusedSectors++;
            }
            B >>= 1;
        }
    }

    size_t BitPtr = NumSectorPtr + 1 + (S / 8);     //BAM Position for Bit Change
    unsigned char BitToSet = 1 << (S % 8);          //BAM Bit to be deleted

    if ((Disk[BitPtr] & BitToSet) != 1)
    {
        Disk[BitPtr] |= BitToSet;
        NumUnusedSectors++;
    }

    Disk[NumSectorPtr] = NumUnusedSectors;

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void FindNextEmptyDirSector()
{
    //Check if the BAM shows any free sectors on the current track

    LastDirTrack = DirTrack;
    LastDirSector = DirSector;

    size_t BAMPos = Track[18] + (size_t)(DirTrack * 4);

    while(DirTrack > 0)
    {
        if ((Disk[BAMPos] == 0) || ((Disk[BAMPos] == 1) && (Disk[BAMPos + 1] == 0x01)))
        {
            //if (OutputType == "d64")
            //{
                //DirSector = 0;
                //DirPos = 0;     //This will indicate that the directory is full, no more entries are possible
                //cout << "***INFO***\tDirectory is full.\n";
                //return;
            //}
            //else if ((OutputType == "png") ||(OutputType == "gif"))
            //{
                if (DirTrack == 35)
                {
                    DirTrack = 17;
                    NumSectorsOnTrack = 21;
                }
                else if (DirTrack >= 18)
                {
                    DirTrack++;
                    if (DirTrack < 25)
                    {
                        NumSectorsOnTrack = 19;
                    }
                    else if (DirTrack < 31)
                    {
                        NumSectorsOnTrack = 18;
                    }
                    else
                    {
                        NumSectorsOnTrack = 17;
                    }
                }
                else if (DirTrack > 1)
                {
                    DirTrack--;
                }
                else
                {
                    DirSector = 0;
                    DirPos = 0;
                    cout << "***INFO***\tDirectory is full.\n";
                    return;
                }
            //}
        }

        DirSector = 1; //Skip the first sector on each track (weird, but sector 0 as next sector in chain on -any- track means end of directory)

        while (DirSector < (size_t)NumSectorsOnTrack)   //last sector on track 18 is sector 18 (out of 0-18)
        {
            //Check in BAM if this is really a free sector
            size_t NumSectorPtr = Track[18] + (DirTrack * 4);
            size_t BitPtr = NumSectorPtr + 1 + (DirSector / 8);     //BAM Position for Bit Change
            unsigned char BitToCheck = 1 << (DirSector % 8);        //BAM Bit to be deleted

            if ((Disk[BitPtr] & BitToCheck) != 0)
            {
                //cout << hex << DirTrack << "\t" << DirSector << "\t" << (int)Disk[NumSectorPtr] << "\n";

                Disk[Track[LastDirTrack] + (LastDirSector * 256) + 0] = DirTrack;
                Disk[Track[LastDirTrack] + (LastDirSector * 256) + 1] = DirSector;

                //Make sure the empty sector is actually empty (to avoid unwanted file type attributes)
                for (int i = 0; i < 256; i++)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + i] = 0;
                }
                Disk[Track[DirTrack] + (DirSector * 256) + 1] = 255;
                DirPos = 2;
                return;
            }
            DirSector++;
        }
    }
    
    //Track 18 is full, no more empty sectors
    DirSector = 0;
    DirPos = 0;
    cout << "***INFO***\tDirectory is full.\n";
    return;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------

void FindNextDirPos() {

    if (DirPos == 0)
    {
        DirPos = 2;
    }
    else
    {
        DirPos += 32;
        if (DirPos > 256)   //This sector is full, let's find the next dir sector
        {
            if ((Disk[Track[DirTrack] + (DirSector * 256) + 0] > 0) && (Disk[Track[DirTrack] + (DirSector * 256) + 0] < 36))
            {
                //This is NOT the last sector in the T:S chain, we are overwriting existing dir entries
                
                int NextT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
                int NextS = Disk[Track[DirTrack] + (DirSector * 256) + 1];
                
                //if (NextT != 18)
                //{
                //    //DART doesn't support directories outside track 18
                //    DirSector = 0;
                //    DirPos = 0;
                //    cout << "***INFO***\tThis directory sector chain leaves track 18. DART doesn't support directories outside track 18!\n";
                //    return;
                //}
                //else
                //{
                    //Otherwise, go to next dir sector in T:S chain and overwrite first directory entry
                    DirTrack = NextT;
                    DirSector = NextS;
                    DirPos = 2;
                //}
            }
            else
            {
                //We are adding new sectors to the chain
                FindNextEmptyDirSector();
            }
        }
    }
    
    if (DirPos != 0)
    {
        NumDirEntries++;
        if (NumDirEntries == MaxNumDirEntries)
        {
            DirPos = 0;
        }
        else
        {
            MarkSectorAsUsed(Disk, DirTrack, DirSector);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool FindLastUsedDirPos()
{
    //Check if the BAM shows free sectors on track 18
    //if (Disk[Track[DirTrack] + (size_t)(18 * 4)] == 0)
    //{
        //cerr << "***ABORT***Track 18 is full. DART is unable to append to this directory.\n";
        //return false;
    //}

    NumDirEntries = 0;

    unsigned char SectorChain[664]{};
    unsigned char TrackChain[664]{};
    
    int ChainIndex = 0;

    TrackChain[ChainIndex] = DirTrack;
    SectorChain[ChainIndex] = DirSector;

    size_t DiskPos = Track[DirTrack] + (DirSector * 256) + 0;

    //First find the last used directory sector
    while ((Disk[DiskPos] > 0) && (Disk[DiskPos] < 36))
    {
        NumDirEntries += 8;

        DirTrack = Disk[DiskPos + 0];
        DirSector = Disk[DiskPos + 1];

        //if (DirTrack != 18)
        //{
            //cerr << "***ABORT***The directory on this disk extends beyond track 18. DART only supports directories on track 18.\n";
            //return false;
        //}

        TrackChain[++ChainIndex] = DirTrack;
        SectorChain[ChainIndex] = DirSector;

        DiskPos = Track[DirTrack] + (DirSector * 256) + 0;
    }

    Disk[Track[DirTrack] + (DirSector * 256) + 0] = 0;
    Disk[Track[DirTrack] + (DirSector * 256) + 1] = 255;

    //Then find last used dir entry slot
    DirPos = (7 * 32) + 2;

    NumDirEntries += 8;

    while (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] == 0)
    {
        DirPos -= 32;
        NumDirEntries--;
        if (DirPos < 0)
        {
            if (ChainIndex > 0)
            {
                DirPos += 256;
                DirTrack = TrackChain[--ChainIndex];
                DirSector = SectorChain[ChainIndex];
            }
            else
            {
                DirPos = 0;               
                break;
            }
        }
    }

    //if (NumDirEntries == MaxNumDirEntries - 1)
    //{
        //cerr << "***ABORT***\tThe target directory already contains the maximum number of " << (dec) << MaxNumDirEntries << " entries. No more entries can be appended!\n";
        //return false;
    //}
    //else
    //{
        return true;
    //}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
/*
void DeleteOldDir()
{
    //Free up unused dir sectors from old T:S chain in Overwrite Mode
    
    int NextT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
    int NextS = Disk[Track[DirTrack] + (DirSector * 256) + 1];

    Disk[Track[DirTrack] + (DirSector * 256) + 0] = 0;
    Disk[Track[DirTrack] + (DirSector * 256) + 1] = 0xff;

    while (NextT != 0)
    {
        DirTrack = NextT;
        DirSector = NextS;
        
        NextT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
        NextS = Disk[Track[DirTrack] + (DirSector * 256) + 1];

        for (int i = 0; i < 256; i++)
        {
            Disk[Track[DirTrack] + (DirSector * 256) + i] = 0;
        }
        MarkSectorAsFree(DirTrack, DirSector);
    }
}
*/
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool SkipDirEntries()
{
    DirTrack = 18;
    DirSector = 1;
    DirPos = 0;

    size_t DT = DirTrack;
    size_t DS = DirSector;

    if (NumSkippedEntries > 0)
    {       
        //We are looking for the last sector and DirPos to be skipped, FindNextDirPos will find the first unused one

        int NumSkippedSectors = (NumSkippedEntries - 1) / 8;       
        
        DirPos = 2 + ((NumSkippedEntries - 1) % 8) * 32;      //Last used entry to be skipped

        //Skip max. NumSkippedEntries number of entries

        if (NumSkippedSectors > 0)
        {
            while (NumSkippedSectors > 0)
            {
                DT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
                DS = Disk[Track[DirTrack] + (DirSector * 256) + 1];

                if ((DT > 0) && (DT < 41))   //Valid track number
                {
                    DirTrack = DT;
                    DirSector = DS;
                    NumSkippedSectors--;
                    NumDirEntries += 8;
                }
                else
                {
                    //Less sectors are used than what we want to skip
                    //We are on the last used dir track, find last used DirPos

                    DirPos = 7 * 32 + 2;
                    while ((Disk[Track[DirTrack] + (DirSector * 256) + DirPos] == 0) && (DirPos > 0))
                    {
                        DirPos -= 32;
                    }

                    if (DirPos < 0)
                        DirPos = 0;

                    AppendMode = true;
                    NumDirEntries += 1 + (DirPos == 0 ? 0 : (DirPos - 2) / 32);
                    return true;
                }
            }

            NumDirEntries += 1 + (DirPos == 0 ? 0 : (DirPos - 2) / 32);
        }
        else
        {
            while ((Disk[Track[DirTrack] + (DirSector * 256) + DirPos] == 0) && (DirPos > 0))
            {
                AppendMode = true;
                DirPos -= 32;
            }

            if (DirPos < 0)
                DirPos = 0;

        }

        NumDirEntries = (DirPos == 0 ? 0 : (DirPos - 2) / 32) + 1;

        if (AppendMode)
            return true;

    }

    //Last used entry to be skipped found - now clear the rest of the existing directory

    bool Finished = false;

    DT = DirTrack;
    DS = DirSector;

    unsigned char NT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
    unsigned char NS = Disk[Track[DirTrack] + (DirSector * 256) + 1];

    size_t i = (DirPos != 0) ? DirPos + 32  : 2;

    unsigned char SectorValue = 0xff;

    while (!Finished)
    {
        while (i < 256)
        {
            Disk[Track[DT] + (DS * 256) + i++] = 0;
        }

        i = 0;

        Disk[Track[DT] + (DS * 256) + 0] = 0;
        Disk[Track[DT] + (DS * 256) + 1] = SectorValue;
        
        if (SectorValue == 0)
        {
            MarkSectorAsFree(DT, DS);
        }
        
        SectorValue = 0;       

        if ((NT != 0) && (NT < 41))
        {
            DT = NT;
            DS = NS;
            NT = Disk[Track[DT] + (DS * 256) + 0];
            NS = Disk[Track[DT] + (DS * 256) + 1];
        }
        else
        {
            Finished = true;
        }
    }

    return true;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------

void CreateDisk()
{
    size_t CP = Track[18];

    DiskSize = StdDiskSize;
    Disk.clear();
    Disk.resize(StdDiskSize, 0);

    Disk[CP + 0] = 0x12;        //Track 18
    Disk[CP + 1] = 0x01;        //Sector 1
    Disk[CP + 2] = 0x41;        //'A"

    for (size_t i = 0x90; i < 0xab; i++)        //Name, ID, DOS type
    {
        Disk[CP + i] = 0xa0;
    }

    for (size_t i = 4; i < ((size_t)36 * 4); i++)
    {
        Disk[CP + i] = 0xff;
    }

    for (size_t i = 1; i < 18; i++)
    {
        Disk[CP + (i * 4) + 0] = 21;
        Disk[CP + (i * 4) + 3] = 31;
    }

    for (size_t i = 18; i < 25; i++)
    {
        Disk[CP + (i * 4) + 0] = 19;
        Disk[CP + (i * 4) + 3] = 7;
    }

    for (size_t i = 25; i < 31; i++)
    {
        Disk[CP + (i * 4) + 0] = 18;
        Disk[CP + (i * 4) + 3] = 3;
    }

    for (size_t i = 31; i < 36; i++)
    {
        Disk[CP + (i * 4) + 0] = 17;
        Disk[CP + (i * 4) + 3] = 1;
    }

    Disk[CP + ((size_t)18 * 4) + 0] = 18;
    Disk[CP + ((size_t)18 * 4) + 1] = 252;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void FixBAM(vector<unsigned char> &DiskImage)
{
    DirTrack = 18;
    DirSector = 0;

    //Earlier versions of Sparkle disks don't mark DirArt sectors as "used" in the BAM. So let's follow the directory block chain and mark them off
    while ((DiskImage[Track[DirTrack] + (DirSector * 256) + 0] > 0) && (DiskImage[Track[DirTrack] + (DirSector * 256) + 0] < 41))
    {
        int NextT = DiskImage[Track[DirTrack] + (DirSector * 256) + 0];
        int NextS = DiskImage[Track[DirTrack] + (DirSector * 256) + 1];

        if ((NextT == 18) && (NextS == 0))  //Sparkle 1.x marked last secter in chain with 12:00 instead of 00:FF
        {
            break;
        }
        else
        {
            if (DirTrack == 18)
            {
                MarkSectorAsUsed(DiskImage, DirTrack, DirSector);   //Only mark sectors off on track 18, to keep the original number of blocks free
            }
            DirTrack = NextT;
            DirSector = NextS;
        }
    }

    //Mark last sector as used if there is at least one dir entry (this will avoid marking the first sector off if the dir is completely empty)
    if (DiskImage[Track[DirTrack] + (DirSector * 256) + 2] != 0)
    {
        if (DirTrack == 18)
        {
            MarkSectorAsUsed(DiskImage, DirTrack, DirSector);   //but only if we are still on track 18
        }
    }

    //Correct first two bytes of last sector
    DiskImage[Track[DirTrack] + (DirSector * 256) + 0] = 0;
    DiskImage[Track[DirTrack] + (DirSector * 256) + 1] = 0xff;

    //Sparkle 2 and 2.1 disks don't mark the internal directory sectors (18:17 and 18:18) off, let's fix it.
    //Sparkle 2.0 dir(0) = $f7 $ff $73 $00
    //Sparkle 2.1 dir(0) = $77 $7f $63 $00 (same in 3.0+)

    DirTrack = 18;
    DirSector = 17;
    if (((DiskImage[Track[DirTrack] + (DirSector * 256) + 0] == 0xf7) &&
        (DiskImage[Track[DirTrack] + (DirSector * 256) + 255] == 0xff) &&
        (DiskImage[Track[DirTrack] + (DirSector * 256) + 254] == 0x73) &&
        (DiskImage[Track[DirTrack] + (DirSector * 256) + 253] == 0x00)) ||
        ((DiskImage[Track[DirTrack] + (DirSector * 256) + 0] == 0x77) &&
            (DiskImage[Track[DirTrack] + (DirSector * 256) + 255] == 0x7f) &&
            (DiskImage[Track[DirTrack] + (DirSector * 256) + 254] == 0x63) &&
            (DiskImage[Track[DirTrack] + (DirSector * 256) + 253] == 0x00)))
    {
        //Sparkle 2.0 or Sparkle 2.1+ disk -> mark 18:17 and 18:18 off (internal directory sectors)
        MarkSectorAsUsed(DiskImage, DirTrack, DirSector);
        MarkSectorAsUsed(DiskImage, DirTrack, DirSector + 1);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool OpenOutFile()
{
    if ((OutputType == "png") || (OutputType == "gif"))
    {
        CreateDisk();           //PNG output - creating a virtual D64 first from the input file which will then be converted to PNG
    }
    else if (OutputType == "d64")
    {
        DiskSize = ReadBinaryFile(OutFileName, Disk);   //Load the output file if it exists

        if (DiskSize < 0)
        {
            //Output file doesn't exits, create an empty D64
            CreateDisk();
        }
        else if ((DiskSize != StdDiskSize) && (DiskSize != ExtDiskSize))    //Otherwise make sure the output disk is the correct size
        {
            cerr << "***ABORT***\t Invalid output disk file size!\n";
            return false;
        }

        FixBAM(Disk);        //Earlier Sparkle versions didn't mark DirArt and internal directory sectors off in the BAM
    }

    DirTrack = 18;
    DirSector = 1;
    DirPos = 0;             //This will allow to start with the very first dir slot in overwrite mode

    return true;

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void CorrectFilePathSeparators()
{
    for (size_t i = 0; i < InFileName.size(); i++)
    {
        if (InFileName[i] == '\\')
        {
            InFileName.replace(i, 1, "/");     //Replace '\' with '/' in file paths, Windows can also handle this
        }
    }

    for (size_t i = 0; i < OutFileName.size(); i++)
    {
        if (OutFileName[i] == '\\')
        {
            OutFileName.replace(i, 1, "/");     //Replace '\' with '/' in file paths, Windows can also handle this
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM D64
//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromD64()
{
    vector <unsigned char> DA;

    if (ReadBinaryFile(InFileName, DA) == -1)
    {
        cerr << "***ABORT***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }
    else if ((DA.size() != StdDiskSize) && (DA.size() != ExtDiskSize))
    {
        cerr << "***ABORT***\tInvalid D64 DirArt file size: " << dec << DA.size() << " byte(s)\n";
        return false;
    }

    if (DA[Track[18] + 0x90] != 0xa0)       //Import directory header only if one exists in input disk image
    {
        if (!argDiskName.empty())
        {
            cout << "***INFO***\tDisk name is imported from D64 source file. Ignoring option -n\n";
        }

        for (int i = 0; i < 16; i++)
        {
            Disk[Track[18] + 0x90 + i] = DA[Track[18] + 0x90 + i];
        }
    }
    else if (!argDiskName.empty())          //Otherwise, check if disk name is specified in command line
    {
        for (size_t i = 0; i < 16; i++)
        {
            if (i < argDiskName.size())
            {
                Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(argDiskName[i])];
            }
            else
            {
                Disk[Track[18] + 0x90 + i] = 0xa0;
            }
        }
    }

    if (DA[Track[18] + 0xa2] != 0xa0)       //Import directory ID only if one exists in input disk image
    {
        if (!argDiskID.empty())
        {
            cout << "***INFO***\tDisk ID is imported from D64 source file. Ignoring option -n\n";
        }

        for (int i = 0; i < 5; i++)
        {
            Disk[Track[18] + 0xa2 + i] = DA[Track[18] + 0xa2 + i];
        }
    }
    else if (!argDiskID.empty())            //Otherwise, check if disk ID is specified in command line
    {
        for (size_t i = 0; i < 5; i++)
        {
            if (i < argDiskID.size())
            {
                Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(argDiskID[i])];
            }
            else
            {
                Disk[Track[18] + 0xa2 + i] = 0xa0;
            }
        }
    }
    
    //DirTrack = 18;
    //DirSector = 1;

    size_t T = 18;
    size_t S = 1;

    //bool DirFull = false;
    size_t DAPtr{};

    //Copy Dir Entries until
    //(1) Finished (T==0) OR
    //(2) Max number of Dir Entries reached OR
    //(3) Disk is full (DirPos == 0)

    while ((NumDirEntries < MaxNumDirEntries) && (T != 0))
    {
        DAPtr = Track[T] + (S * 256);
        int b = 2;
        while ((b < 256) && (NumDirEntries < MaxNumDirEntries))
        {
            if (DA[DAPtr + b] != 0)
            {
                EntryIndex++;

                if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
                {
                    FindNextDirPos();

                    if (DirPos != 0)
                    {
                        //Update file type indicator
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos] = DA[DAPtr + b];
                        
                        //Update dir entry's name and block size, skipping T:S pointer
                        for (int i = 3; i < 30; i++)
                        {
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + i] = DA[DAPtr + b + i];
                        }
                    }
                    else
                    {
                        //return true;
                        break;
                    }
                }
            }
            b += 32;
        }

        if (DirPos == 0)
        {
            break;
        }

        T = DA[DAPtr];
        S = DA[DAPtr + 1];
    }

    //Copy BAM if the output is PNG or GIF. Do NOT copy if output is D64 - we don't want to overwrite the existing BAM.
    if ((OutputType == "png") || (OutputType == "gif"))
    {
        for (int i = 0; i < 72; i++)
        {
            Disk[Track[18] + i] = DA[Track[18] + i];
        }
        //Skip track 18 - some imported D64s have a faulty BAM, track 18 is not included in free block count and we need the correct one there
        for (int i = 76; i < 144; i++)
        {
            Disk[Track[18] + i] = DA[Track[18] + i];
        }
        FixBAM(Disk);       //In case we imported from an old Sparkle disk...
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM TXT
//----------------------------------------------------------------------------------------------------------------------------------------------------

void AddTxtDirEntry(string TxtDirEntry) {

    //Define file type and T:S if not yet denifed
    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
    {
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;      //"DEL"
    }
    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] == 0)            //Update T:S pointer only if it doesn't exits
    {
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
    }

    //Remove vbNewLine characters and add 16 SHIFT+SPACE tail characters
    for (int i = 0; i < 16; i++)
    {
        TxtDirEntry += 0xa0;
    }

    //Copy only the first 16 characters of the edited TxtDirEntry to the Disk Directory
    for (int i = 0; i < 16; i++)
    {
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Ascii2DirArt[toupper(TxtDirEntry[i])];
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromTxt()
{

    DirArt = ReadFileToString(InFileName);

    if (DirArt.empty())
    {
        cerr << "***ABORT***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }

    string delimiter = "\n";

    while (DirArt.find(delimiter) != string::npos)
    {
        DirEntry = DirArt.substr(0, DirArt.find(delimiter));
        DirArt.erase(0, DirArt.find(delimiter) + delimiter.length());
        FindNextDirPos();
        if (DirPos != 0)
        {
            AddTxtDirEntry(DirEntry);
        }
        else
        {
            return true;
        }
    }

    if (!DirArt.empty())
    {
        EntryIndex++;

        if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
        {
            FindNextDirPos();

            if (DirPos != 0)
            {
                AddTxtDirEntry(DirArt);
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM PRG AND OTHER BINARY
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromBinary() {

    vector<unsigned char> DA;

    if (ReadBinaryFile(InFileName, DA) == -1)
    {
        cerr << "***ABORT***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }
    else if (DA.size() == 0)
    {
        cerr << "***ABORT***\tThe DirArt file is 0 bytes long.\n";
        return false;
    }

    size_t DAPtr = (DirArtType == "prg") ? 2 : 0;
    size_t EntryStart = DAPtr;
    while (DAPtr <= DA.size())
    {
        if (((DAPtr == DA.size()) && (DAPtr != EntryStart)) || (DAPtr == EntryStart + 16) || (DAPtr < DA.size() && (DA[DAPtr] == 0xa0)))
        {
            EntryIndex++;

            if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
            {
                FindNextDirPos();

                if (DirPos != 0)
                {
                    //Define file type and T:S if not yet denifed
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;      //"DEL"
                    }
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] == 0)            //Update T:S pointer only if it doesn't exits
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
                    }

                    for (size_t i = 0; i < 16; i++)                 //Fill dir entry with 0xa0 chars first
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = 0xa0;
                    }

                    for (size_t i = 0; i < DAPtr - EntryStart; i++) //Then copy dir entry
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Petscii2DirArt[DA[EntryStart + i]];
                    }

                }
                else
                {
                    return true;
                }
            }

            while ((DAPtr < DA.size()) && (DAPtr < EntryStart + 39) && (DA[DAPtr] != 0xa0))
            {
                DAPtr++;        //Find last char of screen row (start+39) or first $a0-byte if sooner
            }

            EntryStart = DAPtr + 1; //Start of next line
        }
        DAPtr++;
    }
    
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM C
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool AddCArrayDirEntry(int RowLen)
{
    DirEntryAdded = false;

    if (DirEntry.find(0x0d) != string::npos)        //Make sure 0x0d is removed from end of string
    {
        DirEntry = DirEntry.substr(0, DirEntry.find(0x0d));
    }

    string Entry[16];
    size_t EntryStart = 0;
    int NumEntries = 0;
    string delimiter = ",";

    while ((EntryStart = DirEntry.find(delimiter)) != string::npos)
    {
        if (!DirEntry.substr(0, EntryStart).empty())
        {
            Entry[NumEntries++] = DirEntry.substr(0, EntryStart);
        }

        DirEntry.erase(0, EntryStart + delimiter.length());

        if (NumEntries == RowLen)   //Max row length reached, ignore rest of the char row
        {
            break;
        }
    }

    if ((NumEntries < 16) && (!DirEntry.empty()))
    {
        for (size_t i = 0; i < DirEntry.length(); i++)
        {
            string S = DirEntry.substr(0, 1);
            DirEntry.erase(0, 1);
            if (IsNumeric(S))
            {
                Entry[NumEntries] += S;
            }
            else
            {
                break;
            }

        }
        NumEntries++;
    }

    if (NumEntries == RowLen)       //Ignore rows that are too short.
    {
        unsigned char bEntry[16]{};
        bool AllNumeric = true;
        int V = 0;

        for (int i = 0; i < NumEntries; i++)
        {
            string prefix = Entry[i].substr(0, 2);
            if ((prefix == "0x") || (prefix == "&h"))
            {
                Entry[i].erase(0, 2);
                if (IsHexString(Entry[i]))
                {
                    bEntry[V++] = ConvertHexStringToInt(Entry[i]);
                }
                else
                {
                    AllNumeric = false;
                    break;
                }
            }
            else if (Entry[i].substr(0, 1) == "&")
            {
                Entry[i].erase(0, 1);
                if (IsHexString(Entry[i]))
                {
                    bEntry[V++] = ConvertHexStringToInt(Entry[i]);
                }
                else
                {
                    AllNumeric = false;
                    break;
                }
            }
            else
            {
                if (IsNumeric(Entry[i]))
                {
                    bEntry[V++] = ConvertStringToInt(Entry[i]);
                }
                else
                {
                    AllNumeric = false;
                    break;
                }
            }
        }

        if (AllNumeric)
        {
            EntryIndex++;

            if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
            {
                FindNextDirPos();

                if (DirPos != 0)
                {
                    //Define file type and T:S if not yet denifed
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;      //"DEL"
                    }
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] == 0)            //Update T:S pointer only if it doesn't exits
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
                    }

                    for (size_t i = 0; i < 16; i++)
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Petscii2DirArt[bEntry[i]];
                    }

                    DirEntryAdded = true;

                }
                else
                {
                    return false;
                }
            }
        }
    }

    return true;

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromCArray() {

    string DA = ReadFileToString(InFileName);

    if (DA.empty())
    {
        cerr << "***ABORT***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }

    //Convert the whole string to lower case for easier processing
    for (size_t i = 0; i < DA.length(); i++)
    {
        DA[i] = tolower(DA[i]);
    }

    //unsigned char* BinFile = new unsigned char[6*256];
    const char* findmeta = "meta:";
    size_t indexMeta = DA.find(findmeta);

    string sRowLen = "0";
    string sRowCnt = "0";

    if (indexMeta != string::npos)
    {
        size_t i = indexMeta;
        while ((i < DA.length()) && (DA[i] != ' '))     //Skip META:
        {
            i++;
        }

        while ((i < DA.length()) && (DA[i] == ' '))     //Skip SPACE between values
        {
            i++;
        }

        while ((i < DA.length()) && (DA[i] != ' '))
        {
            sRowLen += DA[i++];
        }

        while ((i < DA.length()) && (DA[i] == ' '))     //Skip SPACE between values
        {
            i++;
        }

        while ((i < DA.length()) && (DA[i] != ' '))
        {
            sRowCnt += DA[i++];
        }
    }

    int RowLen = min(ConvertStringToInt(sRowLen), 16);
    int RowCnt = min(ConvertStringToInt(sRowCnt), 48);

    size_t First = DA.find('{');
    //size_t Last = DA.find('}');

    DA.erase(0, First + 1);

    string LineBreak = "\n";
    size_t CALineStart = 0;
    int NumRows = 0;
    bool DirFull = false;

    while (((CALineStart = DA.find(LineBreak)) != string::npos) && (NumRows < RowCnt))
    {
        DirEntry = DA.substr(0, CALineStart);     //Extract one dir entry

        if (!AddCArrayDirEntry(RowLen))
        {
            DirFull = true;
            break;
        }

        DA.erase(0, CALineStart + LineBreak.length());

        if (DirEntryAdded)
        {
            NumRows++;
        }
    }

    if ((!DirFull) && (NumRows < RowCnt))
    {
        DirEntry = DA;
        AddCArrayDirEntry(RowLen);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM ASM
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool AddAsmDirEntry(string AsmDirEntry)
{
    for (size_t i = 0; i < AsmDirEntry.length(); i++)
    {
        AsmDirEntry[i] = tolower(AsmDirEntry[i]);
    }

    string EntrySegments[5];
    string delimiter = "\"";        // = " (entry gets split at quotation marks) NOT A BUG, DO NOT CHANGE THIS
    int NumSegments = 0;

    for (int i = 0; i < 5; i++)
    {
        EntrySegments[i] = "";
    }

    while ((AsmDirEntry.find(delimiter) != string::npos) && (NumSegments < 4))
    {
        EntrySegments[NumSegments++] = AsmDirEntry.substr(0, AsmDirEntry.find(delimiter));
        AsmDirEntry.erase(0, AsmDirEntry.find(delimiter) + delimiter.length());
    }
    EntrySegments[NumSegments++] = AsmDirEntry;

    //EntrySegments[0] = '[name =' OR '[name = @'
    //EntrySegments[1] = 'text' OR '\$XX\$XX\$XX'
    //EntrySegments[2] = 'type ='
    //EntrySegments[3] = 'del' OR 'prg' etc.
    //EntrySegments[4] = ']'

    if (NumSegments > 1)
    {
        if ((EntrySegments[0].find('[') != string::npos) && (EntrySegments[0].find("name") != string::npos) && (EntrySegments[0].find('=') != string::npos))
        {
            //FileType = 0x80;  //DEL default file type

            if (NumSegments > 3)
            {
                if ((EntrySegments[2].find("type") != string::npos) && (EntrySegments[2].find('=') != string::npos))
                {
                    if (EntrySegments[3] == "*seq")
                    {
                        FileType = 0x01;
                    }
                    else if (EntrySegments[3] == "*prg")
                    {
                        FileType = 0x02;
                    }
                    else if (EntrySegments[3] == "*usr")
                    {
                        FileType = 0x03;
                    }
                    else if (EntrySegments[3] == "del<")
                    {
                        FileType = 0xc0;
                    }
                    else if (EntrySegments[3] == "seq<")
                    {
                        FileType = 0xc1;
                    }
                    else if (EntrySegments[3] == "prg<")
                    {
                        FileType = 0xc2;
                    }
                    else if (EntrySegments[3] == "usr<")
                    {
                        FileType = 0xc3;
                    }
                    else if (EntrySegments[3] == "rel<")
                    {
                        FileType = 0xc4;
                    }
                    else if (EntrySegments[3] == "seq")
                    {
                        FileType = 0x81;
                    }
                    else if (EntrySegments[3] == "prg")
                    {
                        FileType = 0x82;
                    }
                    else if (EntrySegments[3] == "usr")
                    {
                        FileType = 0x83;
                    }
                    else if (EntrySegments[3] == "rel")
                    {
                        FileType = 0x84;
                    }
                    else
                    {
                        FileType = 0x80;
                    }
                }
            }
            else
            {
                FileType = 0x80;    //Default file type is DEL
            }
            
            EntryIndex++;

            if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
            {
                FindNextDirPos();

                if (DirPos != 0)
                {

                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;          //Always overwrite FileType

                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] == 0)            //Update T:S pointer only if it doesn't exit
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
                    }

                    if (EntrySegments[0].find('@') != string::npos)
                    {
                        //Numeric entry
                        unsigned char Entry[16];
                        string Values[16];

                        //Fill array with 16 SHIFT+SPACE characters
                        fill_n(Entry, 16, 0xa0);
                        fill_n(Values, 16, "");

                        int NumValues = 0;
                        delimiter = "\\";
                        while ((EntrySegments[1].find(delimiter) != string::npos) && (NumValues < 15))
                        {
                            string ThisValue = EntrySegments[1].substr(0, EntrySegments[1].find(delimiter));
                            if (!ThisValue.empty())
                            {
                                Values[NumValues++] = ThisValue;
                            }
                            EntrySegments[1].erase(0, EntrySegments[1].find(delimiter) + delimiter.length());
                        }
                        Values[NumValues++] = EntrySegments[1];

                        int Idx = 0;

                        for (int v = 0; v < NumValues; v++)
                        {
                            if (!Values[v].empty())
                            {
                                unsigned char NextChar = 0x20;          //SPACE default char - if conversion is not possible
                                if (Values[v].find('$') == 0)
                                {
                                    //Hex Entry
                                    Values[v].erase(0, 1);
                                    if (IsHexString(Values[v]))
                                    {
                                        NextChar = ConvertHexStringToInt(Values[v]);
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                                else
                                {
                                    //Decimal Entry
                                    if (IsNumeric(Values[v]))
                                    {
                                        NextChar = ConvertStringToInt(Values[v]);
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                                Entry[Idx++] = NextChar;
                                if (Idx == 16)
                                {
                                    break;
                                }
                            }
                        }
                        for (size_t i = 0; i < 16; i++)
                        {
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Entry[i];
                        }
                    }
                    else
                    {
                        //Text Entry, pad it with inverted space
                        //Fill the slot first with $A0
                        for (size_t i = 0; i < 16; i++)
                        {
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = 0xa0;
                        }
                        string ThisEntry = EntrySegments[1];
                        for (size_t i = 0; (i < ThisEntry.length()) && (i < 16); i++)
                        {
                            unsigned char NextChar = toupper(ThisEntry[i]);
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Ascii2DirArt[NextChar];
                        }
                    }
                }
                else
                {
                    return false;
                }
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool AddAsmDiskParameters()
{
    bool QuoteOn = false;
    for (size_t i = 0; i < DirEntry.length(); i++)
    {
        if (DirEntry[i] == '\"')
        {
            QuoteOn = !QuoteOn;
        }
        else if (!QuoteOn)
        {
            DirEntry[i] = tolower(DirEntry[i]);
        }
    }

    string DiskName = "";
    string delimiter = "filename";

    if (DirEntry.find(delimiter) != string::npos)
    {
        DiskName = DirEntry;
        DiskName.erase(0, DiskName.find(delimiter) + delimiter.length());
        DirEntry.erase(DirEntry.find(delimiter), DirEntry.find(delimiter) + delimiter.length());
        
        if (DiskName.find('\"') != string::npos)
        {
            DiskName = DiskName.substr(DiskName.find('\"') + 1);
            if (DiskName.find('\"') != string::npos)
            {
                DiskName = DiskName.substr(0, DiskName.find('\"'));
            }
            else
            {
                DiskName = "";
            }
        }
        else
        {
            DiskName = "";
        }
    }

    if (!DiskName.empty())
    {
        OutFileName = DiskName;

        CorrectFilePathSeparators();        //Replace "\" with "/" which is recognized by Windows as well
    }

    //Find the input file's extension
    int ExtStart = InFileName.length();

    for (int i = OutFileName.length() - 1; i >= 0; i--)
    {
        if (OutFileName[i] == '/')
        {
            //We've reached a path separator before a "." -> no extension
            break;
        }
        else if (OutFileName[i] == '.')
        {
            ExtStart = i + 1;
            break;
        }
    }

    OutputType = "";

    for (size_t i = ExtStart; i < OutFileName.length(); i++)
    {
        OutputType += tolower(OutFileName[i]);
    }

    if ((OutputType != "png") && (OutputType != "gif") && (OutputType != "d64"))
    {
        cerr << "***ABORT***\tUnrecognized output file type: " << OutFileName << "\n";
        return false;
    }

    if (!OpenOutFile())
        return false;

    cout << "***INFO***\tImport target " << OutFileName << " specified in ASM source file. Ignoring option -o\n";

    if (OutputType == "d64")
    {
        if (AppendMode)
        {
            cout << "Import mode: Append, skipping all existing directory entries in " + OutFileName + "\n";

            if (!FindLastUsedDirPos())
                return false;
        }
        else
        {
            if (!SkipDirEntries())
                return false;    //Currently there is no failure mode in SkipDirEntries()...

            if (AppendMode)
            {
                cout << "Import mode: Append, skipping all " + to_string(NumDirEntries) + " existing directory entries in " + OutFileName + "\n";
            }
            else
            {
                cout << "Import mode: Overwrite";
                if (NumSkippedEntries > 0)
                {
                    cout << ", skipping " + argSkippedEntries + " directory " + ((NumSkippedEntries == 1) ? "entry in " : "entries in ") + OutFileName + "\n";
                }
                else
                {
                    cout << "\n";
                }
            }
        }
    }

    string DirHeader = "";
    delimiter = "name";
    if (DirEntry.find(delimiter) != string::npos)
    {
        DirHeader = DirEntry;
        DirHeader.erase(0, DirHeader.find(delimiter) + delimiter.length());
        
        if (DirHeader.find('\"') != string::npos)
        {
            DirHeader = DirHeader.substr(DirHeader.find('\"') + 1);
            if (DirHeader.find('\"') != string::npos)
            {
                DirHeader = DirHeader.substr(0, DirHeader.find('\"'));
            }
            else
            {
                DirHeader = "";
            }
        }
        else
        {
            DirHeader = "";
        }
    }

    if (!DirHeader.empty())
    {
        if (!argDiskName.empty())
        {
            cout << "***INFO***\tDisk name \"" << DirHeader << "\" specified in ASM source file. Ignoring option -n\n";
        }

        for (size_t i = 0; i < 16; i++)
        {
            if (i < DirHeader.size())
            {
                Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(DirHeader[i])];
            }
            else
            {
                Disk[Track[18] + 0x90 + i] = 0xa0;
            }
        }
    }
    else if (!argDiskName.empty())
    {
        for (size_t i = 0; i < 16; i++)
        {
            if (i < argDiskName.size())
            {
                Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(argDiskName[i])];
            }
            else
            {
                Disk[Track[18] + 0x90 + i] = 0xa0;
            }
        }
    }

    string DirID = "";
    delimiter = "id";
    if (DirEntry.find(delimiter) != string::npos)
    {
        DirID = DirEntry;
        DirID.erase(0, DirID.find(delimiter) + delimiter.length());

        if (DirID.find('\"') != string::npos)
        {
            DirID = DirID.substr(DirID.find('\"') + 1);
            if (DirID.find('\"') != string::npos)
            {
                DirID = DirID.substr(0, DirID.find('\"'));
            }
            else
            {
                DirID = "";
            }
        }
        else
        {
            DirID = "";
        }
    }

    if (!DirID.empty())
    {
        if (!argDiskID.empty())
        {
            cout << "***INFO***\tDisk ID \"" << DirID << "\" specified in ASM source file. Ignoring option -i\n";
        }

        for (size_t i = 0; i < 5; i++)
        {
            if (i < DirID.size())
            {
                Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(DirID[i])];
            }
            else
            {
                Disk[Track[18] + 0xa2 + i] = 0xa0;
            }
        }
    }
    else if (!argDiskID.empty())
    {
        for (size_t i = 0; i < 5; i++)
        {
            if (i < argDiskID.size())
            {
                Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(argDiskID[i])];
            }
            else
            {
                Disk[Track[18] + 0xa2 + i] = 0xa0;
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromAsm()
{

    DirArt = ReadFileToString(InFileName);

    if (DirArt.empty())
    {
        cerr << "***ABORT***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }

    //Convert the whole string to lower case for easier processing
    string delimiter = "\n";

    while (DirArt.find(delimiter) != string::npos)
    {
        DirEntry = DirArt.substr(0, DirArt.find(delimiter));
        DirArt.erase(0, DirArt.find(delimiter) + delimiter.length());
        string EntryType = DirEntry.substr(0, DirEntry.find('['));

        for (size_t i = 0; i < EntryType.length(); i++)
        {
            EntryType[i] = tolower(EntryType[i]);
        }
        
        if (EntryType.find(".disk") != string::npos)
        {
            if (!AddAsmDiskParameters())
                return false;
        }
        else
        {
            if (!AddAsmDirEntry(DirEntry))   //Convert one line at the time
                return true;
        }
    }

    if ((!DirArt.empty()) && (DirPos != 0))
    {
        DirEntry = DirArt;
        string EntryType = DirEntry.substr(0, DirEntry.find('['));

        for (size_t i = 0; i < EntryType.length(); i++)
        {
            EntryType[i] = tolower(EntryType[i]);
        }

        if (EntryType.find(".disk") != string::npos)
        {
            if (!AddAsmDiskParameters())
                return false;
        }
        else
        {
            AddAsmDirEntry(DirEntry);
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM PET
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromPet()
{
    //Binary file With a .pet extension that includes the following information, without any padding:
    //- 1 byte: screen width in chars (40 for whole C64 screen)
    //- 1 byte: screen height in chars (25 for whole C64 screen)
    //- 1 byte: border Color($D020)                              - IGNORED BY SPARKLE
    //- 1 byte: background Color($D021)                          - IGNORED BY SPARKLE
    //- 1 byte: 0 for uppercase PETSCII, 1 for lowercase PETSCII - IGNORED BY SPARKLE
    //- <width * height> bytes: video RAM data
    //- <width * height> bytes: Color RAM data                   - IGNORED BY SPARKLE

    vector<unsigned char> PetFile;

    if (ReadBinaryFile(InFileName, PetFile) == -1)
    {
        cerr << "***ABORT***\tUnable to open the following .PET DirArt file: " << InFileName << "\n";
        return false;
    }
    else if (PetFile.size() == 0)
    {
        cerr << "***ABORT***\tThis .PET DirArt file is 0 bytes long: " << InFileName << "\n";
        return false;
    }

    unsigned char RowLen = PetFile[0];
    unsigned char RowCnt = PetFile[1];

    for (size_t rc = 0; rc < RowCnt; rc++)
    {
        EntryIndex++;

        if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
        {
            FindNextDirPos();

            if (DirPos != 0)
            {
                //Define file type and T:S if not yet denifed
                if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;      //"DEL"
                }
                if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] == 0)            //Update T:S pointer only if it doesn't exits
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
                }

                for (size_t i = 0; i < 16; i++)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = 0xa0;
                }

                for (size_t i = 0; (i < RowLen) && (i < 16); i++)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Petscii2DirArt[PetFile[5 + (rc * RowLen) + i]];
                }
            }
            else
            {
                return true;
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM JSON
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromJson()
{
    DirArt = ReadFileToString(InFileName);

    if (DirArt.empty())
    {
        cerr << "***ABORT***\tUnable to open the following .JSON DirArt file: " << InFileName << "\n";
        return false;
    }

    //Convert the whole string to lower case for easier processing
    for (size_t i = 0; i < DirArt.length(); i++)
    {
        DirArt[i] = tolower(DirArt[i]);
    }
    int RowLen = 40;
    int RowCnt = 25;

    if (DirArt.find("width") != string::npos)
    {
        //find numeric string between "width":XX,
        string W = DirArt.substr(DirArt.find("width"));
        W = W.substr(W.find(':') + 1);
        W = W.substr(0, W.find(','));
        while (W.find(' ') != string::npos)
        {
            //Remove space characters if there's any
            W.replace(W.find (' '),1,"");
        }
        if ((W.length() > 0) && (IsNumeric(W)))
        {
            RowLen = ConvertStringToInt(W);
        }
    }
    
    if (DirArt.find("height") != string::npos)
    {
        //find numeric string between "height":XX,
        string H = DirArt.substr(DirArt.find("height"));
        H = H.substr(H.find(':') + 1);
        H = H.substr(0, H.find(','));
        while (H.find(' ') != string::npos)
        {
            //Remove space characters if there's any
            H.replace(H.find(' '), 1, "");
        }
        if ((H.length() > 0) && (IsNumeric(H)))
        {
            RowCnt = ConvertStringToInt(H);
        }
    }
    unsigned char* ScreenCodes = new unsigned char[RowLen * RowCnt] {};

    if (DirArt.find("screencodes") != string::npos)
    {
        //find numeric string between "height":XX,
        string S = DirArt.substr(DirArt.find("screencodes"));
        S = S.substr(S.find('[') + 1);
        S = S.substr(0, S.find(']'));
        while (S.find(' ') != string::npos)
        {
            //Remove space characters if there's any
            S.replace(S.find(' '), 1, "");
        }

        size_t p = 0;

        while (S.find(',') != string::npos)
        {
            string B = S.substr(0, S.find(','));
            S.erase(0, S.find(',') + 1);
            if (IsNumeric(B))
            {
                ScreenCodes[p++] = ConvertStringToInt(B);
            }
            else if (IsHexString(B))
            {
                ScreenCodes[p++] = ConvertHexStringToInt(B);
            }
        }
        
        //Last screen code, after asr comma
        if (IsNumeric(S))
        {
            ScreenCodes[p++] = ConvertStringToInt(S);
        }
        else if (IsHexString(S))
        {
            ScreenCodes[p++] = ConvertHexStringToInt(S);
        }
    
        for (int rc = 0; rc < RowCnt; rc++)
        {
            EntryIndex++;

            if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
            {
                FindNextDirPos();

                if (DirPos != 0)
                {
                    //Define file type and T:S if not yet denifed
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;      //"DEL"
                    }
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] == 0)            //Update T:S pointer only if it doesn't exits
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
                    }

                    for (int i = 0; i < 16; i++)
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = 0xa0;
                    }

                    for (int i = 0; i < min(16, RowLen); i++)
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Petscii2DirArt[ScreenCodes[(rc * RowLen) + i]];
                    }
                }
                else
                {
                    delete[] ScreenCodes;

                    return true;
                }
            }
        }
    }

    delete[] ScreenCodes;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM IMAGE
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool IdentifyColors()
{   
    unsigned int Col1 = GetPixel(ScrLeft, ScrTop);  //The first of two allowed colors per image
    unsigned int Col2 = Col1;

    //First find two colors (Col1 is already assigned to pixel(0,0))
    for (size_t y = ScrTop; y < ScrHeight; y++)  //Bug fix: check every single pixel, no just one in every Mplr-sized square
    {
        for (size_t x = ScrLeft; x < ScrWidth; x++)
        {
            unsigned int ThisCol = GetPixel(x, y);
            if ((ThisCol != Col1) && (Col2 == Col1))
            {
                Col2 = ThisCol;
            }
            else if ((ThisCol != Col1) && (ThisCol != Col2))
            {
                cerr << "***ABORT***\tThis image contains more than two colors.\n";
                return false;
            }
        }
    }

    if (Col1 == Col2)
    {
        cerr << "***ABORT***\tUnable to determine foreground and background colors in DirArt image file.\n";
        return false;
    }

    if (!HasBorders)//(ImgWidth % 128 == 0)
    {

        //Start with same colors
        FGCol = Col1;
        BGCol = Col1;

        //First let's try to find a SPACE character -> its color will be the background color

        for (size_t cy = 0; cy < ImgHeight; cy += ((size_t)Mplr * 8))
        {
            for (size_t cx = 0; cx < ImgWidth; cx += ((size_t)Mplr * 8))
            {
                int PixelCnt = 0;

                unsigned int FirstCol = GetPixel(cx, cy);

                for (size_t y = 0; y < (size_t)Mplr * 8; y += (size_t)Mplr)
                {
                    for (size_t x = 0; x < (size_t)Mplr * 8; x += (size_t)Mplr)
                    {
                        if (GetPixel(cx + x, cy + y) == FirstCol)
                        {
                            PixelCnt++;
                        }
                    }
                }
                if (PixelCnt == 64)         //SPACE character found (i.e. all 64 pixels are the same color - the opposite (0xa0) is not allowed in dirart)
                {
                    BGCol = FirstCol;
                    FGCol = (BGCol == Col1) ? Col2 : Col1;
                    return true;
                }
            }//End cx
        }//End cy
    }
    else
    {
        if (Col1 == GetPixel(ScrLeft, ScrTop))
        {
            BGCol = Col1;
            FGCol = Col2;
            return true;
        }
        else
        {
            BGCol = Col2;
            FGCol = Col1;
            return true;
        }

    }

#ifdef PXDENSITY
        //No SPACE found in DirArt image, now check the distribution of low and high density pixels (i.e. pixels that have the highest chance to be eighter BG of FG color)
        //THIS IS NOT USED AS THE DISTRIBUTION OF THE INDIVIDUAL CHARACTERS CANNOT BE PREDICTED
    int LoPx = 0;
    int HiPx = 0;

    for (size_t cy = 0; cy < ImgHeight; cy += (size_t)Mplr * 8)
    {
        for (size_t cx = 0; cx < ImgWidth; cx += (size_t)Mplr * 8)
        {
            //Low density pixels = pixels most likely to be 0 (0:2, 7:2, 0:5. 7:5, 5:7)
            if (GetPixel(cx + 0, cy + 2) == Col1) LoPx++;
            if (GetPixel(cx + 7, cy + 2) == Col1) LoPx++;
            if (GetPixel(cx + 0, cy + 5) == Col1) LoPx++;
            if (GetPixel(cx + 7, cy + 5) == Col1) LoPx++;
            if (GetPixel(cx + 5, cy + 7) == Col1) LoPx++;

            //High density pixels = pixels most likely to be 1 (2:3, 3:3, 4:3, 3:6, 4:6)
            if (GetPixel(cx + 2, cy + 3) == Col1) HiPx++;
            if (GetPixel(cx + 3, cy + 3) == Col1) HiPx++;
            if (GetPixel(cx + 4, cy + 3) == Col1) HiPx++;
            if (GetPixel(cx + 3, cy + 6) == Col1) HiPx++;
            if (GetPixel(cx + 4, cy + 6) == Col1) HiPx++;

        }//End cx
    }//End cy

    if (LoPx > HiPx)
    {
        BGCol = Col1;
        FGCol = Col2;
    }
    else
    {
        BGCol = Col2;
        FGCol = Col1;
    }

    if (FGCol != BGCol)
    {
        return true;
    }
#endif // PXDENSITY

    //Let's use the darker color as BGCol
    int R1 = (Col1 >> 16) & 0xff;
    int G1 = (Col1 >> 8) & 0xff;
    int B1 = Col1 & 0xff;
    int R2 = (Col2 >> 16) & 0xff;
    int G2 = (Col2 >> 8) & 0xff;
    int B2 = Col2 & 0xff;

    double C1 = sqrt((0.21 * R1 * R1) + (0.72 * G1 * G1) + (0.07 * B1 * B1));
    double C2 = sqrt((0.21 * R2 * R2) + (0.72 * G2 * G2) + (0.07 * B2 * B2));
    //Another formula: sqrt(0.299 * R ^ 2 + 0.587 * G ^ 2 + 0.114 * B ^ 2)

    BGCol = (C1 < C2) ? Col1 : Col2;
    FGCol = (BGCol == Col1) ? Col2 : Col1;

    if (BGCol == FGCol)
    {
        cerr << "***ABORT***\tUnable to determine foreground and background colors in DirArt image file.\n";
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool DecodeBmp()
{
    const size_t BIH = 0x0e;                                            //Offset of Bitmap Info Header within raw data
    const size_t DATA_OFFSET = 0x0a;                                    //Offset of Bitmap Data Start within raw data
    const size_t MINSIZE = sizeof(tagBITMAPINFOHEADER) + BIH;           //Minimum header size

    if (ImgRaw.size() < MINSIZE)
    {
        cerr << "***ABORT***\tThe size of this BMP file is smaller than the minimum size allowed.\n";
        return false;
    }

    memcpy(&BmpInfoHeader, &ImgRaw[BIH], sizeof(BmpInfoHeader));

    //if (BmpInfoHeader.biWidth % 128 != 0)
    //{
        //cerr << "***ABORT***\tUnsupported BMP size. The image must be 128 pixels (16 chars) wide or a multiple of it if resized.\n";
        //return false;
    //}

    if ((BmpInfoHeader.biCompression != 0) && (BmpInfoHeader.biCompression != 3))
    {
        cerr << "***ABORT***\tUnsupported BMP format. Sparkle can only work with uncompressed BMP files.\nT";
        return false;
    }

    int ColMax = (BmpInfoHeader.biBitCount < 24) ? (1 << BmpInfoHeader.biBitCount) : 0;

    PBITMAPINFO BmpInfo = (PBITMAPINFO)new char[sizeof(BITMAPINFOHEADER) + (ColMax * sizeof(RGBQUAD))];

    //Copy info header into structure
    memcpy(&BmpInfo->bmiHeader, &BmpInfoHeader, sizeof(BmpInfoHeader));

    //Copy palette into structure
    for (size_t i = 0; i < (size_t)ColMax; i++)
    {
        memcpy(&BmpInfo->bmiColors[i], &ImgRaw[0x36 + (i * 4)], sizeof(RGBQUAD));
    }

    //Calculate data offset
    size_t DataOffset = (size_t)ImgRaw[DATA_OFFSET] + (size_t)(ImgRaw[DATA_OFFSET + 1] * 0x100) + (size_t)(ImgRaw[DATA_OFFSET + 2] * 0x10000) + (size_t)(ImgRaw[DATA_OFFSET + 3] * 0x1000000);

    //Calculate length of pixel rows in bytes
    size_t RowLen = ((size_t)BmpInfo->bmiHeader.biWidth * (size_t)BmpInfo->bmiHeader.biBitCount) / 8;

    //BMP pads pixel rows to a multiple of 4 in bytes
    size_t PaddedRowLen = (RowLen % 4) == 0 ? RowLen : RowLen - (RowLen % 4) + 4;
    //size_t PaddedRowLen = ((RowLen + 3) / 4) * 4;

    size_t CalcSize = DataOffset + (BmpInfo->bmiHeader.biHeight * PaddedRowLen);

    if (ImgRaw.size() != CalcSize)
    {
        cerr << "***ABORT***\tCorrupted BMP file size.\n";

        delete[] BmpInfo;

        return false;
    }

    //Calculate size of our image vector (we will use 4 bytes per pixel in RGBA format)
    size_t BmpSize = (size_t)BmpInfo->bmiHeader.biWidth * 4 * (size_t)BmpInfo->bmiHeader.biHeight;

    //Resize image vector
    Image.resize(BmpSize, 0);

    size_t BmpOffset = 0;

    if (ColMax != 0)    //1 bit/pixel, 4 bits/pixel, 8 bits/pixel modes - use palette data
    {
        int BitsPerPx = BmpInfo->bmiHeader.biBitCount;   //1         4          8
        int bstart = 8 - BitsPerPx;                      //1-bit: 7; 4-bit:  4; 8-bit =  0;
        int mod = 1 << BitsPerPx;                        //1-bit: 2; 4-bit: 16; 8-bit: 256;

        for (int y = (BmpInfo->bmiHeader.biHeight - 1); y >= 0; y--)    //Pixel rows are read from last bitmap row to first
        {
            size_t RowOffset = DataOffset + (y * PaddedRowLen);

            for (size_t x = 0; x < RowLen; x++)                         //Pixel row are read left to right
            {
                unsigned int Pixel = ImgRaw[RowOffset + x];

                for (int b = bstart; b >= 0; b -= BitsPerPx)
                {
                    int PaletteIndex = (Pixel >> b) % mod;

                    Image[BmpOffset + 0] = BmpInfo->bmiColors[PaletteIndex].rgbRed;
                    Image[BmpOffset + 1] = BmpInfo->bmiColors[PaletteIndex].rgbGreen;
                    Image[BmpOffset + 2] = BmpInfo->bmiColors[PaletteIndex].rgbBlue;
                    Image[BmpOffset + 3] = 0xff;
                    BmpOffset += 4;
                }
            }
        }
    }
    else
    {
        int BytesPerPx = BmpInfo->bmiHeader.biBitCount / 8;    //24 bits/pixel: 3; 32 bits/pixel: 4

        for (int y = (BmpInfo->bmiHeader.biHeight - 1); y >= 0; y--)    //Pixel rows are read from last bitmap row to first
        {
            size_t RowOffset = DataOffset + (y * PaddedRowLen);

            for (size_t x = 0; x < RowLen; x += BytesPerPx)              //Pixel row are read left to right
            {
                Image[BmpOffset + 0] = ImgRaw[RowOffset + x + 2];
                Image[BmpOffset + 1] = ImgRaw[RowOffset + x + 1];
                Image[BmpOffset + 2] = ImgRaw[RowOffset + x + 0];
                Image[BmpOffset + 3] = 0xff;
                BmpOffset += 4;
            }
        }
    }

    ImgWidth = BmpInfo->bmiHeader.biWidth;
    ImgHeight = BmpInfo->bmiHeader.biHeight;

    delete[] BmpInfo;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

unsigned char IdentifyChar(int PixelCnt, size_t cx, size_t cy)
{

    for (size_t i = 0; i < 256; i++)
    {
        if (PixelCnt == PixelCntTab[i])
        {
            size_t px = i % 16;
            size_t py = i / 16;
            bool Match = true;
            for (size_t y = 0; y < 8; y++)
            {
                for (size_t x = 0; x < 8; x++)
                {
                    size_t PixelX = cx + ((size_t)Mplr * x);
                    size_t PixelY = cy + ((size_t)Mplr * y);
                    unsigned int ThisCol = GetPixel(PixelX, PixelY);
                    unsigned char DACol = 0;
                    
                    if (ThisCol == FGCol)
                    {
                        DACol = 1;
                    }

                    size_t CharSetPos = (py * 8 * 128) + (y * 128) + (px * 8) + x;  //Only check upper case charset

                    if (DACol != CharSetTab[CharSetPos])
                    {
                        Match = false;
                        break;
                    }
                }//Next x
                if (!Match)
                {
                    break;
                }//End if
            }//Next y
            if (Match)
            {
                return (unsigned char)i;
            }//End if
        }//End if
    }//Next i

    return (unsigned char)32;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool Import384()
{

    size_t cy = ScrTop;

    //FIND DIR HEADER

    bool HeaderFound = false;

    while (cy < ScrTop + ScrHeight)
    {
        if (IdentifyChar(64-12, ScrLeft + (size_t)Mplr * 8 * 2, cy) == 0xa2)
        {
            //HEADER FOUND
            unsigned char Entry[16]{};
            size_t Idx = 0;

            for (int j = 0; j < 16; j++)
            {
                Entry[j] = 0x20;
            }

            for (size_t cx = ScrLeft + (size_t)Mplr * 8 * 3; cx < ScrLeft + (size_t)Mplr * 8 * 19; cx += (size_t)Mplr * 8)
            {
                int PixelCnt = 0;
                for (size_t y = 0; y < (size_t)Mplr * 8; y += (size_t)Mplr)
                {
                    for (size_t x = 0; x < (size_t)Mplr * 8; x += (size_t)Mplr)
                    {
                        if (GetPixel(cx + x, cy + y) == FGCol)
                        {
                            PixelCnt++;
                        }
                    }//End x
                }//End y

                unsigned char ThisChar = IdentifyChar(PixelCnt, cx, cy);

                if (ThisChar != 0xa2)
                {
                    Entry[Idx++] = ThisChar & 0x7f;
                }
                else
                {
                    break;
                }
            }//Next cx

            //HEADER
            for (int i = 0; i < 16; i++)
            {
                unsigned int C = Entry[i];
                Disk[Track[DirTrack] + 0x90 + i] = Petscii2DirArt[C];
            }

            Idx = 0;
            for (int j = 0; j < 16; j++)
            {
                Entry[j] = 0x20;
            }

            for (size_t cx = ScrLeft + (size_t)Mplr * 8 * 21; cx < ScrLeft + (size_t)Mplr * 8 * 26; cx += (size_t)Mplr * 8)
            {
                int PixelCnt = 0;
                for (size_t y = 0; y < (size_t)Mplr * 8; y += (size_t)Mplr)
                {
                    for (size_t x = 0; x < (size_t)Mplr * 8; x += (size_t)Mplr)
                    {
                        if (GetPixel(cx + x, cy + y) == FGCol)
                        {
                            PixelCnt++;
                        }
                    }//End x
                }//End y

                unsigned char ThisChar = IdentifyChar(PixelCnt, cx, cy);

                if (ThisChar != 0xa2)
                {
                    Entry[Idx++] = ThisChar & 0x7f;
                }
                else
                {
                    break;
                }
            }//Next cx

            //ID
            for (int i = 0; i < 5; i++)
            {
                unsigned int C = Entry[i];
                Disk[Track[DirTrack] + 0xa2 + i] = Petscii2DirArt[C];
            }
            HeaderFound = true;
        }

        cy += (size_t)Mplr * 8;

        if (HeaderFound)
        {
            break;
        }

    }

    if (!HeaderFound)
    {
        cy = ScrTop;        //Reset cy if we didn't find a dir header line, otherwise continue from current cy
    }

    bool EntryFound = false;

    while (cy < ScrTop + ScrHeight)
    {
        if (IdentifyChar(12, ScrLeft + (size_t)Mplr * 8 * 5, cy) == 0x22)
        {
            EntryFound = true;
            
            unsigned char Entry[16]{};
            size_t Idx = 0;

            for (int j = 0; j < 16; j++)
            {
                Entry[j] = 0xa0;
            }

            for (size_t cx = ScrLeft + (size_t)Mplr * 8 * 6; cx < ScrLeft + (size_t)Mplr * 8 * 22; cx += (size_t)Mplr * 8)
            {
                int PixelCnt = 0;
                for (size_t y = 0; y < (size_t)Mplr * 8; y += (size_t)Mplr)
                {
                    for (size_t x = 0; x < (size_t)Mplr * 8; x += (size_t)Mplr)
                    {
                        if (GetPixel(cx + x, cy + y) == FGCol)
                        {
                            PixelCnt++;
                        }
                    }//End x
                }//End y

                unsigned char ThisChar = IdentifyChar(PixelCnt, cx, cy);

                if (ThisChar != 0x22)
                {
                    Entry[Idx++] = ThisChar;
                }
                else
                {
                    break;
                }
            }//Next cx

            string sType = "";

            for (size_t cx = ScrLeft + (size_t)Mplr * 8 * 23; cx < ScrLeft + (size_t)Mplr * 8 * 28; cx += (size_t)Mplr * 8)
            {
                int PixelCnt = 0;
                for (size_t y = 0; y < (size_t)Mplr * 8; y += (size_t)Mplr)
                {
                    for (size_t x = 0; x < (size_t)Mplr * 8; x += (size_t)Mplr)
                    {
                        if (GetPixel(cx + x, cy + y) == FGCol)
                        {
                            PixelCnt++;
                        }
                    }//End x
                }//End y

                unsigned char ThisChar = IdentifyChar(PixelCnt, cx, cy);
                
                ThisChar = Petscii2DirArt[ThisChar];
                
                if (ThisChar != 32)
                {
                    sType += ThisChar;
                }

            }//Next cx

            if (sType == "*SEQ")
            {
                FileType = 0x01;
            }
            else if (sType == "*PRG")
            {
                FileType = 0x02;
            }
            else if (sType == "*USR")
            {
                FileType = 0x03;
            }
            else if (sType == "DEL<")
            {
                FileType = 0xc0;
            }
            else if (sType == "SEQ<")
            {
                FileType = 0xc1;
            }
            else if (sType == "PRG<")
            {
                FileType = 0xc2;
            }
            else if (sType == "USR<")
            {
                FileType = 0xc3;
            }
            else if (sType == "REL<")
            {
                FileType = 0xc4;
            }
            else if (sType == "DEL")
            {
                FileType = 0x80;
            }
            else if (sType == "SEQ")
            {
                FileType = 0x81;
            }
            else if (sType == "PRG")
            {
                FileType = 0x82;
            }
            else if (sType == "USR")
            {
                FileType = 0x83;
            }
            else if (sType == "REL")
            {
                FileType = 0x84;
            }
            else
            {
                FileType = 0x80;    //Default file type is DEL
            }

            int EntryNumBlocks = 0;
            int Decimal = 1;
            for (size_t cx = ScrLeft + (size_t)Mplr * 8 * 4; cx >= ScrLeft; cx -= (size_t)Mplr * 8)
            {
                int PixelCnt = 0;
                for (size_t y = 0; y < (size_t)Mplr * 8; y += (size_t)Mplr)
                {
                    for (size_t x = 0; x < (size_t)Mplr * 8; x += (size_t)Mplr)
                    {
                        if (GetPixel(cx + x, cy + y) == FGCol)
                        {
                            PixelCnt++;
                        }
                    }//End x
                }//End y

                unsigned char ThisChar = IdentifyChar(PixelCnt, cx, cy);
                if (ThisChar != 32)
                {
                    EntryNumBlocks += Decimal * (ThisChar - 0x30);
                    Decimal *= 10;
                }

            }//Next cx

            EntryIndex++;

            if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
            {
                FindNextDirPos();

                if (DirPos != 0)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;      //"DEL"
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0

                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 28] = EntryNumBlocks % 0x100;
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 29] = EntryNumBlocks / 0x100;

                    for (int i = 0; i < 16; i++)
                    {
                        unsigned int C = Entry[i];
                        if (C != 0xa0)
                        {
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Petscii2DirArt[C];
                        }
                        else
                        {
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = 0xa0;
                        }
                    }
                }
                else
                {
                    return true;
                }
            }

        }
        else
        {
            if (EntryFound)
            {
                break;
            }
        }
        cy += (size_t)Mplr * 8;
    }

    if (EntryFound)
    {
        //DO NOT IMPORT NUMBER OF FREE BLOCKS - IT IS DETERMINED BY THE BAM OF THE FINAL DISK
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void FindMplr()
{

    if (ImgWidth % 384 == 0)
    {
        HasBorders = true;
        Mplr = ImgWidth / 384;

        unsigned int Col = GetPixel(0, 0);

        bool Match = true;

        for (size_t y = 0; y < (size_t)Mplr * 35; y++)
        {
            for (size_t x = 0; x < (size_t)ImgWidth; x++)
            {
                if (GetPixel(x, y) != Col)
                {
                    HasBorders = false;
                    Mplr = ImgWidth / 128;
                    Match = false;
                    break;
                }
            }
            if (!Match)
            {
                break;
            }
        }

        if (Match)
        {
            for (size_t y = ImgHeight - (size_t)Mplr * 37; y < (size_t)ImgHeight; y++)
            {
                for (size_t x = 0; x < (size_t)ImgWidth; x++)
                {
                    if (GetPixel(x, y) != Col)
                    {
                        HasBorders = false;
                        Mplr = ImgWidth / 128;
                        Match = false;
                        break;
                    }
                }
                if (!Match)
                {
                    break;
                }
            }

        }

        if (Match)
        {
            for (size_t y = (size_t)Mplr * 35; y < ImgHeight - (size_t)Mplr * 37; y++)
            {
                for (size_t x = 0; x < (size_t)Mplr * 32; x++)
                {
                    if (GetPixel(x, y) != Col)
                    {
                        HasBorders = false;
                        Mplr = ImgWidth / 128;
                        Match = false;
                        break;
                    }
                }
                if (!Match)
                {
                    break;
                }
            }

        }

        if (Match)
        {
            for (size_t y = (size_t)Mplr * 35; y < ImgHeight - (size_t)Mplr * 37; y++)
            {
                for (size_t x = (size_t)Mplr * 352; x < (size_t)ImgWidth; x++)
                {
                    if (GetPixel(x, y) != Col)
                    {
                        HasBorders = false;
                        Mplr = ImgWidth / 128;
                        Match = false;
                        break;
                    }
                }
                if (!Match)
                {
                    break;
                }
            }
        }
    }
    else
    {
        HasBorders = false;
        Mplr = ImgWidth / 128;
    }

}

bool Import128()
{
    int PixelCnt = 0;

    for (size_t cy = 0; cy < ImgHeight; cy += (size_t)Mplr * 8)
    {
        unsigned int Entry[16]{};
        for (size_t cx = 0; cx < ImgWidth; cx += (size_t)Mplr * 8)
        {
            PixelCnt = 0;
            for (size_t y = 0; y < (size_t)Mplr * 8; y += (size_t)Mplr)
            {
                for (size_t x = 0; x < (size_t)Mplr * 8; x += (size_t)Mplr)
                {
                    if (GetPixel(cx + x, cy + y) == FGCol)
                    {
                        PixelCnt++;
                    }
                }//End x
            }//End y
            Entry[cx / ((size_t)Mplr * 8)] = 32;

            for (size_t i = 0; i < 256; i++)
            {
                if (PixelCnt == PixelCntTab[i])
                {
                    size_t px = i % 16;
                    size_t py = i / 16;
                    bool Match = true;
                    for (size_t y = 0; y < 8; y++)
                    {
                        for (size_t x = 0; x < 8; x++)
                        {
                            size_t PixelX = cx + ((size_t)Mplr * x);
                            size_t PixelY = cy + ((size_t)Mplr * y);
                            unsigned int ThisCol = GetPixel(PixelX, PixelY);
                            unsigned char DACol = 0;
                            if (ThisCol == FGCol)
                            {
                                DACol = 1;
                            }

                            size_t CharSetPos = (py * 8 * 128) + (y * 128) + (px * 8) + x;  //Only check upper case charset

                            if (DACol != CharSetTab[CharSetPos])
                            {
                                Match = false;
                                break;
                            }
                        }//Next x
                        if (!Match)
                        {
                            break;
                        }//End if
                    }//Next y
                    if (Match)
                    {
                        Entry[cx / ((size_t)Mplr * 8)] = i;
                        break;
                    }//End if
                }//End if
            }//Next i
        }//Next cx

        EntryIndex++;

        if ((EntryIndex >= FirstImportedEntry) && (EntryIndex <= LastImportedEntry))
        {
            FindNextDirPos();

            if (DirPos != 0)
            {
                if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;      //"DEL"
                }
                if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] == 0)            //Update T:S pointer only if it doesn't exits
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
                }

                for (int i = 0; i < 16; i++)
                {
                    unsigned int C = Entry[i];
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Petscii2DirArt[C];
                }
            }
            else
            {
                return true;
            }
        }
    }//Next cy

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromImage()
{
    ImgRaw.clear();
    Image.clear();

    if (ReadBinaryFile(InFileName, ImgRaw) == -1)
    {
        cerr << "***ABORT***\tUnable to open image DirArt file.\n";
        return false;
    }
    else if (ImgRaw.size() == 0)
    {
        cerr << "***ABORT***\tThe DirArt file cannot be 0 bytes long.\n";
        return false;
    }

    if (DirArtType == "png")
    {
        //Load and decode PNG image using LodePNG (Copyright (c) 2005-2023 Lode Vandevenne)
        unsigned int error = lodepng::decode(Image, ImgWidth, ImgHeight, ImgRaw);

        if (error)
        {
            cout << "***ABORT***\tPNG decoder error: " << error << ": " << lodepng_error_text(error) << "\n";
            return false;
        }
    }
    else if (DirArtType == "bmp")
    {
        if(!DecodeBmp())
        {
            return false;
        }
    }
    
    //Here we have the image decoded in Image vector of unsigned chars, 4 bytes representing a pixel in RGBA format

    if ((ImgWidth % 128 != 0) && (ImgWidth % 384 != 0))
    {
        cerr << "***ABORT***\tUnsupported image size. The image must be 128 pixels (16 chars) wide or a multiple of it if resized.\n";
        return false;
    }

    FindMplr();

    ScrLeft = (HasBorders ? 32 * (size_t)Mplr : 0);
    ScrTop = (HasBorders ? 35 * (size_t)Mplr : 0);
    ScrWidth = (HasBorders ? 320 * (size_t)Mplr : (size_t)ImgWidth);
    ScrHeight = (HasBorders ? ImgHeight - 72 * (size_t)Mplr : (size_t)ImgHeight);

    if (!IdentifyColors())
    {
        return false;
    }

    if (HasBorders)  //(ImgWidth % 384 == 0)
    {
        return Import384();
    }
    else
    {
        return Import128();
    }
 
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void CreateTrackTable()
{

    Track[1] = 0;

    for (int t = 1; t < 40;t++)
    {
        if (t < 18)
        {
            Track[t + 1] = Track[t] + ((size_t)21 * 256);
        }
        else if (t < 25)
        {
            Track[t + 1] = Track[t] + ((size_t)19 * 256);
        }
        else if (t < 31)
        {
            Track[t + 1] = Track[t] + ((size_t)18 * 256);
        }
        else
        {
            Track[t + 1] = Track[t] + ((size_t)17 * 256);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void ShowInfo()
{
    cout << "DART is a command-line tool that imports C64 directory art from a variety of source file types to D64 disk images.\n";
    cout << "It can also create PNG and animated GIF outputs of the directory art.\n\n";

    cout << "Usage:\n";
    cout << "------\n";
    cout << "dart input -o [output.d64/output.png/output.gif] -n [\"disk name\"] -i [\"disk id\"] -s [skipped entries]\n";
    cout << "           -t [default entry type] -f [first imported entry] -l [last imported entry]\n\n";

    cout << "input - the file from which the directory art will be imported. See accepted file types below.\n\n";

    cout << "-o [output.d64/output.png/output.gif] - the D64/PNG/GIF file to which the directory art will be imported. This\n";
    cout << "       parameter is optional and it will be ignored if it is used with KickAss ASM input files that include the\n";
    cout << "       'filename' disk parameter. If an output is not specified then DART will create an input_dart.d64 file. If the\n";
    cout << "       output file is a PNG or GIF then DART will create a PNG \"screenshot\" of the directory listing or an animated\n";
    cout << "       GIF instead of a D64 file. If there are less than 23 entries then the ouput PNG's size will be 384 x 272 pixels\n";
    cout << "       (same as a VICE screenshot). If there are at least 23 entries then the height will be ((n + 4) * 8) + 72 pixels.\n";
    cout << "       The -s option will be ignored if the output is a PNG of GIF (you can't append entries to an existing PNG or GIF).\n\n";
        
    cout << "-n [\"disk name\"] - the output D64's disk name (left side of the topmost inverted row of the directory listing), max.\n";
    cout << "       16 characters. Wrap text in double quotes. This parameter is optional and it will be ignored if it is used\n";
    cout << "       with D64 and ASM files that contain this information.\n\n";

    cout << "-i [\"disk id\"] - the output d64's disk ID (right side of the topmost inverted row of the directory listing), max. 5\n";
    cout << "       characters. Wrap text in double quotes. This parameter is optional and it will be ignored if it is used with\n";;
    cout << "       D64 and ASM files that contain this information.\n\n";

    cout << "-s [skipped entries] - the number of entries in the directory of the output.d64 that you don't want to overwrite.\n";
    cout << "       E.g. use 1 if you want to leave the first entry untouched. To append the directory art to the end of the\n";
    cout << "       existing directory, use 'all' instead of a numeric value. This parameter is optional. If not specified, the\n";
    cout << "       default value is 0 and DART will overwrite all existing directory entries.\n\n";

    cout << "-t [default entry type] - DART will use the file type specified here for each directory art entry, if not otherwise\n";
    cout << "       defined in the input file. Accepted values include: del, prg, usr, seq, *del, del<, etc. This parameter is\n";
    cout << "       optional. If not specified, DART will use del as default entry type. The -t option will be ignored if the\n";
    cout << "       type is specified in the input file (ASM or D64, see below).\n\n";

    cout << "-f [first imported entry] - a numeric value (1-based) which is used by DART to determine the first DirArt entry to be\n";
    cout << "       imported. This parameter is optional. If not specified, DART will start import with the first DirArt entry.\n\n";

    cout << "-l [last imported entry] - a numeric value (1-based) which DART uses to determine the last DirArt entry to be\n";
    cout << "       imported. This parameter is optional. If not specified, DART will finish import after the last DirArt entry.\n\n";

    cout << "- [palette number (00-22) for PNG output] - a numeric value (0-based) which DART uses to determine the palette to be\n";
    cout << "       used for a PNG or GIF output. You can choose from 23 palettes that are available in VICE 3.8:\n\n";
    cout << "               00 - C64HQ              08 - Godot              16 - Pepto PAL\n";
    cout << "               01 - C64S               09 - PALette            17 - Pepto PAL old\n";
    cout << "               02 - CCS64              10 - PALette 6569R1     18 - Pixcen\n";
    cout << "               03 - ChristopherJam     11 - PALette 6569R5     19 - Ptoing\n";
    cout << "               04 - Colodore           12 - PALette 8565R2     20 - RGB\n";
    cout << "               05 - Community Colors   13 - PC64               21 - VICE 3.8 Original\n";
    cout << "               06 - Deekay             14 - Pepto NTSC Sony    22 - VICE 3.8 Internal\n";
    cout << "               07 - Frodo              15 - Pepto NTSC         \n\n";
    cout << "       If not specified, DART will use Pepto PAL (palette No. 16) as default.\n\n";

    cout << "You can only import from one input file at a time. DART will overwrite the existing directory entries in the output\n";
    cout << "file (after skipping the number of entries defined with the -s option) with the new, imported ones, leaving only the\n";
    cout << "track:sector pointers intact. To attach the imported DirArt to the end of the existing directory, use the -s option\n";
    cout << "with 'all' instead of a number. This allows importing multiple DirArts into the same D64 as long as there is space\n";
    cout << "on track 18. DART does not support directories expanding beyond track 18. The new entries' type can be modified with\n";
    cout << "the -t option or it can be defined separately in D64 and KickAss ASM input files. You can also define the first and\n";
    cout << "last DirArt entries you want to import from the input file using the -f and -l options. Both can take 1-based numbers\n";
    cout << "as values (i.e., 1 means first).\n\n";

    cout << "Accepted input file types:\n";
    cout << "--------------------------\n";
    cout << "D64  - DART will import all directory entries from the input file. The disk name, ID, and the entry types will also\n";
    cout << "       be imported. The track and sector pointers will be left untouched in the output D64. The -n and -i options\n";
    cout << "       will be ignored.\n\n";

    cout << "PRG  - DART accepts two file formats: screen RAM grabs (40-byte long char rows of which the first 16 are used as dir\n";
    cout << "       entries, can have more than 25 char rows) and $a0 byte terminated directory entries*. If an $a0 byte is not\n";
    cout << "       detected sooner, then 16 bytes are imported per directory entry. DART then skips up to 24 bytes or until an\n";
    cout << "       $a0 byte detected. Example source code for $a0-terminated .PRG (KickAss format, must be compiled):\n\n";

    cout << "           * = $1000              // Address can be anything, will be skipped by DART\n";
    cout << "           .text \"hello world!\"   // This will be upper case once compiled\n";
    cout << "           .byte $a0              // Terminates directory entry\n";
    cout << "           .byte $30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$01,$02,$03,$04,$05,$06,$a0\n\n";

    cout << "       *Char code $a0 (inverted space) is also used in standard directories to mark the end of entries.\n\n";

    cout << "BIN  - DART will treat this file type the same way as PRGs, without the first two header bytes.\n\n";

    cout << "ASM  - KickAss ASM DirArt source file. Please refer to Chapter 11 in the Kick Assembler Reference Manual for details.\n";
    cout << "       The ASM file may contain both disk and file parameters within [] brackets. DART recognizes the 'filename',\n";
    cout << "       'name', and 'id' disk parameters and the 'name' and 'type' file parameters. If disk parameters are provided\n";
    cout << "       they will overwrite the -o, -n, and -i options. If a 'type' file parameter is specified then it will be used\n";
    cout << "       instead of the default entry type defined by the -t option.\n";
    cout << "       Example:\n\n";

    cout << "           .disk [filename= \"Test.d64\", name=\"test disk\", id=\"-g*p-\"]\n";
    cout << "           {\n";
    cout << "               [name = \"0123456789ABCDEF\", type = \"prg\"],\n";
    cout << "               [name = @\"\\$75\\$69\\$75\\$69\\$B2\\$69\\$75\\$69\\$75\\$ae\\$B2\\$75\\$AE\\$20\\$20\\$20\", type=\"del\"],\n";
    cout << "           }\n\n";

    cout << "C    - Marq's PETSCII Editor C array file. This file type can also be produced using Petmate. This is essentially a\n";
    cout << "       C source file which consists of a single unsigned char array declaration initialized with the dir entries.\n";
    cout << "       If present, DART will use the 'META:' comment after the array to determine the dimensions of the DirArt.\n\n";

    cout << "PET  - This format is supported by Marq's PETSCII Editor and Petmate. DART will use the first two bytes of the input\n";
    cout << "       file to determine the dimensions of the DirArt, but it will ignore the next three bytes (border and background\n";
    cout << "       colors, charset) as well as color RAM data. DART will import max. 16 chars per directory entry.\n\n";

    cout << "JSON - This format can be created using Petmate. DART will import max. 16 chars from each character row.\n\n";

    cout << "PNG  - Portable Network Graphics image file. Image input files can only use two colors (background and foreground).\n";
    cout << "       DART will try to identify the colors by looking for a space character. If none found, it will use the darker\n";
    cout << "       of the two colors as background color. Image files must display the uppercase charset and their appearance\n";
    cout << "       cannot rely on command characters. DART accepts two input image formats: one without boders and another one\n"; 
    cout << "       with (VICE) borders.\n\n";   

    cout << "       - Borderless images must have a width of exactly 16 characters (128 pixels) and the height must be a multiple\n";
    cout << "       of 8 pixels. Images can only contain directory entries without a directory header and entry type identifiers.\n\n";

    cout << "       - Images with borders must have a width of 384 pixels where the border is 32 pixels on each side. The height\n";
    cout << "       must include a 35-pixel top and a 37-pixel bottom border (as in VICE). The \"screen\" portion of the image\n";
    cout << "       must be 320 pixels wide and at least 200 pixels tall or more if the DirArt has more entries than what would\n";
    cout << "       fit on a standard screen. The  image can also include a directory header with a disk name and ID, and entry\n";
    cout << "       types. Bordered images can be created by listing the directory in VICE and then saving a screenshot. DART\n";
    cout << "       can also create this format.\n";

    cout << "BMP  - Bitmap image file. Same rules and limitations as with PNGs.\n\n";

    cout << "Any other file type will be handled as a binary file and will be treated as PRGs without the first two header bytes.\n\n";

    cout << "Example 1:\n";
    cout << "----------\n\n";

    cout << "dart MyDirArt.pet\n\n";

    cout << "DART will create MyDirArt_dart.d64 (if it doesn't already exist) and will import all DirArt entries from MyDirArt.pet\n";
    cout << "into it, overwriting all existing directory entries, using del as entry type.\n\n";

    cout << "Example 2:\n";
    cout << "----------\n\n";

    cout << "dart MyDirArt.c -o MyDemo.d64 -n \"demo 2023\" -i \"-g*p-\"\n\n";

    cout << "DART will import the DirArt from a Marq's PETSCII Editor C array file into MyDemo.d64 overwriting all existing\n";
    cout << "directory entries, using del as entry type, and it will update the disk name and ID in MyDemo.d64.\n\n";

    cout << "Example 3:\n";
    cout << "----------\n\n";

    cout << "dart MyDirArt.asm -o MyDemo.d64 -s all\n\n";

    cout << "DART will append the DirArt from a KickAss ASM source file to the existing directory entries of MyDemo.d64.\n\n";

    cout << "Example 4:\n";
    cout << "----------\n\n";

    cout << "dart MyDirArt1.png -o MyDemo.d64 -s 1 -t prg\n";
    cout << "dart MyDirArt2.png -o MyDemo.d64 -s all -f 3 -l 7\n\n";

    cout << "DART will first import the DirArt from a PNG image file into MyDemo.d64 overwriting all directory entries except\n";
    cout << "the first one, using prg as entry type. Then DART will append entries 3-7 from the second PNG file to the directory\n";
    cout << "of MyDemo.d64 (keeping all already existing entries in it), using del as (default) entry type.\n\n";

    cout << "Example 5:\n";
    cout << "----------\n\n";

    cout << "dart MyDirArt.d64 -o MyDirArt1.png -p 18\n\n";

    cout << "DART will import the DirArt from a D64 file and convert it to a PNG, using palette 18 (Pixcen).\n\n";
    
    cout << "DART uses the LodePNG library by Lode Vandevenne (http://lodev.org/lodepng/) to encode and decode PNG files,\n";
    cout << "and gif.h by Charlie Tangora (https://github.com/charlietangora/gif-h) to create animated GIFs.\n\n";
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    cout << "\n";
    cout << "*********************************************************\n";
    cout << "DART 1.4 - Directory Art Importer by Sparta (C) 2022-2025\n";
    cout << "*********************************************************\n";
    cout << "\n";

    if (argc == 1)
    {

    #ifdef DEBUG
        InFileName = "c:/dart/test/output/art1.png";
        OutFileName = "c:/dart/test.d64";
        argSkippedEntries = "8";
        argFirstImportedEntry = "16";
        argEntryType = "del";
        argPalette = "18";
    #else
        cout << "Usage: dart input [options]\n";
        cout << "options:    -o <output.d64> or <output.png> or <output.gif>\n";
        cout << "            -n <\"disk name\">\n";
        cout << "            -i <\"disk id\">\n";
        cout << "            -s <skipped entries in output.d64>\n";
        cout << "            -t <default entry type>\n";
        cout << "            -f <first imported entry>\n";
        cout << "            -l <last imported entry>\n";
        cout << "            -p <palette number (00-22) for png output>\n\n";
        cout << "Help:  dart -h\n";
        return EXIT_SUCCESS;
    #endif

    }

    vector <string> args;
    args.resize(argc);

    for (int c = 0; c < argc; c++)
    {
        args[c] = argv[c];
    }

    int i = 1;

    while (i < argc)
    {
        if (i == 1)
        {
            InFileName = args[1];
        }
        else if ((args[i] == "-o") || (args[i] == "-O"))        //output D64
        {
            if (i + 1 < argc)
            {
                OutFileName = args[++i];
            }
            else
            {
                cerr << "***ABORT***\tMissing option -o [output.d64/output.png/output.gif] value.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-n") || (args[i] == "-N"))       //disk name in output D64
        {
            if (i + 1 < argc)
            {
                argDiskName = args[++i];

            }
            else
            {
                cerr << "***ABORT***\tMissing option -n [disk name] value.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-i") || (args[i] == "-I"))       //disk ID in output D64
        {
            if (i + 1 < argc)
            {
                argDiskID = args[++i];

            }
            else
            {
                cerr << "***ABORT***\tMissing option -i [disk id] value.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-s") || (args[i] == "-S"))       //skipped entries in output D64's directory
        {
            if (i + 1 < argc)
            {
                argSkippedEntries = args[++i];
            }
            else
            {
                cerr << "***ABORT***\tMissing option -s [skipped entries] value.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-t") || (args[i] == "-T"))       //default entry type in output D64
        {
            if (i + 1 < argc)
            {
                argEntryType = args[++i];
            }
            else
            {
                cerr << "***ABORT***\tMissing option -t [default entry type] value.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-f") || (args[i] == "-F"))       //first imported entry from input
        {
            if (i + 1 < argc)
            {
                argFirstImportedEntry = args[++i];
            }
            else
            {
                cerr << "***ABORT***\tMissing option -f [first imported entry] value.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-l") || (args[i] == "-L"))       //last imported entry from input
        {
            if (i + 1 < argc)
            {
                argLastImportedEntry = args[++i];
            }
            else
            {
                cerr << "***ABORT***\tMissing option -l [last imported entry] value.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-p") || (args[i] == "-P"))       //last imported entry from input
        {
            if (i + 1 < argc)
            {
                argPalette = args[++i];
            }
            else
            {
                cerr << "***ABORT***\tMissing option -p [palette] value.\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            cerr << "***ABORT***\tUnrecognized option: " << args[i] << "\n";
            return EXIT_FAILURE;
        }
        i++;
    }
    
    if (InFileName.empty())
    {
        cerr << "***ABORT***\tMissing input file name!\n";
        return EXIT_FAILURE;
    }

    if (InFileName == "-h")
    {
        ShowInfo();
        return EXIT_SUCCESS;
    }

    if (argSkippedEntries == "all")
    {
        AppendMode = true;
    }
    else if (!argSkippedEntries.empty())
    {
        if (IsNumeric(argSkippedEntries))
        {
            NumSkippedEntries = ConvertStringToInt(argSkippedEntries);
        }
        else
        {
            cerr << "***ABORT***\tUnrecognized option -s [skipped entries] value!\n";
            return EXIT_FAILURE;
        }
    }

    for (size_t i = 0; i < argEntryType.length(); i++)
    {
        argEntryType[i] = toupper(argEntryType[i]);
    }
    
    if (argEntryType == "*SEQ")
    {
        FileType = 0x01;
    }
    else if (argEntryType == "*PRG")
    {
        FileType = 0x02;
    }
    else if (argEntryType == "*USR")
    {
        FileType = 0x03;
    }
    else if (argEntryType == "DEL<")
    {
        FileType = 0xc0;
    }
    else if (argEntryType == "SEQ<")
    {
        FileType = 0xc1;
    }
    else if (argEntryType == "PRG<")
    {
        FileType = 0xc2;
    }
    else if (argEntryType == "USR<")
    {
        FileType = 0xc3;
    }
    else if (argEntryType == "REL<")
    {
        FileType = 0xc4;
    }
    else if (argEntryType == "DEL")
    {
        FileType = 0x80;
    }
    else if (argEntryType == "SEQ")
    {
        FileType = 0x81;
    }
    else if (argEntryType == "PRG")
    {
        FileType = 0x82;
    }
    else if (argEntryType == "USR")
    {
        FileType = 0x83;
    }
    else if (argEntryType == "REL")
    {
        FileType = 0x84;
    }
    else
    {
        cerr << "***ABORT***\tUnrecognized option -t [default file type] value!\n";
        return EXIT_FAILURE;
    }

    if (!argFirstImportedEntry.empty())
    {
        if (IsNumeric(argFirstImportedEntry))
        {
            FirstImportedEntry = ConvertStringToInt(argFirstImportedEntry);
        }
        else
        {
            cerr << "***ABORT***\tThe value of option -f [first imported entry] must be numeric.\n";
            return EXIT_FAILURE;
        }
    }
    else
    {
        argFirstImportedEntry = "1";
    }

    if (!argLastImportedEntry.empty())
    {
        if (IsNumeric(argLastImportedEntry))
        {
            LastImportedEntry = ConvertStringToInt(argLastImportedEntry);
        }
        else
        {
            cerr << "***ABORT***\tThe value of option -l [last imported entry] must be numeric.\n";
            return EXIT_FAILURE;
        }
    }
    else
    {
        argLastImportedEntry = "last";
    }

    if (!fs::exists(InFileName))
    {
        cerr << "***ABORT***The following DirArt file does not exist:\n\n" << InFileName << "\n";
        return EXIT_FAILURE;
    }

    if (!argPalette.empty())
    {
        if (IsNumeric(argPalette))
        {
            PaletteIdx = ConvertStringToInt(argPalette);
            if ((PaletteIdx < 0) || (PaletteIdx >= NumPalettes))
            {
                cerr << "***ABORT***\tOption -p [palette] must have a value between 00-22.\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            cerr << "***ABORT***\tOption -p [palette] must have a numeric value.\n";
            return EXIT_FAILURE;
        }
    }
    else
    {
        argPalette = "16";
        PaletteIdx = 16;
    }

    CreateTrackTable();

    CorrectFilePathSeparators();        //Replace "\" with "/" which is recognized by Windows as well

    //Find the input file's extension
    int ExtStart = InFileName.length();

    for (int i = InFileName.length() - 1; i >= 0; i--)
    {
        if (InFileName[i] == '/')
        {
            //We've reached a path separator before a "." -> no extension
            break;
        }
        else if (InFileName[i] == '.')
        {
            ExtStart = i + 1;
            break;
        }
    }

    for (size_t i = ExtStart; i < InFileName.length(); i++)
    {
        DirArtType += tolower(InFileName[i]);
    }

    if (OutFileName.empty())
    {
        //Output file name not provided, use input file name without its extension to create output d64 file name
        OutFileName = InFileName.substr(0, InFileName.size() - DirArtType.size() - 1) + "_dart.d64";
    }

    for (int i = OutFileName.length() - 1; i >= 0; i--)
    {
        if (OutFileName[i] == '/')
        {
            //We've reached a path separator before a "." -> no extension
            break;
        }
        else if (OutFileName[i] == '.')
        {
            ExtStart = i + 1;
            break;
        }
    }

    for (size_t i = ExtStart; i < OutFileName.length(); i++)
    {
        OutputType += tolower(OutFileName[i]);
    }

    if ((OutputType != "png") && (OutputType != "gif") && (OutputType != "d64"))
    {
        cerr << "***ABORT***\tUnrecognized output file type: " << OutFileName << "\n";
        return EXIT_FAILURE;
    }

    if (!OpenOutFile())
    {
        return EXIT_FAILURE;
    }

    string Msg = "";

    if ((FirstImportedEntry != 1) || (LastImportedEntry != 0xffff))
    {
        Msg = "Importing DirArt entries: " + argFirstImportedEntry + " through " + argLastImportedEntry + "\n";
    }
    else
    {
        Msg = "Importing DirArt entries: all\n";
    }

    if ((OutputType == "d64") && (DirArtType != "asm"))
    {
        if (AppendMode)
        {
            Msg += "Import mode: Append, skipping all existing directory entries in " + OutFileName + "\n";

            //Let's find the last used dir entry slot, we will continue with the next, empty one
            if (!FindLastUsedDirPos())
                return EXIT_FAILURE;
        }
        else
        {
            if (!SkipDirEntries())
                return EXIT_FAILURE;    //Currently there is no failure mode in SkipDirEntries()...

            if (AppendMode)
            {
                Msg += "Import mode: Append, skipping all " + to_string(NumDirEntries) + " existing directory entries in " + OutFileName + "\n";
            }
            else
            {
                Msg += "Import mode: Overwrite";
                if (NumSkippedEntries > 0)
                {
                    Msg += ", skipping " + argSkippedEntries + " directory " + ((NumSkippedEntries == 1) ? "entry in " : "entries in ") + OutFileName + "\n";
                }
                else
                {
                    Msg += "\n";
                }
            }
        }
    }

    Msg += "Default DirArt entry type: " + argEntryType + "\n";

    if (DirArtType == "d64")
    {
        cout <<"Importing DirArt from D64...\n" << Msg;
        if (!ImportFromD64())               //Import from another D64
            return EXIT_FAILURE;
    }
    else if (DirArtType == "txt")
    {
        cout << "Importing DirArt from text file...\n" << Msg;
        if (!ImportFromTxt())               //Import from a text file, 16 characters per text line
            return EXIT_FAILURE;
    }
    else if (DirArtType == "prg")
    {
        cout << "Importing DirArt from PRG...\n" << Msg;
        if (!ImportFromBinary())            //Import from a PRG file, first 16 bytes from each 40, or until 0xa0 charaxter found
            return EXIT_FAILURE;
    }
    else if (DirArtType == "c")
    {
        cout << "Importing DirArt from Marq's PETSCII Editor C array file...\n" << Msg;
        if (!ImportFromCArray())            //Import from Marq's PETSCII Editor C array file to D64
            return EXIT_FAILURE;
    }
    else if (DirArtType == "asm")
    {
        cout << "Importing DirArt from KickAss ASM source...\n" << Msg;
        if (!ImportFromAsm())               //Import KickAss ASM dirart file to D64
            return EXIT_FAILURE;
    }
    else if (DirArtType == "json")
    {
        cout << "Importing DirArt from JSON source...\n" << Msg;
        if (!ImportFromJson())              //Import from a JSON file
            return EXIT_FAILURE;
    }
    else if (DirArtType == "pet")
    {
        cout << "Importing DirArt from PET source...\n" << Msg;
        if (!ImportFromPet())               //Import from PET binary
            return EXIT_FAILURE;
    }
    else if (DirArtType == "png")
    {
        cout << "Importing DirArt from PNG image...\n" << Msg;
        if (!ImportFromImage())             //Import from PNG image using LodePNG (Copyright (c) 2005-2023 Lode Vandevenne)
            return EXIT_FAILURE;
    }
    else if (DirArtType == "bmp")
    {
        cout << "Importing DirArt from BMP image...\n" << Msg;
        if (!ImportFromImage())             //Import from BMP image
            return EXIT_FAILURE;
    }
    else
    {
        cout << "Importing DirArt from binary file...\n" << Msg;
        if (!ImportFromBinary())            //Import from any other file, first 16 bytes of each 40 or until 0xa0 character found
            return EXIT_FAILURE;
    }

    //Finally, update the directory header and ID (done separately for ASM and D64 input files)
    if ((DirArtType != "asm") && (DirArtType != "d64"))
    {
        if (!argDiskName.empty())
        {
            for (size_t i = 0; i < 16; i++)
            {
                if (i < argDiskName.size())
                {
                    Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(argDiskName[i])];
                }
                else
                {
                    Disk[Track[18] + 0x90 + i] = 0xa0;
                }
            }
        }

        if (!argDiskID.empty())
        {
            for (size_t i = 0; i < 5; i++)
            {
                if (i < argDiskID.size())
                {
                    Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(argDiskID[i])];
                }
                else
                {
                    Disk[Track[18] + 0xa2 + i] = 0xa0;
                }
            }
        }
    }

    if (OutputType == "d64")
    {
        if (!WriteDiskImage(OutFileName))
        {
            return EXIT_FAILURE;
        }
    }
    else if (OutputType == "png")
    {
        cout << "Converting DirArt to PNG using palette No. " << C64PaletteNames[PaletteIdx] << "\n";
        if (!ConvertD64ToPng())
        {
            return EXIT_FAILURE;
        }
        cout << "Done!\n";
    }
    else if (OutputType == "gif")
    {
        cout << "Converting DirArt to GIF using palette No. " << C64PaletteNames[PaletteIdx];
        if (!ConvertD64ToGif())
        {
            return EXIT_FAILURE;
        }
        cout << "Done!\n";
    }

    if (NumDirEntries == MaxNumDirEntries)
    {
        cout << "\nNumber of directory entries has reached the maximum of " << (dec) << MaxNumDirEntries << ".\n";
    }
    else if (NumDirEntries > 0)
    {
        cout << "\nNumber of directory entries: " << (dec) << NumDirEntries << "\n";
    }

    int NumFreeSectors = 0;

    if (DirTrack == 18)
    {
        NumFreeSectors = Disk[Track[18] + (size_t)(18 * 4)];
        NumFreeEntries = NumFreeSectors * 8;
        for (int i = 2; i < 256; i += 32)
        {
            if (Disk[Track[DirTrack] + (DirSector * 256) + i] == 0)
            {
                NumFreeEntries++;
            }
        }
    }

    cout << "Directory entry slots remaining unused on track 18: " << dec << NumFreeEntries << "\n";

    if ((DirTrack != 18) && (DirTrack != 0))
    {
        cout << "DirArt extends beyond track 18.\n";
    }

    return EXIT_SUCCESS;
}
    

