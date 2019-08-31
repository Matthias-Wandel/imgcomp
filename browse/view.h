// Include file for my HTML directory generator program.

typedef struct tm timestruc;

//-------------------------------------------------------------------
// Structure for a named directory entry.
typedef struct {
    char   Name[160];

    // File specific:
    int    Size;        // File size

    // Dir specific:
    void * Subdir; // Points at subdirectory data (if the element is for a directory)
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
    // Statistics on this directory and descending directories:
    char      HtmlPath[300]; // Path to what is pointed to.
    VarList   Dirs;   // List of subdirectories.
    VarList   Images; // List of Image files in this directory.
}Dir_t;


void MakeHtmlOutput(Dir_t * Dir);

//-------------------------------------------------------------------
time_t CollectDirectory(char * PathName, VarList * Files, VarList * Dirs, char * Patterns[]);
//-------------------------------------------------------------------
int  CollectDirectoryImages(char * PathName, VarList * Files);
void CollectDirectoryVideos(char * PathName, VarList * Files);
void ParseDate(timestruc * thisdate, char * Line);
//-------------------------------------------------------------------
int  AddToList(VarList * List, DirEntry * Item);
void RemoveFromList(VarList * List, unsigned int Item);
void TrimList(VarList * List);
void ClearList(VarList * List);
int  FindName(VarList * List, char * Name);
void CopyFile(char * DestPath, char * SrcPath);
void CombinePaths(char * Dest, char * p1, char * p2);
void RelPath(char * Rel, char * From, char * To);
int ExtCheck(char * Ext, char * Pattern);
void SortDirContents(Dir_t * Dir);
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


