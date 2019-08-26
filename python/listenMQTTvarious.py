# # ! /usr/bin/python
# I dont use shebang # ! because the correct python to use is selected by user

from __future__ import print_function
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import serial
import time
from datetime import datetime, timedelta
import ast
import sys, os, getopt
import subprocess
import mysql.connector

# IP address of MQTT broker
# example of free MQTT broker:  'iot.eclipse.org'
hostMQTT='localhost'
portMQTT=1883
filePassMqtt="./passMqtt.txt"

clientId='myNameOfClient'

# for db
dbName="vidotron"
dbTableTWeb="plan_csgn_temp_web"
dbTableTArduino="plan_csgn_temp_arduino"

logfile=sys.stdout
logStartTime=0.

devSerial='/dev/ttyUSB0'   # serial port the arduino is connected to
baudRate=9600

mqTopic1='phytotron/camera/oh/newPicture'
mqTopic2='phytotron/shellCmd/oh/wifiID'
mqTopic3='phytotron/admin/oh/askTime'
mqTopic4='phytotron/shellCmd/oh/whatdoyouwant'
mqTopic5='phytotron/shellCmd/oh/goadmin'
mqTopic6='phytotron/consigne/web/update'
mqTopic7='phytotron/arduMain/py/csgn/temp/status'

#baseRepPython="/home/arnaud/Workspaces/Arduino/PythonScripts/"
baseRepBin="/home/arnaud/Workspaces/bin/"
baseRepPython="/home/pi/bin"
#baseRepBin="/home/pi/python/"
cmdTopic1=baseRepPython + "picam+mqttFake.py"
cmdTopic2="iwgetid -r"
cmdTopic2b="ifconfig wlan0 | sed -n -e 's/.*inet adr://' -e 's/ *Bcast.*//p'"
cmdTopic5=baseRepBin+"/"

mqRepShift2=['oh', 'pysys']
mqRepTopic3='phytotron/admin/pysys/piClock'
mqRepTopic4='phytotron/shellCmd/pysys/what'
mqRepTopic4b='phytotron/shellCmd/pysys/goadmin'

mqPubTopic6='phytotron/arduMain/oh/csgn/temp/status'
mqPubTopic6addPt='phytotron/arduMain/oh/csgn/temp/adPtSched'

meaning = list(range(10))
meaning[5:7] = ["todo5h", "todo6r"]

# var to receive temp consign
tempCsgn = {'nbPt':0, 'lPtHour':[timedelta(0,0)]*10, 'lPtCsgn':[0]*10 }


sleepBetweenLoop=1    # sleep time (eg: 1s) to slow down loop
sleepResponse=0.09    # sleep to leave enough time for the arduino to respond immediately


# use to sort log messages
def logp (msg, gravity='trace'):
	print('['+gravity+']' + msg, file=logfile)

# log file must not grow big
# I need to overwrite it often
def reOpenLogfile(logfileName):
	"re open logfile, I do it because it must not grow big"
	global logStartTime, logfile
	#
	if logfileName != '' :
		try:
			# I close file if needed
			if ( not logfile.closed) and (logfile.name != '<stdout>') :
				logfile.close()
			# file will be overwritten
			if (logfileName != '<stdout>') :
				logfile = open(logfileName, "a", 1)
			logStartTime = time.time()
			logp('logStartTime:' + time.asctime(time.localtime(time.time())), 'info')
		except IOError:
			print('[error] could not open logfile:' + logfileName)
			sys.exit(3)
	else :
		print('I cant re open logfile. name is empty')


def read_args(argv):
    # optional args have default values above
    global logfile, hostMQTT, namePy
    logfileName = ''
    try:
        opts, args = getopt.getopt(argv,"hl:b:",["logfile=","broker="])
    except getopt.GetoptError:
        print ('serial2MQTTduplex.py -l <logfile> -b <broker>')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print ('serial2MQTTduplex.py -l <logfile> -b <broker>')
            sys.exit()
        elif opt in ("-l", "--logfile"):
            logfileName = arg
        elif opt in ("-b", "--broker"):
            hostMQTT = arg
    logp('logfile is '+ logfileName, 'debug')
    logp('broker is '+ hostMQTT, 'debug')
    # I try to open logfile
    if logfileName != '' :
        reOpenLogfile(logfileName)


logStartTime = time.time()
if __name__ == "__main__":
	read_args(sys.argv[1:])


# if logfile is old, we remove it and overwrite it
#   because it must not grow big !
def checkLogfileSize(logfile):
    "if logfile is too big, we remove it and overwrite it because it must not grow big !"
    if (logfile.name != '<stdout>') and (os.path.getsize(logfile.name) > 900900):
        reOpenLogfile(logfile.name)

