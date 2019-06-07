#include "Arduino.h"

#include "vidoFroidMsg.h"

#include "msgSerial.h"
#include "msgExampleFunction.h"
#include "msg2SDCard.h"

#include <DS1307RTC.h>

SerialListener serListener(SERIAL_MSG);

// list of available commandes (system ctrl) that the arduino will accept
// example:  int sendSketchId(const String& dumb);

// list of available commands (user) that the arduino will accept
Command cmdUser[] = {
    Command("SV",                   &sendFakeVal),
    Command("csgn/hum/cmd",         &updateHumCsgn,     "i",    "1-90"),
    Command("csgn/temp/cmd",        &updateTempCsgn,    "i",    "-5-50"),
    Command("setDate",              &setDate,   "i,i,i,i,i,i",  "1900-2100,1-12,,0-23,,"),
    Command("sendDate",             &getDate),
    Command("csgn/temp/schedule",   &updateTempSchedule,    "s"),
    Command("csgn/temp/adPtSched",  &addPointTempSchedule,  "i,i,i,f", "0-23,0-59,-1-59,-5-50"),
    Command("csgn/temp/rmPtSched",  &rmPointTempSchedule,   "i,i,i",   "-1-23,0-59,0-59"),
    Command("csgn/temp/isFixed",    &tempIsFixedOrScheduled, "s", "yes|no|ask"),
    Command("csgn/temp/status",     &tempCsgnStatus,        "s",    "csgn|all"),
    Command("csgn/hum/adPtSched",   &addPointHumSchedule,   "i,i,i,f", "0-23,0-59,-1-59,-5-50"),
    Command("csgn/hum/rmPtSched",   &rmPointHumSchedule,    "i,i,i",   "-1-23,0-59,0-59"),
    Command("csgn/hum/isFixed",     &humIsFixedOrScheduled, "s", "yes|no|ask"),
    Command("csgn/hum/status",      &humCsgnStatus,         "s",    "csgn|all")
};
CommandList cmdLUserPhy("cmdUser", "CM+", SIZE_OF_TAB(cmdUser), cmdUser );

// list of available commands (system ctrl) that the arduino will accept
Command cmdSys[] = {
    Command("idSketch",     &sendSketchId),
    Command("idBuild",      &sendSketchBuild),
    Command("listCmd",      &sendListCmd,      "s,s",      "*,short|full")  // eg: :cmdSys,short  or  :,full
};
CommandList cmdLSysPhy("cmdSys", "AT+", SIZE_OF_TAB(cmdSys), cmdSys );

Command cmdSD[] = {
    Command("openStay",     &srStayOpen,    "s,cc", "*,rwascet"),   // :bob.txt,r  filenameDOS8.3 (short names), openMode (read write... )
    Command("open",         &srPreOpen,     "s,cc", "*,rwascet"),   // :prepare to open at each read/write (it is closed immediately after)
    Command("close",        &srClose),          // pas de param
    Command("readln",       &srReadln),         // pas de param
    Command("writeln",      &srWriteln,     "s",    "*"),           // :a new line to write (not \n terminated)
    Command("readNchar",    &srReadNchar,   "i",    "0-200"),       // :nbchar to read in a row
    Command("readNln",      &srReadNln,     "i",    "0-200"),       // :nb lines to read in a row
    Command("move",         &srMove,        "s"),       // :str2search
    Command("dump2",        &srDump2),                  // pas de param
    Command("ls",           &srLs,          "cc",   "rsda"),      // :sr  (recurse size ...)
    Command("rename",       &srRename,      "s,s"),     // :/adir/old,new
    Command("mkdir",        &srMkdir,       "s"),       // :/adir
    Command("rm",           &srRemove,      "s")        // :file.txt
};
CommandList cmdLSD("cmdSD", "SD+", SIZE_OF_TAB(cmdSD), cmdSD );

/*---------------------------------------------------------------*/
/*                                          */
/*---------------------------------------------------------------*/

// This function has to be added in setup()
int setupTempHumMsg()
{
    serListener.addCmdList(cmdLUserPhy);
    serListener.addCmdList(cmdLSysPhy);
    serListener.addCmdList(cmdLSD);

    // following line is copied in main sketch.ino file
    // I fill info on sketch
//    sketchInfo.setFileDateTime(F(__FILE__), F(__DATE__), F(__TIME__));
    // I send identification of sketch
    cmdLSysPhy.readInternalMessage(F("idSketch"));
    cmdLSysPhy.readInternalMessage(F("idBuild"));

    return 0;
}


