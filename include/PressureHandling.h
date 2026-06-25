#include <Arduino.h>
#include <../PID/PID_v1.h>
#include <../Adafruit-BMP085-Library-master/Adafruit_BMP085.h> //works
// #include <../SparkFun_BME280/src/SparkFunBME280.h>
// #include <../Adafruit_BMP280_Library-master/Adafruit_BMP280.h> //works

class PressureHandling
{
  private:
    /* data */
    static const uint8_t _pumpPin = A1; //A1 CE CUBE2.5, CE CUBE2 0//
    double SetpointPre, OutputPre, ambientPre = 100000.0, pressureSetting = 20000.0;
    unsigned long pressTimeIntegral;
    double InputPre = 100000.0;
    unsigned int WindowSizePre = 300;
    unsigned long windowStartTimePre;

    unsigned long currentIntegralTime, previousIntegralTime;
    PID myPIDPre, myPIDbge;
    Adafruit_BMP085 bmp; //works
    // BME280 bmp280;
    // Adafruit_BMP280 bmp; //works
    // bool bmp180_flag = false;

    unsigned long _collectionTime = 10000, _injectionTime = 10000;

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
    void regulate_vacuum(), regulate_pressure();
    void PIDinit(), PID_bge_init();
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

    bool generate_droplet(unsigned long gen_time);

    double get_pressure();

    void set_injection_time(unsigned long time);
    void set_collection_time(unsigned long time);

    float get_ambient_pressure();
};

PressureHandling::PressureHandling(/* args */)
    : myPIDPre(&InputPre, &OutputPre, &SetpointPre, 3, 5, 2, REVERSE),
    myPIDbge(&InputPre, &OutputPre, &SetpointPre, 1, 2, 0, DIRECT),
     bmp()
{
}

PressureHandling::~PressureHandling()
{
}

float PressureHandling::get_ambient_pressure()
{
    // InputPre = bmp.readPressure();
    return ambientPre; //InputPre  ambientPre
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
    // if (bmp180_flag == true)
    // {
        bmp.begin();
        InputPre = bmp.readPressure();
    // }
    // else
    // {
    //     bme.begin();
    //     InputPre = bme.readPressure();
    // }
    
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

void PressureHandling::regulate_vacuum()
{
    // if (bmp180_flag == true)
    // {
        InputPre = bmp.readPressure();
    // }
    // else
    // {
    //     InputPre = bme.readPressure();
    // }
    // InputPre = bmp.readPressure();
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

void PressureHandling::regulate_pressure()
{
    InputPre = get_pressure();
    // Serial.println(InputPre);
    myPIDbge.Compute();
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
    unsigned long timeDelta = currentIntegralTime - previousIntegralTime;
    unsigned long integPart = timeDelta * long(abs(InputPre - ambientPre) / 1000);
    pressTimeIntegral = pressTimeIntegral + integPart;
    previousIntegralTime = currentIntegralTime;

    // Serial.print(timeDelta);
    // Serial.print(" ");
    // Serial.println(pressTimeIntegral);
    // }
}

void PressureHandling::PIDinit()
{
    windowStartTimePre = millis();
    InputPre = bmp.readPressure();
    // if (bmp180_flag == true)
    // {
        InputPre = bmp.readPressure();
    // }
    // else
    // {
    //     InputPre = bme.readPressure();
    // }
    // Serial.println(InputPre);
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
    while (pressTimeIntegral < (_collectionTime / 1000) * long(pressureSetting))
    {
        regulate_vacuum();
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
    // if (bmp180_flag == true)
    // {
        return bmp.readPressure();
    // }
    // else
    // {
    //     return bme.readPressure();
    // }
    // return bmp.readPressure();
}

bool PressureHandling::inject_sample()
{
    PIDinit();
    while (pressTimeIntegral < (_injectionTime / 1000) * long(pressureSetting))
    {
        regulate_vacuum();
        calcPressTimeInteg();
        // return false;
    }
    // _performcollectionFlag = false;
    pressTimeIntegral = 0;
    return true;
}

void PressureHandling::PID_bge_init()
{
    windowStartTimePre = millis();
    InputPre = get_pressure();
    // Serial.println(InputPre);
    ambientPre = InputPre;
    pressTimeIntegral = 0.0;
    SetpointPre = ambientPre + 40000; //SetpointPre = ambientPre + pressureSetting;
    //turn the PID on
    myPIDbge.SetOutputLimits(0, WindowSizePre);
    myPIDbge.SetMode(AUTOMATIC);
}

void PressureHandling::set_injection_time(unsigned long time)
{
    _injectionTime = time;
}

void PressureHandling::set_collection_time(unsigned long time)
{
    _collectionTime = time;
}

bool PressureHandling::generate_droplet(unsigned long gen_time)
{
    PID_bge_init();
    while (pressTimeIntegral < gen_time * long(pressureSetting))
    {
        regulate_pressure();
        calcPressTimeInteg();
        // return false;
    }
    // _performcollectionFlag = false;
    // Serial.print(pressTimeIntegral);
    // Serial.print("    ");
    // Serial.println((gen_time * long(pressureSetting)));
    pressTimeIntegral = 0;
    return true;
}