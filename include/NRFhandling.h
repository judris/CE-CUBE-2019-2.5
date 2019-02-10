#include <Arduino.h>
#include <../RF24master/nRF24L01.h>
#include <../RF24master/RF24.h>
#include <printf.h>

class NRFhandler
{
  private:
    /* data */
    RF24 radio;
    const uint64_t pipes[2] = {0xDEDEDEDEE7LL, 0xDEDEDEDEE9LL};

    int dataBufferIndex = 0;
    
    bool _data_flag;
    bool _end_flag;
    bool ok;

    char SendPayload[32];
    char RecvPayload[32];
    
    String _combined_data;
    String _data_for_combining;

  public:
    NRFhandler(/* args */);
    ~NRFhandler();
    bool nrf_wait_for_data();
    bool get_end_flag();
    void set_end_flag(bool flag);
    void nrf_init();
    void combine_strings();
    char *get_received_data();
    String get_combined_data();

    void nrf_send_data(char serial_buffer[32]);
    void nfr_send_multi_strings(char sendA_buffer[32], char sendB_buffer[32]);
    void send_start_signal();
};

NRFhandler::NRFhandler(/* args */)
    : radio(9, 10)
{
}

NRFhandler::~NRFhandler()
{
}

bool NRFhandler::nrf_wait_for_data()
{
    int len = 0;
    if (radio.available())
    {
        bool done = false;
        while (!done)
        {
            len = radio.getDynamicPayloadSize();
            done = radio.read(&RecvPayload, len);
            delay(5);
        }

        RecvPayload[len] = 0; // null terminate string
        // RecvPayload[0] = 0;  // Clear the buffers
        // Serial.println(RecvPayload);
        // RecvPayload[0] = 0;
        // return RecvPayload;
        _data_flag = true;
    }
    else
    {
        _data_flag = false;
    }
    return _data_flag;
}

void NRFhandler::nrf_init()
{
    printf_begin();
    radio.begin();

    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_MAX);
    radio.setChannel(61);

    radio.enableDynamicPayloads();
    radio.setRetries(15, 15);
    radio.setCRCLength(RF24_CRC_16);

    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);

    radio.startListening();
    // radio.printDetails();

    //  Serial.println();
    //  Serial.println("RF Chat V0.90");
    delay(500);
}

char *NRFhandler::get_received_data()
{
    return RecvPayload;
    RecvPayload[0] = 0;
}

void NRFhandler::combine_strings()
{
    _end_flag = false;
    _data_for_combining = _data_for_combining + String(RecvPayload);
    for (unsigned int i = 0; i < sizeof(RecvPayload); i++)
    {
        if (RecvPayload[i] == '}')
            _end_flag = true;
    }
    if (_end_flag)
    {
        // Serial.println(_combined_data);
        _combined_data = _data_for_combining;
        _data_for_combining = "";
    }
}

String NRFhandler::get_combined_data()
{
    return _combined_data;
}

/**
 * gets the flag for determination, if the strigns have been combined
**/
bool NRFhandler::get_end_flag()
{
    return _end_flag;
}

/**
 * changes thestatus of end flag
 **/
void NRFhandler::set_end_flag(bool flag)
{
    _end_flag = flag;
}

void NRFhandler::nrf_send_data(char serial_buffer[32])
{
    strcat(SendPayload, serial_buffer);
    // swap TX & Rx addr for writing
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(0, pipes[0]);
    radio.stopListening();
    bool ok = radio.write(&SendPayload, strlen(SendPayload));

    //        Serial.print("S:");

    //        Serial.println(SendPayload);
    //        Serial.println();
        // restore TX & Rx addr for reading
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);
    radio.startListening();
    SendPayload[0] = 0;
}

void NRFhandler::nfr_send_multi_strings(char sendA_buffer[32], char sendB_buffer[32])
{
  char SendPayload[32] = "";
  strcat(SendPayload, sendA_buffer); //valString  //textA
  // swap TX & Rx addr for writing
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(0, pipes[0]);
  radio.stopListening();
  bool ok = radio.write(&SendPayload, strlen(SendPayload));

  memset(SendPayload, 0, sizeof(SendPayload));
  strcat(SendPayload, sendB_buffer);

  ok = radio.write(&SendPayload, strlen(SendPayload));

  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1, pipes[1]);
  radio.startListening();
  SendPayload[0] = 0;
  dataBufferIndex = 0;
}

void NRFhandler::send_start_signal() {
//   nrf_send_data(_startString);
  Serial.println("start signal sent");
}