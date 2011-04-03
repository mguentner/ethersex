ifdef(`conf_STELLA', `', `m4exit(1)')dnl
ifdef(`conf_STELLA_INLINE', `', `m4exit(1)')dnl
<html><head>
<title>StellaLight Control</title>
<link rel="StyleSheet"  href="Sty.c" type="text/css" />
ifdef(`conf_STELLA_INLINE_RGB', ``<script type="text/javascript" src="http://user.rrza.de/~mguentner/jscolor/jscolor.js"></script>'')dnl
<script src="scr.js" type="text/javascript"></script>
<script src="ste.js" type="text/javascript"></script>

</head>
<body>
	<h3>Stella Channels</h3>
	<div><span id="caption">Kein Javascript aktiviert!</span> <span id="upind"></span></div>
	<table id="channels_table"><tr><td></td></tr></table>
</body>
</html>
