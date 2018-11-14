#include "checkvalidtime.h"
#include <TimeLib.h>
#include <Arduino.h>

CheckValidTime::CheckValidTime()
{
  
}

void CheckValidTime::begin(unsigned long starttime, unsigned long stoptime)
{
  _starttime = starttime;
  _stoptime = stoptime;
}

void CheckValidTime::setState(bool bstart, bool bstop, bool bcurrent, bool force)
{
  _bstart = bstart;
  _bstop = bstop;
  _bcurrent = bcurrent;
  _force = force;

  Serial.print("start stop current force : ");
  Serial.print(_bstart); Serial.print(" ");
  Serial.print(_bstop); Serial.print(" ");
  Serial.print(_bcurrent); Serial.print(" ");
  Serial.print(_force); Serial.println(" ");
}

void CheckValidTime::printState()
{
  Serial.print("start: "); Serial.print(_bstart); Serial.print(" ");
  Serial.print("stop: "); Serial.print(_bstop); Serial.print(" ");
  Serial.print("current: "); Serial.print(_bcurrent); Serial.print(" ");
  Serial.print("force: "); Serial.print(_force); Serial.println(" ");
  
}

void CheckValidTime::setOverlap(int overlap)
{
  _overlap = overlap;
}

void CheckValidTime::setTimerMode(bool TIMER)
{
  _TIMER = TIMER;
}

int CheckValidTime::getRelayState()
{
  return _turnon;
}

void CheckValidTime::check()
{
  if (_bstart && _bstop && _bcurrent && !_force) {
    if ( (_currenttime >= _starttime) && (_currenttime <= _stoptime) ) {
      if (!_ON) {
        relayOn();
      }
    }
    else if (_overlap && _TIMER != 1) {
      // day 0 when currenttime >= starttime or currenttime < stoptime turn relay on
        // ON
      if ((_currenttime >= _starttime) || (_currenttime < _stoptime) ) {
        if (!_ON) {
          relayOn();
        }
      }
      // day 0+1 at midnight currenttime <= starttime
      // day 0+1 when currenttime >= stoptime turn relay off
        // OFF
      else if (_currenttime >= _stoptime) {
        if (_ON) {
          relayOff();
        }
      }
    }
    else if (_ON) {
      relayOff();
    }
  }
}

void CheckValidTime::relayOn()
{
  _turnon = 1;
  _ON = true;
}

void CheckValidTime::relayOff()
{
  _turnon = 0;
  _ON = false;
}

void CheckValidTime::run()
{
  _currenttime = (unsigned long) now();
  check();
}
