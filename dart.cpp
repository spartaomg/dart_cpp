
//#define DEBUG
#define AddBlockCount

#include "common.h"

const int StdDiskSize = (664 + 19) * 256;
const int ExtDiskSize = StdDiskSize + (85 * 256);

const int NumSectorsTrack18 = 19;

int NumFreeEntries = 0;

vector <unsigned char> Disk;
vector <unsigned char> Image;   //pixels in RGBA format (4 bytes per pixel)
vector <unsigned char> ImgRaw;

//command line parameters
string InFileName = "";
string OutFileName = "";
string argSkippedEntries = "";
string argEntryType = "DEL";
string argFirstImportedEntry = "";
string argLastImportedEntry = "";
string argDiskName = "";
string argDiskID = "";

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

size_t DirTrack{}, DirSector{}, LastDirSector{};
size_t Track[41]{};

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
        cerr << "***CRITICAL***\tUnable to create the following folder: " << DiskDir << "\n\n";
        return false;
    }
    return true;
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
            cerr << "***CRITICAL***\tError during writing " << OutFileName << "\n";
            myFile.close();
            return false;
        }

        cout << "Done!\n";
        return true;
    }
    else
    {
        cerr << "***CRITICAL***\tError oprning file for writing disk image " << OutFileName << "\n\n";
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
            cerr << "***CRITICAL***\tError during writing " << DiskName << "\n";
            myFile.close();
            return false;
        }
        
        cout << "Done!\n";
        return true;
    }
    else
    {
        cerr << "***CRITICAL***\tError oprning file for writing disk image " << DiskName << "\n\n";
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

void DrawChar(unsigned char PChar, int ImgX, int ImgY, unsigned int Color)
{

    unsigned int px = PChar % 16;
    unsigned int py = PChar / 16;

    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            unsigned int CharSetPos = (py * 8 * 128) + (y * 128) + (px * 8) + x;

            if (CharSetTab[CharSetPos] == 1)
            {
                int PixelX = ImgX + x;
                int PixelY = ImgY + y;
                SetPixel(PixelX, PixelY, Color);
            }
        }//Next x
    }//Next y

}
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ConvertD64ToPng()
{

    int UsedEntries = 144 - NumFreeEntries;

    ImgWidth = 384;
    
#ifdef AddBlockCount
    ImgHeight = UsedEntries > 22 ? (UsedEntries * 8) + 24 + 72 : 272;
#elif
    ImgHeight = UsedEntries > 24 ? (UsedEntries * 8) + 8 + 72 : 272;
#endif

    Image.resize((size_t)ImgWidth * ImgHeight * 4);

    unsigned int BorderColor = 0x7c70da;    //light blue
    unsigned int BkgColor = 0x3e31a2;       //dark blue
    unsigned int ForeColor = 0x7c70da;      //light blue

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

            if ((y < 35) || (y >= (size_t)ImgHeight - 37)|| (x < 32) || (x >= (size_t)ImgWidth - 32))
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

    int ImgX = 32;
    int ImgY = 35;

    unsigned int B = 0;
    DrawChar(0x30, ImgX, ImgY, ForeColor);

    DrawChar(0xa2, 32 + 0x10, ImgY, ForeColor);
    DrawChar(0xa2, 32 + 0x98, ImgY, ForeColor);
    DrawChar(0xa0, 32 + 0xa0, ImgY, ForeColor);

    for (int i = 0; i < 16; i++)
    {
        B = Disk[Track[DirTrack] + 0x90 + i];
        if ((B >= 0x41) && (B < +0x5a))
        {
            B -= 0x40;
        }
        B |= 0x80;
        DrawChar(B, ImgX + 0x18 + (i * 8), ImgY, ForeColor);
    }

    for (int i = 0; i < 5; i++)
    {
        B = Disk[Track[DirTrack] + 0xa2 + i];
        if ((B >= 0x41) && (B < +0x5a))
        {
            B -= 0x40;
        }
        B |= 0x80;
        DrawChar(B, ImgX + 0xa8 + (i * 8), ImgY, ForeColor);
    }

    ImgY += 8;

    DirTrack = 18;
    DirSector = 1;
    DirPos = 2;

    while (DirPos != 0)
    {
        if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] != 0)
        {

            unsigned char ET = Disk[Track[DirTrack] + (DirSector * 256) + DirPos];
            string EntryType = "";
            if (ET == 0x01)
            {
                EntryType = "*SEQ";
            }
            else if (ET == 0x02)
            {
                EntryType = "*PRG";
            }
            else if (ET == 0x03)
            {
                EntryType = "*USR";
            }
            else if (ET == 0x80)
            {
                EntryType = " DEL";
            }
            else if (ET == 0x81)
            {
                EntryType = " SEQ";
            }
            else if (ET == 0x82)
            {
                EntryType = " PRG";
            }
            else if (ET == 0x83)
            {
                EntryType = " USR";
            }
            else if (ET == 0x84)
            {
                EntryType = " REL";
            }
            else if (ET == 0xc0)
            {
                EntryType = " DEL<";
            }
            else if (ET == 0xc1)
            {
                EntryType = " SEQ<";
            }
            else if (ET == 0xc2)
            {
                EntryType = " PRG<";
            }
            else if (ET == 0xc3)
            {
                EntryType = " USR<";
            }
            else if (ET == 0xc4)
            {
                EntryType = " REL<";
            }

            for (size_t i = 0; i < EntryType.length(); i++)
            {
                B = EntryType[i];
                if ((B >= 0x41) && (B <= 0x5a))
                {
                    B -= 0x40;
                }
                DrawChar(B, (size_t)32 + 48 + 128 + 8 + (i * 8), ImgY, ForeColor);
            }

            int BlockCnt = Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 28] + (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 29] * 256);

            if (BlockCnt > 9999)
            {
                B = (BlockCnt / 10000) + 0x30;
                DrawChar(B, ImgX, ImgY, ForeColor);
                ImgX += 8;
            }
            if (BlockCnt > 999)
            {
                B = ((BlockCnt / 1000) % 10) + 0x30;
                DrawChar(B, ImgX, ImgY, ForeColor);
                ImgX += 8;
            }
            if (BlockCnt > 99)
            {
                B = ((BlockCnt / 100) % 10) + 0x30;
                DrawChar(B, ImgX, ImgY, ForeColor);
                ImgX += 8;
            }
            if (BlockCnt > 9)
            {
                B = ((BlockCnt / 10) % 10) + 0x30;
                DrawChar(B, ImgX, ImgY, ForeColor);
                ImgX += 8;
            }

            B = (BlockCnt % 10) + 0x30;
            DrawChar(B, ImgX, ImgY, ForeColor);
                
            DrawChar(0x22, 32+40, ImgY, ForeColor);
            DrawChar(0x22, 32+48+128, ImgY, ForeColor);
            
            ImgX = 32 + 48;

            for (int i = 0; i < 16; i++)
            {
                unsigned int NextChar = Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i];
                unsigned char PChar = Char2Petscii[NextChar];
                DrawChar(PChar, ImgX, ImgY, ForeColor);
                ImgX += 8;
            }
            ImgX = 32;
            ImgY += 8;
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

