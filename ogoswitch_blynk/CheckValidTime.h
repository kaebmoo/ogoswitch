#ifndef checkvalidtime_h
#define checkvalidtime_h

class CheckValidTime
{
  public:
    CheckValidTime();
    void begin(unsigned long starttime, unsigned long stoptime);
    void run();
    void setState(bool bstart, bool bstop, bool bcurrent, bool force);
    void setOverlap(int overlap);
    void setTimerMode(bool TIMER);
    int getRelayState();
    void relayOn();
    void relayOff();

  private:
    
    void check();
    bool _bstart = false;
    bool _bstop = false;
    bool _bcurrent = false;
    bool _force = false;
    unsigned long _starttime = 0;
    unsigned long _stoptime = 0;
    unsigned long _currenttime = 0;
    bool _ON = false;
    bool _TIMER = false; // timer mode
    int _overlap = 0;
    int _turnon = 0;  // turn relay on or off
};


#endif