int getDate(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    tmElements_t tm;
    if ( RTC.read ( tm ) )
    {
      char buffer[20];
      snprintf(buffer, 20, "%d-%.2d-%.2dT%.2d:%.2d:%.2d", tmYearToCalendar(tm.Year),
               tm.Month,tm.Day, tm.Hour,tm.Minute,tm.Second );

      aCL.msgPrint(aCL.getCommand(aInput) + "/state:"+ buffer);
      aCL.msgOK(aInput, buffer);
    }
    else   {
        aCL.msgKO(aInput, F("I could not read back time !"));
        return 1;
    }
    return 0;
}

// set date thanks to format Y:M:D T h:m:s: "i,i,i,i,i,i",  "1900-2100,1-12,,0-23,,"
int setDate(const CommandList& aCL, Command& aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    tmElements_t tm;

    // get values
    tm.Year = CalendarYrToTm(parsedCmd.getValueInt(1));
    tm.Month = parsedCmd.getValueInt(2);
    tm.Day = parsedCmd.getValueInt(3);
    tm.Hour = parsedCmd.getValueInt(4);
    tm.Minute = parsedCmd.getValueInt(5);
    tm.Second = parsedCmd.getValueInt(6);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);

    // we can write this time in RTC
    RTC.write(tm);


    // I read back time, to send it back
    if ( RTC.read ( tm ) )
    {
        char buffer[20];
        snprintf(buffer, 20, "%d-%.2d-%.2dT%.2d:%.2d:%.2d", tmYearToCalendar(tm.Year),
                 tm.Month,tm.Day, tm.Hour,tm.Minute,tm.Second );

        aCL.msgPrint(aCL.getCommand(aInput) + "/state:"+ buffer);
        aCL.msgOK(aInput, buffer);
    }
    else   {
        aCL.msgKO(aInput, F("I could not read back time !"));
        return 1;
    }

    return 0;
}


// "i",  "0-90"
int updateHumCsgn(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    int iValue = parsedCmd.getValueInt(1);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);


    consigneHum.setFixedCsgn(iValue);

    ecritConsigneDansFichier();

    // I send back state msg
    sendConsigne();
    // I send back OK msg
    aCL.msgOK(aInput, String(iValue));

    return 0;
}

// "i",  "-5-50"
int updateTempCsgn(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    int iValue = parsedCmd.getValueInt(1);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);


    consigneTemp.setFixedCsgn(iValue);

    ecritConsigneDansFichier();

    // I send back state msg
    sendConsigne();
    // I send back OK msg
    aCL.msgOK(aInput, String(iValue));

    return 0;
}


/*---------------------------------------------------------------*/
/*       functions to manage temperature schedule                */
/*---------------------------------------------------------------*/

// "s" , the format is checked with function beneath
// format is  nbPointN;h0:m0:s0;csgn0;h1:m1:s1;csgn1 ... hN:mN:sN;csgnN
int updateTempSchedule(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    String sValue = parsedCmd.getValueStr(1);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);


    int ret = consigneTemp.copyEraseScheduleString(sValue);

    if (ret >= 0)
    // I send back OK msg
        aCL.msgOK(aInput, "");
    else
        aCL.msgKO(aInput, ret);

    return 0;
}

// "i,i,i,f",   "0-23,0-59,-1-59,-5-50"
// to erase schedule, you can send -1 in field second
int addPointTempSchedule(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    int ih = parsedCmd.getValueInt(1);
    int im = parsedCmd.getValueInt(2);
    int is = parsedCmd.getValueInt(3);
    float fValue = parsedCmd.getValueFloat(4);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);


    int ret = consigneTemp.addPointSchedule(ih, im, is, fValue);

    if (ret >= 0)
    // I send back OK msg
        aCL.msgOK(aInput, ret);
    else
        aCL.msgKO(aInput, ret);

    return 0;
}

// "i,i,i",   "0-23,0-59,-1-59"
// to erase whole schedule, you can send -1 in field second
int rmPointTempSchedule(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    int ih = parsedCmd.getValueInt(1);
    int im = parsedCmd.getValueInt(2);
    int is = parsedCmd.getValueInt(3);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);


    int ret = consigneTemp.rmPointSchedule(ih, im, is);

    if (ret >= 0)
    // I send back OK msg
        aCL.msgOK(aInput, ret);
    else
        aCL.msgKO(aInput, ret);

    return 0;
}

