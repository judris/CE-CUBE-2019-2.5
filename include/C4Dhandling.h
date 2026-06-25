#include <Arduino.h>
#include <Wire.h>
// #include <RunningAverage.h>


class C4Dsystem
{
  private:
    /* data */
    // RunningAverage _tRA; //(2019 02 12)
    // float _weight_factor = 1.0;

    double _capacitance;
    // double _capacitance_offset;
    double _temperature;
    double _zero_capacitance = 0;
    // bool temperatureCompensation = false;
    // bool _temp_running_avg = false;
    // bool _temp_baseline_comp_flag = false;
    // bool _temp_comp_offset_flag = true;

    /**C4D adresses
     */
    int _DataReady = B0000;

    int StatusAddress = 0x00;
    // int CapDataH = 0x01;
    // int CapDataM = 0x02;
    // int CapDataL = 0x03;
    int Address = B1001000;

    // int CapSetup = 0x07; //capacitive channel setup address
    // int VTSetup = 0x08;
    // int ExcSetup = 0x09;    //excitation setup address
    // int ConfigSetup = 0x0A; //configuration setup addrss
    // int CapDacAReg = 0x0B;  //Capacitive DAC setup address
    // int CapGainRegH = 0x0F; //Cap gain high adress
    // int CapGainRegL = 0x10; //Cap gain high adress
    // int CapOffsetH = 0x0D;  //Cap Offset high adress
    // int CapOffsetL = 0x0E;  //Cap gain Offset adress

    // int CapChanProp = B10100001;    //capacitive channel properties
    // int ExcProp = B01100011;        //excitation properties Default B01100011
    // int ConfigProp = B00111001;     //configuration properties
    // int CapDacProp = B0;            //Capacitive DAC setup properties
    // int CapGainPropH = B01011101;   //cap gain properties high
    // int CapGainPropL = B10111101;   //cap gain properties low
    // int CapOffsetPropH = B01110111; //cap offset properties high
    // int CapOffsetPropL = B00011010; //cap Offset properties low

    // void Configuration();
    // void CapGainHighAdjust();
    // void CapGainLowAdjust();
    // void Excitation();
    // void CapInput();
    // void CapDacARegister();
    // void CapOffsetHighAdjust();
    // void CapOffsetLowAdjust();
    // void continuous();
    // void VT_on();
    // void VT_off();

  public:
    C4Dsystem(/* args */);
    ~C4Dsystem();
    byte statusRead();
    void init_C4D();
    void data_read();
    void auto_zero();
    // void auto_zero_del();
    double get_capacitance();
    double get_temperature();
    bool get_data_rdy_status();

    // void set_temp_avg(bool flag);
    // void set_baseline_comp(bool flag);
    // void calc_temp_comp_poly();
    // bool calc_temp_comp_offset();
};

C4Dsystem::C4Dsystem(/* args */)
// : _tRA(10)
{
}

C4Dsystem::~C4Dsystem()
{
}

void C4Dsystem::init_C4D()
{
    Wire.begin();
    // Configuration();
    Wire.beginTransmission(Address);
    Wire.write(0x0A);
    Wire.write(B00111001);
    Wire.endTransmission();

    // Excitation();
    Wire.beginTransmission(Address);
    Wire.write(0x09);
    Wire.write(B01100011);
    Wire.endTransmission();

    // CapInput();
    Wire.beginTransmission(Address);
    Wire.write(0x07);
    Wire.write(B10100001);
    Wire.endTransmission();

    // CapDacARegister();
    Wire.beginTransmission(Address);
    Wire.write(0x0B);
    Wire.write(B0);
    Wire.endTransmission();

    // CapOffsetHighAdjust();
    // CapOffsetLowAdjust();
    // addressRead();
    // continuous();
    Wire.beginTransmission(Address);

    Wire.write(StatusAddress);
    //  Wire.write(CapDataM);
    //  Wire.write(CapDataL);
    //  Wire.write(CapDataL);

    Wire.endTransmission();

    // VT_on();
    Wire.beginTransmission(Address);
    Wire.write(0x08);
    Wire.write(B10000001);
    Wire.endTransmission();
    Wire.beginTransmission(Address);
    Wire.write(0x0A);
    Wire.write(B11111001);
    Wire.endTransmission();
    // CapGainHighAdjust();
    // CapGainLowAdjust();

    // _tRA.clear();
}

// void C4Dsystem::calc_temp_comp_poly()
// {
//     if (_temp_comp_offset_flag)
//     {
//         _capacitance_offset = (-0.00000560285 * _temperature * _temperature + 0.00169 * _temperature) * _weight_factor;
//         _temp_comp_offset_flag = false;
//     }
//     _capacitance = _capacitance_offset + _capacitance - (-0.00000560285 * _temperature * _temperature + 0.00169 * _temperature) * _weight_factor;
// }