def readFileUpdateDict(fileName, defaultDict):
    "read filename and append valid dict lines in returned dict"
    logp('reading dictionary in file : ' + fileName)
    outSketch = defaultDict
    try:
        fileDictionary = open(fileName, 'r')
    except:
        logp('could not open dictionary file: ' + fileName, 'error')
        return outSketch
    #
    strLines = fileDictionary.readlines()
    fileDictionary.close()
    #
    # update dictionary with each line
    for strl in strLines:
        try:
            # we can have comment lines, when they begin with '#'
            if (strl[0] != '#'):
                itemDict = ast.literal_eval(strl)
                if (type(itemDict) == type({})):
                    outSketch.update(itemDict)
                else:
                    logp ('line NOT dict:' + strl)
        except:
          logp('line fails as dict:' + strl)
    return outSketch


def on_log(client, userdata, level, buf):
    print("log: ",buf)

# The callback for when the client receives a CONNACK response from the server.
#def on_connect(client, userdata, flags, rc):   # python 2.7 signature
def on_connect(client, userdata, flags, rc):
    logp("Connected to mosquitto with result code "+str(rc), 'trace')
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    mqttc.subscribe([(mqTopic1, 0) , (mqTopic2+'/#', 0), (mqTopic3, 0), 
                     (mqTopic4, 0) , (mqTopic5, 0) , (mqTopic6, 0),
                     (mqTopic7+'/#', 0)
                    ] )
    mqttc.message_callback_add(mqTopic1, on_message_mqTopicOH1)
    mqttc.message_callback_add(mqTopic2+'/#', on_message_mqTopicOH2)
    mqttc.message_callback_add(mqTopic3, on_message_mqTopicOH3)
    mqttc.message_callback_add(mqTopic4, on_message_mqTopicOH4)
    mqttc.message_callback_add(mqTopic5, on_message_mqTopicOH5)
    mqttc.message_callback_add(mqTopic6, on_message_mqTopicOH6)
    mqttc.message_callback_add(mqTopic7+'/#', on_message_mqTopic7)


# The callback for when a PUBLISH message is received from the server.
# usually we dont go to  on_message
#  because we go to a specific callback that we have defined for each topic
def on_message(client, userdata, msg):
    logp('msg:' +msg.topic+" : "+str(msg.payload), 'unknown msg')


# The callback for when the server receives a message of  mqTopic1.
def on_message_mqTopicOH1(client, userdata, msg):
    logp("mqTopicOH:"+msg.topic+" : "+str(msg.payload), 'info')
    try :
        subprocess.call([cmdTopic1, str(msg.payload)])
    except:
      logp('exception managing msg:'+msg.topic+" : "+str(msg.payload), 'com error')

# The callback for when the server receives a message of  mqTopic2.
def on_message_mqTopicOH2(client, userdata, msg):
    logp("mqTopicOH:"+msg.topic+" : "+str(msg.payload), 'info')
    output2=''; output2b='';
    try :
        output2 = subprocess.Popen(cmdTopic2, shell=True, stdout=subprocess.PIPE).stdout.read()
        output2b = subprocess.Popen(cmdTopic2b, shell=True, stdout=subprocess.PIPE).stdout.read()
    except:
        logp('exception managing msg:'+msg.topic+" : "+str(msg.payload), 'com error')
        retTopic = mqTopic2.replace(mqRepShift2[0], mqRepShift2[1]) + '/KO'
        mqttc.publish(retTopic, "")
    retTopic = mqTopic2.replace(mqRepShift2[0], mqRepShift2[1])
    mqttc.publish(retTopic + '/essid', output2)
    mqttc.publish(retTopic + '/IP', output2b)
    

# The callback for when the server receives a message of  mqTopic3.
def on_message_mqTopicOH3(client, userdata, msg):
    logp("mqTopicOH:"+msg.topic+" : "+str(msg.payload), 'info')
    # we publish system time  (module datetime) au format 2015-05-21T12:34:56 (16 char)
    mqttc.publish(mqRepTopic3, datetime.now().isoformat()[:19])

# The callback for when the server receives a message of  mqTopic4.
def on_message_mqTopicOH4(client, userdata, msg):
    logp("mqTopicOH:"+msg.topic+" : "+str(msg.payload), 'info')
    if msg.payload.isdigit() :
        indMeaning = int(msg.payload)
    else :
        indMeaning = 0
    # indMeaning is between 0 .. 9
    if indMeaning < 0:   indMeaning = 0
    if indMeaning > 9:   indMeaning = 0
    mqttc.publish(mqRepTopic4, indMeaning)   # response to sender
    # link to next topic 5
    mqttc.publish(mqRepTopic4b, meaning[indMeaning])

# The callback for when the server receives a message of  mqTopic5.
def on_message_mqTopicOH5(client, userdata, msg):
    logp("mqTopicOH:"+msg.topic+" : "+str(msg.payload), 'info')
    indMeaning = 0
    ind = 0
    while (ind <= 9 and ind == 0) :
        if (str(msg.payload) == meaning[indMeaning]) :
            indMeaning = ind
    try :
        logp('executing mqTopicOH5 with meaning:'+ meaning[indMeaning], 'info')
        output = subprocess.Popen(cmdTopic5+ meaning[indMeaning] +" "+ str(msg.payload),
                                   shell=True, stdout=subprocess.PIPE).stdout.read()
    except:
        logp('exception managing msg:'+msg.topic+" : "+str(msg.payload), 'error')
        mqttc.publish(mqRepTopic4b + '/KO', "")
        return
    mqttc.publish(mqRepTopic4b + '/OK', '')

