#include <Arduino.h>
#include <CEhandling.h>

class AnalysisHandling : public CEsystem
{
  private:
    /* data */
    int vialsNumber = 12, vialPosition = 0, analysisNumber = 6;
    int currentAnalysis = 1, repetitions = 3, currentRepetition = 0;
    int turnPositions = 0, cycleTask = 0, cycleTaskNumber = 25;
    float analysisTimemin = 30.2; //min 5.5
    unsigned long analysisTimeMillis, currentAnalysisTime, conditioningTime = 180000, flushTime = 45000;
    long vacuumFlushTime = 45000, currentConditioningTime, currentFlushTime, currentVacuumFlushTime, currentInjectionTime, currentCollectionTime; //seconds
    const int vialBGE1 = 0, vialBGE2 = 1;
    float vialResolution = 4076.0 / 12.0;

    bool _timer_flag = false;
    unsigned long _period_millis = 0, _current_millis = 0;

    bool analysisStartedFlag = false;
    bool analysisInitFlag = false;
    bool analysisWaitingFlag = false, analysisStateFlag = false;
    bool _vacuum_flush_flag = false, _pressure_flush_flag = false, _electro_cond_flag = false;
    bool _pre_conditioning_flag = false;
    bool _switch_off_flag = false;
    bool _send_start_signal_flag = false;

  public:
    AnalysisHandling(/* args */);
    ~AnalysisHandling();
    bool analysis_timer();
    void analysis_init();
    bool get_BGE1();
    bool get_BGE2();
    bool get_Sample();

    void set_mod_timer(bool flag, unsigned long period_millis, unsigned long current_millis);
    void set_analysis_start(bool start_flag, bool init_flag);
    void abort_analysis();
    void handleAnalysisCycle();
    void capillary_vacuum_flush();
    void capillary_pressure_flush();
    void capillary_electroconditioning();
    void setOnElectroConditioning(), setOffElectroConditioning();
    void setOnPressureFlush(), setOffPressureFlush();
    void setOnVacuumFlush(), setOffVacuumFlush();
    void startCEanalysis(), finishCEanalysis();
    bool get_start_signal_status();
    void set_start_signal_status(bool status);
    void set_analysis_time(float analysis_min);
};

AnalysisHandling::AnalysisHandling(/* args */)
{
}

AnalysisHandling::~AnalysisHandling()
{
}

void AnalysisHandling::set_analysis_start(bool start_flag, bool init_flag)
{
    analysisStartedFlag = start_flag;
    analysisInitFlag = init_flag;
}

void AnalysisHandling::abort_analysis()
{
    analysisStartedFlag = false;
    HVOFF();
    setPumpOFF();
}

void AnalysisHandling::set_analysis_time(float analysis_min)
{
    analysisTimemin = analysis_min;
}

void AnalysisHandling::analysis_init()
{
    cycleTask = 0;
    vialsNumber = 12;
    vialPosition = 0;
    analysisNumber = 7;
    repetitions = 3;
    analysisTimeMillis = analysisTimemin * 60 * 1000;
    currentRepetition = 1;
    currentAnalysis = 1;
}

bool AnalysisHandling::get_BGE1()
{
    turnPositions = vialPosition - vialBGE1;
    Serial.print("turnPositions:");
    Serial.println(turnPositions);
    if (turnPositions > 0)
    {
        set_carousel(false, int(vialResolution * float(turnPositions)));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        vialPosition = vialPosition - turnPositions;
        return true;
    }
    else if (turnPositions < 0)
    {
        set_carousel(true, -1 * int(vialResolution * float(turnPositions)));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        vialPosition = vialPosition - turnPositions;
        return true;
    }
    else if (turnPositions == 0)
    {
        set_carousel(true, vialResolution * float(turnPositions));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        return true;
    }
}

bool AnalysisHandling::get_BGE2()
{
    turnPositions = vialPosition - vialBGE2;
    Serial.print("turnPositions:");
    Serial.println(turnPositions);
    if (turnPositions > 0)
    {
        set_carousel(false, int(vialResolution * float(turnPositions)));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        vialPosition = vialPosition - turnPositions;
        return true;
    }
    else if (turnPositions < 0)
    {
        set_carousel(true, -1 * int(vialResolution * float(turnPositions)));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        vialPosition = vialPosition - turnPositions;
        return true;
    }
    else if (turnPositions == 0)
    {
        set_carousel(true, vialResolution * float(turnPositions));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        return true;
    }
}

bool AnalysisHandling::get_Sample()
{
    turnPositions = vialPosition - currentAnalysis + 1 - 2;
    Serial.print("currentAnalysis:");
    Serial.println(currentAnalysis);
    Serial.print("turnPositions:");
    Serial.println(turnPositions);
    if (turnPositions > 0)
    {
        set_carousel(false, int(vialResolution * float(turnPositions)));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        vialPosition = vialPosition - turnPositions;
        return true;
    }
    else if (turnPositions < 0)
    {
        set_carousel(true, -1 * int(vialResolution * float(turnPositions)));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        vialPosition = vialPosition - turnPositions;
        return true;
    }
    else if (turnPositions == 0)
    {
        set_carousel(true, vialResolution * float(turnPositions));
        while (!turnCarousel())
        {
            turnCarousel();
        }
        turnOffMotor();
        return true;
    }
}

void AnalysisHandling::set_mod_timer(bool flag, unsigned long period_millis, unsigned long current_millis)
{
    _period_millis = period_millis;
    _current_millis = current_millis;
    _timer_flag = flag;
}

bool AnalysisHandling::analysis_timer()
{
    if (_timer_flag && millis() >= _period_millis + _current_millis)
    {
        _timer_flag = false;
        _switch_off_flag = true;
        // cycleTask++;
        Serial.println("timer Finished");
        return true;
    }
}

