#include <Arduino.h>

class Filters
{
  private:
    /* data */
    double _input_array[10];
    signed int _position = 0;

  public:
    Filters(/* args */);
    ~Filters();
    void append(double value);
    double get_avg();
};

Filters::Filters(/* args */)
{
}

Filters::~Filters()
{
}

void Filters::append(double value)
{
    if (_position >= 9)
    {
        for (int i = 1; i < 10; i++)
        {
            _input_array[i-1] = _input_array[i];
        }
        _input_array[9] = value;
    }
    else
    {
        _input_array[_position] = value;
        _position++;
    }
//    Serial.println(_position);
}

double Filters::get_avg()
{
    double current_avg;
    for(int i = 0; i < 10; i++)
    {
        /* code */
        current_avg = current_avg + _input_array[i];
    }
    current_avg = current_avg/double(_position+1.0);
    return current_avg;    
}