<html><head>
<link rel="apple-touch-icon-precomposed" sizes="57x57" href="pinpad57x57.png" />
<link rel="apple-touch-icon-precomposed" sizes="72x72" href="pinpad72x72.png" />
<link rel="apple-touch-icon-precomposed" sizes="114x114" href="pinpad114x114.png" />
<link rel="apple-touch-icon-precomposed" sizes="144x144" href="pinpad144x144.png" />
<?php 
    $addr = explode('.',$_SERVER['REMOTE_ADDR']);
	if(count($addr)==4){										// localhost connect just returns a 1?
		if(($addr[0] != "0") || ($addr[1] != "0")){  // first two octets of local IP
			if (!isset($_SERVER['PHP_AUTH_USER']) || !isset($_SERVER['PHP_AUTH_PW'])) {
				header('WWW-Authenticate: Basic realm="My Realm"');
				header('HTTP/1.0 401 Unauthorized');
				exit;
			} else {
				if(($_SERVER['PHP_AUTH_USER'] != "uname") || ($_SERVER['PHP_AUTH_PW'] != "pword")){  // quick and dirty username/password
					header('WWW-Authenticate: Basic realm="Me"');
					header('HTTP/1.0 401 Unauthorized');
					exit;
				}
			}
		}
	}
	header("Refresh: 5;");   // only refresh if we got a good username/password
?></head>
<body>
<?php

