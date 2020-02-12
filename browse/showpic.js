function UpdatePix(){
    var imgname = subdir+prefix+piclist[pic_index]+".jpg"
    var url = pixpath+imgname;
    if (AdjustBright) url = "tb.cgi?"+imgname+(ShowBigOn?"$1":"$2")

    document.getElementById("view").src = url
    document.title = imgname

    // Update the links at the bottom of the image.
    var links = ""
    var BEFORE = 4
    var AFTER = 5
    var From = pic_index-BEFORE;
    if (From < -1) From = -1;
    var To = From+BEFORE+AFTER+1;
    if (To > piclist.length+1){
        To = piclist.length+1;
        From = To-BEFORE-AFTER-1;
        if (From < -1) From = -1;
    }
    if (From > 0) links += "<< "

    var PrevSecond = -1
    for (a=From;a<To;a++){
        if (a < 0){
            links += "<a href='view.cgi?"+PrevDir+"//#9999'>[Prev dir]</a>"
            continue;
        }
        if (a >= piclist.length){
            links += " <a href='view.cgi?"+NextDir+"//#0'>[Next dir]</a>"
            continue;
        }
        
        // Extract the time part of the file name to show.
        Name = piclist[a];
        if (!isSavedDir){
            TimeStr = Name.substring(0,2)+":"+Name.substring(2,4)
        }else{
            TimeStr = Name.substring(0,5);
        }
        var Second = Name.substring(0,2)*60 + Name.substring(2,4)*1

        var between = " "
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

    var nu = window.location.toString()
    window.location = nu.split("#")[0]+"#"+piclist[pic_index].substring(0,4)

    if (!isSavedDir) document.getElementById("save").innerHTML = "Save"
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
    var img = document.querySelector('img')
    if (!img.complete) {
        // Image is not loaded yet.  Once it's loaded, set a shorter timer
        // (to avoid recursion problems)
        img.addEventListener('load', SetLateTimer())
        return;
    }

    if (ScrollDir){
        if (DoNext(ScrollDir)){
            ScrollTimer = setTimeout(ScrollMoreTimer, 100)
        }
    }
}
function SetLateTimer(){
    ScrollTimer = setTimeout(ScrollMoreTimer, 20)
}

function PicMd(dir){
    DoNext(dir);
    ScrollDir = dir
    ScrollTimer = setTimeout(ScrollMoreTimer, 400)
}
function PicMu(){
    ScrollDir = 0
    clearTimeout(ScrollTimer)
}

function SetIndex(index)
{
    pic_index = index
    UpdatePix()
}

function DoSavePic(){
  var SaveUrl = "view.cgi?~"+subdir+prefix+piclist[pic_index]+".jpg"
  var xhttp = new XMLHttpRequest()
  xhttp.onreadystatechange=function(){
    if (this.readyState==4 && this.status==200){
      var wt=xhttp.responseText.trim()
      if(wt.indexOf('Fail:')>=0)
        wt="<span style='color: rgb(255,0,0);'>["+wt+"]</span>"
      document.getElementById("save").innerHTML=wt
    }
  };
  xhttp.open("GET", SaveUrl, true)
  xhttp.send()
}

ShowBigOn = 0
function ShowBig(){
    ShowBigOn = !ShowBigOn
    SizeImage(ShowBigOn ? PicWidth : 950)
    UpdatePix()
    //var picurl = "pix/"+subdir+prefix+piclist[pic_index]+".jpg"
    //window.location = picurl
}
AdjustBright = 0
function ShowAdj(){
    AdjustBright = !AdjustBright
    UpdatePix()
}

function ShowOld(){
    var nu = window.location.toString()
    nu = nu.substring(0,nu.indexOf("#")-1)+prefix+piclist[pic_index]+".jpg"
    window.location = nu
    //alert(nu)
}


function SizeImage(ShwW)
{
    var ShwH, Qt
    if (ShwW && ShwH){
        ShwH = Math.round(ShwW/PicWidth*PicHeight)
        Qt = Math.round(ShwW/4)
    }else{
        ShwW = 320; ShwH = 240
    }
    document.getElementById("image").innerHTML
      ="<map name='prevnext'><area shape='rect' coords='0,0,"+Qt+","+ShwH+"'"
      +"onmousedown='PicMd(-1)' onmouseup='PicMu()'> <area shape='rect' "
      +"coords='"+(ShwW-Qt)+",0,"+ShwW+","+ShwH+"' onmousedown='PicMd(1)' onmouseup='PicMu()'></map>"
      +"<img id='view' width="+ShwW+" height="+ShwH+" src='' usemap='#prevnext'>"
}

SizeImage(950);

// Find which picture is meant to show.
pic_index=0
pictime = (window.location+" ").split("#")[1];
if (pictime){
    for (;pic_index<piclist.length-1;pic_index++){
        if (piclist[pic_index] >= pictime) break;
    }
}
UpdatePix()