// "s" , "yes|no|ask"
int tempIsFixedOrScheduled(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    String sValue = parsedCmd.getValueStr(1);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);

    int ret = 0;
    if (sValue.equals("yes"))
        ret = consigneTemp.setCsgnAsFixedOrNot(true);
    else if (sValue.equals("no"))
        ret = consigneTemp.setCsgnAsFixedOrNot(false);
    else   {
        aCL.msgOK(aInput, String(consigneTemp.isCsgnFixed()) );
        return 0;
    }

    if (ret == 0)
    // I send back OK msg
        aCL.msgOK(aInput, "");
    else
        aCL.msgKO(aInput, ret);

    return 0;
}

// "s",    "csgn|all"
int tempCsgnStatus(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    String sValue = parsedCmd.getValueStr(1);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);

    if (sValue.equals("csgn"))
        sendConsigne();
    else   {
        sendConsigne();
        aCL.msgPrint( String("isCsgnFixed: ") + consigneTemp.isCsgnFixed() );
        int nbPoints=0;
        unsigned long listTime[ScheduledCsgn::_MAX_POINTS];
        float listCsgn[ScheduledCsgn::_MAX_POINTS];
        consigneTemp.getSchedule(nbPoints, listTime, listCsgn);
        aCL.msgPrint( aCL.getCommand(aInput) + "/SchedNbPt/:" + nbPoints);
        for (int i=0; i<nbPoints; i++)
            aCL.msgPrint( aCL.getCommand(aInput) + "/pt/" +i+": " +
                          listTime[i] / 3600 + ":" +
                          (listTime[i] % 3600) / 60 + ":" +
                          listTime[i] % 60 + ";" +
                          listCsgn[i]
                          );
    }

    // I send back OK msg
    aCL.msgOK(aInput, "");

    return 0;
}

/*---------------------------------------------------------------*/
/*       functions to manage humidity schedule                   */
/*---------------------------------------------------------------*/

// "i,i,i,f",   "0-23,0-59,-1-59,-5-50"
// to erase schedule, you can send -1 in field second
int addPointHumSchedule(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    int ih = parsedCmd.getValueInt(1);
    int im = parsedCmd.getValueInt(2);
    int is = parsedCmd.getValueInt(3);
    float fValue = parsedCmd.getValueFloat(4);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);


    int ret = consigneHum.addPointSchedule(ih, im, is, fValue);

    if (ret >= 0)
    // I send back OK msg
        aCL.msgOK(aInput, ret);
    else
        aCL.msgKO(aInput, ret);

    return 0;
}

// "i,i,i",   "0-23,0-59,-1-59"
// to erase whole schedule, you can send -1 in field second
int rmPointHumSchedule(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    int ih = parsedCmd.getValueInt(1);
    int im = parsedCmd.getValueInt(2);
    int is = parsedCmd.getValueInt(3);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);


    int ret = consigneHum.rmPointSchedule(ih, im, is);

    if (ret >= 0)
    // I send back OK msg
        aCL.msgOK(aInput, ret);
    else
        aCL.msgKO(aInput, ret);

    return 0;
}

// "s" , "yes|no|ask"
int humIsFixedOrScheduled(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    String sValue = parsedCmd.getValueStr(1);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);

    int ret = 0;
    if (sValue.equals("yes"))
        ret = consigneHum.setCsgnAsFixedOrNot(true);
    else if (sValue.equals("no"))
        ret = consigneHum.setCsgnAsFixedOrNot(false);
    else   {
        aCL.msgOK(aInput, String(consigneTemp.isCsgnFixed()) );
        return 0;
    }

    if (ret == 0)
    // I send back OK msg
        aCL.msgOK(aInput, "");
    else
        aCL.msgKO(aInput, ret);

    return 0;
}

// "s",    "csgn|all"
int humCsgnStatus(const CommandList& aCL, Command &aCmd, const String& aInput)
{
    ParsedCommand parsedCmd(aCL, aCmd, aInput);

    // verify that msg with arguments is OK with format and limit
    if (parsedCmd.verifyFormatMsg(aCmd, aInput) != ParsedCommand::NO_ERROR)
        return aCL.returnKO(aCmd, parsedCmd);

    // get values
    String sValue = parsedCmd.getValueStr(1);

    // we check that parsedCmd has not detected any error
    if (parsedCmd.hasError())
        return aCL.returnKO(aCmd, parsedCmd);

    if (sValue.equals("csgn"))
        sendConsigne();
    else   {
        sendConsigne();
        aCL.msgPrint( String("isCsgnFixed: ") + consigneHum.isCsgnFixed() );
        int nbPoints=0;
        unsigned long listTime[ScheduledCsgn::_MAX_POINTS];
        float listCsgn[ScheduledCsgn::_MAX_POINTS];
        consigneHum.getSchedule(nbPoints, listTime, listCsgn);
        aCL.msgPrint( aCL.getCommand(aInput) + "/SchedNbPt/:" + nbPoints);
        for (int i=0; i<nbPoints; i++)
            aCL.msgPrint( aCL.getCommand(aInput) + "/pt/" +i+": " +
                          listTime[i] / 3600 + ":" +
                          (listTime[i] % 3600) / 60 + ":" +
                          listTime[i] % 60 + ";" +
                          listCsgn[i]
                          );
    }

    // I send back OK msg
    aCL.msgOK(aInput, "");

    return 0;
}