const S_PanelStatus = 0;
const S_PanelString = 1;
const S_OpenZonesA = 2;
const S_BusErr = 3;
const S_YY = 4;
const S_MM = 5;
const S_DD = 6;
const S_HH = 7;
const S_MN = 8;
const S_RSSI = 9;


	error_log(basename(__FILE__)."[".__LINE__."/".__FUNCTION__."]\t ====== Start ".$_SERVER['REMOTE_ADDR']." ======",0);

    if($_SERVER['REQUEST_METHOD'] == "POST")
    {
		if (isset($_POST['Disarm']))  {Disarm();}
		if (isset($_POST['ArmAway'])) {ArmAway();}
		if (isset($_POST['ArmStay'])) {ArmStay();}
    } else {
		$server   = '0.0.0.0';  // ipaddress of Photon
		$port = nnnn; // port number for TCP connection to Photon
		$fp = fsockopen ($server, $port, $errorcode, $errormsg); 
		if (!$fp) { 
			$Result = "Error: could not open socket connection"; 
			error_log(basename(__FILE__)."[".__LINE__."]\t Could not send data: [$errorcode] $errormsg \n");
			die( "oops");
		} else {
			fputs($fp, "S");   // make sure its null terminated
			$Result = fread($fp,512);
			fclose ($fp); 
		}

		$Ready = (ord($Result[S_PanelStatus]) & 0x01) != 0;
		$Armed = (ord($Result[S_PanelStatus]) & 0x02) != 0;
		$Memory = (ord($Result[S_PanelStatus]) & 0x04) != 0;
		$Bypass = (ord($Result[S_PanelStatus]) & 0x08) != 0;
		$Trouble = (ord($Result[S_PanelStatus]) & 0x10) != 0;
		$Program = (ord($Result[S_PanelStatus]) & 0x20) != 0;
		$Fire = (ord($Result[S_PanelStatus]) & 0x40) != 0;
		$BackLight = (ord($Result[S_PanelStatus]) & 0x80) != 0;
		
		$ZoneOne = (ord($Result[S_OpenZonesA]) & 0x01) != 0;
		$ZoneTwo = (ord($Result[S_OpenZonesA]) & 0x02) != 0;
		$ZoneThree = (ord($Result[S_OpenZonesA]) & 0x04) != 0;
		$ZoneFour = (ord($Result[S_OpenZonesA]) & 0x08) != 0;
		$ZoneFive = (ord($Result[S_OpenZonesA]) & 0x10) != 0;
		$ZoneSix = (ord($Result[S_OpenZonesA]) & 0x20) != 0;
		$ZoneSeven = (ord($Result[S_OpenZonesA]) & 0x40) != 0;
		$ZoneEight = (ord($Result[S_OpenZonesA]) & 0x80) != 0;
		
		$BusErr = ord($Result[S_BusErr]);
		
		$YY = ord($Result[S_YY]);
		$MM = ord($Result[S_MM]);
		$DD = ord($Result[S_DD]);
		$HH = ord($Result[S_HH]);
		$MN = ord($Result[S_MN]);
		
		$RSSI = ord($Result[S_RSSI]);
		if($RSSI > 127) $RSSI = -1*(256 - $RSSI);
		
		echo "<font size=\"+5\">Status LEDs: ";
		if($Ready)echo "<font color=\"green\">Ready</font> ";
		if($Armed)echo "<font color=\"red\">Armed</font> ";
		if($Memory)echo "Memory <font color=\"red\">Alarmed</font>";
		if($Bypass)echo "Bypass ";
		if($Trouble)echo "Trouble ";
		if($Program)echo "Program ";
		if($Fire)echo "Fire ";
		echo "<br>Open Zones: <font color=\"red\">";
		if($ZoneOne)echo "One ";
		if($ZoneTwo)echo "Two ";
		if($ZoneThree)echo "Three ";
		if($ZoneFour)echo "Four ";
		if($ZoneFive)echo "Five ";
		if($ZoneSix)echo "Six ";
		if($ZoneSeven)echo "Seven ";
		if($ZoneEight)echo "Eight ";
		echo "</font></br>LCD: ";
		$LCD = "unknown";
		switch (ord($Result[S_PanelString])){
        case 0x00:
            $LCD = "";
            break;
        case 0x01:
            $LCD = "Enter Code";
            break;
        case 0x03:
            $LCD = "Secure Before Arming";
            break;
        case 0x11:
            $LCD = "System Armed";
            break;
        case 0x08:
            $LCD = "Exit Delay in Progress";
            break;
        case 0x09:
            $LCD = "Armed with no Delay";
            break;
        case 0x06:  // armed in away mode
            $LCD = "Armed in Away Mode";
            break;
        case 0x3E:  // system disarmed
            $LCD = "System Disarmed";
            break;
        case 0x9E: // enter zone to bypass
            $LCD = "Enter Zones to bypass";
            break;
        case 0x9f:
            $LCD = "Enter your access code";
            break;
        case 0x8f:
            $LCD = "Invalid Access Code";
            break;
        case 0xBA:
            $LCD = "There are no zone low Bats";
            break;
        default:
            $LCD = bin2hex($Result[1]);
		}
		echo $LCD."</br>";
		
		echo "20".$YY."/".$MM."/".$DD." ".$HH.":".$MN."</br>";
		echo "Bus Error code: ".$BusErr."</br>";
		echo "RSSI: ".$RSSI."</br>";
		echo "</font>";
		
		

		
		echo "<form method=\"post\"><table width=\"100%\"><tr>";
		echo "<td align=\"center\"><input style=\"font-size:+80;\" type=\"submit\" name=\"Disarm\" value=\"Disarm\" ".(!$Armed ? 'disabled':'')."/></td>";
		echo "<td align=\"center\"><input style=\"font-size:+80;\" type=\"submit\" name=\"ArmAway\" value=\"Arm Away\" ".(($Armed or !$Ready) ? 'disabled':'')." /></td>";
		echo "<td align=\"center\"><input style=\"font-size:+80;\" type=\"submit\" name=\"ArmStay\" value=\"Arm Stay\" ".(($Armed or !$Ready) ? 'disabled':'')." /></td>";
		echo "</tr></table></form>";
	}
	
    function Disarm()
    {
	    CallFunctionTCP("K1111");   // keycode to use on alarm
    }
    function ArmAway()
    {
			CallFunctionTCP("K1111"); 		
    }
    function ArmStay()
    {
	    CallFunctionTCP("K*91111"); 	// *9 to kill all open delay + keycode
    }
	
//
// send keystrokes to the keypad
//
function CallFunctionTCP($Routine) {
	error_log(basename(__FILE__)."[".__LINE__."]\t CallFunctionTCP ".$Routine);
		$server   = '0.0.0.0';  // ipaddress of Photon
		$port = nnnn; // port number for TCP connection to Photon
	$fp = fsockopen ($server, $port, $errorcode, $errormsg); 
	if (!$fp) { 
		$result = "Error: could not open socket connection"; 
		error_log(basename(__FILE__)."[".__LINE__."]\t Could not send data: [$errorcode] $errormsg \n");
	} else { 
		error_log(basename(__FILE__)."[".__LINE__."]\t TCP Send: $Routine \n");
		fputs($fp, $Routine);   // make sure its null terminated
		error_log(basename(__FILE__)."[".__LINE__."]\t TCP Answer: ".fread($fp,512)." \n");  // read until end of file
		fclose ($fp); 
	}
}	
?>
</body>
</html>
