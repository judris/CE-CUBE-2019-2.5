#include <Arduino.h>

class StepperHandling
{
  private:
    /* data */
    bool _Direction = true;
    int _Steps = 0;
    int _stepFlag = 0;
    bool _StepError = false;
    bool _carouselFlag = false;
    unsigned long _last_timeStep;
    unsigned long _currentMillis;
    unsigned int _carouselSpeed = 3;
    long _timeStep;
    /**
     * stepper pins (carousel)
     */
    static const uint8_t _IN1 = 3;
    static const uint8_t _IN2 = 4;
    static const uint8_t _IN3 = 5;
    static const uint8_t _IN4 = 6;

    float _vialResolution = 4076.0 / 12.0;

  public:
    StepperHandling(/* args */);
    ~StepperHandling();
    void set_carousel(bool turn_direction, int step_flag);
    void stepper_init();
    bool turnCarousel();
    bool turnOffMotor();

    void stepControl(int steps);
    void Direction();
    bool get_carousel_status();
};

StepperHandling::StepperHandling(/* args */)
{
}

StepperHandling::~StepperHandling()
{
}

bool StepperHandling::get_carousel_status()
{
    return _carouselFlag;
}

void StepperHandling::set_carousel(bool turn_direction, int step_flag)
{
    // _carouselFlag = carousel_flag;
    _Direction = turn_direction;
    _stepFlag = step_flag;
}

bool StepperHandling::turnCarousel()
{
    _currentMillis = millis();
    _last_timeStep = _currentMillis;
    // if (_stepFlag > 0 && _currentMillis - _last_timeStep >= _carouselSpeed && _carouselFlag == true)
    while (_stepFlag > 0)
    {
        if (_currentMillis - _last_timeStep <= _carouselSpeed)
        {
            _currentMillis = millis();
        }
        else if (_currentMillis - _last_timeStep >= _carouselSpeed)
        {
            //    Direction = true;
            stepControl(1);
            // delay(_carouselSpeed);
            _timeStep = _timeStep + millis() - _last_timeStep;
            _last_timeStep = millis();
            //    steps_left--;
            _stepFlag--;
        }
    
    }
    return true;
}

bool StepperHandling::turnOffMotor()
{
    // if (_stepFlag == 0 && _carouselFlag == true)
    if (_stepFlag == 0)
    {
        digitalWrite(_IN1, LOW);
        digitalWrite(_IN2, LOW);
        digitalWrite(_IN3, LOW);
        digitalWrite(_IN4, LOW);
        _carouselFlag = false;
        // cycleTask++;
        // Serial.println("stepper off");
    }
    return true;
}

void StepperHandling::stepper_init()
{
    pinMode(_IN1, OUTPUT);
    pinMode(_IN2, OUTPUT);
    pinMode(_IN3, OUTPUT);
    pinMode(_IN4, OUTPUT);
    digitalWrite(_IN1, LOW);
    digitalWrite(_IN2, LOW);
    digitalWrite(_IN3, LOW);
    digitalWrite(_IN4, LOW);
}

void StepperHandling::stepControl(int xw)
{
    for (int x = 0; x < xw; x++)
    {
        switch (_Steps)
        {
        case 0:
            digitalWrite(_IN1, LOW);
            digitalWrite(_IN2, LOW);
            digitalWrite(_IN3, LOW);
            digitalWrite(_IN4, HIGH);
            break;
        case 1:
            digitalWrite(_IN1, LOW);
            digitalWrite(_IN2, LOW);
            digitalWrite(_IN3, HIGH);
            digitalWrite(_IN4, HIGH);
            break;
        case 2:
            digitalWrite(_IN1, LOW);
            digitalWrite(_IN2, LOW);
            digitalWrite(_IN3, HIGH);
            digitalWrite(_IN4, LOW);
            break;
        case 3:
            digitalWrite(_IN1, LOW);
            digitalWrite(_IN2, HIGH);
            digitalWrite(_IN3, HIGH);
            digitalWrite(_IN4, LOW);
            break;
        case 4:
            digitalWrite(_IN1, LOW);
            digitalWrite(_IN2, HIGH);
            digitalWrite(_IN3, LOW);
            digitalWrite(_IN4, LOW);
            break;
        case 5:
            digitalWrite(_IN1, HIGH);
            digitalWrite(_IN2, HIGH);
            digitalWrite(_IN3, LOW);
            digitalWrite(_IN4, LOW);
            break;
        case 6:
            digitalWrite(_IN1, HIGH);
            digitalWrite(_IN2, LOW);
            digitalWrite(_IN3, LOW);
            digitalWrite(_IN4, LOW);
            break;
        case 7:
            digitalWrite(_IN1, HIGH);
            digitalWrite(_IN2, LOW);
            digitalWrite(_IN3, LOW);
            digitalWrite(_IN4, HIGH);
            break;
        default:
            digitalWrite(_IN1, LOW);
            digitalWrite(_IN2, LOW);
            digitalWrite(_IN3, LOW);
            digitalWrite(_IN4, LOW);
            break;
        }
        Direction();
    }
}

void StepperHandling::Direction()
{
    if (_Direction == 1)
    {
        _Steps++;
    }
    if (_Direction == 0)
    {
        _Steps--;
    }
    if (_Steps > 7)
    {
        _Steps = 0;
    }
    if (_Steps < 0)
    {
        _Steps = 7;
    }
}