/*---------------------------------------------------------------*/
/*       classes pour filtrer                                    */
/*---------------------------------------------------------------*/

float FilterLastDigit::update(float aNewVal)
{
    // Si la new val est assez differente, on la replique et on reset le filtre
    if (fabs(_value-aNewVal) > _diffStep)   {
        _value = aNewVal;
        _nbDiff = 0;
    }
    //  _diffStep > aNewVal > _noDiffStep
    //  on accepte la  aNewVal  seulement au bout de  _nbDiffStep  fois
    else if (fabs(_value-aNewVal) > _noDiffStep)   {
        _nbDiff++ ;
//        SERIAL_MSG.println(String("filter small diff nb: ") + _nbDiff);
        // Si le changement de valeur est assez frequent, on l accepte
        if (_nbDiff >= _nbDiffMin)   {
            _value = aNewVal;
            _nbDiff = 0;
        }
    }
    else   // _noDiffStep > aNewVal  > 0
    {
        if (_nbDiff < 2)
            _nbDiff = 0;
        else
            _nbDiff = _nbDiff - 2 ;  // on diminue le compte de valeurs differentes rapidement
        // la valeur precedente n est pas modifiee
//        SERIAL_MSG.println(String("filter no diff, nb: ") + _nbDiff);
    }

    return _value;
}

/*---------------------------------------------------------------*/

float FilterDallas::update(float aval)
{
    // le Dallas peut renvoyer plusieurs valeurs erratiques:
    // un 85. , une valeur très basse ~ -2000., peut-être d'autres très hautes
    // ou un 0.
    // j elimine une bonne partie avec des bornes [-50.; 84.]
    if ( (aval < -50.) || (aval > 84.) )   {
        SERIAL_MSG.println(String("temp Dallas rejetee: ") + aval);
        // on garde l ancienne  _value
    }
    else if ( fabs(aval) < 0.1)
    {
        SERIAL_MSG.println(String("temp Dallas rejetee: ") + aval);
        // on garde l ancienne  _value
    }
    else
    {
        _value.update(float(aval));
    }
    return _value.get();
}

/*---------------------------------------------------------------*/
/*       classe pour Chauffage                                   */
/*---------------------------------------------------------------*/

Chauffage::Chauffage(uint8_t pinRad1, uint8_t pinRad2, uint8_t pinFan, int pctChauffe) :
    _pinRad1(pinRad1), _pinRad2(pinRad2), _pinFan(pinFan), _pctChauffe(pctChauffe)
{
    _isOn = false;
    if (_pctChauffe > 100)   _pctChauffe = 0;

    // switch off  chauffage
    analogWrite(_pinRad1, _pctChauffe);
    analogWrite(_pinRad2, _pctChauffe);
    digitalWrite(_pinFan, LOW);
}


int Chauffage::switchOn(void)   {
    _isOn = true;
    analogWrite(_pinRad1, _pctChauffe);
    analogWrite(_pinRad2, _pctChauffe);
    digitalWrite(_pinFan, HIGH);
    return 0;
}

int Chauffage::switchOff(void)   {
    _isOn = false;
    analogWrite(_pinRad1, _pctChauffe);
    analogWrite(_pinRad2, _pctChauffe);
    digitalWrite(_pinFan, LOW);
    return 0;
}

/*---------------------------------------------------------------*/
/*       fonctions de remplacement materiel                      */
/*---------------------------------------------------------------*/

void fakeReleveValeurs()
{
    temperatureInterieureEntiere = 25 ;
    humiditeInterieureEntiere = 41 ;
    temperatureExterieureEntiere = 28 ;
    humiditeExterieureEntiere = 62 ;
}


/*---------------------------------------------------------------*/
/*       classe pour gerer des consignes                         */
/*---------------------------------------------------------------*/

constexpr const char ScheduledCsgn::_isFixedStr[] = "isFixed";
constexpr const char ScheduledCsgn::_isScheduledStr[] = "isScheduled";

