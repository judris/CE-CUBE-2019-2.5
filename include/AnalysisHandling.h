#include <Arduino.h>
#include <CEhandling.h>

class AnalysisHandling : public CEsystem
{
  private:
    /* data */
    int vialsNumber = 12, vialPosition = 0, analysisNumber = 0, number_of_analysis = 4;
    int currentAnalysis = 1, repetitions = 0, repetitions_per_analysis = 3, currentRepetition = 0;
    int turnPositions = 0, cycleTask = 0, cycleTaskNumber = 35;
    float analysisTimemin = 21.2;                                                                                                                  //min 5.5
    unsigned long analysisTimeMillis, currentAnalysisTime, conditioningTime = 0, flushTime = 0;                                                    //flushtime 90000
    long vacuumFlushTime = 150000, currentConditioningTime, currentFlushTime, currentVacuumFlushTime, currentInjectionTime, currentCollectionTime; //seconds
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

    bool _replenish_flag = false, _sampling_flag = true;
    void task1(), task2(), task3(), task4();

  public:
    AnalysisHandling(/* args */);
    ~AnalysisHandling();
    bool analysis_timer();
    void analysis_init();
    bool get_BGE(int vialNo);
    // bool get_BGE2();
    bool get_Sample(int offset_positions);

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
    int get_analysis_number();
    int get_repetition_number();
    bool get_analysis_started_flag();
    void return_carousel_to_zero();
    void set_replenish_flag(bool flag), set_sampling_flag(bool flag);
};

AnalysisHandling::AnalysisHandling(/* args */)
{
}

AnalysisHandling::~AnalysisHandling()
{
}

void AnalysisHandling::return_carousel_to_zero()
{
    while (!LiftDown())
    {
        LiftDown();
    }
    while (!get_BGE(vialBGE1))
    {
        get_BGE(vialBGE1);
    }
    while (!LiftUp())
    {
        LiftUp();
    }
}

bool AnalysisHandling::get_analysis_started_flag()
{
    return analysisStartedFlag;
}

int AnalysisHandling::get_analysis_number()
{
    return currentAnalysis;
}

int AnalysisHandling::get_repetition_number()
{
    return currentRepetition;
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
    // analysisNumber = number_of_analysis;
    if (_sampling_flag)
    {
        analysisNumber = 4;
    }
    else
    {
        analysisNumber = 7;
    }
    repetitions = repetitions_per_analysis;
    analysisTimeMillis = analysisTimemin * 60 * 1000;
    currentRepetition = 1;
    currentAnalysis = 1;
}

bool AnalysisHandling::get_BGE(int vialNo)
{
    turnPositions = vialPosition - vialNo;
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
    else //2019 02 12
    {
        return false;
    }
}

