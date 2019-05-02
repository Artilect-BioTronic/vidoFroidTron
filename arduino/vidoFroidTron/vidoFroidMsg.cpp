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
    Command("csgn/humidity/cmd",    &updateHumCsgn,     "i",    "1-90"),
    Command("csgn/temp/cmd",        &updateTempCsgn,    "i",    "-5-50"),
    Command("setDate",              &setDate,   "i,i,i,i,i,i",  "1900-2100,1-12,,0-23,,"),
    Command("sendDate",             &getDate)
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

ScheduledCsgn::ScheduledCsgn(float initCsgn): _nbPoints(0), _currentVal(initCsgn),
                _isCsgnFixed(true), _csgnFilename("")
{

}

// read time from  DS1307RTC.h  and converts to seconds from midnight (0H)
unsigned long ScheduledCsgn::getSecondSince0H ( void )
{
    tmElements_t tm;
    if ( RTC.read ( tm ) )
        return tm.Hour*3600 + tm.Minute*60 + tm.Second;
    else
        return 0;
}

// erase old schedule
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

    return _nbPoints;
}

int ScheduledCsgn::checkCsgnFromTimedSchedule()   {
    if (_nbPoints == 0)   {
        SERIAL_MSG.println(F("no schedule available; csgn not modified"));
        return 1;
    }

    unsigned long now_s = getSecondSince0H();

    // We check _indTime is in bounds
    if (_indTime > _nbPoints-1)   _indTime = _nbPoints-1;

    // (if _indTime is low) we increase _indTime
    while ( (_indTime < _nbPoints-1) && (_listPtTime[_indTime+1] > now_s) )
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

int ScheduledCsgn::setCsgnAsFixed(boolean isFixed)   {
    // passing in fixed mode
    if (isFixed)   {
        // the current value (that surely corresponds to schedule) is now the fixed value
        _isCsgnFixed = true;
        return 0;
    }
    else   {  // passing in (not fixed) scheduled mode
        // we cannot pass in scheduled,  there is no schedule available
        if (_nbPoints == 0)   {
            SERIAL_MSG.println(F("no schedule available; csgn stays Fixed"));
            return 1;
        }
        checkCsgnFromTimedSchedule();
        _isCsgnFixed = false;
    }
    return 0;
}

int ScheduledCsgn::readCsgnFile()   {
    SERIAL_MSG.println(F("readCsgnFile not implemented"));
    return 0;
}