ScheduledCsgn::ScheduledCsgn(float initCsgn, String scheduleFilename):
                _nbPoints(0), _currentVal(initCsgn), _isCsgnFixed(true),
                _scheduleFilename(scheduleFilename)
{

}

// read time from  DS1307RTC.h  and converts to seconds from midnight (0H)
unsigned long ScheduledCsgn::getSecondSince0H ( void )
{
    tmElements_t tm;
    if ( RTC.read ( tm ) )
        return tm.Hour*3600UL + tm.Minute*60 + tm.Second;
    else
        return 0;
}

// erase old schedule and  write new one
int ScheduledCsgn::copyEraseSchedule(int nbPoints, unsigned long listTime[],
                                     float listCsgn[])
{
    int nbMax = nbPoints;

    // silly input
    if (nbPoints <=0)
        return -1;

    // display only a warning if too many points
    if (nbPoints > _MAX_POINTS)   {
        nbMax = _MAX_POINTS;
        SERIAL_MSG.println(String(F("Warning max points is: "))+_MAX_POINTS);
    }

    _nbPoints = nbMax;

    for (int i=0; i<_nbPoints; i++)   {
        _listPtTime[i] = listTime[i];
        _listPtVal[i]  = listCsgn[i];
    }

    // save new schedule in file
    saveCsgnFile();

    return _nbPoints;
}

// extract time from string
// stime: input string   of format  hh:mm[:ss]
// return: long nb of seconds : hh*3600 + mm*60 + ss
//           or < 0 if error
long extractTimeIn_hh_mm_ss(String stime) {
    long ltime=0;
    int ind=-1;
    String sval="";
    int ival=0;

    ind = stime.indexOf(':');
    if (ind < 0)
        return -5;

    sval = stime.substring(0,ind);   // extract hour in hh:mm[:ss]
    ival = sval.toInt();
    // if  sval is not an int
    if ( (ival == 0) && (! sval.equals("0")) && (! sval.equals("00")) )
        return -6;
    if ( (ival < 0) || (ival > 23) )
        return -7;

    // okay   store hours
    ltime = long(ival)*3600;

    stime = stime.substring(ind+1);   // keep mm[:ss] in hh:mm[:ss]
    ind = stime.indexOf(':');
    if (ind > 0)   {
        sval = stime.substring(0,ind);    // extract minute in mm:ss
    }
    else
        sval = stime;       // no extraction, there is no :ss  in stime

    ival = sval.toInt();
    // if  sval is not an int
    if ( (ival == 0) && (! sval.equals("0")) && (! sval.equals("00")) )
        return -8;
    if ( (ival < 0) || (ival > 59) )
        return -9;

    // okay store minutes
    ltime += ival*60;

    // if there are seconds to extracts
    if ( ! stime.equals(sval) )   {

        sval = stime.substring(ind+1);   // keep ss in mm:ss

        ival = sval.toInt();
        // if  sval is not an int
        if ( (ival == 0) && (! sval.equals("0")) && (! sval.equals("00")) )
            return -10;
        if ( (ival < 0) || (ival > 59) )
            return -11;

        // okay store minutes
        ltime += ival;
    }

    return ltime;
}