bool AnalysisHandling::get_Sample(int offset_positions)
{
    turnPositions = vialPosition - currentAnalysis + 1 - 2 + offset_positions;
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
    else //2019 02 12
    {
        return false;
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
    else //2019 02 12
    {
        return false;
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
            if (_sampling_flag)
            {
                if (currentRepetition == 1)
                {
                    while (!LiftDown())
                    {
                        LiftDown();
                    }
                    while (!get_Sample(4))
                    {
                        get_Sample(4);
                    }
                    while (!LiftUp())
                    {
                        LiftUp();
                    }
                    while (!startSampling())
                    {
                        startSampling();
                    }
                }
            }
            cycleTask++;
            break;
        case 1:
        //gets -3 vial position ()0.1M NaOH, or Water and 
            // while (!LiftDown())
            // {
            //     LiftDown();
            // }
            // while (!get_BGE(-3))
            // {
            //     get_BGE(-3);
            // }
            // while (!LiftUp())
            // {
            //     LiftUp();
            // }
            // set_mod_timer(true, 90000, millis());
            // _vacuum_flush_flag = true;
            while (!LiftDown())
            {
                LiftDown();
            }
            cycleTask++;
            //        Serial.println(cycleTask);
            break;
        case 2:
        //performs vaccum flushing
            // if (vacuumFlushTime > 0)
            // {
            //     capillary_vacuum_flush();
            // }
            // if (_switch_off_flag)
            // {
            //     _switch_off_flag = false;
            //     setPumpOFF();
            //     cycleTask++;
            // }
            while (!get_BGE(-2))
            {
                get_BGE(-2);
            }
            while (!LiftUp())
            {
                LiftUp();
            }
            set_mod_timer(true, 60000, millis());
            _vacuum_flush_flag = true;
            cycleTask++;
            break;
        case 3:
            if (vacuumFlushTime > 0)
            {
                capillary_vacuum_flush();
            }
            if (_switch_off_flag)
            {
                _switch_off_flag = false;
                setPumpOFF();
                cycleTask++;
            }
            break;
        case 4:
            while (!LiftDown())
            {
                LiftDown();
            }
            set_mod_timer(true, 60000, millis());
            _vacuum_flush_flag = true;
            cycleTask++;
            break;
        case 5:
         if (vacuumFlushTime > 0)
            {
                capillary_vacuum_flush();
            }
            if (_switch_off_flag)
            {
                _switch_off_flag = false;
                setPumpOFF();
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
            while (!get_BGE(-1))
            {
                get_BGE(-1);
            }
            while (!LiftUp())
            {
                LiftUp();
            }
            set_mod_timer(true, 60000, millis());
            _vacuum_flush_flag = true;
            cycleTask++;
            break;
        case 8:
            if (vacuumFlushTime > 0)
            {
                capillary_vacuum_flush();
            }
            if (_switch_off_flag)
            {
                _switch_off_flag = false;
                setPumpOFF();
                cycleTask++;
            }
            break;
        case 9:
            while (!LiftDown())
            {
                LiftDown();
            }
            cycleTask++;
            break;
        case 10:
            while (!get_BGE(vialBGE1))
            {
                get_BGE(vialBGE1);
            }
            cycleTask++;
            break;
        case 11:
            while (!LiftUp())
            {
                LiftUp();
            }
            // set_mod_timer(true, 20000, millis());
            // // smplVlve_OFF();
            // _vacuum_flush_flag = true;
            cycleTask++;
            //        Serial.println(cycleTask);
            break;
        case 12:
            cycleTask++;
            break;
        case 13:
            // if (vacuumFlushTime > 0)
            // {
            //     capillary_vacuum_flush();
            // }
            // if (_switch_off_flag)
            // {
            //     _switch_off_flag = false;
            //     setPumpOFF();
            //     // smplVlve_OFF();
            //     // deaerateSample();
            //     cycleTask++;
            // }
            cycleTask++;
            break;
        case 14:
            if (_replenish_flag)
            {
                while (!replenish())
                {
                    replenish();
                }
                set_mod_timer(true, 60000, millis());
                _vacuum_flush_flag = true;
                cycleTask++;
            }
            else
            {
                set_mod_timer(true, vacuumFlushTime, millis());
                _vacuum_flush_flag = true;
                cycleTask++;
            }
            break;
        case 15:
            if (vacuumFlushTime > 0)
            {
                capillary_vacuum_flush();
            }
            if (_switch_off_flag)
            {
                _switch_off_flag = false;
                setPumpOFF();
                // smplVlve_OFF();
                // deaerateSample();
                // set_mod_timer(true, flushTime, millis());
                // _pressure_flush_flag = true;
                cycleTask++;
            }
            break;
        case 16:
        // if (flushTime > 0)
        //     {
        //         capillary_pressure_flush();
        //     }
        //     if (_switch_off_flag)
        //     {
        //         _switch_off_flag = false;
        //         // setValvesOFF();
        //         setPumpOFF();
        //         set_mod_timer(true, conditioningTime, millis());
        //         _electro_cond_flag = true;
        //         cycleTask++;
        //     }
            cycleTask++;
            break;
        case 17:
            // capillary_electroconditioning();
            //     if (_switch_off_flag)
            //     {
            //         _switch_off_flag = false;
            //         HVOFF();
            //         cycleTask++;
            //     }
            cycleTask++;
            break;
        case 18:
            if (_replenish_flag)
            {
                while (!LiftDown())
                {
                    LiftDown();
                }
                while (!get_BGE(vialBGE2))
                {
                    get_BGE(vialBGE2);
                }
                while (!LiftUp())
                {
                    LiftUp();
                }
                cycleTask++;
            }
            else
            {
                cycleTask++;
            }
            break;
        case 19:
            if (_replenish_flag)
            {
                while (!replenish())
                {
                    replenish();
                }
                set_mod_timer(true, vacuumFlushTime, millis());
                _vacuum_flush_flag = true;
                cycleTask++;
            }
            else
            {
                cycleTask++;
            }
            break;
        case 20:
            if (_replenish_flag)
            {
                if (vacuumFlushTime > 0)
                {
                    capillary_vacuum_flush();
                }
                if (_switch_off_flag)
                {
                    _switch_off_flag = false;
                    setPumpOFF();
                    cycleTask++;
                }
            }
            else
            {
                cycleTask++;
            }          
            break;
        case 21:
            cycleTask++;
            break;
        case 22:
            cycleTask++;
            break;
        case 23:
            cycleTask++;
            break;
        case 24:
            cycleTask++;
            break;
        case 25:
            while (!LiftDown())
            {
                LiftDown();
            }
            cycleTask++;
            break;
        case 26:
            while (!get_Sample(0))
            {
                get_Sample(0);
            }
            cycleTask++;
            break;
        case 27:
            while (!LiftUp())
            {
                LiftUp();
            }
            cycleTask++;
            break;
        case 28:
            while (!performInjection())
            {
                performInjection();
            }
            cycleTask++;
            break;
        case 29:
            while (!LiftDown())
            {
                LiftDown();
            }
            cycleTask++;
            break;
        case 30:
            while (!get_BGE(vialBGE2))
            {
                get_BGE(vialBGE2);
            }
            cycleTask++;
            break;
        case 31:
            while (!LiftUp())
            {
                LiftUp();
            }
            cycleTask++;
            analysisWaitingFlag = true;
            break;
        case 32:
            cycleTask++;
            break;
        case 33:
            startCEanalysis();
            break;
        case 34:
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

void AnalysisHandling::set_replenish_flag(bool flag)
{
    _replenish_flag = flag;
}

void AnalysisHandling::set_sampling_flag(bool flag)
{
    _sampling_flag = flag;
}