# The callback for when the server receives a message of  mqTopic6.
def on_message_mqTopicOH6(client, userdata, msg):
    logp("mqTopicOH6:"+msg.topic+" : "+str(msg.payload), 'info')
    try :
        # read planning of csgn in sql db
        db = mysql.connector.connect(host="localhost",
                             user=authInFile["dbusername"],
                             passwd=authInFile["dbpassword"], 
                             db=dbName)
        cur = db.cursor()
        cur.execute("SELECT hour, consign FROM "+ dbTableTWeb)
        for row in cur.fetchall():
            #print("rsql: %s, %f" % (row[0],row[1]) )
            addPtSchedule2arduino(row[0],row[1])
        #
        # We request the shedule in arduino to check what was received
        mqttc.publish(mqPubTopic6, 'all')
    except Exception as e:
        logp('exception managing msg:'+msg.topic+" : "+str(e), 'error')
    finally : 
        db.close()

# hourmns is datetime.timedelta, csgn is float
def addPtSchedule2arduino(hourmns, csgn):
    # string hh:mm:ss is parsed 
    [hh, mm, ss] = hourmns.seconds//3600, (hourmns.seconds%3600)//60, hourmns.seconds%60
    msg = "%i,%i,%i,%.1f" % (hh, mm, ss, csgn)
    mqttc.publish(mqPubTopic6addPt, msg)

# The callback for when the server receives a message of  mqTopic7
def on_message_mqTopic7(client, userdata, msg):
    logp("mqTopic:"+msg.topic+" : "+str(msg.payload), 'info')
    # common part:   phytotron/arduMain/py/csgn/temp/status/
    # final part + msg may be:   SchedNbPt/:6    pt/4: 16:8:0;30.00    OK:
    finalTopic = msg.topic.replace( mqTopic7+'/', '' )
    if ('SchedNbPt' in finalTopic):
        tempCsgn['nbPt'] =  int(msg.payload)
    elif ('pt/' in finalTopic):
        # I add point to list in  tempCsgn
        # it is not inserted in db yet
        ind = int ( finalTopic.replace('pt/', '') )
        if ( 0 <= ind  and  ind < tempCsgn['nbPt'] ) :
            print( msg.payload.split(b';')[0]  )
            [hh, mm, ss] = [int(x) for x in msg.payload.split(b';')[0].split(b':') ]
            tempCsgn['lPtHour'][ind] =  timedelta( 0, hh*3600 + mm*60 + ss )
            tempCsgn['lPtCsgn'][ind] =  float(msg.payload.split(b';')[1])
    elif ('OK' in finalTopic):
        print('tempCsgn:' + str(tempCsgn) )
        try :
            # read planning of csgn in sql db
            db = mysql.connector.connect(host="localhost",
                                 user=authInFile["dbusername"],
                                 passwd=authInFile["dbpassword"], 
                                 db=dbName)
            cur = db.cursor()
            cur.execute("TRUNCATE TABLE "+ dbTableTArduino)
            for ind in range(tempCsgn['nbPt']):
                print("sql: "+ "INSERT INTO %s (hour, consign) VALUES ('%s', %.1f)" %
                        (dbTableTArduino, str(tempCsgn['lPtHour'][ind]),
                             tempCsgn['lPtCsgn'][ind]) )
                cur.execute("INSERT INTO %s (hour, consign) VALUES ('%s', %.1f)" %
                            (dbTableTArduino, str(tempCsgn['lPtHour'][ind]),
                             tempCsgn['lPtCsgn'][ind]) )
            db.commit()
            #
        except Exception as e:
            logp('exception in db, managing msg:'+msg.topic+" : "+str(e), 'error')
        finally : 
            db.close()
    #

#---------------------------------------------------
#                   mosquitto
#---------------------------------------------------

# read password from file  passMqtt.txt
authInFile = readFileUpdateDict(filePassMqtt, 
        {"username":"user", "password":"pass",
        "dbusername":"", "dbpassword":""
        })

# connection to mosquitto
mqttc = mqtt.Client("", True, None, mqtt.MQTTv31)
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_log=on_log   # help debugging
if ('username' in authInFile and 'password' in authInFile) :
    mqttc.username_pw_set(authInFile["username"], authInFile["password"])

cr = mqttc.connect(hostMQTT, port=portMQTT, keepalive=60, bind_address="")
mqttc.loop_start()
#logp("mqttc.connect : %d" % (cr), 'trace')


# infinite loop
while True:
	time.sleep(sleepBetweenLoop)
	#check size
	checkLogfileSize(logfile)