// arg input sSched: the String contains the schedule with this format:
//   nbPointN;h0:m0:s0;csgn0;h1:m1:s1;csgn1 ... hN:mN:sN;csgnN
//   ex: 3;05:15:00;12.0;12:30;15.5;21:02:55;16.7
//      -->  between 21:02:55 and 24:00:00 and until 05:15:00, the val 16.7 is applied.
//      for 12:30  seconds are omitted
int ScheduledCsgn::copyEraseScheduleString(String sSched)   {
    const char BAD_FMT[] = "bad format for schedule";
    String truncSched="";
    String sval="";
    int nbPoints=0;
    unsigned long listTime[_MAX_POINTS];
    float listCsgn[_MAX_POINTS];

    int ind = sSched.indexOf(';');
    if (ind < 0)   {
        SERIAL_MSG.println(BAD_FMT);
        return -1;
    }
    sval = sSched.substring(0,ind);
    nbPoints = sval.toInt();
    if (nbPoints == 0)   {  // sval is not an int (or val = "00")
        SERIAL_MSG.println(BAD_FMT);
        return -2;
    }
    if (nbPoints > _MAX_POINTS)   {
        SERIAL_MSG.println(F("too many points for schedule"));
        return -3;
    }

    truncSched = sSched.substring(ind+1);
    for (int i=0; i<nbPoints; i++)   {
        // extract time
        ind = truncSched.indexOf(';');
        if (ind < 0)   {
            SERIAL_MSG.println(BAD_FMT);
            SERIAL_MSG.println(String("hour n")+i);
            return -4;
        }
        String stime = truncSched.substring(0,ind);   // time with format  hh:mm[:ss]

        // extract  time  as nb of seconds since 00H00mn, from string  hh:mm[:ss]
        long ltime = extractTimeIn_hh_mm_ss(stime);

        if ( ltime < 0 )  {
            SERIAL_MSG.println(BAD_FMT);
            SERIAL_MSG.println(String("hh:mm:ss n")+i+ " error n "+ ltime);
            return ltime;
        }
        listTime[i] = ltime;

        truncSched = truncSched.substring(ind+1);   // last part of schedule for next loop

        // extract val csgn
        String scsgn ="";
        // if last point, no need to extract substring
        if (i == nbPoints-1)   {
            scsgn = truncSched;
        }
        else   {
            ind = truncSched.indexOf(';');
            if (ind < 0)   {
                SERIAL_MSG.println(BAD_FMT);
                SERIAL_MSG.println(String("csgn n")+i);
                return -20;
            }
            scsgn = truncSched.substring(0,ind);

            // last part of schedule for next loop
            truncSched = truncSched.substring(ind+1);
        }

        listCsgn[i] = scsgn.toFloat();

        // if it can not convert to float
        if ( (fabs(listCsgn[i]) < 0.01) && ( ! scsgn.startsWith("0") ) )   {
            SERIAL_MSG.println(BAD_FMT);
            SERIAL_MSG.println(String("csgn n")+i);
            return -21;
        }

    }   // for i

    // transfer schedule in object
    return copyEraseSchedule(nbPoints, listTime, listCsgn);
}

// add 1 point to schedule
// You may begin with erasing schedule with special value -1 on is
// returns the nb of points in schedule
int ScheduledCsgn::addPointSchedule(int ih, int im, int is, float csgn)   {
    // special value:  if is == -1   erase all points
    if (is == -1)   {
        _nbPoints = 0;
        // we need to go to fixed csgn
        _isCsgnFixed = true;
        return 0;
    }

    // if too many points, reject point
    if (_nbPoints >= _MAX_POINTS)   {
        SERIAL_MSG.println(F("too many points"));
        return -1;
    }

    // The point is converted from h,m,s  -->  sec_from_0h,0m,0s
    unsigned long sec_since0H = long(ih)*3600 + im*60 + is;

    // Special case : sec_since0H is equal to a previous point
    // in this case the point is not added, it replaces previous value
    for (int i=0; i<_nbPoints; i++)   {
        if  (sec_since0H == _listPtTime[i])   {
            _listPtVal[i]  = csgn;
            SERIAL_MSG.println(F("erasing a previous point"));
            // update csgn, since schedule has changed
            checkCsgnFromTimedSchedule();
            return _nbPoints;   //   <-- early EXIT
        }
    }

    // i will add point to index i
    // search right position, beginning with last
    int i = _nbPoints;

    // check if I have to decrease  i
    while ( (i > 0) && (sec_since0H <= _listPtTime[i-1]) ) {
        // I shift position of previous point, it increases in position
        _listPtTime[i] = _listPtTime[i-1];
        _listPtVal[i]  = _listPtVal[i-1];
        i-- ;
    }

    // insert point
    _listPtTime[i] = sec_since0H;
    _listPtVal[i]  = csgn;

    // update nb points
    _nbPoints++ ;

    // update csgn, since schedule has changed
    checkCsgnFromTimedSchedule();

    // save new schedule in file
    saveCsgnFile();

    return _nbPoints;
}   // addPointSchedule


