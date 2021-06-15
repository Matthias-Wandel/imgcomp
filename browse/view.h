// Include file for my HTML directory generator program.

typedef struct tm timestruc;

//-------------------------------------------------------------------
// Structure for a named directory entry.
typedef struct {
    char   Name[80];
    int    DaySecond;
}DirEntry;

//-------------------------------------------------------------------
// Structure for a variable length list.   Used for lists of files and subdirs.
typedef struct {
    unsigned NumEntries;
    unsigned NumAllocated;
    DirEntry * Entries;
}VarList;

//-------------------------------------------------------------------
// Structure for a subdirectory.  Sort of like an I-node for the directory.
typedef struct {
    char    HtmlPath[200]; // Path to what is pointed to.
    char    Parent[200];
    char    Previous[200];
    char    Next[200];
    VarList   Dirs;   // List of subdirectories.
    VarList   Images; // List of Image files in this directory.
}Dir_t;

//-------------------------------------------------------------------
extern int Holidays[200];
extern int HolidaysLength;

extern char * ImageExtensions[];
extern int NameIsImage(char * Name);
int IsWeekendString(char * DirString);
int read_holiday_config();

//-------------------------------------------------------------------
float ReadExifHeader(char * ImagePath, int * width, int * height);
//-------------------------------------------------------------------
void MakeHtmlOutput(Dir_t * Dir);
void ShowActagram(int all, int twentyfour);
//-------------------------------------------------------------------
void MakeImageHtmlOutput(char * ImagePath, Dir_t * Dir, float AspectRatio);
void MakeViewPage(char * ImageName, Dir_t * dir);
//-------------------------------------------------------------------
void CollectDirectory(char * PathName, VarList * Files, VarList * Dirs, char * Patterns[]);
//-------------------------------------------------------------------
int  CollectDirectoryImages(char * PathName, VarList * Files);
void CollectDirectoryVideos(char * PathName, VarList * Files);
//-------------------------------------------------------------------
int  AddToList(VarList * List, DirEntry * Item);
void RemoveFromList(VarList * List, unsigned int Item);
void TrimList(VarList * List);
void ClearList(VarList * List);
void SortList(VarList * List);

int  FindName(VarList * List, char * Name);

void CombinePaths(char * Dest, char * p1, char * p2);
void RelPath(char * Rel, char * From, char * To);
int ExtCheck(char * Ext, char * Pattern);

//-------------------------------------------------------------------

#ifdef _WIN32
    #define stat _stat
#endif

typedef int BOOL;

#define TRUE 1
#define FALSE 0

#ifndef _WIN32
    #define _MAX_PATH PATH_MAX
#endif


