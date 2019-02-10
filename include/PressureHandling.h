#include <Arduino.h>
#include <../PID/PID_v1.h>
#include <../Adafruit-BMP085-Library-master/Adafruit_BMP085.h>

class PressureHandling
{
  private:
    /* data */
    static const uint8_t _pumpPin = A1;
    double SetpointPre, OutputPre, ambientPre, pressureSetting = 20000.0;
    unsigned long pressTimeIntegral;
    double InputPre = 100000.0;
    unsigned int WindowSizePre = 300;
    unsigned long windowStartTimePre;

    unsigned long currentIntegralTime, previousIntegralTime;
    PID myPIDPre;
    Adafruit_BMP085 bmp;

    unsigned long collectionTime = 10000, _injectionTime = 5000;

    bool _pressureIndicator = false;
    bool _pressureActionStatus = false;
    bool _performcollectionFlag = false;
    bool _collectionInitFlag = false;
    bool _injectionFlag = false;
    bool _performInjectionFlag = false;

  public:
    PressureHandling(/* args */);
    ~PressureHandling();
    void pressure_action();
    void regulate_pressure();
    void PIDinit();
    void pressure_init();
    void pump_on();
    void pump_off();
    void calcPressTimeInteg();

    void set_injection();
    void set_collection();
    bool get_collection_status();
    bool get_injection_status();

    bool collectSample();
    bool inject_sample();

    double get_pressure();

    void set_injection_time(unsigned long time);
    void set_collection_time(unsigned long time);
};

PressureHandling::PressureHandling(/* args */)
    : myPIDPre(&InputPre, &OutputPre, &SetpointPre, 3, 5, 2, REVERSE), bmp()
{
}

PressureHandling::~PressureHandling()
{
}

bool PressureHandling::get_collection_status()
{
    return _performcollectionFlag;
}

bool PressureHandling::get_injection_status()
{
    return _performInjectionFlag;
}

void PressureHandling::set_collection()
{
    _collectionInitFlag = true;
    _performcollectionFlag = true;
}

void PressureHandling::set_injection()
{
    _injectionFlag = true;
    _performInjectionFlag = true;
}

void PressureHandling::pump_on()
{
    digitalWrite(_pumpPin, LOW);
}

void PressureHandling::pump_off()
{
    digitalWrite(_pumpPin, HIGH);
}

void PressureHandling::pressure_init()
{
    pinMode(_pumpPin, OUTPUT);
    digitalWrite(_pumpPin, HIGH);
    bmp.begin();
    InputPre = bmp.readPressure();
    ambientPre = InputPre;
    // Serial.println(ambientPre);
}

void PressureHandling::pressure_action()
{
    if (/*_pressureIndicator == false &&*/ _pressureActionStatus == true)
    {
        regulate_pressure();
    }
}

void PressureHandling::regulate_pressure()
{
    InputPre = bmp.readPressure();
    // Serial.println(InputPre);
    myPIDPre.Compute();
    if (millis() - windowStartTimePre > WindowSizePre) //Temperature PID
    {                                                  //time to shift the Relay Window
        windowStartTimePre += WindowSizePre;
        //    Serial.println("PIDas1");
    }
    if (OutputPre < millis() - windowStartTimePre)
    {
        digitalWrite(_pumpPin, HIGH);
    }
    else
    {
        digitalWrite(_pumpPin, LOW);
    }
}

void PressureHandling::calcPressTimeInteg()
{
    // if (_pressureActionStatus == true)
    // {
    currentIntegralTime = millis();
    unsigned long integPart = (currentIntegralTime - previousIntegralTime) * long((ambientPre - InputPre) / 1000.0);
    pressTimeIntegral = pressTimeIntegral + integPart;
    previousIntegralTime = currentIntegralTime;
    // }
}

void PressureHandling::PIDinit()
{
    windowStartTimePre = millis();
    InputPre = bmp.readPressure();
    Serial.println(InputPre);
    ambientPre = InputPre;
    pressTimeIntegral = 0.0;
    SetpointPre = ambientPre - pressureSetting;
    //turn the PID on
    myPIDPre.SetOutputLimits(0, WindowSizePre);
    myPIDPre.SetMode(AUTOMATIC);
}

bool PressureHandling::collectSample()
{
    // if (_performcollectionFlag)
    // {
    PIDinit();
    while (pressTimeIntegral < (collectionTime / 1000) * long(pressureSetting))
    {
        regulate_pressure();
        calcPressTimeInteg();
        // return false;
    }
    // _performcollectionFlag = false;
    pressTimeIntegral = 0;
    return true;
    // }
}

double PressureHandling::get_pressure()
{
    return bmp.readPressure();
}

bool PressureHandling::inject_sample()
{
    PIDinit();
    while (pressTimeIntegral < (_injectionTime / 1000) * long(pressureSetting))
    {
        regulate_pressure();
        calcPressTimeInteg();
        // return false;
    }
    // _performcollectionFlag = false;
    pressTimeIntegral = 0;
    return true;
}

void PressureHandling::set_injection_time(unsigned long time)
{
    _injectionTime = time;
}

void PressureHandling::set_collection_time(unsigned long time)
{
    collectionTime = time;
}