// If we assume that lap data will always be set to 0
// Session ends will always be 0
// we only ever return invalid handle

//*******************************************************************************************
// Lap storage and retreival definitions
//*******************************************************************************************
#define EMPTY_LAP_TIME 0
#define INVALID_LAP_HANDLE 0XFFFF

typedef uint16_t lap_handle_t;
typedef uint16_t lap_time_t;

//*******************************************************************************************
// ILapStore 
//
// Defines a pure virtual class (C++ Terminology) or interface (Java Terminology)
// It simply defines the functions that can be used to get, set and clear laps
// in a lap store.
//
// The following lap store is provided
// 1) CEEPromLapStore - this one stores laps in the EEPROM 
// If you wanted to add SD Card Storage you could define a new class CSDCardLapStore
// 
//*******************************************************************************************
class ILapStore
{
public:
  virtual lap_time_t getLapTime(lap_handle_t lapHandle) = 0;
  virtual void setLapTime(lap_handle_t lapHandle,lap_time_t lapTime) = 0;
  virtual void clearAll() = 0;
  virtual uint16_t getMaxLaps() = 0;
};

//*******************************************************************************************
// CEEPROMLapStore 
//
// Store laps in memory -
//
// For - simple, easy, its just using an array in memory
// Against - lose all laps if power is lost or Arduino is reset
//
//*******************************************************************************************
#define EEPROM_LAP_STORE_MAX_LAPS 100

class CEEPROMLapStore : public ILapStore
{
public:
 virtual lap_time_t getLapTime(lap_handle_t lapHandle);
 virtual void setLapTime(lap_handle_t lapHandle,lap_time_t lapTime);
 virtual void clearAll();
 virtual uint16_t getMaxLaps();
protected:
 lap_time_t m_LapTimes[EEPROM_LAP_STORE_MAX_LAPS];
};

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
class CLapTimes
{
public:
  CLapTimes(ILapStore *pLapStore);
  lap_handle_t createNewSession();
  void setLapTime(lap_handle_t lapHandle,lap_time_t lapTime);
  lap_time_t getLapTime(lap_handle_t lapHandle);
  void getTotals(uint16_t &nSessions,uint16_t &nLapsRecorded,uint16_t &nLapsRemaining);
  void clearAll();
  lap_handle_t getSessionSummary(lap_handle_t lapHandle,uint16_t &nSessionAverage,uint16_t &nSessionBest,uint16_t &nSessionLapCount,uint16_t &nTotalTime);
  lap_handle_t addLapTime(lap_handle_t lapHandle,lap_time_t lapTime);
  lap_handle_t moveNext(lap_handle_t lapHandle);
  lap_handle_t movePrevious(lap_handle_t lapHandle);
  lap_handle_t getSessionHandle(uint8_t nSession);
  static lap_time_t convertMillisToLapTime(uint32_t ulTime);
  static char* formatTime(lap_time_t time,unsigned char bPrecision);
protected:
  ILapStore *m_pLapStore;
public:
  static char m_pTimeStringBuffer[9];/*m:ss:dd - dd represents hundredths of a second */
};


