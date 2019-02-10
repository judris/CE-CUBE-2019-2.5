#include <Arduino.h>
#include <Servo.h>
#include <StepperHandling.h>
#include <PressureHandling.h>
#include <C4Dhandling.h>

class CEsystem : public StepperHandling, public PressureHandling, public C4Dsystem
{
  private:
    /* data */
    Servo _myservo;
    /**
   * HV, pump and valves pins
   */
    static const uint8_t _HVpin = A3;

    static const uint8_t _valvesPin = A2;
    static const uint8_t _samplingValve = A0;
    static const uint8_t _servoPin = 8;

    /**
     * servo stuff
     */
    int _servoWaitingTime = 400;
    unsigned long _servoCurrentTime = 0;

    bool _injectionMethod = true;
    bool _LiftPlatform = false;
    float _vialResolution = 4076.0 / 12.0;

    /**
     * conditioning properties
     */

    unsigned long _conditioningTime = 180000, _flushTime = 40000, _vacuumFlushTime = 25000;

  public:
    CEsystem(/* args */);
    ~CEsystem();
    void setValvesON();
    void setValvesOFF();
    void setPumpON();
    void setPumpOFF();
    bool LiftUp();
    bool LiftDown();
    bool servoOff();
    void HVON();
    void HVOFF();
    bool startSampling();
    bool stopSampling();
    bool deaerateSample();
    bool performInjection();
    //   setOnElectroConditioning();
    //   setOffElectroConditioning();
    //   setOnPressureFlush();
    //   setOffPressureFlush();
    //   setOnVacuumFlush();
    //   setOffVacuumFlush();
    bool carouselClock();
    bool carouselCounter();
    bool adjustClock();
    bool adjustCounter();
    void setHydrodynamicInjection();
    void setElectrokineticInjection();
    void setOnElectroConditioning();
    void setOffElectroConditioning();
    void setOnPressureFlush();
    void setOffPressureFlush();
    void setOnVacuumFlush();
    void setOffVacuumFlush();

    void init_modules();
    bool get_activity_status();
};

CEsystem::CEsystem(/* args */)
{
    stepper_init();
}

CEsystem::~CEsystem()
{
}

bool CEsystem::get_activity_status()
{
    if (get_carousel_status() || _LiftPlatform)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void CEsystem::init_modules()
{
    pinMode(_valvesPin, OUTPUT);
    pinMode(_HVpin, OUTPUT);
    pinMode(_samplingValve, OUTPUT);
    // pinMode(_servoPin, OUTPUT);
    digitalWrite(_valvesPin, LOW);
    digitalWrite(_HVpin, HIGH);
    digitalWrite(_samplingValve, LOW);
    pressure_init();
    stepper_init();
    _myservo.attach(_servoPin);
    _myservo.write(180);
    delay(500);
    _myservo.detach();
    // init_C4D();
}

bool CEsystem::carouselClock()
{
    set_carousel(false, _vialResolution);
    while (!turnCarousel())
    {
        turnCarousel();
    }
    turnOffMotor();
    return true;
}

bool CEsystem::carouselCounter()
{
    set_carousel(true, _vialResolution);
    while (!turnCarousel())
    {
        turnCarousel();
    }
    turnOffMotor();
    return true;
}

bool CEsystem::adjustClock()
{
    set_carousel(false, _vialResolution / 15);
    while (!turnCarousel())
    {
        turnCarousel();
    }
    turnOffMotor();
    return true;
}
bool CEsystem::adjustCounter()
{
    set_carousel(true, _vialResolution / 15);
    while (!turnCarousel())
    {
        turnCarousel();
    }
    turnOffMotor();
    return true;
}

void CEsystem::setValvesON()
{
    digitalWrite(_valvesPin, HIGH);
}

void CEsystem::setValvesOFF()
{
    digitalWrite(_valvesPin, LOW);
}

void CEsystem::setPumpON()
{
    pump_on();
}

void CEsystem::setPumpOFF()
{
    setValvesON();
    delay(100);
    pump_off();
    setValvesOFF();
    delay(100);
    setValvesON();
    delay(100);
    setValvesOFF();
    delay(100);
    setValvesON();
    delay(100);
    setValvesOFF();
}

bool CEsystem::LiftUp()
{
    // _LiftPlatform = true;
    _servoCurrentTime = millis();
    _myservo.attach(_servoPin);
    _myservo.write(55);
    while (!servoOff())
    {
        servoOff();
    }
    return true;
}

bool CEsystem::LiftDown()
{
    // _LiftPlatform = true;
    _servoCurrentTime = millis();
    _myservo.attach(_servoPin);
    _myservo.write(180);
    while (!servoOff())
    {
        servoOff();
    }
    return true;
}

bool CEsystem::servoOff()
{
    // if (_LiftPlatform == true && millis() > _servoCurrentTime + _servoWaitingTime)
    while (millis() < _servoCurrentTime + _servoWaitingTime)
    {
    }
    _myservo.detach();
    // _LiftPlatform = false;
    // cycleTask++;
    // Serial.println("servo off");
    return true;
}

void CEsystem::HVON()
{
    digitalWrite(_HVpin, LOW);
}

void CEsystem::HVOFF()
{
    digitalWrite(_HVpin, HIGH);
}

bool CEsystem::startSampling()
{
    digitalWrite(_samplingValve, HIGH);
    while (!collectSample())
    {
        collectSample();
    }
    setPumpOFF();
    deaerateSample();
    return true;
}

bool CEsystem::stopSampling()
{
    digitalWrite(_samplingValve, LOW);
    // performcollectionFlag = false;
    // collectionInitFlag = false;
    // pressureActionStatus = false;
    setPumpOFF();
    deaerateSample();

    return true;
}

bool CEsystem::deaerateSample()
{
    digitalWrite(_samplingValve, LOW);
    delay(200);
    digitalWrite(_samplingValve, HIGH);
    delay(200);
    digitalWrite(_samplingValve, LOW);
    delay(200);
    digitalWrite(_samplingValve, HIGH);
    delay(200);
    digitalWrite(_samplingValve, LOW);
    delay(200);
    digitalWrite(_samplingValve, HIGH);
    delay(200);
    return true;
}

void CEsystem::setHydrodynamicInjection()
{
    _injectionMethod = true;
}

void CEsystem::setElectrokineticInjection()
{
    _injectionMethod = false;
}

bool CEsystem::performInjection()
{
    digitalWrite(_samplingValve, HIGH);
    while (!inject_sample())
    {
        inject_sample();
    }
    setPumpOFF();
    return true;
}