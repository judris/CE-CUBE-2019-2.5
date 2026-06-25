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
    static const uint8_t _HVpin = A3; //CE CUBE 2.5 A3 ; CE CUBE2 7

    static const uint8_t _valvesPin = A2;     //CE CUBE 2.5 A2 ; CE CUBE2 1
    static const uint8_t _samplingValve = A0; //CE CUBE 2.5 A0 ; CE CUBE2 A1
    // static const uint8_t _replenishValve = A0; //CE CUBE 2.5 A0 ; CE CUBE2.6 7
    static const uint8_t _servoPin = 8;            //CE CUBE 2.5 8 ; CE CUBE2 8
    static const uint8_t _replenish_servo_pin = 1; // CE CUBE 2.7

    /**
     * servo stuff
     */
    int _servoWaitingTime = 500;
    int replenish_servo_up_pos = 85;
    unsigned long _servoCurrentTime = 0;

    bool _injectionMethod = true;
    bool _LiftPlatform = false;
    float _vialResolution = 4076.0 / 12.0;

    /**
     * conditioning properties
     */

    unsigned long _conditioningTime = 180000, _flushTime = 40000, _vacuumFlushTime = 25000;
    unsigned long _bge_gen_time = 180;

public:
    CEsystem(/* args */);
    ~CEsystem();
    void setValvesON();
    void setValvesOFF();
    void smplVlve_ON();
    void smplVlve_OFF();
    void setPumpON();
    void setPumpOFF();
    bool LiftUp(), replenish_servo_up();
    bool LiftDown(), replenish_servo_down();
    bool servoOff();
    void HVON();
    void HVOFF();
    bool startSampling();
    bool stopSampling();
    bool deaerateSample();

    bool replenish(), drain();

    bool make_bge_droplet();
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
    // void setHydrodynamicInjection();
    // void setElectrokineticInjection();
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
    digitalWrite(_valvesPin, LOW); //CE CUBE 2.5 LOW, CE CUBE2 HIGH
    digitalWrite(_HVpin, HIGH);
    digitalWrite(_samplingValve, LOW); //CE CUBE 2.5 LOW, CE CUBE2 HIGH
    pressure_init();
    stepper_init();
    _myservo.attach(_servoPin);
    _myservo.write(55); //CE CUBE 2.5 180
    delay(500);
    _myservo.detach();

    _myservo.attach(_replenish_servo_pin);
    _myservo.write(replenish_servo_up_pos); //CE CUBE 2.5 180
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
    digitalWrite(_valvesPin, HIGH); //CE CUBE 2.5 LOW, CE CUBE2 HIGH
}

void CEsystem::setValvesOFF()
{
    digitalWrite(_valvesPin, LOW); //CE CUBE 2.5 LOW, CE CUBE2 HIGH
}

void CEsystem::smplVlve_ON()
{
    digitalWrite(_samplingValve, HIGH); //CE CUBE 2.5 LOW, CE CUBE2 HIGH
}

void CEsystem::smplVlve_OFF()
{
    digitalWrite(_samplingValve, LOW); //CE CUBE 2.5 LOW, CE CUBE2 HIGH
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

bool CEsystem::replenish_servo_up()
{
    // _LiftPlatform = true;
    _servoCurrentTime = millis();
    _myservo.attach(_replenish_servo_pin);
    _myservo.write(replenish_servo_up_pos);
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

bool CEsystem::replenish_servo_down()
{
    // _LiftPlatform = true;
    _servoCurrentTime = millis();
    _myservo.attach(_replenish_servo_pin);
    _myservo.write(178);
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
    digitalWrite(_samplingValve, HIGH); //CE CUBE 2.5 LOW, CE CUBE2 HIGH
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
    return true;
}

bool CEsystem::make_bge_droplet()
{
    digitalWrite(_valvesPin, HIGH);
    digitalWrite(_samplingValve, LOW);
    PID_bge_init();
    while (!generate_droplet(_bge_gen_time))
    {
        generate_droplet(_bge_gen_time);
    }
    setPumpOFF();
    deaerateSample();
    return true;
}

// void CEsystem::setHydrodynamicInjection()
// {
//     _injectionMethod = true;
// }

// void CEsystem::setElectrokineticInjection()
// {
//     _injectionMethod = false;
// }

bool CEsystem::performInjection()
{
    digitalWrite(_samplingValve, LOW);
    while (!inject_sample())
    {
        inject_sample();
    }
    setPumpOFF();
    return true;
}

bool CEsystem::replenish()
{
    while (!replenish_servo_down())
    {
        replenish_servo_down();
    }
    smplVlve_ON();
    setPumpON();
    delay(5000);
    setPumpOFF();
    smplVlve_OFF();
    deaerateSample();

    make_bge_droplet();
    make_bge_droplet();
    // make_bge_droplet();

    while (!replenish_servo_up())
    {
        replenish_servo_up();
    }
    return true;
}

bool CEsystem::drain()
{
    while (!replenish_servo_down())
    {
        replenish_servo_down();
    }
    smplVlve_ON();
    setPumpON();
    delay(5000);
    setPumpOFF();
    smplVlve_OFF();
    deaerateSample();
    while (!replenish_servo_up())
    {
        replenish_servo_up();
    }
    return true;
}