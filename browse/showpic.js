function UpdatePix(){
    imgname = prefix+piclist[pic_index]+".jpg"
    document.getElementById("view").src = pixpath+subdir+imgname
    document.title = imgname

    // Update the links at the bottom of the image.
    links = ""
    BEFORE = 4
    AFTER = 5
    From = pic_index-BEFORE;
    if (From < 0) From = 0;
    To = From+BEFORE+AFTER+1;
    if (To > piclist.length){
        To = piclist.length;
        From = To-BEFORE-AFTER-1;
        if (From < 0) From = 0;
    }
    if (From > 0) links += "<< "
    
    PrevSecond = -1
    for (a=From;a<To;a++){
        // Extract the time part of the file name to show.
        Name = piclist[a];
        if (!isSavedDir){
            TimeStr = Name.substring(0,2)+":"+Name.substring(2,4)
        }else{
            TimeStr = Name.substring(0,5);
        }
        Second = Name.substring(0,2)*60 + Name.substring(2,4)*1

        between = " "
        if (a > From){
            dt = Second - PrevSecond;
            if (dt >= 3 && dt < 5) between = " &nbsp;"
            if (dt >= 5 && dt < 10) between = " - "
            if (dt > 20) between = " || &nbsp;"
        }
        PrevSecond = Second
        links += between
        
        if (a == pic_index){
            links += "<b>["+TimeStr+"]</b>"
        }else{
            links += "<a href=\"#"+Name.substring(0,4)+"\" onclick=\"SetIndex("+a+")\">["+TimeStr+"]</a>";
        }
    }
    if (To < piclist.length) links += ">>";
    document.getElementById("links").innerHTML=links;
    
    nu = window.location+""
    window.location = nu.split("#")[0]+"#"+piclist[pic_index].substring(0,4)
    
    if (!isSavedDir) document.getElementById("save").innerHTML = "[Save]"
}

function DoNext(dir){
    if (pic_index+dir < 0 || pic_index+dir >= piclist.length){
        return 0
    }else{
        pic_index += dir
        UpdatePix();
        return 1
    }
}

ScrollDir = 0
ScrollTimer = 0
function ScrollMoreTimer()
{
    if (ScrollDir){
        if (DoNext(ScrollDir)){
            ScrollTimer = setTimeout(ScrollMoreTimer, 100)
        }
    }
}

function PicMouseDown(dir)
{
    DoNext(dir);
    ScrollDir = dir
    ScrollTimer = setTimeout(ScrollMoreTimer, 400)
}

function PicMouseUp()
{
    ScrollDir = 0
    clearTimeout(ScrollTimer)
}

function SetIndex(index)
{
    pic_index = index
    UpdatePix()
}

// Find which picture is meant to show.
pic_index=0
pictime = (window.location+" ").split("#")[1];
if (pictime){
    for (;pic_index<piclist.length-1;pic_index++){
        if (piclist[pic_index] >= pictime) break;
    }
}
UpdatePix()

function DoSavePic(){
  SaveUrl = "view.cgi?~"+subdir+prefix+piclist[pic_index]+".jpg"
  var xhttp = new XMLHttpRequest()
  xhttp.onreadystatechange=function(){
    if (this.readyState==4 && this.status==200){
      wt=xhttp.responseText.trim()
      if(wt.indexOf('Fail:')>=0)
        wt="<span style='color: rgb(255,0,0);'>["+wt+"]</span>"
      else
        wt="["+wt+"]"
      document.getElementById("save").innerHTML=wt
    }
  };
  xhttp.open("GET", SaveUrl, true)
  xhttp.send()
}

function ShowBig(){
    picurl = "pix/"+subdir+prefix+piclist[pic_index]+".jpg"
    window.location = picurl
}
function ShowAdj(){
    picurl = "tb.cgi?"+subdir+prefix+piclist[pic_index]+".jpg$2"
    window.location = picurl
}


index = 5

