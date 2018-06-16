<?php
// Actions for IFTTT actions are viewed JSON
// IFTTT Web Request should be configured for
//     URL - pointing to publicly accessible URL to access *this* file
//     METHOD - POST
//	   Content Type - application/json
//     Body - <JSON formated message>
//          - for example {"Function":"Disarm","Username":"MyRandomUsername","OccuredAt","{{CreatedAt}}"}

error_reporting(E_STRICT);

// get the incoming IFTTT POST data all in one read....
$json = file_get_contents('php://input'); 

// this bit just so that the POST returns right away and doesn't wait
// for this script to complete
set_time_limit(0);
ob_end_clean();
ignore_user_abort(true);
header("Connection: close\r\n");
header("Content-Encoding: none\r\n");  
ob_start();             
echo date('Y-m-d H:i:s').PHP_EOL;
$size = ob_get_length();   
header("Content-Length: $size",TRUE);
ob_end_flush();
flush();

$dbg = true;

date_default_timezone_set('America/Chicago');

// Parse out the JSON object that came from IFTTT
// OK, to add more attributes to the JSON in IFTTT and parse them here
$obj = json_decode($json);
$PostFunc = trim($obj->{'Function'});
$PostUser = trim($obj->{'Username'});
if (array_key_exists('OccuredAt',$obj)) {
	$PostOccuredAt = str_ireplace(" at "," ",trim($obj->{'OccuredAt'}));  // silly IFTTT returns date 'at' time format!
} else {
	$PostOccuredAt = date('Y/m/d H:i:s');
}

error_log("\t\t\t".basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t\t\t ===== Start IFTTT-State (".$StartTime.") from ".$_SERVER['REMOTE_ADDR']." ======",0);
if($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t json:".$json,0);
if($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t PostFunc:".$PostFunc,0);
if($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t PostOccured At:".$PostOccuredAt,0);
if($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t PostUser:".$PostUser,0);

if( $PostUser == "Whatever The Password Should Be") {  // crude security, stop just anyone from sending JSON
	
	error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t Got PostFunction ".$PostFunc,0);
	switch ($PostFunc){
		
		case "Arm":
			if ($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t LeaveArm called, voice=".$Voice,0);
			Alarm_Arm();
			break;
			
		case "Disarm":
			if ($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t Disarm called, voice=".$Voice,0);
			Alarm_Disarm();
			break;
			
		default:

			if($dbg)error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t caught ".$PostFunc." at default case",0);

	}
} else {
	error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t Invalid IFTTT username specified:".$PostUser,0);
	// don't show anything on the resulting screen page...
}

error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t\t\t ====== IFTTT-State Completed (".$StartTime.")====== Execution Time:".$EndTime,0);

function Alarm_Disarm(){
	global $dbg;
	if($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]",0);
	Alarm_CallFunctionTCP("K0000");  // K + your 4 digit keycode here
}

function Alarm_Arm() {
	global $dbg;
	if($dbg) error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]",0);  
	Alarm_CallFunctionTCP("K0000"); // K + your 4 digit keycode here
}

//
// send keystrokes to the keypad
//
function Alarm_CallFunctionTCP($Routine) {
	global $dbg;
	error_log(basename(__FILE__)."[".__LINE__."]\t AlarmCallFunctionTCP ");
	$server   = 'Your Photon nodename';
	$port = your photon port;
	$fp = fsockopen ($server, $port, $errorcode, $errormsg);  // open a client connection 
	if (!$fp) { 
		error_log(basename(__FILE__)."[".__LINE__."]\t Could not send data: [$errorcode] $errormsg \n");
	} else { 
		error_log(basename(__FILE__)."[".__LINE__."]\t TCP Send \n");
		fputs($fp, $Routine);  
		error_log(basename(__FILE__)."[".__LINE__."]\t TCP Answer: ".fread($fp,512)." \n");  // read until end of file
		fclose ($fp); 
	}
}

?>