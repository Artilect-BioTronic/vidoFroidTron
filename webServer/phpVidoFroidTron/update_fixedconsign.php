
<?php

header('Content-type: application/json');

// get parameters
$isFixed = isset($_GET["isFixed"]) ? json_decode($_GET["isFixed"]) : null;
$valCsgn = isset($_GET["valCsgn"]) ? json_decode($_GET["valCsgn"]) : null;


$servername = "localhost";
$username = "openhab";
$password = "openhab3d";
$dbname = "vidotron";
$table  = "consign_state";

$cr = "if:". $isFixed .", vc:". $valCsgn;
// Create connection
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    //die("Connection failed: " . $conn->connect_error);
    $cr = $conn->connect_error;
}

// if possible, we put values in table
if ( ($isFixed !== null) and ($valCsgn !== null) )   
{

    $sql = "UPDATE ". $table ." SET isFixed=". $isFixed .", value=". $valCsgn .
        " WHERE item='temperature'";

    if ($conn->query($sql) === TRUE) {
        //echo "New record created successfully";
        $cr = "record updated successfully";
    } else {
        //echo "Error: " . $sql . "<br>" . $conn->error;
        $cr = $conn->error . "; sql: ". $sql;
    }
    
    if ($isFixed == 1)   {
        $oshell = exec("(LD_LIBRARY_PATH='';/usr/bin/mosquitto_pub -t phytotron/arduMain/py/csgn/temp/cmd -m ". $valCsgn .")");
     }   else   {   // $isFixed == 0
        $oshell = exec("(LD_LIBRARY_PATH='';/usr/bin/mosquitto_pub -t phytotron/arduMain/py/csgn/temp/isFixed -m no)");
     }

}

$conn->close();


$jsonReturn = '{ "cr": "'. $cr .'"}';
echo $jsonReturn;

?>