// remove 1 point to schedule
// You may either erase whole schedule with special value -1 on ih
// we find the point that applies for the time entered; this point is erased
// returns the nb of points in schedule
int ScheduledCsgn::rmPointSchedule(int ih, int im, int is)   {
    // special value:  if ih == -1   erase all points
    if (ih == -1)   {
        _nbPoints = 0;
        // we need to go to fixed csgn
        _isCsgnFixed = true;
        return 0;
    }

    // if no point, nothing to erase
    if (_nbPoints == 0)   {
        SERIAL_MSG.println(F("no point already"));
        return -1;
    }

    // The point is converted from h,m,s  -->  sec_from_0h,0m,0s
    unsigned long sec_since0H = long(ih)*3600 + im*60 + is;

    // look for  point  _listPtTime[i]    that applies for time  sec_since0H
    // Special case : sec_since0H is equal to a previous point
    // in this case the point is not added, it replaces previous value
    int i;
    for (i=0; i<_nbPoints-1 && sec_since0H >= _listPtTime[i+1]; i++)
    {}

    // special case:  if   sec_since0H <  1st point, then the csgn of last point applies
    //   nothing to shift, the point will be removed with  _nbPoints--
    if ( sec_since0H >= _listPtTime[0] )   {
        // point i is erased, following points are shifted
        for (; i<_nbPoints-1; i++)   {
            _listPtTime[i] = _listPtTime[i+1];
            _listPtVal[i]  = _listPtVal[i+1];
        }
    }
    // update nb points
    _nbPoints-- ;

    // update csgn, since schedule has changed
    if (_nbPoints == 0)
        _isCsgnFixed = true;   // we need to go to fixed csgn
    else
        checkCsgnFromTimedSchedule();

    // save new schedule in file
    saveCsgnFile();

    return _nbPoints;
}   // rmPointSchedule


void ScheduledCsgn::getSchedule(int &nbPoints, unsigned long listTime[], float listCsgn[])   {
    nbPoints = _nbPoints;
    for (int i=0; i<_nbPoints; i++)   {
        listTime[i] = _listPtTime[i];
        listCsgn[i] = _listPtVal[i];
    }
    SERIAL_MSG.println(String("ind: ")+ _indTime);
    return;
}   // getSchedule


// read csgn in schedule
int ScheduledCsgn::checkCsgnFromTimedSchedule()   {
    // we return immediately if csgn is fixed and not read from schedule
    if (_isCsgnFixed)
        return 0;

    if (_nbPoints == 0)   {
        everyMillis(1000, SERIAL_MSG.println(F("no schedule available; csgn not modified"));)
        return 1;
    }

    unsigned long now_s = getSecondSince0H();

    // We check _indTime is in bounds
    if (_indTime > _nbPoints-1)   _indTime = _nbPoints-1;

    // (if _indTime is low) we increase _indTime
    while ( (_indTime < _nbPoints-1) && (_listPtTime[_indTime+1] < now_s) )
        _indTime++;
    // (if _indTime is high) we decrease _indTime
    while ( (_indTime > 0) && (_listPtTime[_indTime] > now_s) )
        _indTime-- ;

    // if now  is before  1st point, we take  value  from last point
    if ((_listPtTime[_indTime] > now_s)  && (_indTime == 0) )
        _currentVal = _listPtVal[_nbPoints-1];
    else
        _currentVal = _listPtVal[_indTime];

    return 0;
}

float ScheduledCsgn::get()   {
    // if the csgn is on schedule, we may update the csgn
    if (! _isCsgnFixed)   {
        // I want that update is made at most  _periodUpdateCsgn_ms
        static unsigned long lastUpdate=0;
        if (millis() - lastUpdate > _periodUpdateCsgn_ms)
            checkCsgnFromTimedSchedule();
    }
    return _currentVal;
}

// usage: isFixed=true  -->  the csgn will be fixed (keep the current value)
//               =false -->  the csgn will follow the schedule
int ScheduledCsgn::setCsgnAsFixedOrNot(boolean isFixed)   {
    // passing in fixed mode
    if (isFixed)   {
        // the current value (that surely corresponds to schedule) is now the fixed value
        _isCsgnFixed = true;
    }
    else   {  // passing in (not fixed) scheduled mode
        // we cannot pass in scheduled,  there is no schedule available
        if (_nbPoints == 0)   {
            SERIAL_MSG.println(F("no schedule available; csgn stays Fixed"));
            return 1;
        }
        _isCsgnFixed = false;
        checkCsgnFromTimedSchedule();
    }
    // save new schedule in file
    saveCsgnFile();

    return 0;
}

