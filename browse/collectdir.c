//----------------------------------------------------------------------------------
// Module to do operating system dependent directory scanning.
//----------------------------------------------------------------------------------
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "view.h"

#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
#else
    #include <sys/types.h>
    #include <dirent.h>
    #include <ctype.h>
    #include <unistd.h>
    #include <sys/stat.h>
#endif

//--------------------------------------------------------------------------
// Checking of an extension.  Case insenstive.
//--------------------------------------------------------------------------
int ExtCheck(char * Ext, char * Pattern)
{
    for(;;Ext++,Pattern++){
        if (*Ext == *Pattern){
            if (*Ext == '\0') return 0; //Matches.
            continue;
        }
        if (*Ext > 'A' && *Ext <= 'Z'){
            if (*Ext+'a'-'A' == *Pattern) continue;
        }
        return 1; // Differs.
    }
}


#ifndef _WIN32
//----------------------------------------------------------------------------------
// Collect information for a directory.  Linux version.
//----------------------------------------------------------------------------------
void CollectDirectory(char * PathName, VarList * Files, VarList * Dirs, char * Patterns[])
{
    char FullPath[400];
    DIR * dirpt;
    struct stat filestat;
    unsigned l;
    int a;

    //printf("DIR: '%s'<br>\n",PathName);

    if (PathName[0]){
        dirpt = opendir(PathName);
    }else{
        // No path means current directory.
        dirpt = opendir(".");
    }

    if (dirpt == NULL){
         printf("Error: could not read dir %s: %s\n",PathName, strerror(errno));
         return;
    }

    for (;;){
        struct dirent * entry;
        DirEntry ThisOne;

        entry = readdir(dirpt);
        if (entry == NULL) break;

        memset(&ThisOne, 0, sizeof(ThisOne));
        strcpy(ThisOne.Name, entry->d_name);

        CombinePaths(FullPath, PathName, entry->d_name);

        if (stat(FullPath, &filestat)){
            printf("Error on '%s'<br>\n",FullPath);
            continue;
        }

        if(S_ISDIR(filestat.st_mode)){
            // Exclude non real paths.
            if (Dirs){
                if (ThisOne.Name[0] == '.'){ 
                    continue;
                }
                AddToList(Dirs, &ThisOne);
            }
        }else{
            if (Patterns != NULL && Files != NULL){
                // Reject anything that does not end in one of the specified patterns.
                l=strlen(entry->d_name);
                for (a=0;;a++){
                    int lp;
                    if (Patterns[a] == NULL){
						break;
					}						
                    lp = strlen(Patterns[a]);
                    if (l > lp){
                        if (ExtCheck(entry->d_name+l-lp, Patterns[a]) == 0){
                            //ThisOne.Size = filestat.st_size;
                            AddToList(Files, &ThisOne);
                            break;
                        }
                    }
                }
            }
        }
    }
    closedir(dirpt);
	
    if (Files) SortList(Files);
	if (Dirs) SortList(Dirs);
}
#else
//----------------------------------------------------------------------------------
// Collect information for a directory.  Windows version.
//----------------------------------------------------------------------------------
time_t CollectDirectory(char * PathName, VarList * Files, VarList * Dirs, char * Patterns[])
{
    char MatchPath[500];

    // For getting directory list...
    struct _finddata_t finddata;
    long find_handle;
    int l;
    int a;

    CombinePaths(MatchPath, PathName, "*");

    find_handle = _findfirst(MatchPath, &finddata);

    for (;;){
        DirEntry ThisOne;

        if (find_handle == -1) break;

        memset(&ThisOne, 0, sizeof(ThisOne));
        strcpy(ThisOne.Name, finddata.name);

        if (finddata.attrib & _A_SUBDIR){
            if (ThisOne.Name[0] == '.' || ThisOne.Name[0] == '_'){
                // Skip ".", ".."
            }else{
                AddToList(Dirs, &ThisOne);
            }
        }else{
            if (Patterns != NULL && Files != NULL){
                // Reject anything that does not end in one of the specified patterns.
                l=strlen(finddata.name);
                //if (l < 5) goto next; // Too short a name to contain '.jpg'
                for (a=0;;a++){
                    int lp;
                    if (Patterns[a] == NULL) break;
                    lp = strlen(Patterns[a]);
                    if (l > lp){
                        if (ExtCheck(finddata.name+l-lp, Patterns[a]) == 0){
                            //ThisOne.Size = finddata.size;
                            AddToList(Files, &ThisOne);
                            break;
                        }
                    }
                }
            }
        }

        if (_findnext(find_handle, &finddata) != 0) break;
    }
    _findclose(find_handle);
	
	if (Files) SortList(Files);
	if (Dirs) SortList(Dirs);
}
#endif
