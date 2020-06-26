//----------------------------------------------------------------------------------
// HTML based image browser to use with imgcomp output.
//
// Make HTML page for actagram view.
//
// Imgcomp and html browsing tool is licensed under GPL v2 (see README.txt)
//---------------------------------------------------------------------------------- 
#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/statvfs.h>

#include "view.h"

static int is_valid_date(int date)
{
    int d = date%100;
    int m = (date/100)%100;
    int y = date/10000;

    return d >= 1 && d <= 31
        && m >= 1 && m <= 12
        && y >= 10 && y <= 99;
}

//----------------------------------------------------------------------------------
// Read the browse.conf file to get holidays in the format YYMMDD and
// store them in the Holidays array.
//
// Returns the count stored in the Holidays array.
int read_holiday_config()
{

    printf("<head><meta charset=\"utf-8\"/></head>");

    FILE * file = fopen("browse.conf", "r");
    if (file == NULL) {
        printf("<script>\n"
            "console.log(\"[ERROR]: Could not open file browse.conf.\")\n"
            "\n</script>\n");
        return 0;
    }

    int count = 0;
    int linenum = 0;
    char line[200];
    while (fgets(line, sizeof line, file) != NULL) {
        int date = atoi(line);
        linenum++;
        if (date == 0) {
            // comment, empty line, or garbage
            continue;
        } else if (is_valid_date(date)) {
            if (count > 200) {
                printf("<script>console.log(\"[ERROR]: browse.conf has too many dates.\")"
                        "</script>\n");
                break;
            }
            Holidays[count++] = date;
        } else {
            printf("<script>console.log(\"[ERROR]: %d is not a valid date, line %d.\")"
                    "</script>\n", date, linenum);
        }
    }
    fclose(file);
    return count;
}


