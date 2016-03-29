/*
  time.c - low level time and date functions
  Copyright (c) Michael Margolis 2009-2014

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  1.0  6  Jan 2010 - initial release
  1.1  12 Feb 2010 - fixed leap year calculation error
  1.2  1  Nov 2010 - fixed setTime bug (thanks to Korman for this)
  1.3  24 Mar 2012 - many edits by Paul Stoffregen: fixed timeStatus() to update
                     status, updated examples for Arduino 1.0, fixed ARM
                     compatibility issues, added TimeArduinoDue and TimeTeensy3
                     examples, add error checking and messages to RTC examples,
                     add examples to DS1307RTC library.
  1.4  5  Sep 2014 - compatibility with Arduino 1.5.7

  2.0  4  Mar 2016 - switch to 64b time
*/

//#if ARDUINO >= 100 // this >= is causing an error TODO: fix
#include <Arduino.h> 
//#else
//#include <WProgram.h> 
//#endif

#include <limits.h>
#include "Time.h"

static tmElements_t tm;          // a cache of time elements
static time_t cacheTime;   // the time the cache was updated
static uint32_t syncInterval = 300;  // time sync will be attempted after this many seconds

// drift correction per system clock second
const uint8_t DRIFT_SMOOTHING_FACTOR = 180; // 180/256 = 0.7, finite response filter
int32_t driftCorrection = 0;  // fract seconds per second of system time
uint32_t lastSyncSec = 0;

void refreshCache(time_t t) {
  if (t != cacheTime) {
    breakTime(t, tm); 
    cacheTime = t; 
  }
}

uint8_t hour() { // the hour now 
  return hour(now()); 
}

uint8_t hour(time_t t) { // the hour for the given time
  refreshCache(t);
  return tm.Hour;  
}

uint8_t hourFormat12() { // the hour now in 12 hour format
  return hourFormat12(now()); 
}

uint8_t hourFormat12(time_t t) { // the hour for the given time in 12 hour format
  refreshCache(t);
  if( tm.Hour == 0 )
    return 12; // 12 midnight
  else if( tm.Hour  > 12)
    return tm.Hour - 12 ;
  else
    return tm.Hour ;
}

uint8_t isAM() { // returns true if time now is AM
  return !isPM(now()); 
}

uint8_t isAM(time_t t) { // returns true if given time is AM
  return !isPM(t);  
}

uint8_t isPM() { // returns true if PM
  return isPM(now()); 
}

uint8_t isPM(time_t t) { // returns true if PM
  return (hour(t) >= 12); 
}

uint8_t minute() {
  return minute(now()); 
}

uint8_t minute(time_t t) { // the minute for the given time
  refreshCache(t);
  return tm.Minute;  
}

uint8_t second() {
  return second(now()); 
}

uint8_t second(time_t t) {  // the second for the given time
  refreshCache(t);
  return tm.Second;
}

uint8_t msec(){          // the second now 
  return msec(now());
}

uint8_t msec(time_t t){  // the second for the given time
  refreshCache(t);
  return tm.mSec;
}

uint8_t day(){
  return(day(now())); 
}

uint8_t day(time_t t) { // the day for the given time (0-6)
  refreshCache(t);
  return tm.Day;
}

uint8_t weekday() {   // Sunday is day 1
  return  weekday(now()); 
}

uint8_t weekday(time_t t) {
  refreshCache(t);
  return tm.Wday;
}
   
uint8_t month(){
  return month(now()); 
}

uint8_t month(time_t t) {  // the month for the given time
  refreshCache(t);
  return tm.Month;
}

uint16_t year() {  // as in Processing, the full four digit year: (2009, 2010 etc) 
  return year(now()); 
}

uint16_t year(time_t t) { // the year for the given time
  refreshCache(t);
  return tmYearToCalendar(tm.Year);
}

/*============================================================================*/	
/* functions to convert to and from system time */
/* These are for interfacing with time serivces and are not normally needed in a sketch */

// leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)     ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )

static  const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0
 
void breakTime(time_t timeInput, tmElements_t &tm){
// break the given time_t into time components
// this is a more compact version of the C library localtime function
// note that year is offset from 1970 !!!

  uint8_t year;
  uint8_t month, monthLength;
  uint32_t time;
  unsigned long days;

  tm.mSec = toMSecs(timeInput); 
  time = toSecs(timeInput);
  tm.Second = time % 60;
  time /= 60; // now it is minutes
  tm.Minute = time % 60;
  time /= 60; // now it is hours
  tm.Hour = time % 24;
  time /= 24; // now it is days
  tm.Wday = ((time + 4) % 7) + 1;  // Sunday is day 1 
  
  year = 0;  
  days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  tm.Year = year; // year is offset from 1970 
  
  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // now it is days in this year, starting at 0
  
  days=0;
  month=0;
  monthLength=0;
  for (month=0; month<12; month++) {
    if (month==1) { // february
      if (LEAP_YEAR(year)) {
        monthLength=29;
      } else {
        monthLength=28;
      }
    } else {
      monthLength = monthDays[month];
    }
    
    if (time >= monthLength) {
      time -= monthLength;
    } else {
        break;
    }
  }
  tm.Month = month + 1;  // jan is month 1  
  tm.Day = time + 1;     // day of month
}

