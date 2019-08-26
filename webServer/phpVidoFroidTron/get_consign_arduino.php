
<?php

header('Content-type: application/json');

$servername = "localhost";
$username = "openhab";
$password = "openhab3d";
$dbname = "vidotron";
$table  = "plan_csgn_temp_arduino";

$cr = "";
// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    //die("Connection failed: " . $conn->connect_error);
    $cr = $conn->connect_error;
}

$sql = "SELECT hour, consign FROM ". $table;
$result = $conn->query($sql);

$cr = [];
$hourl = [];
$csgnl = [];
if ($result->num_rows > 0) {
    // output data of each row
    while($row = $result->fetch_assoc()) {
        $hourl[] = $row["hour"];
        $csgnl[] = $row["consign"];
    }
} else {
    $cr[] = "0 results";
}

if ($result === TRUE) {
    $cr[] = "record queried successfully";
} else {
    $cr[] = $conn->error;
}


// get fixed status and consign val from another table
$sql = "SELECT isFixed, value FROM ". "consign_state" ." WHERE item='temperature'";
if ($result === FALSE) {
    $cr[] = $conn->error;
}
$result = $conn->query($sql);
$row = $result->fetch_assoc();

$isFixed = $row["isFixed"];
$valCsgn = $row["value"];
//$isFixed = "fixed";
//$valCsgn = 12.3;

$conn->close();


if ( ! ( ( isset($_GET["option"]) and ($_GET["option"] === "fixedCsgn") ) ) )  {
    //$oshell = exec("/bin/bash ". dirname(__FILE__) ."/../script/perso/ringArduino.bash");
    //$oshell = exec("../script/perso/ringArduino.bash");
    $oshell = exec("(LD_LIBRARY_PATH='';/usr/bin/mosquitto_pub -t phytotron/arduMain/oh/csgn/temp/status -m all)");
}
else
{
    $oshell = exec("(LD_LIBRARY_PATH='';/usr/bin/mosquitto_pub -t phytotron/arduMain/oh/csgn/temp/status -m csgn)");
}

$jsonReturn = '{ "cr":'. json_encode($cr) .', "data": { ';
$jsonReturn .= '"hour":'. json_encode($hourl);
$jsonReturn .= ', "csgn":'. json_encode($csgnl);
$jsonReturn .= ', "isFixed":"'. $isFixed .'"';
$jsonReturn .= ', "valCsgn":'. $valCsgn;
$jsonReturn .= ' } }';
echo $jsonReturn;


?>

