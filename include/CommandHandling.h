#include <Arduino.h>
#include <AnalysisHandling.h>
#include <NRFhandling.h>

class Commands : public AnalysisHandling, public NRFhandler
{

  private:
    /* data */
    /**
     * command map
     * data type, variable name[x], the command
     */
    bool _cmd_received_flag = false;
  public:
    Commands(/* args */);
    ~Commands();
    void check_cmd(String incomingString);
    void set_cmd_recv_flag(bool flag);
    bool get_cmd_rcv_flag();
};

Commands::Commands(/* args */)
{
}

Commands::~Commands()
{
}

void Commands::set_cmd_recv_flag(bool flag)
{
    _cmd_received_flag = flag;
}

bool Commands::get_cmd_rcv_flag()
{
    return _cmd_received_flag;
}

void Commands::check_cmd(String incomingString)
{
    _cmd_received_flag = true;
    if (incomingString.equals("M01") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        auto_zero();
        incomingString = "";
    }
    // else if (incomingString.equals("M02") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     auto_zero_del();
    //     incomingString = "";
    // }
    else if (incomingString.equals("M03") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        carouselClock();
        incomingString = "";
    }
    else if (incomingString.equals("M04") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        carouselCounter();
        incomingString = "";
    }
    else if (incomingString.equals("M05") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        adjustClock();
        incomingString = "";
    }
    else if (incomingString.equals("M06") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        adjustCounter();
        incomingString = "";
    }
    else if (incomingString.equals("M07") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        LiftUp();
        incomingString = "";
    }
    else if (incomingString.equals("M08") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        LiftDown();
        incomingString = "";
    }
    else if (incomingString.equals("M09") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        HVON();
        incomingString = "";
    }
    else if (incomingString.equals("M10") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        HVOFF();
        incomingString = "";
    }
    else if (incomingString.equals("M11") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        setPumpON();
        incomingString = "";
    }
    else if (incomingString.equals("M12") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        setPumpOFF();
        incomingString = "";
    }
    else if (incomingString.equals("M13") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        setValvesON();
        incomingString = "";
    }
    else if (incomingString.equals("M14") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        setValvesOFF();
        incomingString = "";
    }

    else if (incomingString.equals("M15") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        startSampling();
        incomingString = "";
    }
    else if (incomingString.equals("M16") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        stopSampling();
        incomingString = "";
    }
    // else if (incomingString.equals("M17") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     replenish_servo_up();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("M18") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     replenish_servo_down();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("M19") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     replenish();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("M20") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     drain();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("M21") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_replenish_flag(true);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("M22") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_replenish_flag(false);
    //     incomingString = "";
    // }

    else if (incomingString.equals("M23") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_sampling_flag(true);
        incomingString = "";
    }
    else if (incomingString.equals("M24") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_sampling_flag(false);
        incomingString = "";
    }


    else if (incomingString.equals("F01") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        performInjection();
        incomingString = "";
    }
    else if (incomingString.equals("F02") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
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
        nrf_send_ack();
        _cmd_received_flag = false;
        abort_analysis();        
        // pressureActionStatus = false;
        incomingString = "";
    }
    // else if (incomingString.equals("F04") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     setHydrodynamicInjection();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("F05") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     setElectrokineticInjection();
    //     incomingString = "";
    // }
    else if (incomingString.equals("F06") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        setOnElectroConditioning();
        incomingString = "";
    }
    else if (incomingString.equals("F07") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        setOffElectroConditioning();
        incomingString = "";
    }

    // else if (incomingString.equals("F08") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     setOnPressureFlush();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("F09") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     setOffPressureFlush();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("F10") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     setOnVacuumFlush();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("F11") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     setOffVacuumFlush();
    //     incomingString = "";
    // }
    else if (incomingString.equals("F12") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        return_carousel_to_zero();
        incomingString = "";
    }

    // else if (incomingString.equals("F30") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_temp_avg(true);   
    //     Serial.println(incomingString);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("F31") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_temp_avg(false);   
    //     Serial.println(incomingString);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("F32") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_baseline_comp(true);    
    //     Serial.println(incomingString);       
    //     incomingString = "";
    // }
    // else if (incomingString.equals("F33") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_baseline_comp(false); 
    //     Serial.println(incomingString);       
    //     incomingString = "";
    // }

    else if (incomingString.equals("INJ5") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_injection_time(5000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ10") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_injection_time(10000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ15") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_injection_time(15000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ20") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_injection_time(20000);
        incomingString = "";
    }
    else if (incomingString.equals("INJ40") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_injection_time(40000);
        incomingString = "";
    }
    // else if (incomingString.equals("INJ80") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_injection_time(80000);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("INJ160") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_injection_time(160000);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("INJ320") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_injection_time(320000);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("INJ640") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_injection_time(640000);
    //     incomingString = "";
    // }

    // else if (incomingString.equals("COL1") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_collection_time(60000);
    //     incomingString = "";
    // }
    else if (incomingString.equals("COL2") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_collection_time(120000);
        incomingString = "";
    }
    else if (incomingString.equals("COL4") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_collection_time(240000);
        incomingString = "";
    }
    else if (incomingString.equals("COL8") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_collection_time(480000);
        incomingString = "";
    }
    else if (incomingString.equals("COL16") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_collection_time(960000);
        incomingString = "";
    }
    else if (incomingString.equals("COL32") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_collection_time(1920000);
        incomingString = "";
    }

    // else if (incomingString.equals("ANT5") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_analysis_time(5.0);
    //     incomingString = "";
    // }
    else if (incomingString.equals("ANT10") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_analysis_time(10.0);
        incomingString = "";
    }
    // else if (incomingString.equals("ANT12") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_analysis_time(12.0);
    //     incomingString = "";
    // }
    else if (incomingString.equals("ANT15") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_analysis_time(15.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT30") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_analysis_time(30.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT45") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_analysis_time(45.0);
        incomingString = "";
    }
    else if (incomingString.equals("ANT60") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        set_analysis_time(60.0);
        incomingString = "";
    }
    // else if (incomingString.equals("ANT90") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_analysis_time(90.0);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("ANT120") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_analysis_time(120.0);
    //     incomingString = "";
    // }
    // else if (incomingString.equals("ANT150") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     set_analysis_time(150.0);
    //     incomingString = "";
    // }

    // else if (incomingString.equals("B07") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     smplVlve_ON();
    //     incomingString = "";
    // }
    // else if (incomingString.equals("B08") == true)
    // {
    //     nrf_send_ack();
    //     _cmd_received_flag = false;
    //     smplVlve_OFF();
    //     incomingString = "";
    // }
    else if (incomingString.equals("B09") == true)
    {
        nrf_send_ack();
        _cmd_received_flag = false;
        make_bge_droplet();
        incomingString = "";
    }

    else
    {
        _cmd_received_flag = false;
    }
    
}