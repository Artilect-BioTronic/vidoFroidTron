<html>
    <head>

    <!-- example found here: https://codepen.io/ashblue/pen/mCtuA
            title HTML5 Editable Table   a pen by Ash Blue
    -->

    <meta content="text/html;charset=utf-8" http-equiv="Content-Type">
    <meta content="utf-8" http-equiv="encoding">

<!--    <script src="http://cdn.bootcss.com/jquery/3.1.1/jquery.min.js"></script> <!-- jQuery source -->
    <script src="../script/jquery/jquery-3.4.1.js"></script> <!-- jQuery source -->
    <link rel="stylesheet" type="text/css" href="http://netdna.bootstrapcdn.com/bootstrap/3.0.2/css/bootstrap.min.css">

	<script src="../script/chart/Chart.min.js"></script>
	<script src="../script/chart/utilsNotChart.js"></script>
	<style>
	canvas {
		-moz-user-select: none;
		-webkit-user-select: none;
		-ms-user-select: none;
	}
	.chart-container {
		width: 500px;
		margin-left: 40px;
		margin-right: 40px;
		margin-bottom: 40px;
	}
	.containerChart {
		display: flex;
		flex-direction: row;
		flex-wrap: wrap;
		justify-content: center;
	}
	</style>
    
    <style>
        @import "compass/css3";

        .table-editable {
          position: relative;
          
          .glyphicon {
            font-size: 20px;
          }
        }

        .table-remove {
          color: #700;
          cursor: pointer;
          
          &:hover {
            color: #f00;
          }
        }

        .table-up, .table-down {
          color: #007;
          cursor: pointer;
          
          &:hover {
            color: #00f;
          }
        }

        .table-add {
          color: #070;
          cursor: pointer;
          position: absolute;
          top: 8px;
          right: 0;
          
          &:hover {
            color: #0b0;
          }
        }

        .table-row-add {
          color: #070;
          cursor: pointer;
          
          &:hover {
            color: #0b0;
          }
        }

    </style>

    <style>
        .table-new {
          position: relative;
          
          .glyphicon {
            font-size: 20px;
          }
        }
        table td {
            border: 2px solid #e1e1e1;
            padding: 3px;
        }
    </style>
    
    </head>
    
    