// read last csgn in csgn file
// ex of csgn: 10/04/2017;20:00:00;31;isScheduled;2;16:08:00;30;16:08:10;31
// that means: date;time;current_csgn;isScheduled|isFixed;nb_of_points;time1;val1;...
int ScheduledCsgn::readCsgnFile(String aFilename)   {
    String filename = aFilename;
    if (filename.equals(""))
        filename = _scheduleFilename;
    File csgnFile = SD.open ( filename , FILE_READ ) ;
    if ( ! csgnFile )   {
        SERIAL_MSG.print(F("cannot open file:"));
        SERIAL_MSG.println(filename);
        csgnFile.close();
        return -11;
    }

    // search the last line
    // it is after \n char
    // we search before invisible \r\n at end of file and possible empty line
    // we position before a minimal line
    long pos = csgnFile.size() - 20;
    // failure case
    if (pos < 0)   {
        SERIAL_MSG.print(F("no schedule in file"));
        SERIAL_MSG.println(filename);
        csgnFile.close();
        return -1;
    }
    csgnFile.seek(pos);
    // we read backward, until \n
    char readChar = ' ';
    while ( (pos > 1) && readChar != '\n' )   {
        pos-- ;
        csgnFile.seek(pos);
        readChar = csgnFile.read();
        csgnFile.seek(pos);
    }

    // if readChar != '\n', we are at the beginnig of file.
    // otherwise we read char '\n'
    if (readChar == '\n')
        pos += 1;
    // go past date and time +19 char
    pos += 19;

    if ( ! csgnFile.seek(pos) )   {
        SERIAL_MSG.println(F("last line of csgn file too short!"));
        csgnFile.close();
        return -2;
    }
    // we read one char, it must be a ;
    readChar = csgnFile.read();
    if (readChar != ';')   {
        SERIAL_MSG.println(F("bad format on last line of csgn file!"));
        csgnFile.close();
        return -3;
    }

    // we read the next field in the line: the current csgn
    String strBuf = "";
    readChar = csgnFile.read();
    while ( (readChar > 0) && (readChar != ';') )   {
        strBuf += readChar;
        readChar = csgnFile.read();
    }
    float csgn = strBuf.toFloat();
    // test if conversion is not success
    if ( (csgn == String(F("not a float")).toFloat()) && ( ! strBuf.startsWith("0")) )   {
        SERIAL_MSG.println(F("no float csgn on last line of csgn file!"));
        csgnFile.close();
        return -4;
    }
    _currentVal = csgn;

    // we read the next field in the line: isScheduled | isFixed
    strBuf = "";
    readChar = csgnFile.read();
    while ( (readChar > 0) && (readChar != ';') )   {
        strBuf += readChar;
        readChar = csgnFile.read();
    }
    if (strBuf.equals(_isFixedStr))
        _isCsgnFixed = true;
    else if (strBuf.equals(_isScheduledStr))
        _isCsgnFixed = false;
    else   {
        SERIAL_MSG.println(F("bad value: isScheduled | isFixed"));
        csgnFile.close();
        return -5;
    }

    // we read the remaining of the line
    strBuf = "";
    readChar = csgnFile.read();
    while ( (readChar > 0) && (readChar != '\n') )   {
        strBuf += readChar;
        readChar = csgnFile.read();
    }
    csgnFile.close();

    // update schedule with string
    int cr = copyEraseScheduleString(strBuf);
    if (cr < 0)
        SERIAL_MSG.println(F("could not update schedule from file"));

    return cr;
}


// save csgn in csgn file, added on last line
// ex of csgn: 10/04/2017;20:00:00;31.5;isScheduled;2;16:08:00;30;16:08:10;31
// that means: date;time;current_csgn;isScheduled|isFixed;nb_of_points;time1;val1;...
int ScheduledCsgn::saveCsgnFile(String aFilename)   {
    String filename = aFilename;
    if (filename.equals(""))
        filename = _scheduleFilename;
    File csgnFile = SD.open ( filename , FILE_WRITE | O_APPEND | O_CREAT ) ;
    if ( ! csgnFile )   {
        SERIAL_MSG.print(F("cannot open file:"));
        SERIAL_MSG.println(filename);
        csgnFile.close();
        return -1;
    }

    tmElements_t tm;
    if ( ! RTC.read ( tm ) )   {
        SERIAL_MSG.println(F("cannot read RTC"));
        csgnFile.close();
        return -2;
    }

    char buffer[22];
    snprintf(buffer, 22, "%d-%.2d-%.2d;%.2d:%.2d:%.2d;", tmYearToCalendar(tm.Year),
             tm.Month,tm.Day, tm.Hour,tm.Minute,tm.Second );
    csgnFile.print( buffer ) ;
    csgnFile.print( String(_currentVal,1) +";" ) ;
    if (_isCsgnFixed)
        csgnFile.print( String(_isFixedStr) +";" ) ;
    else
        csgnFile.print( String(_isScheduledStr) +";" ) ;

    csgnFile.print( String(_nbPoints) ) ;
    for (int i=0; i<_nbPoints; i++)
        csgnFile.print( String(";") +
                        (_listPtTime[i] / 3600) + ":" +
                        (_listPtTime[i] % 3600) / 60 + ":" +
                        _listPtTime[i] % 60 + ";" +
                        String(_listPtVal[i],1)
                      );

    csgnFile.print("\n");

    csgnFile.close();
    return 0;

}