time_t makeTime(tmElements_t &tm){   
// assemble time elements into time_t 
// note year argument is offset from 1970 (see macros in time.h to convert to other formats)
// previous version used full four digit year (or digits since 2000),i.e. 2009 was 2009 or 9
  
  int i;
  uint32_t seconds;

  // TODO switch to (year/4)*(4*SECS_PER_YEAR+SEC_PER_DAY) and only check leap year for last 4 years
  // seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds= tm.Year*(SECS_PER_YEAR); 
  for (i = 0; i < tm.Year; i++) {
    if (LEAP_YEAR(i)) {
      seconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }
  
  // add days for this year, months start from 1
  for (i = 1; i < tm.Month; i++) {
    if ( (i == 2) && LEAP_YEAR(tm.Year)) { 
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
  }
  seconds+= (tm.Day-1) * SECS_PER_DAY;
  seconds+= tm.Hour * SECS_PER_HOUR;
  seconds+= tm.Minute * SECS_PER_MIN;
  seconds+= tm.Second;

  uint32_t fracSecs = (((uint32_t)tm.mSec)*UINT_MAX/1000)<<16;
  return (time_t)(((uint64_t)seconds<<32) + fracSecs); 
}
/*=====================================================*/	
/* Low level system time functions  */

static time_t sysTime = 0;    // 32b sec, 32b frac sec
static uint32_t prevMillis = 0;
static uint32_t nextSyncTime = 0; // in straight seconds
static timeStatus_t Status = timeNotSet;

getExternalTime getTimePtr;  // pointer to external sync function
//setExternalTime setTimePtr; // not used in this version

#ifdef TIME_DRIFT_INFO   // define this to get drift data
time_t sysUnsyncedTime = 0; // the time sysTime unadjusted by sync  
#endif

time_t now() {
  return now(1);
}

time_t now( uint8_t allowSync ) {
  uint32_t dt_ms = millis() - prevMillis;
#ifdef TIME_DRIFT_INFO
  uint32_t sysUnsyncedSec = 0; // this can be compared to the synced time to measure long term drift     
#endif
  uint32_t sysSec = 0;
	// calculate number of seconds passed since last call to now()
  while ( dt_ms >= 1000) {
		// millis() and prevMillis are both unsigned ints thus the subtraction will always be the absolute value of the difference
    sysSec++;
    prevMillis += 1000;	
    adjustTime(((int64_t)driftCorrection) << 1); // need to double because of the precision of a int is 1 less than a uint
#ifdef TIME_DRIFT_INFO
    sysUnsyncedSec++; // this can be compared to the synced time to measure long term drift     
#endif
    dt_ms = millis() - prevMillis;
  }

  //prevMillis += dt_ms;// report fraction seconds but dont increment with them
  uint32_t fracSecs = (uint32_t)((((uint64_t)dt_ms)*ULONG_MAX)/1000);
#ifdef TIME_DRIFT_INFO
  //sysUnsyncedTime += (time_t)(((time_t)sysUnsyncedSec<<32) + fracSecs);
  sysUnsyncedTime += (time_t)(((time_t)sysUnsyncedSec<<32));
#endif
  //sysTime += (time_t)(((time_t)sysSec<<32) + fracSecs);
  // report fraction seconds but dont increment with them
  sysTime += (time_t)(((time_t)sysSec<<32));

  // if its time to update do it, if we are allowed
  sysSec = toSecs(sysTime);
  if ( (nextSyncTime <= sysSec) && allowSync ){
    if (getTimePtr != 0) {
      time_t t = getTimePtr();
      if (t != 0) {
        time_t unsyncedSysTime = now(0);  // save current time for comparison to network time
        setTime(t);
        uint32_t t_sec = toSecs(t);
        if( lastSyncSec > 0 ){
          int64_t time_error = t - unsyncedSysTime;
          int32_t time_drift = (int32_t)(time_error/(t_sec - lastSyncSec)); // per second
          // drift correction has a finite response filter
          driftCorrection = (int32_t)((DRIFT_SMOOTHING_FACTOR*(int64_t)driftCorrection)/UCHAR_MAX);
          driftCorrection += (int32_t)(((UCHAR_MAX - DRIFT_SMOOTHING_FACTOR)*(int64_t)time_drift)/UCHAR_MAX);
        }
        lastSyncSec = t_sec;
      } else {
        nextSyncTime = sysSec + syncInterval;
        Status = (Status == timeNotSet) ?  timeNotSet : timeNeedsSync;
      }
    }
  }  
  // report fraction seconds but dont increment with them
  return (time_t)sysTime + fracSecs;
}

void setTime(time_t t) { 
#ifdef TIME_DRIFT_INFO
 if(sysUnsyncedTime == 0) 
   sysUnsyncedTime = t;   // store the time of the first call to set a valid Time   
#endif

  prevMillis = millis();  // restart counting from now (thanks to Korman for this fix)
  sysTime = (time_t)t;  
  nextSyncTime = toSecs(sysTime) + syncInterval;
  Status = timeSet;
} 

void setTime(uint8_t hr,uint8_t min,uint8_t sec, uint16_t msec, uint8_t dy, uint8_t mnth, uint16_t yr){
 // year can be given as full four digit year or two digts (2010 or 10 for 2010);  
 //it is converted to years since 1970
  if( yr > 99)
      yr = yr - 1970;
  else
      yr += 30;  
  tm.Year = yr;
  tm.Month = mnth;
  tm.Day = dy;
  tm.Hour = hr;
  tm.Minute = min;
  tm.Second = sec;
  tm.mSec = msec;
  setTime(makeTime(tm));
}

void adjustTime(time_t adjustment) {
  sysTime += adjustment;
}

// indicates if time has been set and recently synchronized
timeStatus_t timeStatus() {
  now(); // required to actually update the status
  return Status;
}

void setSyncProvider( getExternalTime getTimeFunction){
  getTimePtr = getTimeFunction;  
  nextSyncTime = sysTime;
  now(); // this will sync the clock
}

void setSyncInterval(uint32_t interval){ // set the number of seconds between re-sync
  syncInterval = (uint32_t)interval;
  nextSyncTime = toSecs(sysTime) + syncInterval;
}

int32_t getDriftCorrection(){
  return driftCorrection;
}
