#include <Arduino.h>
#include "laptimes.h"
#include <EEPROM.h>

// Does what it says, gets a lap time from EEPROM - does not do any validation
lap_time_t CEEPROMLapStore::getLapTime(lap_handle_t lapHandle)
{
  lap_time_t lapTime = (EEPROM.read((lapHandle*sizeof(uint16_t))+1)<<8);
  lapTime += EEPROM.read(lapHandle*sizeof(uint16_t)); 
  
  return lapTime;
}

// Does what it says, sets a lap time in EEPROM - does not do any validation
void CEEPROMLapStore::setLapTime(lap_handle_t lapHandle,lap_time_t lapTime)
{
  EEPROM.write(lapHandle*sizeof(uint16_t),lowByte(lapTime));
  EEPROM.write((lapHandle*sizeof(uint16_t))+1,highByte(lapTime));
}

// Initialise the lap store to EMPTY_LAP_TIME through out
// this is an important function, we find empty space by looking
// for one EMPTY_LAP_TIME that defines the end of a session followed
// immediatley by another EMPTY_LAP_TIME, this show that there
// are not sessions following the previous session in which case
// we are free to create a new session.
// We cannot be sure what SD, Memory or EEPROM will contain on the first run
// and so it is important we have this option to initialise the storage to 
// a known value.
void CEEPROMLapStore::clearAll()
{
  for(uint16_t unIndex = 0;unIndex < (getMaxLaps()*sizeof(lap_time_t));unIndex++)
  {
    EEPROM.write(unIndex,EMPTY_LAP_TIME);
  }
}

// Return the maximum number of laps for this storage media (or device ATMega8,328,1240 etc)
uint16_t CEEPROMLapStore::getMaxLaps()
{
  return EEPROM_LAP_STORE_MAX_LAPS;
}


//*******************************************************************************************
// CLapTimes 
//
// A lot of the work in this class is simply finding the start and end of sessions, and
// finding space to start a new session.
//
// With more memory I would have used headers to do a lot of the work inside CLapTimes
// A file system could also have done a lot of the work.
//
// It isn't pretty and could be refactored but it works.
//
//*******************************************************************************************

// Initialise CLapTimes which whichever class we want to provide the actual lap storage
// all lap storage is through the ILapStore interface and so we can use any class
// that implements this interface. Only CEEPROMLapStore is provided in this release,
// others may follow
CLapTimes::CLapTimes(ILapStore *pLapStore)
{
  m_pLapStore = pLapStore;
}
  
// The end of a session is marked by an empty lap (0)
// to create a new session, we first look at the very first lap, if its invalid, there are no sessions
// and we can start a new one from position 0.
// if there is a valid lap at position 0 we need to scan for two consecutive invalid laps. A single invalid lap indicates
// the end of an existing session, if this is followed by anything other than an invalid lap, it is the beginning 
// of a new session, if its followed by an invalid lap then we have found the end of the existing sessions and 
// can use the second invalid lap handle as the start of our new session. 
lap_handle_t CLapTimes::createNewSession()
{
  lap_handle_t newSessionLapHandle = 0;
  lap_handle_t currentLapHandle = 0;
  
  // if the first lap is a valid lap - we need to scan through the recorded laps 
  // and sessions to find two consecutive invalid laps - the first we leave in place to
  // mark the end of the existing sessions, the second is a free space for us to create
  // a new session.
  if(m_pLapStore->getLapTime(newSessionLapHandle) != EMPTY_LAP_TIME)
  {
    // assume the worst - there is no space left
    newSessionLapHandle = INVALID_LAP_HANDLE;
    
    // loop until we have a valid lap handle or we reach the end of the lap store
    while(newSessionLapHandle == INVALID_LAP_HANDLE && currentLapHandle < m_pLapStore->getMaxLaps())
    { 
     // loop until we reach the end of the lap store or we find an empty lap time
     while(currentLapHandle < m_pLapStore->getMaxLaps() && (m_pLapStore->getLapTime(currentLapHandle) != EMPTY_LAP_TIME))
     {
       currentLapHandle++;
     };
     
     // we found an invalid lap, so check the the next lap handle is less than the end of the lap store
     // and that the content of the next lap is an empty lap meaning it is free for us to use 
     if(((currentLapHandle+1)<m_pLapStore->getMaxLaps()) && (m_pLapStore->getLapTime(++currentLapHandle) == EMPTY_LAP_TIME))
     {
       // Yay ! we got two consecutive empty laps so lets set the firstLapHandle so we can start our new session.
       newSessionLapHandle = currentLapHandle;
     }
  }
}

return newSessionLapHandle;
}

void CLapTimes::setLapTime(lap_handle_t lapHandle,lap_time_t lapTime)
{
  m_pLapStore->setLapTime(lapHandle,lapTime);
}

lap_time_t CLapTimes::getLapTime(lap_handle_t lapHandle)
{
  return  m_pLapStore->getLapTime(lapHandle);
}

// scan through all of the recorded laps, total the number of sessions, total the number of laps
// recorded and return and indicative number of remaining laps - its indicative becuase
// each session requires on end of session marker so 10 sessions of 5 laps takes 60 laps 
// (10 * 5 laps + 10 end of session markers) one session of 5 sessions of 10 laps takes 55 laps
// (5 * 10 + 5 end of session markers)
void CLapTimes::getTotals(uint16_t &nSessions,uint16_t &nLapsRecorded,uint16_t &nLapsRemaining)
{
  lap_handle_t lapHandle = 0;
  nSessions = 0;
  nLapsRecorded = 0;
  nLapsRemaining = 0;
   
  while(lapHandle < m_pLapStore->getMaxLaps() && (m_pLapStore->getLapTime(lapHandle) != EMPTY_LAP_TIME))
  {
    // we have a session so count it
    nSessions++;
    // and count the laps within the session
    while(lapHandle < m_pLapStore->getMaxLaps() && (m_pLapStore->getLapTime(lapHandle++) != EMPTY_LAP_TIME))
    {
      nLapsRecorded++;
    }
  }
  
  nLapsRemaining = m_pLapStore->getMaxLaps() - nLapsRecorded;
}

