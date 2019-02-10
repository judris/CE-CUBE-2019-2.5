#include <Arduino.h>
/**
 * New classes
 */
#include <CurrentHandling.h>
#include <NRFhandling.h>
#include <JSONhandling.h>
#include <CommandHandling.h>

CurrentHandling CurrentObj;
NRFhandler NRFobj;
JsonProcessor JSONprocObj;
Commands CommandObj;


void setup(void)
{
  delay(500);
  Serial.begin(115200);
  //  Serial.println(startString);
  NRFobj.nrf_init();
  CommandObj.init_C4D();
  delay(15);
  CommandObj.init_modules();
  CommandObj.PIDinit();
}

void loop(void)
{
  if (NRFobj.nrf_wait_for_data())
  {
    CommandObj.check_cmd(NRFobj.get_received_data());
  }
  if (!CommandObj.get_activity_status())
  {
    if (CommandObj.get_data_rdy_status())
    {
      CommandObj.data_read();
      CurrentObj.read_current();
      JSONprocObj.get_json(CommandObj.get_capacitance(), CommandObj.get_temperature(), CurrentObj.get_current());
      NRFobj.nfr_send_multi_strings(JSONprocObj.get_textA(), JSONprocObj.get_textB());
    }
  }
  CommandObj.analysis_timer();
  CommandObj.handleAnalysisCycle();
  if (CommandObj.get_start_signal_status())
  {
    // NRFobj.send_start_signal();
    JSONprocObj.split_start_array();
    NRFobj.nfr_send_multi_strings(JSONprocObj.get_textA(), JSONprocObj.get_textB());
    CommandObj.set_start_signal_status(false);
  }
  // handleAnalysisCycle();
} // end loop()