#ifdef AddBlockCount
    int NumBlocksFree = 0;
    DirTrack = 18;
    for (size_t i = 1; i < 36; i++)
    {
        if (i != 18)        //Skip track 18
        {
            NumBlocksFree += Disk[Track[DirTrack] + (i * 4)];
        }
    }
    if (NumBlocksFree > 99)
    {
        B = ((NumBlocksFree / 100) % 10) + 0x30;
        DrawChar(B, ImgX, ImgY, ForeColor);
        ImgX += 8;
    }
    if (NumBlocksFree > 9)
    {
        B = ((NumBlocksFree / 10) % 10) + 0x30;
        DrawChar(B, ImgX, ImgY, ForeColor);
        ImgX += 8;
    }

    B = (NumBlocksFree % 10) + 0x30;
    DrawChar(B, ImgX, ImgY, ForeColor);
    ImgX += 16;
    
    string BlocksFreeMsg = "BLOCKS FREE.";

    for (size_t i = 0; i < BlocksFreeMsg.length(); i++)
    {
        B = Char2Petscii[(size_t)BlocksFreeMsg[i]];
        DrawChar(B, ImgX, ImgY, ForeColor);
        ImgX += 8;
    }
    ImgX = 32;
    ImgY += 8;

    string ReadyMsg = "READY.";

    for (size_t i = 0; i < ReadyMsg.length(); i++)
    {
        B = Char2Petscii[(size_t)ReadyMsg[i]];
        DrawChar(B, ImgX, ImgY, ForeColor);
        ImgX += 8;
    }