void CLapTimes::clearAll()
{
  m_pLapStore->clearAll();
}

// This is similar to get totals but works within a session only, returns the average of all laps in the session, 
// the best lap and the total number of laps   
lap_handle_t CLapTimes::getSessionSummary(lap_handle_t lapHandle,uint16_t &nSessionAverage,uint16_t &nSessionBest,uint16_t &nSessionLapCount,uint16_t &nTotalTime)
{
  nSessionAverage = 0;
  nSessionBest = 0xFFFF;
  nSessionLapCount = 0;
  
  lap_time_t nLapTime = 0;
  nTotalTime = 0;

  while((INVALID_LAP_HANDLE != (nLapTime = m_pLapStore->getLapTime(lapHandle))) && (nLapTime != EMPTY_LAP_TIME))
  {
    nTotalTime += nLapTime;
    
    if(nLapTime < nSessionBest)
    {
      nSessionBest = nLapTime;
    }
    
    nSessionLapCount++;
    
    lapHandle++;
  }

  nSessionAverage = nTotalTime/nSessionLapCount;
  
  return nLapTime;
}

lap_handle_t CLapTimes::addLapTime(lap_handle_t lapHandle,lap_time_t lapTime)
{
  if(lapHandle < m_pLapStore->getMaxLaps())
  {
    m_pLapStore->setLapTime(lapHandle,lapTime);
  }
  else
  {
    lapHandle = INVALID_LAP_HANDLE;
  }
 
return lapHandle;
}
   
lap_handle_t CLapTimes::moveNext(lap_handle_t lapHandle)
{
  if(lapHandle < m_pLapStore->getMaxLaps())
  {
    lapHandle++;
  }
  else
  {
    lapHandle = INVALID_LAP_HANDLE;
  }
   
  return lapHandle;
}
   
lap_handle_t CLapTimes::movePrevious(lap_handle_t lapHandle)
{
  if(lapHandle >= 1)
  {
    lapHandle--;
  }
  else
  {
    lapHandle = INVALID_LAP_HANDLE;
  }
 
  return lapHandle;
}

// given a session number, find it the start of the session and return a handle to it   
lap_handle_t CLapTimes::getSessionHandle(uint8_t nSession)
{
  lap_handle_t currentLapHandle = 0;
  uint8_t nCurrentSession = 0;
  uint16_t nLapTime = 0;
   
  while(nCurrentSession != nSession && currentLapHandle < m_pLapStore->getMaxLaps())
  {
    // loop until we read the max laps or we find and empty lap
    do
    {
      currentLapHandle++;  
    }
    while((currentLapHandle) < m_pLapStore->getMaxLaps() && m_pLapStore->getLapTime(currentLapHandle) != EMPTY_LAP_TIME);
     
    nCurrentSession++;
     
    if(currentLapHandle < m_pLapStore->getMaxLaps())
    {
      // move next to step over the 0 terminator for the previous session    
      currentLapHandle++;
       
      // if the first lap of the session is empty there is no session
      // so return invalid lap to indicate no session found.
      if(getLapTime(currentLapHandle) == EMPTY_LAP_TIME)
      {
        currentLapHandle = INVALID_LAP_HANDLE;
      }
    }
    else
    { 
      currentLapHandle = INVALID_LAP_HANDLE;
    }
  }
   
  return currentLapHandle;
}

// 10 minutes is 600 seconds or 600,000 milli seconds, this is too big to fit into a uint32_t
// so we divide by 10 to convert the value into a lap_time_t which contains the lap time in 100's
// of seconds.
lap_time_t CLapTimes::convertMillisToLapTime(uint32_t ulTime)
{
  return ulTime/10;
}

// turn a lap_time_t into a time string formatted as - m:ss:dd
// bPrecision turns 100's on or off
char* CLapTimes::formatTime(lap_time_t time,unsigned char bPrecision)
{
  char *pResult = NULL;
 
  lap_time_t nSeconds = time/100;
  lap_time_t nMinutes = nSeconds/60;
  lap_time_t nHundredths = 0;
 
  if(nMinutes <= 9)
  {
    if(bPrecision)
    {     
      nHundredths = time - (nSeconds*100);
    }

    nSeconds -= (nMinutes * 60);   
 
    m_pTimeStringBuffer[7] = 0;
    m_pTimeStringBuffer[6] = (nHundredths%10)+'0';
    m_pTimeStringBuffer[5] = (nHundredths/10)+'0';
    m_pTimeStringBuffer[4] = '.';
    m_pTimeStringBuffer[3] = (nSeconds%10)+'0';
    m_pTimeStringBuffer[2] = (nSeconds/10)+'0';
    m_pTimeStringBuffer[1] = ':';
    m_pTimeStringBuffer[0] = nMinutes + '0';    
    
    pResult = m_pTimeStringBuffer;
  }
  
  return pResult;
}
  
char CLapTimes::m_pTimeStringBuffer[9];/*m:ss:dd - dd represents hundredths of a second */




