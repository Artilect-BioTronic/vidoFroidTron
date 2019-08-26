
<?php

header('Content-type: application/json');

// get parameters
$hours = isset($_GET["hour"]) ? json_decode($_GET["hour"]) : null;
$csgns = isset($_GET["csgn"]) ? json_decode($_GET["csgn"]) : null;


$servername = "localhost";
$username = "openhab";
$password = "openhab3d";
$dbname = "vidotron";
$table  = "plan_csgn_temp_web";

$cr = "";
// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    //die("Connection failed: " . $conn->connect_error);
    $cr = $conn->connect_error;
}

// if possible, we put values in table
if ( ($hours != null) and ($csgns != null) )   
{
    // erase former values
    $sql = "TRUNCATE TABLE " . $table;
    $conn->query($sql);

    // create sql query
    $values = "";
    for ($i=0; $i < count($hours); $i++) {
        if ($i != 0)
            $values .= ", ";
        $values .= "('". $hours[$i] ."', ". $csgns[$i] .")";
    }
    
    $sql = "INSERT INTO ". $table ." (hour, consign) VALUES ". $values;

    if ($conn->query($sql) === TRUE) {
        //echo "New record created successfully";
        $cr = "New record created successfully";
    } else {
        //echo "Error: " . $sql . "<br>" . $conn->error;
        $cr = $conn->error;
    }
    
    //$oshell = exec("/bin/bash ". dirname(__FILE__) ."/../script/perso/ringArduino.bash");
    //$oshell = exec("../script/perso/ringArduino.bash");
    $oshell = exec("(LD_LIBRARY_PATH='';/usr/bin/mosquitto_pub -t phytotron/consigne/web/update -m to_update)");

}
else   // we dont update table, but we query it and return older data
{
    $sql = "SELECT hour, consign FROM ". $table;
    
    $result = $conn->query($sql);

    $hours = [];
    $csgns = [];
    if ($result->num_rows > 0) {
        // output data of each row
        while($row = $result->fetch_assoc()) {
            $hours[] = $row["hour"];
            $csgns[] = $row["consign"];
        }
    } else {
        $cr = "0 results";
    }

    if ($result === TRUE) {
        $cr = "record queried successfully";
    } else {
        $cr = $conn->error;
    }
}

$conn->close();


$jsonReturn = '{ "cr": "'. $cr .'", "data": { ';
$jsonReturn .= '"hour":'. json_encode($hours);
$jsonReturn .= ', "csgn":'. json_encode($csgns);
$jsonReturn .= ' } }';
echo $jsonReturn;

?>