#endif



    unsigned int error = lodepng::encode(OutFileName, Image, ImgWidth, ImgHeight);

    if (error)
    {
        cout << "Error during encoding and saving PNG.\n";
        return false;
    }

    return true;

}


//----------------------------------------------------------------------------------------------------------------------------------------------------

void MarkSectorAsUsed(size_t T, size_t S)
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
    unsigned char BitToDelete = 1 << (S % 8);       //BAM Bit to be deleted

    if ((Disk[BitPtr] & BitToDelete) != 0)
    {
        Disk[BitPtr] &= (255 - BitToDelete);
        NumUnusedSectors--;
    }

    Disk[NumSectorPtr] = NumUnusedSectors;

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
    unsigned char BitToSet = 1 << (S % 8);       //BAM Bit to be deleted

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
    //Check if the BAM shows any free sectors on track 18
    if (Disk[Track[DirTrack] + (size_t)(18 * 4)] == 0)
    {
        DirSector = 0;
        DirPos = 0;     //This will indicate that the directory is full, no more entries are possible
        cout << "***INFO***\tDirectory is full.\n";
        return;
    }

    LastDirSector = DirSector;

    DirSector = 1; //Skip BAM (Sectors are numbered 0-18 on track 18)

    while (DirSector < NumSectorsTrack18)   //last sector on track 18 is sector 18 (out of 0-18)
    {
        //Check in BAM if this is really a free sector
        size_t NumSectorPtr = Track[18] + (DirTrack * 4);
        size_t BitPtr = NumSectorPtr + 1 + (DirSector / 8);     //BAM Position for Bit Change
        unsigned char BitToCheck = 1 << (DirSector % 8);        //BAM Bit to be deleted

        if ((Disk[BitPtr] & BitToCheck) != 0)
        {
            Disk[Track[DirTrack] + (LastDirSector * 256) + 0] = DirTrack;
            Disk[Track[DirTrack] + (LastDirSector * 256) + 1] = DirSector;
            
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
            if(Disk[Track[DirTrack] + (DirSector * 256) + 0] != 0)
            {
                //This is NOT the last sector in the T:S chain, we are overwriting existing dir entries
                int NextT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
                int NextS = Disk[Track[DirTrack] + (DirSector * 256) + 1];
                
                if (NextT != 18)
                {
                    //DART doesn't support directories outside track 18
                    DirSector = 0;
                    DirPos = 0;
                    cout << "***INFO***\tThis directory sector chain leaves track 18. DART doesn't support directories outside track 18!\n";
                    return;
                }
                else
                {
                    //Otherwise, go to next dir sector in T:S chain and overwrite first directory entry
                    DirTrack = NextT;
                    DirSector = NextS;
                    DirPos = 2;
                    //BUG reported by Dr Science - we want to keep the existing T:S info 
                    //for (int i = 2; i < 256; i ++)
                    //{
                    //    Disk[Track[DirTrack] + (DirSector * 256) + i] = 0;  //delete all file in dir sector
                    //}
                }
            }
            else
            {
                //We are adding new sectors to the chain
                FindNextEmptyDirSector();
            }
        }
    }
    
    if (DirSector != 0)
    {
        MarkSectorAsUsed(DirTrack, DirSector);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool FindLastUsedDirPos()
{
    //Check if the BAM shows free sectors on track 18
    if (Disk[Track[DirTrack] + (size_t)(18 * 4)] == 0)
    {
        cerr << "***CRITICAL***Track 18 is full. DART is unable to append to this directory.\n";
        return false;

    }

    unsigned char SectorChain[18]{};
    int ChainIndex = 0;

    SectorChain[ChainIndex] = DirSector;

    //First find the last used directory sector
    while (Disk[Track[DirTrack] + (DirSector * 256) + 0] != 0)
    {
        DirTrack = Disk[Track[DirTrack] + (DirSector * 256) + 0];
        DirSector = Disk[Track[DirTrack] + (DirSector * 256) + 1];

        if (DirTrack != 18)
        {
            cerr << "***CRITICAL***The directory on this disk extends beyond track 18. DART only supports directories on track 18.\n";
            return false;
        }

        SectorChain[++ChainIndex] = DirSector;
    }

    Disk[Track[DirTrack] + (DirSector * 256) + 0] = 0;
    Disk[Track[DirTrack] + (DirSector * 256) + 1] = 255;

    //Then find last used dir entry slot
    DirPos = (7 * 32) + 2;

    while (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] == 0)
    {
        DirPos -= 32;
        if (DirPos < 0)
        {
            if (ChainIndex > 0)
            {
                DirPos += 256;
                DirSector = SectorChain[--ChainIndex];
            }
            else
            {
                DirPos = 0;
                break;
            }
        }
    }
    return true;
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

bool ResetDirEntries()
{
    DirTrack = 18;
    DirSector = 1;
    DirPos = 0;

    int n = 0;

    while (n < NumSkippedEntries)
    {
        FindNextDirPos();
        if (DirPos == 0)
        {
            cerr << "***CRITICAL***\tUnable to skip " << NumSkippedEntries << "entries!\n";
            return false;
        }

        if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos] != 0)
        {
            n++;
        }
        else
        {
            NumSkippedEntries = n;  //we have reached the end of the directory, this is th next empty directory slot
            cout << "***INFO***\tSkipping the whole existing directory. Switching to Append Mode.\n";
            AppendMode = true;
            return true;
        }
    }

    //Here DirPos should point at the first dir entry that we will overwrite

    int i = (DirPos != 0) ? DirPos : 2;
    size_t DT = DirTrack;
    size_t DS = DirSector;

    while (i != 0)
    {
        i += 32;

        if (i > 256)
        {
            if (Disk[Track[DT] + (DS * 256) + 0] != 0)
            {
                DT = Disk[Track[DT] + (DS * 256) + 0];
                DS = Disk[Track[DT] + (DS * 256) + 1];
                i = 2;
            }
            else
            {
                break;
            }
        }

        Disk[Track[DT] + (DS * 256) + i] = 0;          //Delete file type indicator
        for (int j = i + 3; j < i + 30; j++)
        {
            Disk[Track[DT] + (DS * 256) + j] = 0;      //Delete dir entry's name and block size, skipping T:S pointer
        }
    }

    return true;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------

