#include <Arduino.h>

class CurrentHandling
{
  private:
    /* data */
    float _current;
    bool read_current_flag = true;
    static const uint8_t _currentSensePin = A6;

  public:
    CurrentHandling(/* args */);
    ~CurrentHandling();
    float get_current();
    void read_current();
    const int _avgNum = 100;
};

CurrentHandling::CurrentHandling(/* args */)
{
}

CurrentHandling::~CurrentHandling()
{
}

float CurrentHandling::get_current()
{
    return _current;
}

void CurrentHandling::read_current()
{
    int Creading[_avgNum];
    unsigned long AVG1 = 0;
    for (int i = 0; i < _avgNum; i++)
    {
        Creading[i] = analogRead(_currentSensePin);
        delayMicroseconds(100);
        AVG1 = AVG1 + Creading[i];
    }
    _current = float(AVG1) / float(_avgNum);
    _current = _current * 0.007229;
}