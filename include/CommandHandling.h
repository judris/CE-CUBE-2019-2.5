#include <Arduino.h>
#include <AnalysisHandling.h>

class Commands : public AnalysisHandling
{

  private:
    /* data */
    /**
     * command map
     * data type, variable name[x], the command
     */
  public:
    Commands(/* args */);
    ~Commands();
    void check_cmd(String incomingString);
};

Commands::Commands(/* args */)
{
}

Commands::~Commands()
{
}

void Commands::check_cmd(String incomingString)
{
    if (incomingString.equals("M01") == true)
    {
        auto_zero();
        incomingString = "";
    }
    else if (incomingString.equals("M02") == true)
    {
        auto_zero_del();
        incomingString = "";
    }
    else if (incomingString.equals("M03") == true)
    {
        carouselClock();
        incomingString = "";
    }
    else if (incomingString.equals("M04") == true)
    {
        carouselCounter();
        incomingString = "";
    }
    else if (incomingString.equals("M05") == true)
    {
        adjustClock();
        incomingString = "";
    }
    else if (incomingString.equals("M06") == true)
    {
        adjustCounter();
        incomingString = "";
    }
    else if (incomingString.equals("M07") == true)
    {
        LiftUp();
        incomingString = "";
    }
    else if (incomingString.equals("M08") == true)
    {
        LiftDown();
        incomingString = "";
    }
    else if (incomingString.equals("M09") == true)
    {
        HVON();
        incomingString = "";
    }
    else if (incomingString.equals("M10") == true)
    {
        HVOFF();
        incomingString = "";
    }
    else if (incomingString.equals("M11") == true)
    {
        setPumpON();
        incomingString = "";
    }
    else if (incomingString.equals("M12") == true)
    {
        setPumpOFF();
        incomingString = "";
    }
    else if (incomingString.equals("M13") == true)
    {
        setValvesON();
        incomingString = "";
    }
    else if (incomingString.equals("M14") == true)
    {
        setValvesOFF();
        incomingString = "";
    }

    else if (incomingString.equals("M15") == true)
    {
        startSampling();
        incomingString = "";
    }
    else if (incomingString.equals("M16") == true)
    {
        stopSampling();
        incomingString = "";
    }
    else if (incomingString.equals("F01") == true)
    {
        performInjection();
        incomingString = "";
    }
    else if (incomingString.equals("F02") == true)
    {
        set_analysis_start(true, true);
        // analysisStartedFlag = true;
        // analysisInitFlag = true;
        // lifterActionFlag = true;
        // while (!perform_analysis())
        // {
        //     perform_analysis();
        // }
        incomingString = "";
    }
    else if (incomingString.equals("F03") == true)
    {
        abort_analysis();        
        // pressureActionStatus = false;
        incomingString = "";
    }
    else if (incomingString.equals("F04") == true)
    {
        setHydrodynamicInjection();
        incomingString = "";
    }
    else if (incomingString.equals("F05") == true)
    {
        setElectrokineticInjection();
        incomingString = "";
    }
    else if (incomingString.equals("F06") == true)
    {
        setOnElectroConditioning();
        incomingString = "";
    }
    else if (incomingString.equals("F07") == true)
    {
        setOffElectroConditioning();
        incomingString = "";
    }

    else if (incomingString.equals("F08") == true)
    {
        setOnPressureFlush();
        incomingString = "";
    }
    else if (incomingString.equals("F09") == true)
    {
        setOffPressureFlush();
        incomingString = "";
    }
    else if (incomingString.equals("F10") == true)
    {
        setOnVacuumFlush();
        incomingString = "";
    }
    else if (incomingString.equals("F11") == true)
    {
        setOffVacuumFlush();
        incomingString = "";
    }

    else if (incomingString.equals("F30") == true)
    {
        set_temp_avg(true);   
        Serial.println(incomingString);
        incomingString = "";
    }
    else if (incomingString.equals("F31") == true)
    {
        set_temp_avg(false);   
        Serial.println(incomingString);
        incomingString = "";
    }
    else if (incomingString.equals("F32") == true)
    {
        set_baseline_comp(true);    
        Serial.println(incomingString);       
        incomingString = "";
    }
    else if (incomingString.equals("F33") == true)
    {
        set_baseline_comp(false); 
        Serial.println(incomingString);       
        incomingString = "";
    }

    else if (incomingString.equals("INJ5") == true)
    {
        set_injection_time(5000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ10") == true)
    {
        set_injection_time(10000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ20") == true)
    {
        set_injection_time(20000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ40") == true)
    {
        set_injection_time(40000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ180") == true)
    {
        set_injection_time(180000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ160") == true)
    {
        set_injection_time(160000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ320") == true)
    {
        set_injection_time(320000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ640") == true)
    {
        set_injection_time(640000);
        incomingString = "";
    }

    else if (incomingString.equals("COL1") == true)
    {
        set_collection_time(60000);
        incomingString = "";
    }
    else if (incomingString.equals("COL2") == true)
    {
        set_collection_time(120000);
        incomingString = "";
    }
    else if (incomingString.equals("COL4") == true)
    {
        set_collection_time(240000);
        incomingString = "";
    }
    else if (incomingString.equals("COL8") == true)
    {
        set_collection_time(480000);
        incomingString = "";
    }
    else if (incomingString.equals("COL16") == true)
    {
        set_collection_time(960000);
        incomingString = "";
    }

    else if (incomingString.equals("ANT5") == true)
    {
        set_analysis_time(5.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT10") == true)
    {
        set_analysis_time(10.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT12") == true)
    {
        set_analysis_time(12.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT15") == true)
    {
        set_analysis_time(15.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT30") == true)
    {
        set_analysis_time(30.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT45") == true)
    {
        set_analysis_time(45.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT60") == true)
    {
        set_analysis_time(60.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT90") == true)
    {
        set_analysis_time(90.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT150") == true)
    {
        set_analysis_time(150.0);
        incomingString = "";
    }
}