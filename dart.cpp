
//#define DEBUG

#include "common.h"

const int StdDiskSize = (664 + 19) * 256;
const int ExtDiskSize = StdDiskSize + (85 * 256);

vector <unsigned char> Disk;
vector <unsigned char> Image;   //pixels in RGBA format (4 bytes per pixel)
vector <unsigned char> ImgRaw;

string InFileName = "";
string OutFileName = "";
string DirEntry = "";
string DirArt = "";
string DirArtType = "";

bool DirEntryAdded = false;
bool AppendDir = false;

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

string ConvertIntToHextString(const int& i, const int& hexlen)
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
        cout << "Writing " + OutFileName + "...\n";
        myFile.write((char*)&Disk[0], DiskSize);
        cout << "Done!\n";
        return true;
    }
    else
    {
        cerr << "***CRITICAL***\tError during writing disk image: " << DiskName << "\n\n";
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

    return length;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

string ReadFileToString(const string& FileName, bool CorrectFilePath = false)
{

    if (!fs::exists(FileName))
    {
        return "";
    }

    ifstream t(FileName);

    if (t.fail())
    {
        return "";
    }

    string str;

    t.seekg(0, ios::end);
    str.reserve(t.tellg());
    t.seekg(0, ios::beg);

    str.assign((istreambuf_iterator<char>(t)), istreambuf_iterator<char>());

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

    return str;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void DeleteBit(size_t T, size_t S)
{

    size_t NumSectorPtr = Track[18] + (T * 4) + ((T > 35) ? 7 * 4 : 0);
    size_t BitPtr = NumSectorPtr + 1 + (S / 8);        //BAM Position for Bit Change
    unsigned char BitToDelete = 1 << (S % 8);       //BAM Bit to be deleted

    if ((Disk[BitPtr] & BitToDelete) != 0)
    {
        Disk[BitPtr] &= (255 - BitToDelete);

        unsigned char NumUnusedSectors = 0;

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
        Disk[NumSectorPtr] = NumUnusedSectors;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void FindNextEmptyDirSector()
{

    LastDirSector = DirSector;

    DirSector = 1;

    while (DirSector < 19)
    {
        //Check in BAM if this is really a free sector
        size_t NumSectorPtr = Track[18] + (DirTrack * 4);
        size_t BitPtr = NumSectorPtr + 1 + (DirSector / 8);      //BAM Position for Bit Change
        unsigned char BitToCheck = 1 << (DirSector % 8);      //BAM Bit to be deleted

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
        DirSector += 1;
    }
    
    //Track 18 is full, no more empty sectors
    DirSector = 0;
    DirPos = 0;
    cout << "***INFO***\tDirectory is full!\n";
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
        if (DirPos > 256)
        {
            if(Disk[Track[DirTrack] + (DirSector * 256) + 0] != 0)
            {
                //We are overwriting existing dir entries in the T:S chain
                unsigned char T = Disk[Track[DirTrack] + (DirSector * 256) + 0];
                unsigned char S = Disk[Track[DirTrack] + (DirSector * 256) + 1];
                if ((T == DirTrack) && (S == 0))
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + 0] = 0;
                    Disk[Track[DirTrack] + (DirSector * 256) + 1] = 255;
                    FindNextEmptyDirSector();

                }
                else
                {
                    DirTrack = T;
                    DirSector = S;
                    DirPos = 2;
                    T = Disk[Track[DirTrack] + (DirSector * 256) + 0];
                    S = Disk[Track[DirTrack] + (DirSector * 256) + 1];
                    if ((T == DirTrack) && (S == 0))
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + 0] = 0;
                        Disk[Track[DirTrack] + (DirSector * 256) + 1] = 255;
                    }
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
        DeleteBit(DirTrack, DirSector);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM D64
//----------------------------------------------------------------------------------------------------------------------------------------------------

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
                FindNextDirPos();

                if (DirPos != 0)
                {
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                    {
                        //File type and T:S are not yet defined
                        for (int i = 0; i < (16 + 3); i++)     //Include file type and T:S
                        {
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + i] = DA[DAPtr + b + i];
                        }
                    }
                    else
                    {
                        //File type and T:S are already defined
                        for (int i = 0; i < 16; i++)        //Do not include file type and T:S
                        {
                            Disk[Track[DirTrack] + (DirSector * 256) + DirPos + i + 3] = DA[DAPtr + b + i + 3];
                        }
                    }
                }
                else
                {
                    //DirFull = true;
                    //break;
                    return true;
                }
            }
        }
        T = DA[DAPtr];
        S = DA[DAPtr + 1];
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM TXT
//----------------------------------------------------------------------------------------------------------------------------------------------------

void AddDirEntry(string DirEntry) {

    //Define file type and T:S if not yet denifed
    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
    {
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = 0x80;      //"DEL"
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;        //Track 18
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;         //Sector 0
    }

    //Remove vbNewLine characters and add 16 SHIFT+SPACE tail characters
    for (int i = 0; i < 16; i++)
    {
        DirEntry += 0xa0;
    }

    //Copy only the first 16 characters of the edited DirEntry to the Disk Directory
    for (int i = 0; i < 16; i++)
    {
        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Ascii2DirArt[toupper(DirEntry[i])];    //=toupper(DirEntry[i]);
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromTxt()
{

    string DirArt = ReadFileToString(InFileName);

    if (DirArt == "")
    {
        cerr << "***CRITICAL***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }

    string delimiter = "\n";

    while (DirArt.find(delimiter) != string::npos)
    {
        string DirEntry = DirArt.substr(0, DirArt.find(delimiter));
        DirArt.erase(0, DirArt.find(delimiter) + delimiter.length());
        FindNextDirPos();
        if (DirPos != 0)
        {
            AddDirEntry(DirEntry);
        }
        else
        {
            return true;
        }
    }

    if (DirArt != "")
    {
        FindNextDirPos();
        if (DirPos != 0)
        {
            AddDirEntry(DirArt);
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
            //if ((DAPtr == EntryStart) && (DAPtr == DA.size()))   //BUG FIX - accept empty lines, i.e. treat each 0xa0 chars as line ends
            //{
            //    break;
            //}
            //else
            //{
                FindNextDirPos();

                if (DirPos != 0)
                {
                    //Define file type and T:S if not yet denifed
                    if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                    {
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = 0x80;      //"DEL"
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;        //Track 18
                        Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;         //Sector 0
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

                while ((DAPtr < DA.size()) && (DAPtr < EntryStart + 39) && (DA[DAPtr] != 0xa0))
                {
                    DAPtr++;        //Find last char of screen row (start+39) or first $a0-byte if sooner
                }
            //}
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
        if (DirEntry.substr(0, EntryStart) != "")
        {
            Entry[NumEntries++] = DirEntry.substr(0, EntryStart);
        }

        DirEntry.erase(0, EntryStart + delimiter.length());

        if (NumEntries == RowLen)   //Max row length reached, ignore rest of the char row
        {
            break;
        }
    }

    if ((NumEntries < 16) && (DirEntry != ""))
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

    if (NumEntries == RowLen)       //Ignore rows that are too short (e.g.
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

            FindNextDirPos();

            if (DirPos != 0)
            {
                //Define file type and T:S if not yet denifed
                if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = 0x80;      //"DEL"
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;        //Track 18
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;         //Sector 0
                }

                for (size_t i = 0; i < 16; i++)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 3 + i] = Petscii2DirArt[bEntry[i]];
                }

                DirEntryAdded = true;

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

bool ImportFromCArray() {

    string DA = ReadFileToString(InFileName);

    if (DA == "")
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

    size_t First = DA.find("{");
    //size_t Last = DA.find("}");

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

void AddAsmDirEntry(string DirEntry) {

    string EntrySegments[5];
    string delimiter = "\"";        // = " (entry gets split at quotation mark) NOT A BUG, DO NOT CHANGE THIS
    int NumSegments = 0;

    for (int i = 0; i < 5; i++)
    {
        EntrySegments[i] = "";
    }

    while ((DirEntry.find(delimiter) != string::npos) && (NumSegments < 4))
    {
        EntrySegments[NumSegments++] = DirEntry.substr(0, DirEntry.find(delimiter));
        DirEntry.erase(0, DirEntry.find(delimiter) + delimiter.length());
    }
    EntrySegments[NumSegments++] = DirEntry;

    //EntrySegments[0] = '[name =' OR '[name = @'
    //EntrySegments[1] = 'text' OR '\$XX\$XX\$XX'
    //EntrySegments[2] = 'type ='
    //EntrySegments[3] = 'del' OR 'prg' etc.
    //EntrySegments[4] = ']'

    if (NumSegments > 1)
    {
        if ((EntrySegments[0].find("[") != string::npos) && (EntrySegments[0].find("name") != string::npos) && (EntrySegments[0].find("=") != string::npos))
        {
            unsigned char FileType = 0x80;  //DEL default file type

            if (NumSegments > 3)
            {
                if ((EntrySegments[2].find("type") != string::npos) && (EntrySegments[2].find("=") != string::npos))
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

            if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
            {
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = FileType;
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;            //Track 18
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;             //Sector 0
            }

            if (EntrySegments[0].find("@") != string::npos)
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
                    if (ThisValue != "")
                    {
                        Values[NumValues++] = ThisValue;
                    }
                    EntrySegments[1].erase(0, EntrySegments[1].find(delimiter) + delimiter.length());
                }
                Values[NumValues++] = EntrySegments[1];

                int Idx = 0;

                for (int v = 0; v < NumValues; v++)
                {
                    if (Values[v] != "")
                    {
                        unsigned char NextChar = 0x20;          //SPACE default char - if conversion is not possible
                        if (Values[v].find("$") == 0)
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
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromAsm()
{

    DirArt = ReadFileToString(InFileName);

    if (DirArt == "")
    {
        cerr << "***CRITICAL***\tUnable to open the following DirArt file: " << InFileName << "\n";
        return false;
    }

    //Convert the whole string to lower case for easier processing
    for (size_t i = 0; i < DirArt.length(); i++)
    {
        DirArt[i] = tolower(DirArt[i]);
    }

    string delimiter = "\n";

    while (DirArt.find(delimiter) != string::npos)
    {
        string DirEntry = DirArt.substr(0, DirArt.find(delimiter));
        DirArt.erase(0, DirArt.find(delimiter) + delimiter.length());
        FindNextDirPos();
        if (DirPos != 0)
        {
            AddAsmDirEntry(DirEntry);   //Convert one line at the time
        }
        else
        {
            return true;
        }
    }

    if (DirArt != "")
    {
        FindNextDirPos();
        if (DirPos != 0)
        {
            AddAsmDirEntry(DirArt);
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM PET
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ConvertPetToDirArt()
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
        FindNextDirPos();
        if (DirPos != 0)
        {
            //Define file type and T:S if not yet denifed
            if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
            {
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = 0x80;      //"DEL"
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;        //Track 18
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;         //Sector 0
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

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
//  IMPORT FROM JSON
//----------------------------------------------------------------------------------------------------------------------------------------------------

bool ImportFromJson()
{
    DirArt = ReadFileToString(InFileName);

    if (DirArt == "")
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
        W = W.substr(W.find(":") + 1);
        W = W.substr(0, W.find(","));
        while (W.find(" ") != string::npos)
        {
            //Remove space characters if there's any
            W.replace(W.find (" "),1,"");
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
        H = H.substr(H.find(":") + 1);
        H = H.substr(0, H.find(","));
        while (H.find(" ") != string::npos)
        {
            //Remove space characters if there's any
            H.replace(H.find(" "), 1, "");
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
        S = S.substr(S.find("[") + 1);
        S = S.substr(0, S.find("]"));
        while (S.find(" ") != string::npos)
        {
            //Remove space characters if there's any
            S.replace(S.find(" "), 1, "");
        }

        size_t p = 0;

        while (S.find(",") != string::npos)
        {
            string B = S.substr(0, S.find(","));
            S.erase(0, S.find(",") + 1);
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
            FindNextDirPos();
            
            if (DirPos != 0)
            {
                //Define file type and T:S if not yet denifed
                if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
                {
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = 0x80;  //"DEL"
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;    //Track 18
                    Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;     //Sector 0
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
                return true;
            }
        }
    }

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

    unsigned int Col = (R * 0x1000000) + (G * 0x10000) + (B * 0100) + A;

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
    size_t DataOffset = (size_t)ImgRaw[DATA_OFFSET] + (size_t)(ImgRaw[DATA_OFFSET + 1] * 0x100) + (size_t)(ImgRaw[DATA_OFFSET + 2] * 0x10000) + (size_t)(ImgRaw[DATA_OFFSET + 3] * 01000000);

    //Calculate length of pixel rows in bytes
    size_t RowLen = ((size_t)BmpInfo->bmiHeader.biWidth * (size_t)BmpInfo->bmiHeader.biBitCount) / 8;

    //BMP pads pixel rows to a multiple of 4 in bytes
    size_t PaddedRowLen = (RowLen % 4) == 0 ? RowLen : RowLen - (RowLen % 4) + 4;
    //size_t PaddedRowLen = ((RowLen + 3) / 4) * 4;

    size_t CalcSize = DataOffset + (BmpInfo->bmiHeader.biHeight * PaddedRowLen);

    if (ImgRaw.size() != CalcSize)
    {
        cerr << "***CRITICAL***\tCorrupted BMP file size.\n";
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


        FindNextDirPos();

        if (DirPos != 0)
        {
            if (Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] == 0)
            {
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 0] = 0x80;  //"DEL"
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 1] = 18;    //Track 18
                Disk[Track[DirTrack] + (DirSector * 256) + DirPos + 2] = 0;     //Sector 0
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
    }//Next cy

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void FindLastUsedDirPos()
{
    //First find the last used sector
    while ((Disk[Track[DirTrack] + (DirSector * 256) + 0] == 18) && (Disk[Track[DirTrack] + (DirSector * 256) + 1] != 0) && (Disk[Track[DirTrack] + (DirSector * 256) + 1] != 255))
    {
        unsigned char S = Disk[Track[DirTrack] + (DirSector * 256) + 1];

        //DirTrack = T        'DART assumes that we keep the directory on track 18...
        DirSector = S;

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
            DirPos = 0;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------

void FixSparkleBAM()
{
    //Earlier versions of Sparkle disks don't mark DirArt sectors as "used". So let's follow the directory block chain and mark them off
    DirTrack = 18;
    DirSector = 1;

    while (Disk[Track[DirTrack] + (DirSector * 256)] != 0)
    {
        DeleteBit(DirTrack, DirSector);
        int NextT = Disk.at(Track[DirTrack] + (DirSector * 256));
        int NextS = Disk.at(Track[DirTrack] + (DirSector * 256) + 1);

        if (NextT == 18)
        {
            if (NextS == 0)
            {
                break;
            }
            DirTrack = NextT;
            DirSector = NextS;
        }
    }
    //Also mark last sector as used and correct first two bytes of last sector
    DeleteBit(DirTrack, DirSector);
    Disk[Track[DirTrack] + (DirSector * 256) + 0] = 0;
    Disk[Track[DirTrack] + (DirSector * 256) + 1] = 0xff;

    //Sparkle 2 and 2.1 disks don't mark the internal directory sectors (18:17 and 18:18) off, let's fix it.
    //Sparkle 2.0 dir(0) = $f7 $ff $73 $00
    //Sparkle 2.1 dir(0) = $77 $7f $63 $00 
    
    DirTrack = 18;
    DirSector = 17;
    if(((Disk[Track[DirTrack] + (DirSector * 256) + 0] == 0xf7) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 255] == 0xff) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 254] == 0x73) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 253] == 0x00)) ||
        ((Disk[Track[DirTrack] + (DirSector * 256) + 0] == 0x77) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 255] == 0x7f) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 254] == 0x63) &&
        (Disk[Track[DirTrack] + (DirSector * 256) + 253] == 0x00)))
    {
        //Sparkle 2.0 or Sparkle 2.1+ disk -> mark 18:17 and 18:18 off (internal directory sectors)
        DeleteBit(18, 17);
        DeleteBit(18, 18);
    }
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

//----------------------------------------------------------------------------------------------------------------------------------------------------

void ShowInfo()
{
    cout << "DART is a simple command line tool that imports directory art from a variety of source file types to D64 disk images.\n\n";
    cout << "Usage:\n";
    cout << "------\n";
    cout << "dart input[+] [output.d64]\n\n";
    cout << "By default, DART will overwrite any existing directory entries in output.d64 without modifying the entries' type.\n";
    cout << "To append the imported DirArt to the existing directory entries without overwriting them, mark the input file with a\n";
    cout << "plus sign (e.g., input.d64+, see example below). Thus, multiple DirArts can be imported in the same D64 as long as\n";
    cout << "there is space on track 18.\n\n";
    cout << "The [output.d64] parameter is optional. If not specified, DART will create an input_out.d64 file.\n\n";
    cout << "Accepted input file types:\n";
    cout << "--------------------------\n";
    cout << "D64  - DART will import all directory entries from the input file to the output file.\n\n";
    cout << "PRG  - DART accepts two file formats: screen RAM grabs (40-byte long char rows of which the first 16 are used as dir\n";
    cout << "       entries, can be more than 25 char rows long) and $a0 byte terminated directory entries*. If an $a0 byte is not\n";
    cout << "       detected sooner, then 16 bytes are imported per directory entry. DART then skips up to 24 bytes or until an\n";
    cout << "       $a0 byte detected. Example source code for $a0-terminated .PRG (KickAss format, must be compiled):\n\n";
    cout << "           * = $1000              // Address can be anything, will be skipped by DART\n";
    cout << "           .text \"hello world!\"   // This will be upper case once compiled\n";
    cout << "           .byte $a0              // Terminates directory entry\n";
    cout << "           .byte $30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$01,$02,$03,$04,$05,$06,$a0\n\n";
    cout << "       *Char code $a0 (inverted space) is also used in standard directories to mark the end of entries.\n\n";
    cout << "BIN  - DART will treat this file type the same way as PRGs, without the first two header bytes.\n\n";
    cout << "ASM  - KickAss ASM DirArt source file. Please refer to Chapter 11.6 in the Kick Assembler Reference Manual for\n";
    cout << "       details. The ASM file must only contain the file parameters within [] brackets, without disk parameters.\n";
    cout << "       DART only recognizes the name and type file parameters. Example:\n\n";
    cout << "       [name = \"0123456789\", type = \"rel\"],\n";
    cout << "       [name = @\"\\$75\\$69\\$75\\$69\\$B2\\$69\\$75\\$69\\$75\\$ae\\$B2\\$75\\$AE\\$20\\$20\\$20\", type=\"del\"]\n\n";
    cout << "C    - Marq's PETSCII Editor C array file. This file type can also be produced using Petmate. This is essentially a\n";
    cout << "       C source file which consists of a single unsigned char array declaration initialized with the dir entries.\n";
    cout << "       If present, DART will use the META: comment after the array to determine the dimensions of the DirArt.\n\n";
    cout << "PET  - This format is supported by Marq's PETSCII Editor and Petmate. DART will use the first two bytes of the input\n";
    cout << "       file to determine the dimensions of the DirArt, but it will ignore the next three bytes (border and background\n";
    cout << "       colors, charset) as well as color RAM data. DART will import max. 16 chars per directory entry.\n\n";
    cout << "JSON - This format can be created using Petmate. DART will import max. 16 chars from each character row.\n\n";
    cout << "PNG  - Portable Network Graphics image file. Image input files can only use two colors (background and foreground).\n";
    cout << "       DART will try to identify the colors by looking for a space character. If none found, it will use the darker\n";
    cout << "       of the two colors as background color. Image width must be exactly 16 characters (128 pixels or its multiples\n";
    cout << "       if the image is resized) and the height must be multiples of 8 pixels. Borders are not allowed. Image files\n";
    cout << "       must display the uppercase charset and their appearance cannot rely on command characters.\n\n";
    cout << "BMP  - Bitmap image file. Same rules and limitations as with PNGs.\n\n";
    cout << "Any other file type will be handled as a binary file and will be treated as PRGs without the first two header bytes.\n\n";
    cout << "Limitations:\n";
    cout << "------------\n";
    cout << "DART doesn't import the directory's header and ID. The input file must only consist of the directory entries (with\n";
    cout << "the exception of D64 files where the directory header and ID will be ignored).\n\n";
    cout << "Example 1:\n";
    cout << "----------\n\n";
    cout << "dart MyDirArt.d64\n\n";
    cout << "DART will create MyDirArt_out.d64 (if it doesn't already exist), and will import the whole directory of MyDirArt.d64\n";
    cout << "in it overwriting any preexisting directory entries.\n\n";
    cout << "Example 2:\n";
    cout << "----------\n\n";
    cout << "dart MyDirArt.c MyDemo.d64\n\n";
    cout << "DART will import the DirArt from a Marq's PETSCII Editor C array file in MyDemo.d64 overwriting any preexisting\n";
    cout << "directory entries.\n\n";
    cout << "Example 3:\n";
    cout << "----------\n\n";
    cout << "dart MyDirArt.asm+ MyDemo.d64\n\n";
    cout << "DART will append the DirArt from a KickAss ASM source file to the existing directory entries of MyDemo.d64.\n";
}

int main(int argc, char* argv[])
{
    cout << "\n";
    cout << "*********************************************************\n";
    cout << "DART 1.2 - Directory Art Importer by Sparta (C) 2022-2023\n";
    cout << "*********************************************************\n";
    cout << "\n";

    if (argc == 1)
    {

    #ifdef DEBUG
        InFileName = "c:\\Users\\Tamas\\OneDrive\\C64\\C++\\dart\\test\\Bin2.bin+";
    #else

        cout << "Usage: dart input[*] [output.d64]\n\n";
        cout << "Help:  dart -?\n";
        return EXIT_SUCCESS;

    #endif

    }
    else if (argc == 2)
    {
        InFileName = argv[1];
    }
    else
    {
        InFileName = argv[1];
        OutFileName = argv[2];
    }

    if (InFileName == "")
    {
        cerr << "***CRITICAL***\tMissing input file name!\n";
        return EXIT_FAILURE;
    }

    if (InFileName == "-?")
    {
        ShowInfo();
        return EXIT_SUCCESS;
    }

    if (InFileName.at(InFileName.size() - 1) == '+')
    {
        AppendDir = true;
        InFileName = InFileName.substr(0, InFileName.size() - 1);
    }

    if (!fs::exists(InFileName))
    {
        cerr << "***CRITICAL***The following DirArt file does not exist:\n\n" << InFileName << "\n";
        return EXIT_FAILURE;
    }

    CreateTrackTable();
    
    //Find the input file's extension
    int ExtStart = InFileName.length();

    for (int i = InFileName.length() - 1; i >= 0; i--)
    {
#if _WIN32
        if ((InFileName[i] == '\\') || (InFileName[i] == '/'))
        {
            //We've reached a path separator before a "." -> no extension
            break;
        }
        else if (InFileName[i] == '.')
        {
            ExtStart = i + 1;
            break;
        }
#elif __APPLE__ || __linux__
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
#endif
    }

    for (size_t i = ExtStart; i < InFileName.length(); i++)
    {
        DirArtType += tolower(InFileName[i]);
    }

    if (OutFileName == "")
    {
        //Output file name not provided, use input file name without its extension to create output d64 file name
        OutFileName = InFileName.substr(0, InFileName.size() - DirArtType.size() - 1) + "_out.d64";
    }

    CorrectFilePathSeparators();        //Replace "\" with "/" which is recognized by Windows as well

    DiskSize = ReadBinaryFile(OutFileName, Disk);   //Load the output file if it exists

    if (DiskSize < 0)
    {
        //Output file doesn't exits, create an empty D64
        CreateDisk();
    }
    else if ((DiskSize != StdDiskSize) && (DiskSize != ExtDiskSize))    //Otherwise make sure the output disk is the correct size
    {
        cerr << "***CRITICAL***\t Invalid output disk file size!\n";
        return EXIT_FAILURE;
    }

    FixSparkleBAM();

    DirTrack = 18;
    DirSector = 1;
    DirPos = 0;          //This will allow to start with the very first dir slot in overwrite mode

    if (AppendDir)
    {
        cout << "Import mode: Append\n";
        //Lets find the last used dir slot, we will continue with the next, empty one
        FindLastUsedDirPos();
    }
    else
    {
        cout << "Import mode: Overwrite\n";
    }

    if (DirArtType == "d64")
    {
        cout <<"Importing DirArt from D64...\n";
        if (!ImportFromD64())               //Import from another D64
            return EXIT_FAILURE;
    }
    else if (DirArtType == "txt")
    {
        cout << "Importing DirArt from text file...\n";
        if (!ImportFromTxt())               //Import from a text file, 16 characters per text line
            return EXIT_FAILURE;
    }
    else if (DirArtType == "prg")
    {
        cout << "Importing DirArt from PRG...\n";
        if (!ImportFromBinary())            //Import from a PRG file, first 16 bytes from each 40, or until 0xa0 charaxter found
            return EXIT_FAILURE;
    }
    else if (DirArtType == "c")
    {
        cout << "Importing DirArt from Marq's PETSCII Editor C array file...\n";
        if (!ImportFromCArray())            //Import from Marq's PETSCII Editor C array file to D64
            return EXIT_FAILURE;
    }
    else if (DirArtType == "asm")
    {
        cout << "Importing DirArt from KickAss ASM source...\n";
        if (!ImportFromAsm())               //Import KickAss ASM dirart file to D64
            return EXIT_FAILURE;
    }
    else if (DirArtType == "json")
    {
        cout << "Importing DirArt from JSON source...\n";
        if (!ImportFromJson())              //Import from a JSON file
            return EXIT_FAILURE;
    }
    else if (DirArtType == "pet")
    {
        cout << "Importing DirArt from PET source...\n";
        if (!ConvertPetToDirArt())          //Import from PET binary
            return EXIT_FAILURE;
    }
    else if (DirArtType == "png")
    {
        cout << "Importing DirArt from PNG image...\n";
        if (!ImportFromImage())             //Import from PNG image using LodePNG (Copyright (c) 2005-2023 Lode Vandevenne)
            return EXIT_FAILURE;
    }
    else if (DirArtType == "bmp")
    {
        cout << "Importing DirArt from BMP image...\n";
        if (!ImportFromImage())             //Import from BMP image
            return EXIT_FAILURE;
    }
    else
    {
        cout << "Importing DirArt from binary file...\n";
        if (!ImportFromBinary())          //Import from any other file, first 16 bytes of each 40 or until 0xa0 character found
            return EXIT_FAILURE;
    }
    
    if (!WriteDiskImage(OutFileName))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
    

