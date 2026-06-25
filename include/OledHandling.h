#include <Arduino.h>
#include <../lib/SSD1306Ascii-master/src/SSD1306Ascii.h>
#include <../lib/SSD1306Ascii-master/src/SSD1306AsciiWire.h>



class OledHandling
{
private:
    /* data */
    const int I2C_ADDRESS = 0x3C;
    SSD1306AsciiWire oled;
    unsigned int _counter;
public:
    OledHandling(/* args */);
    ~OledHandling();
    void oled_init();
    // void testscrolltext();
    // void display_data(float capacitance, float temperature, float current, float pressure, int nrf_ch, int analysis, int repetition, bool analysis_status);
    void display_data(String name, long value);
    int get_counter();
    void clear();
};

OledHandling::OledHandling(/* args */)
: oled()
{
}

OledHandling::~OledHandling()
{
}

int OledHandling::get_counter()
{
    _counter++;
    return _counter;
}

void OledHandling::oled_init()
{
oled.begin(&Adafruit128x64, I2C_ADDRESS);
oled.setFont(System5x7);
oled.clear();
}

void OledHandling::display_data(String name, long value)
{
  oled.print(name);
  oled.print(value);
  oled.println();
  _counter = 0;
}

void OledHandling::clear()
{
  oled.clear();
}