bool C4Dsystem::get_data_rdy_status()
{
    if (statusRead() & 0 == _DataReady && Wire.available() >= 0) //arenthesis added 2019 02 12
    {
        return true;
    }
    else
    {
        return false;
    }
    
}

// void C4Dsystem::set_temp_avg(bool flag)
// {
//     _temp_running_avg = flag;
//     // _tRA.clear(); 
// }

// void C4Dsystem::set_baseline_comp(bool flag)
// {
//     _temp_baseline_comp_flag = flag;
//     _temp_comp_offset_flag = true;
// }

void C4Dsystem::data_read()
{
    // if (Wire.available() >= 0)
    // {
    Wire.requestFrom(Address, 7);
    byte byte1 = Wire.read(); //byte byte1 = Wire.read(); 2019 02 12
    long byte2 = Wire.read();
    long byte3 = Wire.read();
    long byte4 = Wire.read();
    long byte5 = Wire.read();
    long byte6 = Wire.read();
    long byte7 = Wire.read();

    _capacitance = (((byte2 * 0x10000 + byte3 * 0x100 + byte4) * 8.192 / 16777216) - 4.096);
    _temperature = ((double(byte5 * 0x10000 + byte6 * 0x100 + byte7) / 2048.0) - 4096.0);
    // if (_temp_running_avg)
    // {
    //     // _tRA.addValue(_temperature);
    //     _temperature = _tRA.getAverage();
    // }
    // if (_temp_baseline_comp_flag)
    // {
    //     // _cRA.addValue(_capacitance);
    //     // _capacitance = _cRA.getAverage();
    //     // _capacitance = _capacitance - _temperature * 0.00141 * _weight_factor;
    //     calc_temp_comp_poly();
    // }
}

void C4Dsystem::auto_zero()
{
    _zero_capacitance = _capacitance;
}
// void C4Dsystem::auto_zero_del()
// {
//     _zero_capacitance = 0;
// }

double C4Dsystem::get_capacitance()
{
    return _capacitance - _zero_capacitance;
}
double C4Dsystem::get_temperature()
{
    return _temperature;
}

/**
 * setup functions
 */
// void C4Dsystem::Configuration()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(ConfigSetup);
//     Wire.write(ConfigProp);
//     Wire.endTransmission();
// }

// void C4Dsystem::CapGainHighAdjust()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(CapGainRegH);
//     Wire.write(CapGainPropH);
//     Wire.endTransmission();
// }

// void C4Dsystem::CapGainLowAdjust()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(CapGainRegL);
//     Wire.write(CapGainPropL);
//     Wire.endTransmission();
// }

// void C4Dsystem::Excitation()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(ExcSetup);
//     Wire.write(ExcProp);
//     Wire.endTransmission();
// }

// void C4Dsystem::CapInput()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(CapSetup);
//     Wire.write(CapChanProp);
//     Wire.endTransmission();
// }
// void C4Dsystem::CapDacARegister()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(CapDacAReg);
//     Wire.write(CapDacProp);
//     Wire.endTransmission();
// }

// void C4Dsystem::CapOffsetHighAdjust()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(CapOffsetH);
//     Wire.write(CapOffsetPropH);
//     Wire.endTransmission();
// }

// void C4Dsystem::CapOffsetLowAdjust()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(CapOffsetL);
//     Wire.write(CapOffsetPropL);
//     Wire.endTransmission();
// }

// void C4Dsystem::continuous()
// {
//     Wire.beginTransmission(Address);

//     Wire.write(StatusAddress);
//     //  Wire.write(CapDataM);
//     //  Wire.write(CapDataL);
//     //  Wire.write(CapDataL);

//     Wire.endTransmission();
// }

byte C4Dsystem::statusRead()
{
    Wire.beginTransmission(Address);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.requestFrom(Address, 1);
    while (Wire.available() == 0)
        ;
    return Wire.read();
    //  Serial.print("Status");
    //  Serial.println(Status, BIN);
}

// void C4Dsystem::VT_on()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(VTSetup);
//     Wire.write(B10000001);
//     Wire.endTransmission();
//     Wire.beginTransmission(Address);
//     Wire.write(ConfigSetup);
//     Wire.write(B11111001);
//     Wire.endTransmission();

//     // temperatureCompensation = true;
// }

// void C4Dsystem::VT_off()
// {
//     Wire.beginTransmission(Address);
//     Wire.write(VTSetup);
//     Wire.write(B00000000);
//     Wire.endTransmission();
//     Wire.beginTransmission(Address);
//     Wire.write(ConfigSetup);
//     Wire.write(B00111001);
//     Wire.endTransmission();

//     temperatureCompensation = false;
// }