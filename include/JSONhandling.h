#include <Arduino.h>
#include <../ArduinoJson/ArduinoJson.h>

class JsonProcessor
{
  private:
    /* data */
    const int _bufLen = 32;
    char _json_string[64];
    void split_send_array(char inputStr[64]);
    char _textA[32];
    char _textB[32];
    char _startString[50] = "{\"c\":\"1.234567890\",\"t\":\"0\",\"i\":\"0\"}";

  public:
    JsonProcessor(/* args */);
    ~JsonProcessor();
    bool get_json(double capacitance, double temperature, float current);
    void split_start_array();
    char *get_json_string();
    char *get_textA();
    char *get_textB();
};

JsonProcessor::JsonProcessor(/* args */)
{
}

JsonProcessor::~JsonProcessor()
{
}

bool JsonProcessor::get_json(double capacitance, double temperature, float current)
{
    StaticJsonBuffer<32 * 2> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    char c[12];
    dtostrf(capacitance, 8, 8, c);

    char t[9];
    dtostrf(temperature, 5, 5, t);

    char i[8];
    dtostrf(current, 4, 4, i);

    root["c"] = c;
    root["t"] = t;
    root["i"] = i;

    root.printTo(_json_string);
    split_send_array(_json_string);
    return true;
}

char *JsonProcessor::get_json_string()
{
    return _json_string;
}

void JsonProcessor::split_send_array(char inputStr[64])
{
    char tmp[_bufLen];
    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, inputStr, _bufLen - 1);
    //  Serial.println(tmp);
    snprintf(_textA, sizeof(_textA), "%-32s", tmp);
    //  Serial.println(textA);
    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, &inputStr[_bufLen - 1], _bufLen - 2);
    //  Serial.println(tmp);
    snprintf(_textB, sizeof(_textB), "%-32s", tmp);
    //  Serial.println(textB);
}

void JsonProcessor::split_start_array()
{
    char tmp[_bufLen];
    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, _startString, _bufLen - 1);
    //  Serial.println(tmp);
    snprintf(_textA, sizeof(_textA), "%-32s", tmp);
    //  Serial.println(textA);
    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, &_startString[_bufLen - 1], _bufLen - 2);
    //  Serial.println(tmp);
    snprintf(_textB, sizeof(_textB), "%-32s", tmp);

    // snprintf(_textA, sizeof(_textA), "%-32s", "{\"c\":\"1.234567890\",\"t");

    // snprintf(_textB, sizeof(_textB), "%-32s", "\":\"0\",\"i\":\"0\"}");
    //  Serial.println(textB);
    // Serial.println("start signal split");
}

char *JsonProcessor::get_textA()
{
    return _textA;
}

char *JsonProcessor::get_textB()
{
    return _textB;
}