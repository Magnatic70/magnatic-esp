function checkForNewFirmware(newReleaseURL){
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.onreadystatechange=function(){if(xmlhttp.readyState == 4){if(xmlhttp.status == 200){document.getElementById('updateButton').style.visibility="visible"} else{document.getElementById('noUpdateAvailable').style.visibility="visible"}}};
	xmlhttp.open("HEAD",newReleaseURL);
	xmlhttp.send();
}

function pollForDeviceRebooted(level){
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.timeout=500;
	xmlhttp.onreadystatechange=function(){if(xmlhttp.readyState == 4 && xmlhttp.status == 200){location.reload(true)}};
	xmlhttp.ontimeout=function(){window.setTimeout(pollForDeviceRebooted,500,level)};
	xmlhttp.open("HEAD","/config?level="+level);
	xmlhttp.send();
}	


function updateNow(){
	document.getElementById("updateButton").style.visibility="hidden";
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET","/updateNow",true);
	xmlhttp.onreadystatechange=function(){if(xmlhttp.readyState == 4 && xmlhttp.status == 200){window.setTimeout(pollForDeviceRebooted,100,2)}};
	xmlhttp.send();
}
