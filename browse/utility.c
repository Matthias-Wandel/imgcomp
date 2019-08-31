#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "view.h"

//----------------------------------------------------------------------------------
// Add something to a VarList.
//----------------------------------------------------------------------------------
int AddToList(VarList * List, DirEntry * Item)
{
    if (List->Entries == NULL){
        DirEntry * Temp;
        Temp = (DirEntry *)malloc(sizeof(DirEntry) * 20);

    List->Entries = Temp;

        //List->Entries = (DirEntry *)malloc(sizeof(DirEntry) * 20);
        List->NumAllocated = 20;
        List->NumEntries = 0;

        if (List->Entries == NULL){
            printf("\nError: Malloc failure!\n");
            exit(-1);
        }
    }else{
        if ((List->NumEntries) >= List->NumAllocated){
            List->NumAllocated += List->NumAllocated/2;
            List->Entries = (DirEntry *)realloc(List->Entries, sizeof(DirEntry)*List->NumAllocated);

            if (List->Entries == NULL){
                printf("\nError: Realloc failure!\n");
                exit(-1);
            }
        }
    }

    List->Entries[List->NumEntries] = *Item;
    return List->NumEntries++;
}

//----------------------------------------------------------------------------------
// Delete an item from the list.
//----------------------------------------------------------------------------------
void RemoveFromList(VarList * List, unsigned int Item) 
{
    if (Item < 0 || Item >= List->NumEntries){
        printf("invalid remove index\n");
        exit(-1);
    }
    memcpy(List->Entries+Item, List->Entries+Item+1, sizeof(DirEntry) * (List->NumEntries-1-Item));
    List->NumEntries -= 1;
}

//----------------------------------------------------------------------------------
// Un-allocate extra allocated memory.
//----------------------------------------------------------------------------------
void TrimList(VarList * List)
{
    if (List->NumAllocated > List->NumEntries+2){
        // Leave a spare element - we still add the themes link after trimming.
        List->NumAllocated = List->NumEntries+1;
        List->Entries = (DirEntry *)realloc(List->Entries, sizeof(DirEntry)*List->NumAllocated);
    }
}

//----------------------------------------------------------------------------------
// Get rid of a VarList
//----------------------------------------------------------------------------------
void ClearList(VarList * List)
{
    free(List->Entries);
    List->NumEntries = 0;
    List->NumAllocated = 0;
}

//----------------------------------------------------------------------------------
// Find a name in VarList
//----------------------------------------------------------------------------------
int FindName(VarList * List, char * Name)
{
    unsigned int a;
    for (a=0;a<List->NumEntries;a++){
        if (strcmp(List->Entries[a].Name, Name) == 0){
            return a;
        }
    }
    // Name was not found in the list.
    return -1;
}
//----------------------------------------------------------------------------------
// Combine two paths into one.
//----------------------------------------------------------------------------------
void CombinePaths(char * Dest, char * p1, char * p2)
{
    int Lastp1char;
    strcpy(Dest, p1);
    Lastp1char = 0;
    if (p1[0]){
        Lastp1char = p1[strlen(p1)-1];
    }

    if (p1[0] != '\0'){
        if (Lastp1char != '\\' && Lastp1char != '/'){
            strcat(Dest, "/");
        }
    }
    strcat(Dest, p2);
}

//----------------------------------------------------------------------------------
// Compute how to get from 'From' path to 'To' path
//----------------------------------------------------------------------------------
void RelPath(char * Rel, char * From, char * To)
{
    int a;
    int FirstDifference;
    int WasSlash;
    int LenFrom, LenTo;

    FirstDifference = 0;
    Rel[0] = 0;

    // Starting from left, pop off all equal directory components.
    for (a=0;;a++){
        BOOL SepFrom, SepTo;
        SepFrom = (From[a] == '\0' || From[a] == '\\' || From[a] == '/');
        SepTo   = (To[a] == '\0' || To[a] == '\\' || To[a] == '/');

        if (SepFrom && SepTo){
            FirstDifference = a+1;
        }

        if (From[a] != To[a]){
            break;
        }

        if (From[a] == '\0'){
            // Paths are identical.
            return;
        }
    }

    //printf("first diff at %d\n",FirstDifference);

    // Now count how many directories we need to go up before going back down.
    // FirstDifference is index of first path segment that differs.

    // Note: This code not smart enough to understand '..' in the 'from' path.

    LenFrom = strlen(From);
    LenTo = strlen(To);

    WasSlash = 1;
    for (a=FirstDifference;;a++){
        if (a > LenFrom) break;
        if (From[a] == '\0') break;
        if (From[a] == '\\' || From[a] == '/'){
            WasSlash = 1;                                       
        }else{
            if (WasSlash){
                // Using forward slashes - Windows browsers understand this, and so
                // do Unitx browsers (backslash is windows only)
                strcat(Rel, "../");
                WasSlash = 0;
            }
        }
    } 
    if (LenTo > FirstDifference){
        strcat(Rel, To+FirstDifference);
    }

    /*
    printf("From:%s\n",From);
    printf("To  :%s\n",To  );
    printf("Rel :%s\n",Rel );
    */

}

//----------------------------------------------------------------------------------
// Compare dates of two entries - helper function for qsort call.
//----------------------------------------------------------------------------------
static int SortCompareNames(const void * el1,  const void * el2)
{
    return strcmp((char *)&((DirEntry *)el1)->Name, (char *)&((DirEntry *)el2)->Name);
}

//----------------------------------------------------------------------------------
// Create browsable HTML index files for the directories.
//----------------------------------------------------------------------------------
void SortDirContents(Dir_t * Dir)
{
    VarList Images;
    VarList Directories;

    Images = Dir->Images;
    Directories = Dir->Dirs;    

    // Sort FileList.
    qsort( Images.Entries, Images.NumEntries, sizeof(DirEntry), SortCompareNames);

    // Sort the directories.
    qsort( Directories.Entries, Directories.NumEntries, sizeof(DirEntry), SortCompareNames);
}