<body>

        <div class="container">
          <h1>Planning de consigne de temperature</h1>
          <p>Mise a jour des consignes de temperature de l'enceinte.</p>
          
        </div>

    <div  id="chartTempArd" class="containerChart">
    </div>
    <div  id="underChartTempArd" class="container">
        <button id="check-arduino-schedule-btn" class="btn btn-primary">check arduino schedule</button>
        <button id="check-arduino-csgn-btn" class="btn btn-primary">check arduino consign</button>
        Is consign fixed: 
        <span  id="isCsgnFixed"></span>
        Value:
        <span  id="csgnTemperature"></span>
        °C
    </div>
    
    <div  id="chartTempWeb" class="containerChart">
    </div>
    </br>
    <div id="divMyContent"  class="container">
        <div id="tableB" class="table-new">
        <table id="tableWeb" class="table" style="border:1px solid brown;" ></table>
        </div>
        <button id="export-btn" class="btn btn-primary">Export Data</button>
        <button id="arduino-btn" class="btn btn-primary">send to arduino</button>
        <p id="export-par">pataglop</p>
        
        <form name="aye" onSubmit="return handleClick()">
             <p><input type="radio" name="isFixed" value="0" />scheduled consign</p>
             <p><input type="radio" name="isFixed" value="1" />fixed consign</p>
             <p><input type="text" name="valCsgn" /></p>
             <input name="Submit"  type="submit" value="Update fixed consign" />
        </form>
    </div>

    
    <script>

      function handleClick() {
        console.log("rb: " + $('input[name=isFixed]:checked').val());
        console.log("val: " + $('input[name=valCsgn]').val());
        var url = 'http://localhost/phpVidoFroidTron/update_fixedconsign.php';
        url += '?isFixed='+ $('input[name=isFixed]:checked').val();
        url += '&valCsgn='+ $('input[name=valCsgn]').val();
        console.log("url: "+ url);
        getJSON(url,
        function(err, jrep) {
          if (err !== null) {
            console.log("handleclick err:" +err );
          } else {
            console.log("cr:" +JSON.stringify(jrep) );
          }
        });
        
        return false; // prevent further bubbling of event
      }


    // callback for buttons
    // --------------------
    
    $("#check-arduino-schedule-btn").click(function () {
        getCsgnArduino('#chartTempArd');
    });

    $("#check-arduino-csgn-btn").click(function () {
        getCsgnArduino('#chartTempArd', 'fixedCsgn', );    
    });
    
    $("#export-btn").click(function () {
        var csgn = table2tabJsonCsgn('#tableWeb');
        $('#export-par').text(JSON.stringify(csgn));
        createChart('#chartTempWeb', csgn);
    });

    $("#arduino-btn").click(function () {
        var csgn = table2tabJsonCsgn('#tableWeb');
        sendCsgn2Arduino(csgn, '#export-par');
        createChart('#chartTempWeb', csgn);
    });


    // init values and filling
    // -----------------------
    
    // tabJson  and  tabJsonArd  are init here
    // they will be queried  onload
    var tabJson = {
        heading: [ "hour", "csgn" ],
        data: {
            "hour": [ "10:00", "11:00", "13:00", ],
            "csgn": [ 20.0, 21.0, 23.0 ],
        }
    }
    var tabJsonArd = tabJson;


    $(document).ready(function () {

        json2table('#tableWeb',tabJson);
		createChart('#chartTempWeb', tabJson);
        getCsgnWeb('#tableWeb', '#chartTempWeb');
        getCsgnArduino('#chartTempArd');
        
    }); // $(document).ready
    
    
    // functions
    // ---------
    
    function json2table(tagTable, tabJson) {
        $(tagTable).html("");
        var tr;
        tr = $('<tr style="background-color:#335533; color:white; "/>');
        
        var lHeading = tabJson["heading"];
        var nbCol = lHeading.length;
        var nbLgn = tabJson.data[lHeading[0]].length;
        // build header
        for (var d = 0; d < lHeading.length; d++) {
            tr.append("<th>" + lHeading[d] + "</th>");
        }
        tr.append("<th>edit</th>");
        $(tagTable).append(tr);

        // build each row
        for (var r = 0; r < nbLgn; r++) {
            tr = $('<tr/>');
            for (var d = 0; d < nbCol; d++) {
                tr.append('<td contenteditable="true">' + 
                            tabJson["data"][lHeading[d]][r] + '</td>');
            }
            strAddBtn1 = "<input type='button' value='add' id='add' onclick='addRow(" + r + ")' />";
            strAddBtn2 = "<input type='button' value='add' id='add' onclick='addRow(this)' />";
            
            strAddBtn = '<span class="table-row-add glyphicon glyphicon-plus"></span>';
            strRemove = '<span class="table-remove glyphicon glyphicon-remove"></span>'
            tr.append("<td>" + strAddBtn + strRemove + "</td>");
            $(tagTable).append(tr);
        }
        $('.table-remove').click(function () {
          $(this).parents('tr').detach();
        });
        $('.table-row-add').click( function (  )   {
            addRow(tagTable, $(this).parents('tr')[0].rowIndex) ;
        }); 
    }   // json2table
    

    function addRow(tagTable, nRow) {
        var jsonListCsgn = table2tabJsonCsgn('#tableWeb');
        // insert old val to new row position
        jsonListCsgn.heading.forEach(function(heading) {
            jsonListCsgn.data[heading].splice(nRow, 0, 
                jsonListCsgn.data[heading][nRow-1]);
        });
        // result is: displayed, updating table and graph
        $('#export-par').text(JSON.stringify(jsonListCsgn));
        json2table('#tableWeb', jsonListCsgn);
        createChart('#chartTempWeb', jsonListCsgn);
    }

    jQuery.fn.shift = [].shift;
    
    function table2tabJsonCsgn(tagTable)   {
          var $rows = $(tagTable).find('tr:not(:hidden)');
          var headers = [];
          var data = {};   // eg data = { hour: [11h00],  csgn: [20] }
          
          // Get the headers (add special header logic here)
          $($rows.shift()).find('th:not(:empty)').each(function () {
            if ($(this).text() != "edit")   {
                headers.push($(this).text().toLowerCase());
                data[$(this).text().toLowerCase()] = [];
                }
          });
          
          // Turn all existing rows into a loopable array
          $rows.each(function () {
            var $td = $(this).find('td');
            
            // Use the headers from earlier to name our hash keys
            headers.forEach(function (header, i) {
                if ($td.eq(i).text() != "edit")
                    data[header].push( $td.eq(i).text() );   
            });
            
          });

          return { "heading": headers,  "data": data };
    }   // table2tabJsonCsgn
    
    </script>
    
	<script>
    
    function sendCsgn2Arduino(hrCsgn, tagResponse) {
        var url = 'http://localhost/phpVidoFroidTron/update_consign.php';
        url += '?hour='+ JSON.stringify(hrCsgn.data["hour"]);
        url += '&csgn='+ JSON.stringify(hrCsgn.data["csgn"]);
        console.log("url: "+ url);
        getJSON(url,
        function(err, jrep) {
          if (err !== null) {
            $(tagResponse).text('Something went wrong: ' + err);
          } else {
            console.log("data:" +JSON.stringify(jrep) );
            $(tagResponse).text(JSON.stringify(jrep));
          }
        });
    }   // sendCsgn2Arduino

    function getCsgnWeb(tagTable, tagChart) {
        var url = 'http://localhost/phpVidoFroidTron/update_consign.php';
        console.log("url: "+ url);
        getJSON(url,
        function(err, jrep) {
          if (err !== null) {
            $(tagResponse).text('Something went wrong: ' + err);
          } else {
            console.log("data get:" +JSON.stringify(jrep) );
            if (jrep.data.hour.length > 0)   {
                tabJson.data.hour = jrep.data.hour;
                tabJson.data.csgn = jrep.data.csgn;
                json2table('#tableWeb',tabJson);
		        createChart('#chartTempWeb', tabJson);
		    }
          }
        });
    }   // getCsgnWeb

    function getCsgnArduino(tagResponse, option) {
        var url = 'http://localhost/phpVidoFroidTron/get_consign_arduino.php';
        if (option == "fixedCsgn")
            url += '?option=' + option;
        console.log("url: "+ url);
        
        getJSON(url,
        function(err, jrep) {
          if (err !== null) {
            $(tagResponse).text('Something went wrong: ' + err);
          } else {
            console.log("data get:" +JSON.stringify(jrep) );
            if (jrep.data.hour.length > 0)   {
                tabJsonArd.data.hour = jrep.data.hour;
                tabJsonArd.data.csgn = jrep.data.csgn;
                tabJsonArd.data.isFixed = jrep.data.isFixed;
                tabJsonArd.data.valCsgn = jrep.data.valCsgn;
                //json2table('#tableArd',tabJsonArd);
		        createChart(tagResponse, tabJsonArd);
		        $('#isCsgnFixed').text(tabJsonArd.data.isFixed);
		        $('#csgnTemperature').text(tabJsonArd.data.valCsgn);
		    }

          }
        });
    }   // getCsgnArduino

    function getJSON(url, callback) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', url, true);
        xhr.responseType = 'json';
        xhr.onload = function() {
          var status = xhr.status;
          if (status === 200) {
            callback(null, xhr.response);
          } else {
            callback(status, xhr.response);
          }
        };
        xhr.send();
    };
    
    function createChart(tagChart, consigne) {
		var containerChart = document.querySelector(tagChart);
		containerChart.innerHTML = "\n";
		
		var chartContent = {
			type: 'line',
			data: {
				labels: consigne.data["hour"],
				datasets: [{
					label: 'consigne (°C)',
					steppedLine: true,
					data: consigne.data["csgn"],
					borderColor: window.chartColors.green,
					fill: false,
				}]
			},
			options: {
				responsive: true,
				title: {
					display: true,
					text: 'planning des consignes de temperature',
				},
			    scales: {
				    xAxes: [{
					    //type: 'time',
					    //time: {
						//    parser: timeFormat,
						    // round: 'day'
						//    tooltipFormat: 'll HH:mm'
					    //},
					    scaleLabel: {
						    display: true,
						    labelString: 'hour'
					    }
				    }],
				    yAxes: [{
					    scaleLabel: {
						    display: true,
						    labelString: 'consigne'
					    }
				    }]
			    }

			}
		};
		
		var div = document.createElement('div');
		div.classList.add('chart-container');

		var canvas = document.createElement('canvas');
		div.appendChild(canvas);
		containerChart.appendChild(div);

		var ctx = canvas.getContext('2d');
		new Chart(ctx, chartContent);
    }   // createChart
    
    </script>
    
</body>
</html>