//----------------------------------------------------------------------------------
// Determine if it's a weekend day or a holiday
// Returns < 0 for weekday, >= 0 for weekend/holiday.  3 lsbs indicate day of week.
//----------------------------------------------------------------------------------
int IsWeekendString(char * DirString)
{
    int a, daynum, weekday;
    int y,m,d;
    daynum=0;
    for (a=0;a<6;a++){
        if (DirString[a] < '0' || DirString[a] > '9') break;
        daynum = daynum*10+DirString[a]-'0';
    }
    if (a != 6 || DirString[6] != '\0'){
        // dir name must be six digits only.

        return -10;
    }

    d = daynum%100;
    m = (daynum/100)%100;
    y = daynum/10000 + 2000;
    //printf("%d %d %d",d,m,y);

    {
        // A clever bit of code from the internet!
        static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
        y -= m < 3;
        weekday = ( y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
        // Sunday = 0, Saturday = 6
    }

    for (a=0;a<HolidaysLength;a++){
        // Check if date is a holday.
        if (Holidays[a] == daynum) return weekday;
    }

    if (weekday == 0 || weekday == 6) return weekday;

    return weekday-8;
}

//----------------------------------------------------------------------------------
// Do actagram output
//----------------------------------------------------------------------------------
void ShowActagram(int all, int h24)
{
    int daynum;
    VarList DayDirs;
    memset(&DayDirs, 0, sizeof(DayDirs));
    int BinsPerHour = 15;
    int MinutesPerBin = 60/BinsPerHour;

    printf("<head><meta charset=\"utf-8\"/>\n"
           "<title>Actagram</title>\n"
           "</head>");

    printf(
        "<style type=text/css>\n"
        "  body { font-family: sans-serif; font-size: 20;}\n"
        "  span.wkend {background-color: #E8E8E8}\n"
        "  pre { font-size: 10;}\n"
        "  a {text-decoration: none;}\n"
        "</style>\n");
    
    printf("Actagram: <a href='view.cgi?/'>[Back]</a>"
           "<a href='view.cgi?actagram,all'>[All]</a></big>\n");
    printf("<pre><b>");
    
    CollectDirectory("pix/", NULL, &DayDirs, NULL);

    if (all){
        daynum = 0;
    }else{
        daynum = DayDirs.NumEntries-60;
    }

    if (daynum < 0) daynum = 0;

    int prevwkd = 6, thiswkd=6;
    const int NUMBINS = 15*24;

    // Only show from 7 am to 8 pm
    int from = 7*BinsPerHour;
    int to = BinsPerHour*20+1;


        //printf("n");
    
    int ShowLegend = 1;
    
    int HrefOpen = 0;
    
    for (;;daynum++){
        int bins[NUMBINS];
        char BinImgName[NUMBINS][24];
        char DirName[NUMBINS];
        int a, h;
        VarList HourDirs;
        char * DayName = DayDirs.Entries[daynum].Name;

        if (ShowLegend || daynum == DayDirs.NumEntries){
            // Show legend at the start and end of the actagram view.
            printf("Time:");
            for (int a=from;a<=to;a++){
                char nc = ' ';
                if (a % BinsPerHour == 0 && a+5 < to){
                    printf("%02d:00",a/BinsPerHour);
                    a += 5;
                }
                putchar(nc);
            }
            
            if (ShowLegend) puts("<br>");
            ShowLegend = 0;
        }
        if (daynum >= DayDirs.NumEntries){
            printf("\n");
            break;
        }
        
        memset(BinImgName, 0, sizeof(BinImgName));
        if (strcmp(DayDirs.Entries[daynum].Name, "saved") == 0) continue;
        
        memset(bins, 0, sizeof(bins));

        int isw = IsWeekendString(DayDirs.Entries[daynum].Name);

        thiswkd = isw & 7;
        if (thiswkd != (prevwkd+1) %7){
            printf("<br>");
        }
        prevwkd = thiswkd;


        if (isw >= 0) printf("<span class=\"wkend\">");
        
        printf("<a href='view.cgi?%s'>%s</a> ",DayName, DayName+2);
        sprintf(DirName, "pix/%s",DayName);
        memset(&HourDirs, 0, sizeof(HourDirs));
        
        CollectDirectory(DirName, NULL, &HourDirs, NULL);
        
        for (h=0;h<HourDirs.NumEntries;h++){
            int a;
            VarList HourPix;
            memset(&HourPix, 0, sizeof(HourPix));
            sprintf(DirName, "pix/%s/%s",DayName, HourDirs.Entries[h].Name);
            CollectDirectory(DirName, &HourPix, NULL, ImageExtensions);
            
            for (a=0;a<HourPix.NumEntries;a++){
                int minute, binno;
                char * Name;
                Name = HourPix.Entries[a].Name;
                
                minute = (Name[5]-'0')*60*10 + (Name[6]-'0')*60
                       + (Name[7]-'0')*10    + (Name[8]-'0');
                       
                binno = minute/MinutesPerBin;
                if (binno >= 0 && binno < NUMBINS){
                    bins[binno] += 1;
                    if (bins[binno] < 10) strncpy(BinImgName[binno],Name,23);
                }
            }
            free(HourPix.Entries);
        }
        free (HourDirs.Entries);
        
        if (h24){
            from=0;
            to=BinsPerHour*24;
        }
        for (a=from;a<=to;a++){
            char nc = ' ';
            if (a % BinsPerHour == 0) nc = ':';
            if (a % (BinsPerHour*6) == 0) nc = '|';
            
            //if (bins[a] >= 1 && nc == ' ') nc = '.';
            if (bins[a] >= 5) nc = '-';
            if (bins[a] >= 12) nc = '1';
            if (bins[a] >= 40) nc = '2';
            if (bins[a] >= 100) nc = '#';
            
            if (bins[a] >= 1){
                printf("<a href='view.cgi?%s/%02d/#%s'",DayName,a/BinsPerHour, BinImgName[a]);
                printf(" onmouseover=\"mmo('%s/%02d/%s')\"",DayName,a/BinsPerHour,BinImgName[a]);
                printf(">%c", nc);
                HrefOpen = 1; // Don't close the href till after the next char, makes it easier to hover over single dot.
            }else{
                putchar(nc);
                if (HrefOpen) printf("</a>");
                HrefOpen = 0;
            }
        }
        if (HrefOpen) printf("</a>");
        HrefOpen = 0;
        if (isw >= 0) printf("</span>");
        printf("\n");
    }
    free(DayDirs.Entries);
    
    
    // Preview image html code.
    printf("</pre><small id='prevn'></small><br>\n"
           "<a id='prevh' href=""><img id='preview' src='' width=0 height=0></a>\n");


    // Add javascript for hover-over preview when showing a whole day's worth of images
    printf("<script>\n" // Script to resize the image to the right aspect ratio
           "function sizeit(){\n"
           "  var h = 300\n"
           "  var w = h/el.naturalHeight*el.naturalWidth;\n"
           "  if (w > 850){ w=850;h=w/el.naturalWidth*el.naturalHeight;}\n"
           "  el.width=w;el.height=h;\n"
           "}\n");
           
    printf("function mmo(str){\n"
           "el = document.getElementById('preview')\n"
           "   el.src = 'pix/'+str\n"
           "   el.onload = sizeit\n"
           "   var eh = document.getElementById('prevh')\n"
           "   var ls = str.lastIndexOf('/')\n"
           "   eh.href = 'view.cgi?'+str.substring(0,ls)+'/#'+str.substring(ls+1)\n"
           "   var en = document.getElementById('prevn')\n"
           "   en.innerHTML = str + ' &nbsp; &nbsp; 20'"
           " + str.substring(0, 2)+'-'+str.substring(2,4)+'-'+str.substring(4,6)\n"
           " + ' &nbsp;'+str.substring(15, 17)+':'+str.substring(17,19);"
           "}\n</script>\n");
}




