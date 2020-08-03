function enableAllApplyButtons(enabled){
	var buttons=document.querySelectorAll("input[type=button]");
	for(i=0;i<buttons.length;i++){
		if(buttons[i].value=="Apply"){
			buttons[i].disabled=!enabled;
			if(enabled){
				buttons[i].style.visibility="visible";
			}
			else{
				buttons[i].style.visibility="hidden";
			}
		}
	}
}

function pollForDeviceReady(level){
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.timeout=500;
	xmlhttp.onreadystatechange=function(){if(xmlhttp.readyState == 4 && xmlhttp.status == 200){enableAllApplyButtons(true)}};
	xmlhttp.ontimeout=function(){window.setTimeout(pollForDeviceReady,500,level)};
	xmlhttp.open("HEAD","/config?level="+level);
	xmlhttp.send();
}	

function saveSetting(id,level){
	var settingValue=document.getElementById(id).value;
	enableAllApplyButtons(false);
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET","/saveSetting?setting="+id+"&value="+settingValue+"&level="+level,true);
	xmlhttp.onreadystatechange=function(){if(xmlhttp.readyState == 4 && xmlhttp.status == 200){window.setTimeout(pollForDeviceReady,100,level)}};
	xmlhttp.send();
}