void CreateDisk()
{
    size_t CP = Track[18];

    DiskSize = StdDiskSize;

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

void FixSparkleBAM()
{
    DirTrack = 18;
    DirSector = 1;

    //Earlier versions of Sparkle disks don't mark DirArt sectors as "used" in the BAM. So let's follow the directory block chain and mark them off
    while (Disk[Track[DirTrack] + (DirSector * 256) + 0] != 0)
    {
        int NextT = Disk[Track[DirTrack] + (DirSector * 256) + 0];
        int NextS = Disk[Track[DirTrack] + (DirSector * 256) + 1];

        if ((NextT == 18) && (NextS == 0))  //Sparkle 1.x marked last secter in chain with 12:00 instead of 00:FF
        {
            break;
        }
        else
        {
            MarkSectorAsUsed(DirTrack, DirSector);
            DirTrack = NextT;
            DirSector = NextS;
        }
    }

    //Mark last sector as used if there is at least one dir entry (this will avoid marking the first sector off if the dir is completely empty)
    if (Disk[Track[DirTrack] + (DirSector * 256) + 2] != 0)
    {
        MarkSectorAsUsed(DirTrack, DirSector);
    }

    //Correct first two bytes of last sector
    Disk[Track[DirTrack] + (DirSector * 256) + 0] = 0;
    Disk[Track[DirTrack] + (DirSector * 256) + 1] = 0xff;

    //Sparkle 2 and 2.1 disks don't mark the internal directory sectors (18:17 and 18:18) off, let's fix it.
    //Sparkle 2.0 dir(0) = $f7 $ff $73 $00
    //Sparkle 2.1 dir(0) = $77 $7f $63 $00 

    DirTrack = 18;
    DirSector = 17;
    if (((Disk[Track[DirTrack] + (DirSector * 256) + 0] == 0xf7) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 255] == 0xff) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 254] == 0x73) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 253] == 0x00)) ||
        ((Disk[Track[DirTrack] + (DirSector * 256) + 0] == 0x77) &&
            (Disk[Track[DirTrack] + (DirSector * 256) + 255] == 0x7f) &&
            (Disk[Track[DirTrack] + (DirSector * 256) + 254] == 0x63) &&
            (Disk[Track[DirTrack] + (DirSector * 256) + 253] == 0x00)))
    {
        //Sparkle 2.0 or Sparkle 2.1+ disk -> mark 18:17 and 18:18 off (internal directory sectors)
        MarkSectorAsUsed(18, 17);
        MarkSectorAsUsed(18, 18);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool OpenOutFile()
{
    if (OutputType == "png")
    {
        CreateDisk();           //PNG output - creating a virtual D64 first from the input file which will then be converted to PNG
    }
    else
    {
        DiskSize = ReadBinaryFile(OutFileName, Disk);   //Load the output file if it exists

        if (DiskSize < 0)
        {
            //Output file doesn't exits, create an empty D64
            CreateDisk();
        }
        else if ((DiskSize != StdDiskSize) && (DiskSize != ExtDiskSize))    //Otherwise make sure the output disk is the correct size
        {
            cerr << "***CRITICAL***\t Invalid output disk file size!\n";
            return false;
        }

        FixSparkleBAM();        //Earlier Sparkle versions didn't mark DirArt and internal directory sectors off in the BAM
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
        cerr << "***CRITICAL***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }
    else if ((DA.size() != StdDiskSize) && (DA.size() != ExtDiskSize))
    {
        cerr << "***CRITICAL***\tInvalid D64 DirArt file size: " << dec << DA.size() << " byte(s)\n";
        return false;
    }

    size_t T = 18;
    size_t S = 1;

    bool DirFull = false;
    size_t DAPtr{};

    while ((!DirFull) && (T != 0))
    {
        DAPtr = Track[T] + (S * 256);
        for (int b = 2; b < 256; b += 32)
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
                        return true;
                    }
                }
            }
        }
        T = DA[DAPtr];
        S = DA[DAPtr + 1];
    }

    if (DA[Track[18] + 0x90] != 0xa0)   //Import directory header only if one exists in input disk image
    {
        for (int i = 0; i < 16; i++)
        {
            Disk[Track[18] + 0x90 + i] = DA[Track[18] + 0x90 + i];
        }
    }
    else if (!argDiskName.empty())         //Otherwise, check if disk name is specified in command line
    {
        for (int i = 0; i < 16; i++)
        {
            Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(argDiskName[i])];
        }
    }

    if (DA[Track[18] + 0xa2] != 0xa0)    //Import directory ID only if one exists in input disk image
    {
        for (int i = 0; i < 5; i++)
        {
            Disk[Track[18] + 0xa2 + i] = DA[Track[18] + 0xa2 + i];
        }
    }
    else if (!argDiskID.empty())           //Otherwise, check if disk ID is specified in command line
    {
        for (int i = 0; i < 5; i++)
        {
            Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(argDiskID[i])];
        }
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
        cerr << "***CRITICAL***\tUnable to open the following DirArt file: " << InFileName << "\n";
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
        cerr << "***CRITICAL***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }
    else if (DA.size() == 0)
    {
        cerr << "***CRITICAL***\t The DirArt file is 0 bytes long.\n";
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
        cerr << "***CRITICAL***\tUnable to open the following DirArt file: " << InFileName << "\n";
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
    string delimiter = "\"";        // = " (entry gets split at quotation mark) NOT A BUG, DO NOT CHANGE THIS
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
                    //else
                    //{
                    //    FileType = 0x80;
                    //}
                }
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

        if (!OpenOutFile())
            return false;

        if (AppendMode)
        {
            if (!FindLastUsedDirPos())
                return false;
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
        for (size_t i = 0; i < 16; i++)
        {
            DirHeader += 0xa0;
            Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(DirHeader[i])];
        }
    }
    else if (!argDiskName.empty())
    {
        for (size_t i = 0; i < 16; i++)
        {
            Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(argDiskName[i])];
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
        for (size_t i = 0; i < 5; i++)
        {
            DirID += 0xa0;
            Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(DirID[i])];
        }
    }
    else if (!argDiskID.empty())
    {
        for (size_t i = 0; i < 5; i++)
        {
            Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(argDiskID[i])];
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
        cerr << "***CRITICAL***\tUnable to open the following DirArt file: " << InFileName << "\n";
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
        cerr << "***CRITICAL***\tUnable to open the following .PET DirArt file: " << InFileName << "\n";
        return false;
    }
    else if (PetFile.size() == 0)
    {
        cerr << "***CRITICAL***\t This .PET DirArt file is 0 bytes long: " << InFileName << "\n";
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
        cerr << "***CRITICAL***\tUnable to open the following .JSON DirArt file: " << InFileName << "\n";
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

unsigned int GetPixel(size_t X, size_t Y)
{
    size_t Pos = Y * ((size_t)ImgWidth * 4) + (X * 4);

    unsigned char R = Image[Pos + 0];
    unsigned char G = Image[Pos + 1];
    unsigned char B = Image[Pos + 2];
    unsigned char A = Image[Pos + 3];

    unsigned int Col = (R * 0x1000000) + (G * 0x10000) + (B * 0x100) + A;

    return Col;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool IdentifyColors()
{
    unsigned int Col1 = GetPixel(0, 0);  //The first of two allowed colors per image
    unsigned int Col2 = Col1;

    //First find two colors (Col1 is already assigned to pixel(0,0))
    for (size_t y = 0; y < ImgHeight; y++)  //Bug fix: check every single pixel, no just one in every Mplr-sized square
    {
        for (size_t x = 0; x < ImgWidth; x++)
        {
            unsigned int ThisCol = GetPixel(x, y);
            if ((ThisCol != Col1) && (Col2 == Col1))
            {
                Col2 = ThisCol;
            }
            else if ((ThisCol != Col1) && (ThisCol != Col2))
            {
                cerr << "***CRITICAL***\tThis image contains more than two colors.\n";
                return false;
            }
        }
    }

    if (Col1 == Col2)
    {
        cerr << "***CRITICAL***Unable to determine foreground and background colors in DirArt image file.\n";
        return false;
    }

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
    int R1 = Col1 / 0x1000000;
    int G1 = (Col1 & 0xff0000) / 0x10000;
    int B1 = (Col1 & 0x00ff00) / 0x100;
    int R2 = Col2 / 0x1000000;
    int G2 = (Col2 & 0xff0000) / 0x10000;
    int B2 = (Col2 & 0x00ff00) / 0x100;

    double C1 = sqrt((0.21 * R1 * R1) + (0.72 * G1 * G1) + (0.07 * B1 * B1));
    double C2 = sqrt((0.21 * R2 * R2) + (0.72 * G2 * G2) + (0.07 * B2 * B2));
    //Another formula: sqrt(0.299 * R ^ 2 + 0.587 * G ^ 2 + 0.114 * B ^ 2)

    BGCol = (C1 < C2) ? Col1 : Col2;
    FGCol = (BGCol == Col1) ? Col2 : Col1;

    if (BGCol == FGCol)
    {
        cerr << "***CRITICAL***Unable to determine foreground and background colors in DirArt image file.\n";
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
        cerr << "***CRITICAL***\tThe size of this BMP file is smaller than the minimum size allowed.\n";
        return false;
    }

    memcpy(&BmpInfoHeader, &ImgRaw[BIH], sizeof(BmpInfoHeader));

    if (BmpInfoHeader.biWidth % 128 != 0)
    {
        cerr << "***CRITICAL***\tUnsupported BMP size. The image must be 128 pixels (16 chars) wide or a multiple of it if resized.\n";
        return false;
    }

    if ((BmpInfoHeader.biCompression != 0) && (BmpInfoHeader.biCompression != 3))
    {
        cerr << "***CRITICAL***\tUnsupported BMP format. Sparkle can only work with uncompressed BMP files.\nT";
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
        cerr << "***CRITICAL***\tCorrupted BMP file size.\n";

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

bool ImportFromImage()
{
    ImgRaw.clear();
    Image.clear();

    if (ReadBinaryFile(InFileName, ImgRaw) == -1)
    {
        cerr << "***CRITICAL***\tUnable to open image DirArt file.\n";
        return false;
    }
    else if (ImgRaw.size() == 0)
    {
        cerr << "***CRITICAL***\tThe DirArt file cannot be 0 bytes long.\n";
        return false;
    }

    if (DirArtType == "png")
    {
        //Load and decode PNG image using LodePNG (Copyright (c) 2005-2023 Lode Vandevenne)
        unsigned int error = lodepng::decode(Image, ImgWidth, ImgHeight, ImgRaw);

        if (error)
        {
            cout << "***CRITICAL***\tPNG decoder error: " << error << ": " << lodepng_error_text(error) << "\n";
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

    if (ImgWidth % 128 != 0)
    {
        cerr << "***CRITICAL***\tUnsupported image size. The image must be 128 pixels (16 chars) wide or a multiple of it if resized.\n";
        return false;
    }

    Mplr = ImgWidth / 128;

    if (!IdentifyColors())
    {
        return false;
    }

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

                            size_t CharSetPos = (py * 8 * 128) + (y * 128) + (px * 8) + x;

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
    cout << "DART is a simple command-line tool that imports directory art from a variety of source file types to D64 disk images.\n\n";

    cout << "Usage:\n";
    cout << "------\n";
    cout << "dart input -o [output.d64/output.png] -n [\"disk name\"] -i [\"disk id\"] -s [skipped entries] -t [default entry type]\n";
    cout << "           -f [first imported entry] - l [last imported entry]\n\n";

    cout << "input - the file from which the directory art will be imported. See accepted file types below.\n\n";

    cout << "-o [output.d64/output.png] - the D64/PNG file to which the directory art will be imported. This parameter is optional\n";
    cout << "       and it will be ignored if it is used with KickAss ASM input files that include the 'filename' disk parameter.\n";
    cout << "       If an output is not specified then DART will create an input_out.d64 file. If the output file is a PNG then\n";
    cout << "       DART will create a PNG \"screenshot\" of the directory listing instead of a D64 file. If there are less than 23\n";
    cout << "       entries then the ouput PNG's size will be 384 x 272 pixels (same as a VICE screenshot). If there are at least\n";
    cout << "       23 entries then the width will be 384 pixels and the height will be ((n + 3) * 8) + 72 pixels. The -s option\n";
    cout << "       will be ignored if the output is PNG (you can't append entries to an existing PNG).\n\n";
        
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

    cout << "-l [last imported entry] - a numeric value (1-based which DART uses to determine the last DirArt entry to be imported.\n";
    cout << "       This parameter is optional. If not specified, DART will finish import after the last DirArt entry.\n\n";

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
    cout << "       of the two colors as background color. Image width must be exactly 16 characters (128 pixels or its multiples\n";
    cout << "       if the image is resized) and the height must be a multiple of 8 pixels. Borders are not allowed. Image files\n";
    cout << "       must display the uppercase charset and their appearance cannot rely on command characters. DART uses the\n";
    cout << "       LodePNG library by Lode Vandevenne to decode PNG files.\n\n";

    cout << "BMP  - Bitmap image file. Same rules and limitations as with PNGs.\n\n";

    cout << "Any other file type will be handled as a binary file and will be treated as PRGs without the first two header bytes.\n\n";

    cout << "Example 1:\n";
    cout << "----------\n\n";

    cout << "dart MyDirArt.pet\n\n";

    cout << "DART will create MyDirArt_out.d64 (if it doesn't already exist), and will import all DirArt entries from MyDirArt.pet\n";
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
    cout << "of MyDemo.d64 (keeping all already existing entries in it), using del as (default) entry type.\n";
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    cout << "\n";
    cout << "*********************************************************\n";
    cout << "DART 1.4 - Directory Art Importer by Sparta (C) 2022-2024\n";
    cout << "*********************************************************\n";
    cout << "\n";

    if (argc == 1)
    {

    #ifdef DEBUG
        InFileName = "c:/dart/test/Propaganda34.d64";
        OutFileName = "c:/dart/test/d64test.png";
        argSkippedEntries = "all";
        argEntryType = "del";
    #else
        cout << "Usage: dart input [options]\n";
        cout << "options:    -o <output.d64> or <output.png>\n";
        cout << "            -n <\"disk name\">\n";
        cout << "            -i <\"disk id\">\n";
        cout << "            -s <skipped entries in output.d64>\n";
        cout << "            -t <default entry type>\n";
        cout << "            -f <first imported entry>\n";
        cout << "            -l <last imported entry>\n\n";
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
                cerr << "***CRITICAL***\tMissing [output.d64] parameter.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-n") || (args[i] == "-N"))       //disk name in output D64
        {
            if (i + 1 < argc)
            {
                argDiskName = args[++i];

                for (int j = 0; j < 16; j++)
                {
                    argDiskName += 0xa0;
                }
            }
            else
            {
                cerr << "***CRITICAL***\tMissing [disk name] parameter.\n";
                return EXIT_FAILURE;
            }
        }
        else if ((args[i] == "-i") || (args[i] == "-I"))       //disk ID in output D64
        {
            if (i + 1 < argc)
            {
                argDiskID = args[++i];

                for (int j = 0; j < 5; j++)
                {
                    argDiskID += 0xa0;
                }
            }
            else
            {
                cerr << "***CRITICAL***\tMissing [disk id] parameter.\n";
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
                cerr << "***CRITICAL***\tMissing [skipped entries] parameter.\n";
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
                cerr << "***CRITICAL***\tMissing [default entry type] parameter.\n";
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
                cerr << "***CRITICAL***\tMissing [first imported entry] parameter.\n";
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
                cerr << "***CRITICAL***\tMissing [last imported entry] parameter.\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            cerr << "***CRITICAL***\tUnrecognized option: " << args[i] << "\n";
            return EXIT_FAILURE;
        }
        i++;
    }
    
    if (InFileName.empty())
    {
        cerr << "***CRITICAL***\tMissing input file name!\n";
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
            cerr << "***CRITICAL***\tUnrecognized [skipped entries] parameter!\n";
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
        cerr << "***CRITICAL***\tUnrecognized default file type parameter!\n";
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
            cerr << "***CRITICAL***\tThe [first imported entry] parameter must be numeric.\n";
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
            cerr << "***CRITICAL***\tThe [last imported entry] parameter must be numeric.\n";
            return EXIT_FAILURE;
        }
    }
    else
    {
        argLastImportedEntry = "last";
    }

    if (!fs::exists(InFileName))
    {
        cerr << "***CRITICAL***The following DirArt file does not exist:\n\n" << InFileName << "\n";
        return EXIT_FAILURE;
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
        OutFileName = InFileName.substr(0, InFileName.size() - DirArtType.size() - 1) + "_out.d64";
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

    //if (OutputType == "d64")
    //{
        if (!OpenOutFile())
        {
            return EXIT_FAILURE;
        }
    //}

    string Msg = "";

    if ((FirstImportedEntry != 1) || (LastImportedEntry != 0xffff))
    {
        Msg = "Importing DirArt entries: " + argFirstImportedEntry + " through " + argLastImportedEntry + "\n";
    }
    else
    {
        Msg = "Importing DirArt entries: all\n";
    }

    if (AppendMode)
    {
        Msg += "Import mode: Append, skipping all existing directory entries in " + OutFileName + "\n";
        
        //Let's find the last used dir entry slot, we will continue with the next, empty one
        if (!FindLastUsedDirPos())
            return EXIT_FAILURE;
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

        if (!ResetDirEntries())
            return EXIT_FAILURE;
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
            for (int i = 0; i < 16; i++)
            {
                Disk[Track[18] + 0x90 + i] = Ascii2DirArt[toupper(argDiskName[i])];
            }
        }

        if (!argDiskID.empty())
        {
            for (int i = 0; i < 5; i++)
            {
                Disk[Track[18] + 0xa2 + i] = Ascii2DirArt[toupper(argDiskID[i])];
            }
        }
    }

    int NumFreeSectors = Disk[Track[DirTrack] + (size_t)(18 * 4)];
    NumFreeEntries = NumFreeSectors * 8;

    if (DirPos != 0)
    {
        for (int i = DirPos + 32; i < 256; i += 32)
        {
            NumFreeEntries++;
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
        if (!ConvertD64ToPng())
        {
            return EXIT_FAILURE;
        }
    }

    cout << dec << NumFreeEntries << " directory entries remaining unused on track 18.\n";

    return EXIT_SUCCESS;
}
    

