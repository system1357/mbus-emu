<!DOCTYPE html>
<meta charset="utf-8" />
<html>
	<head>
		<title>MBUS WebSockets %counter%</title>
		<link rel="stylesheet" type="text/css" href="style.css">
		<script src="ws.js"></script> 
	</head>
	<body>
		<div id="main">
			<h1>Welcome to the MBUS WebSockets thing...</h1> <a href="/flash">FW Update</a><br>
			<button onClick="javascript:doSend('enableDebug')">enableDebug</button>
			<button onClick="javascript:doSend('disableDebug')">disableDebug</button>
			<br>
			<button onClick="javascript:doSend('emuDisable')">emuDisable</button>
			<button onClick="javascript:doSend('emuEnable')">emuEnable</button>
			<button onClick="javascript:doSend('emuPcktEnable')">emuPcktEnable</button>
			<button onClick="javascript:doSend('emuPcktDisable')">emuPcktDisable</button>
			<br>
			<button onClick="javascript:doSend('cdcPwrDn')">cdcPwrDn</button>
			<button onClick="javascript:doSend('cdcPlay')">cdcPlay</button>
			<button onClick="javascript:doSend('cdcNext')">cdcNext</button>
			<button onClick="javascript:doSend('cdcPrev')">cdcPrev</button>
			<button onClick="javascript:doSend('cdcMagEject')">cdcMagEject</button>
			<button onClick="javascript:doSend('cdcMagInsert')">cdcMagInsert</button>
			<br>
			<button onClick="javascript:doSend('cdcLoadDisk1')">cdcLoadDisk1</button>
			<button onClick="javascript:doSend('cdcLoadDisk2')">cdcLoadDisk2</button>
			<button onClick="javascript:doSend('cdcLoadDisk3')">cdcLoadDisk3</button>
			<button onClick="javascript:doSend('cdcLoadDisk4')">cdcLoadDisk4</button>
			<button onClick="javascript:doSend('cdcLoadDisk5')">cdcLoadDisk5</button>
			<button onClick="javascript:doSend('cdcLoadDisk6')">cdcLoadDisk6</button>
			<br>
			<button onClick="javascript:doSend('rLoadDisk1')">rLoadDisk1</button>
			<button onClick="javascript:doSend('rLoadDisk2')">rLoadDisk2</button>
			<button onClick="javascript:doSend('rLoadDisk3')">rLoadDisk3</button>
			<button onClick="javascript:doSend('rLoadDisk4')">rLoadDisk4</button>
			<button onClick="javascript:doSend('rLoadDisk5')">rLoadDisk5</button>
			<button onClick="javascript:doSend('rLoadDisk6')">rLoadDisk6</button>
			<br>
			<button onClick="javascript:doSend('phoneModeEnable')">phoneModeEnable</button>
			<button onClick="javascript:doSend('phoneModeDisable')">phoneModeDisable</button>
			<button onClick="javascript:doSend('intcdModeEnable')">intcdModeEnable</button>
			<button onClick="javascript:doSend('btModeEnable')">btModeEnable</button>
			<br><textarea id="textarea_id" name="textarea" rows="16" cols="50"></textarea>
		</div>

	</body>
</html>