void AnalysisHandling::capillary_vacuum_flush()
{
    if (_vacuum_flush_flag)
    {
        setPumpON();
        _vacuum_flush_flag = false;
    }
}
void AnalysisHandling::capillary_pressure_flush()
{
    if (_pressure_flush_flag)
    {
        setValvesON();
        setPumpON();
        _pressure_flush_flag = false;
    }
}
void AnalysisHandling::capillary_electroconditioning()
{
    if (_electro_cond_flag)
    {
        HVON();
        _electro_cond_flag = false;
    }
    else if (_electro_cond_flag == false && _electro_cond_flag)
    {
        HVOFF();
        cycleTask++;
    }
}

void AnalysisHandling::startCEanalysis()
{
    if (analysisWaitingFlag == true)
    {
        auto_zero();
        currentAnalysisTime = millis();
        HVON();
        //    Serial.println("START");
        // sendStartSignal();
        _send_start_signal_flag = true;
        analysisWaitingFlag = false;
        analysisStateFlag = true;
        cycleTask++;
    }
}

bool AnalysisHandling::get_start_signal_status()
{
    return _send_start_signal_flag;
}
void AnalysisHandling::set_start_signal_status(bool status)
{
    _send_start_signal_flag = status;
}

void AnalysisHandling::finishCEanalysis()
{
    if (millis() >= analysisTimeMillis + currentAnalysisTime && analysisStateFlag == true)
    {
        HVOFF();
        Serial.println("STOP");

        analysisStateFlag = false;
        cycleTask++;
        //    Serial.println(cycleTask);
        currentRepetition++;
        if (currentRepetition > repetitions)
        {
            currentRepetition = 1;
            currentAnalysis++;
        }
        if (currentAnalysis > analysisNumber)
        {
            analysisStartedFlag = false;
            HVOFF();
            Serial.println("finished");
        }
        Serial.print("currentRepetition:");
        Serial.println(currentRepetition);
        Serial.print("currentAnalysis:");
        Serial.println(currentAnalysis);
    }
}

void AnalysisHandling::handleAnalysisCycle()
{
    if (analysisStartedFlag == true)
    {
        if (analysisInitFlag == true)
        {
            analysis_init();
            analysisInitFlag = false;
        }
        switch (cycleTask)
        {
        case 0:
            while (!LiftDown())
            {
                LiftDown();
            }
            cycleTask++;
            break;
        case 1:
            while (!get_BGE1())
            {
                get_BGE1();
            }
            cycleTask++;
            break;
        case 2:
            while (!LiftUp())
            {
                LiftUp();
            }
            set_mod_timer(true, vacuumFlushTime, millis());
            _vacuum_flush_flag = true;
            cycleTask++;
            //        Serial.println(cycleTask);
            break;
        case 3:
            capillary_vacuum_flush();
            if (_switch_off_flag)
            {
                _switch_off_flag = false;
                setPumpOFF();
                set_mod_timer(true, flushTime, millis());
                _pressure_flush_flag = true;
                cycleTask++;
            }
            //        Serial.println(cycleTask);
            break;
        case 4:
            capillary_pressure_flush();
            if (_switch_off_flag)
            {
                _switch_off_flag = false;
                setValvesOFF();
                setPumpOFF();
                set_mod_timer(true, conditioningTime, millis());
                _electro_cond_flag = true;
                cycleTask++;
            }
            break;
        case 5:
            capillary_electroconditioning();
            if (_switch_off_flag)
            {
                _switch_off_flag = false;
                HVOFF();
                cycleTask++;
            }
            break;
        case 6:
            while (!LiftDown())
            {
                LiftDown();
            }
            cycleTask++;
            break;
        case 7:
            while (!get_Sample())
            {
                get_Sample();
            }
            cycleTask++;
            break;
        case 8:
            while (!LiftUp())
            {
                LiftUp();
            }
            cycleTask++;
            break;
        case 9:
            while (!performInjection())
            {
                performInjection();
            }
            cycleTask++;
            break;
        case 10:
            while (!LiftDown())
            {
                LiftDown();
            }
            cycleTask++;
            break;
        case 11:
            while (!get_BGE2())
            {
                get_BGE2();
            }
            cycleTask++;
            break;
        case 12:
            while (!LiftUp())
            {
                LiftUp();
            }
            cycleTask++;
            analysisWaitingFlag = true;
            break;
        case 13:
            cycleTask++;
            break;
        case 14:
            cycleTask++;
            break;
        case 15:
            cycleTask++;
            break;
        case 16:
            cycleTask++;
            break;
        case 17:
            cycleTask++;
            break;
        case 18:
            cycleTask++;
            break;
        case 19:
            cycleTask++;
            break;
        case 20:
            cycleTask++;
            break;
        case 21:
            cycleTask++;
            break;
        case 22:
            cycleTask++;
            break;
        case 23:
            startCEanalysis();
            break;
        case 24:
            finishCEanalysis();
            break;
        default:
            break;
        }
        if (cycleTask >= cycleTaskNumber)
        {
            cycleTask = 0;
            Serial.println(cycleTask);
            Serial.println(analysisStartedFlag);
        }
    }
}

void AnalysisHandling::setOnElectroConditioning()
{
    conditioningTime = 180000;
}

void AnalysisHandling::setOffElectroConditioning()
{
    conditioningTime = 0;
}

void AnalysisHandling::setOnPressureFlush()
{
    flushTime = 40000;
}

void AnalysisHandling::setOffPressureFlush()
{
    flushTime = 0;
}

void AnalysisHandling::setOnVacuumFlush()
{
    vacuumFlushTime = 25000;
}

void AnalysisHandling::setOffVacuumFlush()
{
    vacuumFlushTime = 0;
}