// Include file for my HTML directory generator program.

typedef struct tm timestruc;

//-------------------------------------------------------------------
// Structure for a named directory entry.
typedef struct {
    char   Name[160];
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
    char	HtmlPath[200]; // Path to what is pointed to.
	char    Parent[200];
	char    Previous[200];
	char    Next[200];
    VarList   Dirs;   // List of subdirectories.
    VarList   Images; // List of Image files in this directory.
}Dir_t;


void MakeHtmlOutput(Dir_t * Dir);
void MakeImageHtmlOutput(char * ImagePath, char * HtmlDir, VarList Images);

extern char * ImageExtensions[];
extern float AspectRatio;

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

void CopyFile(char * DestPath, char * SrcPath);
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


