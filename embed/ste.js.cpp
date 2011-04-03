function stella_per(valuestr) {
	return Math.round(parseInt(valuestr)*100/255);
}
#if defined(STELLA_INLINE_RGB_SUPPORT)
function stella_inittable(request) {
	ow_table = $('channels_table');
	ow_caption = $('caption');

	if (ecmd_error(request)) {
		ow_caption.innerHTML = "Fetching error!";
		return;
	}

	var channels_source = request.responseText.split("\n");
	var channels = channels_source.slice(1,channels_source.length-1); //remove first and last line

	ow_caption.innerHTML = (channels.length) + " Channels loaded.";
	myPickers = new Array();
	for (var i = 0; i < channels.length; i++) {
		txt = "<td>Channel "+i+"</td>";
		for (var j = 0;j <= 10; j++) {
			txt += "<td><a href='javascript:stella("+i+","+Math.round(j*25.5)+");'>"+j*10+"%</a></td>";
		}
		txt += "<td>Current: <span id='current"+i+"'>"+stella_per(channels[i])+"</span>%</td>";
		if((i-1)%3 == 0)
		{
			var id = "myColor" + Math.floor( i/3 );
			txt += "<td><div id=" + id + "></div></td>";
			var input = document.createElement('INPUT');
			input.style.width = '5em';
			input.id = i;
			input.className = "color"
			myPickers[i] = new jscolor.color(input);
			input.onchange = function(){
			stella(parseInt(this.id)-1 , Math.round(255*(myPickers[this.id]).rgb[0]));
			stella(parseInt(this.id) , Math.round(255*(myPickers[this.id]).rgb[1]));
			stella(parseInt(this.id)+1, Math.round(255*(myPickers[this.id]).rgb[2]));
                        };
			ow_table.insertRow(i+1).innerHTML = txt;
			document.getElementById(id).appendChild(input);
		}
                else
		{
			ow_table.insertRow(i+1).innerHTML = txt;
		}

	}
}
#else
function stella_inittable(request) {
	ow_table = $('channels_table');
	ow_caption = $('caption');

	if (ecmd_error(request)) {
		ow_caption.innerHTML = "Fetching error!";
		return;
	}

	var channels_source = request.responseText.split("\n");
	var channels = channels_source.slice(1,channels_source.length-1); //remove first and last line

        ow_caption.innerHTML = (channels.length) + " Channels loaded.";

	for (var i = 0; i < channels.length; i++) {
		txt = "<td>Channel "+i+"</td>";
		for (var j = 0;j <= 10; j++) {
			txt += "<td><a href='javascript:stella("+i+","+Math.round(j*25.5)+");'>"+j*10+"%</a></td>";
		}
		txt += "<td>Current: <span id='current"+i+"'>"+stella_per(channels[i])+"</span>%</td>";
		ow_table.insertRow(i+1).innerHTML = txt;
	}
}
#endif
function stella_updatetable(request) {
	upind = $('upind');

	if (ecmd_error(request)) return;
	upind.innerHTML = "";

	var channels_source = request.responseText.split("\n");
	var channels = channels_source.slice(1,channels_source.length-1); //remove first and last line
	
	for (var i = 0; i < channels.length; i++) {
		$('current'+i).innerHTML = stella_per(channels[i]);
	}
}
function stella_update() {
	upind = $('upind');
	upind.innerHTML = "Updating";
	ArrAjax.ecmd('channel', stella_updatetable);
}
function stella(channel, val) {
	ArrAjax.ecmd('channel+' + channel + '+' + val);
	$('current'+channel).innerHTML = stella_per(val) + "U";
}
window.onload = function() {
	ow_caption = $('caption');
        ow_caption.innerHTML = "Fetching channels...";
	setInterval('stella_update();', 5000);
	ArrAjax.ecmd('channel', stella_inittable);
}
