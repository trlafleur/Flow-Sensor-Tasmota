/*
  xsns_125_flow_ounter.ino - Flow sensors (water meters... sensor support for Tasmota)

  tom@lafleur.us
  Copyright (C) 2022  Tom Lafleur and  Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* **************************************************************************************
 * CHANGE LOG:
 *
 *  DATE         REV  DESCRIPTION
 *  -----------  ---  ----------------------------------------------------------
 *  1-Oct-2021  1.0   TRL - first build
 *  3-Apr-2022  1.1   TRL - refactoring of code base
 *  5-Apr-2022  1.1a  TRL - added check for sufficient flow change
 *
 *
 *  Notes:  1)  Tested with TASMOTA  11.0.0.3
 *          2)  ESP32
 *
 *
 *    TODO:
 *          1)
 *          2)
 *
 *    tom@lafleur.us
 *
 */
/* ************************************************************************************** */

/* **************************************************************************************
    Notes:

    This program is for residential type water and flow meters, 
    may not be suitable for large volume industrial meters.

    There are two basic type of water flow meters, some that produce a pulse per unit of flow,
    and K-Offset flow meters.

    Unit per flow meter, give a pulse per unit of flow, typical devices give 1 to 100gal/pulse or 
    a pulse per cubic feet of water. One "unit" of of water is 100ccf or 748.052 gals.

    Almost all of the turbine type flow sensors used in irrigation, use two calibration factors
    specified: a “K” factor and an “Offset”.

    During calibration the manufacturer measures the pulse rate outputs for a number of precise flow rates.
    These are plotted, but since the turbine has some friction, the graph will not be
    linear especially at the low end and a linear regression is done to get a best fit straight line.
    The “K” factor represents the slope of the fitted line and has a dimension of pulses per unit volume moved.
    Offset represents the small amount of liquid flow required to start the turbine moving.
    You can assume that if any pulses are arriving at all, at least the offset volume of liquid is moving.

    There does not seem to be a standard for how K factor flow meters are presented.
    Sensors output a pulse stream at a frequency proportional to the flow volume as calibrated,
    with some sensors like CST, RainBird, you multiply the pulse frequency by the K factor to obtain a volume rate.
    Others however like Badger, require you to divide the pulse frequency by K.

    So there are two basic type of K-Offset flow sensors, CST and many other are of type = 1,
    Some like Badger are of type 2. So read the vendors data sheet!

    Frequency = (Gallons per Minute / K ) – Offset  or  = (Gallons per Minute * K ) – Offset
    We are measuring pulse frequency so turning the equation around:
    Gallons per minute = (Frequency + Offset) * K  or  = (Frequency + offset) / K

    // flow meter type
    flow_type   0   pulse per unit (GPM....)
                1   K-Offset    flowrate = (freq + offset) * K  --> freq = (PPM / K) - offset
                2   K-Offset    flowrate = (freq + offset) / K  --> freq = (PPM * K) - offset

    // unit per pulse from flow meter
    rate_factor                                flow_units
                0.1   0.1 gal per minute            GPM
                1       1 gal per minute            GPM
                10      10 gal per minute           GPM
                100     100 gal gal per minute      GPM
                7.48052 1 cubic feet                Cft
                74.8052 10 cubic feet               Cft
                748.052 100 cubic feet  (unit)      Cft
                1       1 cubic meter               M3
                1       1 litres                    LM

    flow_units
                0 = GPM     Gallons per minutes   <--- defaults
                1 = CFT     Cubic Feet per minutes
                2 = M3      Cubic Metre per minutes
                3 = LM      Litres per minutes

 *  Some information on the 1 gpm water meter and flow rates.
 *     1 Gal/per/min        period      Freq in Hz
 *  ---------------------------------------------------
 *  1   Pulse = 1   gal     60  sec    .01667 Hz
 *  60  Pulse = 60  gal     1   sec    1 Hz
 *  30  Pulse = 30  gal     2   sec    .5 Hz
 *  10  Pulse = 10  gal     6   sec    .16667 Hz
 *  5   Pulse = 5   gal     12  sec    .08333 Hz
 *  2   Pulse = 2   gal     2   sec    .03333 Hz
 *  .5  Pulse = .5  gal     120 sec    .00833 Hz
 *
 *
 * At 1-GPM flow rate, its takes 1 minutes for one pulse from sensor,
 *  so for 0.25-GPM rate its takes 4 minutes between pulses...
 *
 * We will typical limit the flow range to be about .25gpm to 60gpm for a 1GPM sensor
 *  We will reset flow period timer at 4 minutes if no pulses...
 * 
 * We also sense for excessive flow, we do this by setting a excessive flow limit
 *  and the amount of time to be over this limit.
 *   
 * Most of these settings are changable from commands to the device.
 *
 *
 *    https://www.petropedia.com/definition/7578/meter-k-factor
 *    https://instrumentationtools.com/flow-meter-k-factor-and-calculations/
 *    https://www.creativesensortechnology.com/copy-of-pct-120
 * 
 *
 * 
 *
************************************************************************************** */

/*

  Xsns127Cmnd:

  format: Sensor125 1,2,3

      1     Flow_type   0   pulse per unit (GPM....)
                        1   K-Offset    flowrate = (freq + offset) * K  --> freq = (PPM / K)
                        2   K-Offset    flowrate = (freq + offset) / K  --> freq = (PPM * K)

      2     Flow rate_factor    // unit per pulse from flow meter examples, float
                        0.1   0.1 gal per minute            GPM
                        1       1 gal per minute            GPM
                        10      10 gal per minute           GPM
                        100     100 gal gal per minute      GPM
                        7.48052 1 cubic feet                Cft
                        74.8052 10 cubic feet               Ctf
                        748.052 100 cubic feet              Cft
                        1       1 cubic meter               M3
                        1       1 litres                    LM
      3     K                   // K value from device, float
      4     Offset              // Offset value from device, float
      5     Flow_units
                        0 = GPM     GAL Gallons per minutes
                        1 = Cft     CF  Cubic Feet per minutes
                        2 = M3      CM  Cubic Metre per minutes
                        3 = LM      L   Litres per minutes
      6     Excess Flow Threshold                         // flow rate at which to trigger an excess flow
      7     Excess Flow Threshold Time                    // time in seconds to report excessive flow
      8     Current Send Interval in second's             // how often we send MQTT information when we have a flow
      9     MQTT Bit Mask, 16 bits                        // use to enable/disable MQTT messages
      10    Max Flow reset time in seconds                // Reset flow, if no new pulse by this time...
      11    Max Flow Rate                                 // Max flow rate for this sensor
      12    Debounce 0 = off, 1 = on
      13    Debounce Low Time in MS
      14    Debounce High Time in MS


  MQTT_send_bit_mask:                    // we use this mask to enable/disable MQTT messages
        0       FlowCount      
        1       Flow
        2       FlowPeriod
        3       1hrVolume, last hour
        -
        4       24hrVolume, last 24 hr's
        5       RateFactor
        6       K
        7       OffSet
        -
        8       FlowUnits
        9       VolumeUnits
        10      Current1hrVolume
        11      Current24hrVolume
        -
        12      Excess flow flag
        13      VolumePerFlow
        14
        15      Message (Not Used Yet)

*/

/*
Changes made to Tasmota base code...

tasmota/tasmota_template.h 
line 186
GPIO_FLOW, GPIO_FLOW_NP, GPIO_FLOW_LED,            // Flow Sensor X125      //  <---------------  TRL

line 409
D_SENSOR_FLOW "|" D_SENSOR_FLOW "_n" D_SENSOR_FLOW_LED "|"                  // <---------------  TRL

line 458
#ifdef USE_FLOW
  AGPIO(GPIO_FLOW),                                // Flow xsns_125         // <---------------  TRL
  AGPIO(GPIO_FLOW_NP),
  AGPIO(GPIO_FLOW_LED),
#endif

tasmota/language/en_GB.h
at line 859
#define D_SENSOR_FLOW          "H2O Flow"                                   // <---------------  TRL
#define D_SENSOR_FLOW_N        "H2O Flow N"
#define D_SENSOR_FLOW_LED      "H2O Flow Led"
*/


#ifdef USE_FLOW
#define XSNS_125 125

#ifndef DEBUG
  #define DEBUG
#endif

// #define FlowCtr->   FlowCtr->
/*********************************************************************************************\
 * Flow sensors for Water meters, units per minute or K-Offset types...
\*********************************************************************************************/

// will need to move these to Tasmota base code at a later time...
#define D_FLOW_RATE1  "Flow Rate"
#define D_FLOW_COUNT  "Flow Pulse Count"
#define D_FLOW_PERIOD "Flow Period"
#define D_Flow_Factor "Flow Factor"
#define D_Flow_K      "Flow K"
#define D_Flow_Offset "Flow Offset"
#define D_Flow_Frequency "Flow Frequency"

#define D_FLOWMETER_NAME          "Flow_Meter"
#define D_PRFX_FLOW               "Flow"
#define D_CMND_FLOW_TYPE          "Type"
#define D_CMND_FLOW_RATE          "Flow_Rate"
#define D_CMND_FLOW_DEBOUNCE      "Debounce"
#define D_CMND_FLOW_DEBOUNCELOW   "Debounce_Low"
#define D_CMND_FLOW_DEBOUNCEHIGH  "Debounce_High"

#define D_GPM "GPM"   // 0
#define D_CFT "Cft"   // 1
#define D_M3  "M3"    // 2
#define D_LM  "lM"    // 3

#define D_GAL "GAL"   // 0
#define D_CF  "CF"    // 1
#define D_CM  "CM"    // 2
#define D_L   "L"     // 3

#define D_UNIT_HZ "Hz"

/* ******************************************************** */
// this is here to avoid making to many changes to base Tasmota code...
// these settings are save between re-boots
//    --->   Need to do a global rename of "MySettings." to "Settings->" when we move this structure to Tasmota base code <---------- TRL

// xsns125 Flow Counter varables in settings.h

struct MYSETTINGS
{
  uint8_t  flow_type =                        0;    // Current type of flow sensor, 0 = flow per unit,  1,2 = K-Offset
  uint8_t  flow_units =                       0;    // Current flow units
  uint16_t flow_debounce_low =                0;    // Current debounce values...
  uint16_t flow_debounce_high =               0;
  uint16_t flow_debounce =                    0;
  uint16_t MQTT_send_bit_mask =          0xffff;    // MQTT Bit Mask, Controls what we send
  uint16_t current_sending_interval =        10;    // in seconds
  uint32_t flow_threshold_time =  5 * 60 * 1000000; // Excessive flow threshold timeout, in microseconds (20 Min)  
  uint32_t max_flow_reset_time =  1 * 60 * 1000000; // Reset flow if no pulse within this window, in microseconds
  float    max_flow_rate  =                60.0;    // Sensor Max Flow rate in units of flow...
  float    flow_threshold_max =            20.0;    // Excessive flow threshold in units of flow
  float    flow_rate_factor =               1.0;    // Current Rate Factor
  float    flow_k =                        .153;    // For K-Offset flow sensor (--> CST 1in ELF sensor)
  float    flow_offset =                  1.047;    // Current Offset
} MySettings;
static_assert(sizeof(MySettings) == 40, "MySettings Size is not 40 bytes");


/* ******************************************************** */
// Our Local global variables...
volatile uint32_t flow_pulse_period;
volatile uint32_t current_pulse_counter;
volatile bool     FlowLedState  = false; // LED toggles on every pulse 

struct FLOWCTR
{
  uint8_t     no_pullup;                 // Counter input pullup flag (1 = No pullup)
  uint8_t     pin_state;                 // LSB0..3 Last state of counter pin; LSB7==0 IRQ is FALLING, LSB7==1 IRQ is CHANGE
  uint32_t    timer;                     // Last flow time in micro seconds
  uint32_t    timer_low_high;            // Last low/high flow counter time in micro seconds
  bool        counter;                   // 
  uint32_t    OneHour;                   // event timers....
  uint32_t    OneDay;
  uint32_t    SendingRate;               // Current sample rate count
  uint32_t    LastPulseCount;            // use to check for new flow
  uint32_t    CurrentTime;               // Current time 
  uint32_t    LastPulseTime;             // Last pulse time
  uint32_t    CurrentFlowStartTime;      // Start of current flow      
  uint32_t    ExcessiveFlowStartTime;    // Time that this flow started...
  float       Saved1hrFlowVolume;        // saved volume for the last 1hr period
  float       Saved24hrFlowVolume;       // saved volume for the last 24hr period
  float       Current1hrFlowVolume;      // current run-rate volume for this 1hr period
  float       Current24hrFlowVolume;     // current run-rate volume for this 24hr period
  float       VolumePerFlow;             // Volume for current flow
  float       CurrentFlow;
  float       OldFlowRate;
  char        Current_Units[8];           // Default units...
  char        Current_Volume_Units[8];
  bool        WeHaveFlow;                 // true if we have started a new flow
  bool        WeHaveFlowOverThreshold;    // true if current flow exceed threshold
  bool        WeHaveExcessFlow;           // true if we have exceed threshold and we have exceeded flow_threshold_time
} ;
//static_assert(sizeof(FlowCtr) == 90, "FlowCtr Size is not 60 bytes");
struct FLOWCTR *FlowCtr;

// Forward declarations...
void FlowCtrCheckFlowTimeOut(void);
void FlowCtrCheckExcessiveFlow(void);
void FlowCtrBoundsCheck(void);
void FlowCtrFlowEverySecond(void);

/* ******************************************************** */
/* ******************************************************** */
/* ********************** ISR ***************************** */
void IRAM_ATTR FlowCtrIsr(void)
{
  uint32_t CurrentTimeISR = micros();
  uint32_t debounce_time;

  if (FlowCtr->pin_state)
  {
    // handle low and high debounce times when configured
    if (digitalRead(Pin(GPIO_FLOW)) == bitRead(FlowCtr->pin_state, 0))
    {
      // new pin state to be ignored because debounce Time was not met during last IRQ
      return;
    }
    debounce_time = CurrentTimeISR - FlowCtr->timer_low_high;
    if bitRead (FlowCtr->pin_state, 0)
    {
      // last valid pin state was high, current pin state is low
      if (debounce_time <= MySettings.flow_debounce_high * 1000)
        return;
    }
    else
    {
      // last valid pin state was low, current pin state is high
      if (debounce_time <= MySettings.flow_debounce_low * 1000)
        return;
    }
    // passed debounce check, save pin state and timing
    FlowCtr->timer_low_high = CurrentTimeISR;
    FlowCtr->pin_state ^= 1;
    // do not count on rising edge
    if bitRead (FlowCtr->pin_state, 0)
    {
      return;
    }
  }
  debounce_time = CurrentTimeISR - FlowCtr->timer;
  if (debounce_time > (MySettings.flow_debounce * 1000))
  {
    FlowCtr->timer = CurrentTimeISR;
    flow_pulse_period = debounce_time;          // pulse period
    current_pulse_counter++;                    // pulse count
  }
  // Optional external LED to show each pulse
 if (PinUsed(GPIO_FLOW_LED)) 
  {   
     if (FlowLedState) 
      {
        FlowLedState = false;
        digitalWrite(Pin(GPIO_FLOW_LED), 0);
      }
    else
      {
        FlowLedState = true;
        digitalWrite(Pin(GPIO_FLOW_LED), 1);
      }
  }
}


/* ******************************************************** */
bool FlowCtrPinState(void)
{
  if ((XdrvMailbox.index >= AGPIO(GPIO_FLOW_NP)) && (XdrvMailbox.index < (AGPIO(GPIO_FLOW_NP))))
  {
    bitSet(FlowCtr->no_pullup, XdrvMailbox.index - AGPIO(GPIO_FLOW_NP));
    XdrvMailbox.index -= (AGPIO(GPIO_FLOW_NP) - AGPIO(GPIO_FLOW));
    return true;
  }
  return false;
}


/* ******************************************************** */
void FlowCtrInit(void)
{
  if (PinUsed(GPIO_FLOW))
  {
    //FlowCtr = (struct FLOWCTR*) malloc(sizeof(struct FLOWCTR));
    FlowCtr = (struct FLOWCTR*) malloc(sizeof(*FlowCtr));
    // xdrv_44_iel_hvac.ino
    // sc = (struct miel_hvac_softc *) malloc(sizeof(*sc));
    if (FlowCtr == NULL)  AddLog( LOG_LEVEL_ERROR, PSTR("%s:"), " malloc failed in Flow sensor...!");

    FlowCtr->counter = true; 
    pinMode(Pin(GPIO_FLOW), bitRead(FlowCtr->no_pullup, 0) ? INPUT : INPUT_PULLUP);
    if ((MySettings.flow_debounce_low == 0) && (MySettings.flow_debounce_high == 0))
    {
      FlowCtr->pin_state = 0;
      attachInterrupt(Pin(GPIO_FLOW), FlowCtrIsr, FALLING);
    }
    else
    {
      FlowCtr->pin_state = 0x8f;
      attachInterrupt(Pin(GPIO_FLOW), FlowCtrIsr, CHANGE);
    }

  // Lets setup display units ....
  switch (MySettings.flow_units)  
    {
      default:
      case 0:
        strcpy(FlowCtr->Current_Units, D_GPM);
        strcpy(FlowCtr->Current_Volume_Units, D_GAL);
        break;

      case 1:
        strcpy(FlowCtr->Current_Units, D_CFT);
        strcpy(FlowCtr->Current_Volume_Units, D_CF);
        break;

      case 2:
        strcpy(FlowCtr->Current_Units, D_M3);
        strcpy(FlowCtr->Current_Volume_Units, D_CM);
        break;

      case 3:
        strcpy(FlowCtr->Current_Units, D_LM);
        strcpy(FlowCtr->Current_Volume_Units, D_L);
        break;
     }

    // pre set local working varables
    FlowCtr->ExcessiveFlowStartTime = micros(); // get current time..

    current_pulse_counter =               0;    // set counts to zero on reboot...
    flow_pulse_period  =                  0;    // set period to zero on reboot...

    FlowCtr->WeHaveFlow =                false;
    FlowCtr->WeHaveFlowOverThreshold =   false;
    FlowCtr->WeHaveExcessFlow =          false;

    FlowCtr->LastPulseTime =              0;    // Last pulse time
    FlowCtr->CurrentFlowStartTime =       0;    // Start of current flow 
    FlowCtr->SendingRate =                0;    // Current sample rate count 
    FlowCtr->Current1hrFlowVolume =     0.0;    // current run-rate volume for this 1hr period
    FlowCtr->Current24hrFlowVolume =    0.0;    // current run-rate volume for this 24hr period
    FlowCtr->VolumePerFlow  =           0.0;    // Volume for current flow
    FlowCtr->OneHour =                    0;    // event timers....
    FlowCtr->OneDay =                     0;

  }   // end of: if (PinUsed(GPIO_FLOW))

  // This is an optional LED indicator of flow pulse's from the sensor
  if (PinUsed(GPIO_FLOW_LED)) 
  {
       FlowLedState  =  false;
       pinMode(Pin(GPIO_FLOW_LED), OUTPUT);   // set pin to output
       digitalWrite(Pin(GPIO_FLOW_LED), 0);   // turn off led for now
  }
}   // end of: void FlowInit(void)


/* ******************************************************** */
void FlowCtrBoundsCheck(void)                 // Lets do a bounds check
{
      if (FlowCtr->CurrentFlow < 0.0)                 
      {
        FlowCtr->WeHaveFlow = false;
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " Neg flow rate, we set it to zero !",  FlowCtr->CurrentFlow);
        FlowCtr->CurrentFlow = 0.0;
      }
    
      // Lets Check that we don't get unreasonable large flow value.
      // could happen when long's wraps or false interrupt triggered
      if ( FlowCtr->CurrentFlow >= MySettings.max_flow_rate)
      {  
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " Flow rate over max !",  FlowCtr->CurrentFlow);
        FlowCtr->CurrentFlow = MySettings.max_flow_rate;     // set to max
      }

      // check for possible flow over threshold 
      if ( FlowCtr->CurrentFlow >= MySettings.flow_threshold_max) 
      {
        if (FlowCtr->WeHaveFlowOverThreshold == false)  FlowCtr->ExcessiveFlowStartTime = micros();           // set timer
        FlowCtr->WeHaveFlowOverThreshold = true;
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " We have a flow over threshold !", FlowCtr->CurrentFlow);
      }
       else FlowCtr->WeHaveFlowOverThreshold = false;
}   // end of: void FlowCtrBoundsCheck(void)


/* ******************************************************** */
void FlowCtrFlowEverySecond(void)
{
  TasmotaGlobal.seriallog_timer = 600;                // DEBUG Tool   <---------- TRL

  FlowCtr->CurrentTime = micros();                    // get current time from system
  FlowCtr->OneHour++;                                // advance our event timers...
  FlowCtr->OneDay++;
  FlowCtr->SendingRate++;

  if (PinUsed(GPIO_FLOW))                             // check to make sure we have a pin assigned
  {
      FlowCtr->LastPulseTime = FlowCtr->CurrentTime - FlowCtr->timer;   // get time since last pulse
      FlowCtr->OldFlowRate = FlowCtr->CurrentFlow;                      // Save last flow rate

      if (current_pulse_counter > FlowCtr->LastPulseCount)     // check to see if we have a new flow pulse
      {                                                        // yes, we have a flow...
        FlowCtr->WeHaveFlow = true;
        FlowCtr->LastPulseCount = current_pulse_counter;       // save current pulse count

        float  freq = (1.0 / ((float) flow_pulse_period / 1000000.0)); 
        if (freq <= 0.0) freq = 0.0;
        // Calculate current flow rate based on sensor type
        switch (MySettings.flow_type)
        {
          default:
          case 0:   // we have Type 0 flow sensor, units per minute (ie: GPM..)
                FlowCtr->CurrentFlow = (( (float) (MySettings.current_sending_interval) * 1000000.0) / (float) flow_pulse_period ) * (60.0 / (float) MySettings.current_sending_interval);
          break;

          case 1:   // we have Type 1 flow sensor, K-Offset *      flowrate = (freq + offset) * K  --> freq = (PPM / K) - offset   // MySettings.flow_rate_factor
                FlowCtr->CurrentFlow = (( freq + MySettings.flow_offset ) *  MySettings.flow_k);
          break;

          case 2:   // we have Type 2 flow sensor, K-Offset /      flowrate = (freq + offset) / K  --> freq = (PPM * K) - offset
                FlowCtr->CurrentFlow = (( freq + MySettings.flow_offset ) /  MySettings.flow_k);
          break;
        }
        AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %8.4f, %8.4f,  %u,  %8.4fHz"), "Flow rate", FlowCtr->CurrentFlow, FlowCtr->OldFlowRate, flow_pulse_period, freq);
      }

      FlowCtrBoundsCheck();                                                     // let make sure flow is reasonable..

      FlowCtr->Current1hrFlowVolume  += FlowCtr->CurrentFlow * 0.01666667;      // update flow volumes by 1/60, this give us flow per minute
      FlowCtr->Current24hrFlowVolume += FlowCtr->CurrentFlow * 0.01666667;
      FlowCtr->VolumePerFlow         += FlowCtr->CurrentFlow * 0.01666667;

      FlowCtr->CurrentFlowStartTime = FlowCtr->CurrentTime;                     // save start time of current flow

      // check if it's time to send 1hr flow rate
      if (FlowCtr->OneHour >= 3600)                              // 3600 sec
      {
        FlowCtr->OneHour = 0;
        FlowCtr->Saved1hrFlowVolume = FlowCtr->Current1hrFlowVolume; 
        FlowCtr->Current1hrFlowVolume = 0.0;                     // reset current 1hr flow volume
      }

      // check if it's time to send 24hr flow rate
      if (FlowCtr->OneDay >= 86400)                              // 86400 sec
      {
        FlowCtr->OneDay = 0;
        FlowCtr->Saved24hrFlowVolume = FlowCtr->Current24hrFlowVolume;
        FlowCtr->Current24hrFlowVolume = 0.0;                    // reset current 24hr flow volume
      }

      FlowCtrCheckExcessiveFlow();                               // check for excessive flow over our threshold
      FlowCtrCheckFlowTimeOut();                                 // check for a long no flow pulse time out limit...
 
      // We are sending MQTT data only if we had a sufficient flow change in this period, with a minium send rate...
      // As flow sensor have lots of jitter, we will round the flow values to one decimal point for our test of "sufficient flow change".
      float OFR = round(FlowCtr->OldFlowRate * 10) / 10;
      float CF  = round(FlowCtr->CurrentFlow * 10) / 10;
      if  ((OFR != CF) && (FlowCtr->SendingRate >= MySettings.current_sending_interval))
      {
        FlowCtr->SendingRate = 0;                                // reset sample rate...
        ResponseClear();
        Response_P(PSTR("{\"FLOW\":{"));
        if (MySettings.flow_type == 0)
        {
          if bitRead (MySettings.MQTT_send_bit_mask, 0)  ResponseAppend_P(PSTR("\"FlowCount\":%lu,"), current_pulse_counter );
        }
        if bitRead (MySettings.MQTT_send_bit_mask, 1)  ResponseAppend_P(PSTR("\"Flow\":%9.2f,"), (float) FlowCtr->CurrentFlow * MySettings.flow_rate_factor);
        if bitRead (MySettings.MQTT_send_bit_mask, 10) ResponseAppend_P(PSTR("\"Current1HrVolume\":%9.2f,"),  FlowCtr->Current1hrFlowVolume);
        if bitRead (MySettings.MQTT_send_bit_mask, 11) ResponseAppend_P(PSTR("\"Current24HrVolume\":%9.2f,"), FlowCtr->Current24hrFlowVolume);
        if bitRead (MySettings.MQTT_send_bit_mask, 14) ResponseAppend_P(PSTR("\"VolumeThisFlow\":%9.2f,"), FlowCtr->VolumePerFlow);
        if bitRead (MySettings.MQTT_send_bit_mask, 12) ResponseAppend_P(PSTR("\"ExcessFlow\":%s"), (FlowCtr->WeHaveExcessFlow) ? "true" : "false");

        ResponseJsonEndEnd();
        MqttPublishPayloadPrefixTopicRulesProcess_P(STAT, D_RSLT_SENSOR, (char*) TasmotaGlobal.mqtt_data.c_str());
        ResponseClear();
      }   // end of:  if  ((OFR != CF) && (SendingRate >= MySettings.current_sending_interval))
  }    // end of: if (PinUsed(GPIO_FLOW))                
}   // end of: FlowCtrFlowEverySecond(void)
 

/* ******************************************************** */
// this will reset our flow if no flow pulse for an extended amount of time...
void FlowCtrCheckFlowTimeOut(void) 
{
    if ( ((FlowCtr->CurrentTime - FlowCtr->CurrentFlowStartTime) >= MySettings.max_flow_reset_time) && FlowCtr->WeHaveFlow == true)
    {
      FlowCtr->WeHaveFlow = false;
      FlowCtr->LastPulseCount = current_pulse_counter;             // reset pulse count
      FlowCtr->CurrentFlow = 0.0;                                  // so lets reset counters 
      FlowCtr->VolumePerFlow = 0.0;                  
      flow_pulse_period = 0;
      AddLog( LOG_LEVEL_INFO, PSTR("%s: %7.2f, %u"), " We have a flow time out !",  FlowCtr->CurrentFlow, MySettings.max_flow_reset_time);
    }
}   // end of: void FlowCtrCheckFlowTimeOut(void)


/* ******************************************************** */
// this will check to see if we have a flow over our threshold rate for a preset amount of time..
// we may have a stuck valve or ?? If MySettings.flow_threshold_time = 0, we will skip this 
void FlowCtrCheckExcessiveFlow(void)
{
  if ( MySettings.flow_threshold_time > 0)                          // skip Excessive Flow test if zero
  {
    if ( (FlowCtr->CurrentTime >= (FlowCtr->ExcessiveFlowStartTime + MySettings.flow_threshold_time) ) && (FlowCtr->WeHaveFlowOverThreshold == true) )
    {
      FlowCtr->WeHaveExcessFlow = true;
      AddLog( LOG_LEVEL_INFO, PSTR("%s: %7.2f, %u"), " We have Excess Flow over time !",  FlowCtr->CurrentFlow, MySettings.flow_threshold_time);
    } 
    else
    {
      FlowCtr->WeHaveExcessFlow = false;                             // reset excessive flow flag
      FlowCtr->ExcessiveFlowStartTime = FlowCtr->CurrentTime;        // reset start time
    }
  } 
  else
  {                                                                                             
    FlowCtr->WeHaveExcessFlow = false;                               // reset excessive flow flag
    FlowCtr->ExcessiveFlowStartTime = FlowCtr->CurrentTime;          // reset start time
  }
}   // end of: void FlowCtrCheckExcessiveFlow(void)


/* ******************************************************** */
void FlowCtrSaveState(void)                                    
{
  {
    if (PinUsed(GPIO_FLOW))
    {
       // what do we need to save ?? 
    }
  }
}   // end of: void FlowSaveState(void)


/* ******************************************************** */
void FlowCtrShow(bool json)
{
  bool header = false;
  {
    if (PinUsed(GPIO_FLOW))
    {
      if (json)
      {
        if (!header) ResponseAppend_P(PSTR(",\"FLOW\":{"));

        if (MySettings.flow_type == 0)                  // if we have Type 0 flow sensor.
        {
          if bitRead (MySettings.MQTT_send_bit_mask, 0) ResponseAppend_P(PSTR("%s\"FlowCount\":%lu,"),   (header) ? "," : "", current_pulse_counter);
          if bitRead (MySettings.MQTT_send_bit_mask, 5) ResponseAppend_P(PSTR("%s\"RateFactor\":%9.4f,"),(header) ? "," : "", MySettings.flow_rate_factor); 
          if bitRead (MySettings.MQTT_send_bit_mask, 1) ResponseAppend_P(PSTR("%s\"Flow\":%9.4f,"),      (header) ? "," : "", FlowCtr->CurrentFlow * MySettings.flow_rate_factor);
        }
        else                                            // if we have Type 1 or 2 flow sensor.
        {
          if bitRead (MySettings.MQTT_send_bit_mask, 1) ResponseAppend_P(PSTR("%s\"Flow\":%9.4f,"),      (header) ? "," : "", FlowCtr->CurrentFlow);
          if bitRead (MySettings.MQTT_send_bit_mask, 6) ResponseAppend_P(PSTR("%s\"K\":%9.4f,"),         (header) ? "," : "", MySettings.flow_k);
          if bitRead (MySettings.MQTT_send_bit_mask, 7) ResponseAppend_P(PSTR("%s\"Offset\":%9.4f,"),    (header) ? "," : "", MySettings.flow_offset);
        }
        if bitRead (MySettings.MQTT_send_bit_mask, 2) ResponseAppend_P(PSTR("%s\"FlowPeriod\":%9.4f,"),  (header) ? "," : "", (float) flow_pulse_period / 1000000.0);
        if bitRead (MySettings.MQTT_send_bit_mask, 14) ResponseAppend_P(PSTR("\"VolumeThisFlow\":%9.2f,"),  (header) ? "," : "", FlowCtr->VolumePerFlow);
        if bitRead (MySettings.MQTT_send_bit_mask, 3) ResponseAppend_P(PSTR("%s\"1HrVolume\":%9.2f,"),   (header) ? "," : "", FlowCtr->Saved1hrFlowVolume);
        if bitRead (MySettings.MQTT_send_bit_mask, 4) ResponseAppend_P(PSTR("%s\"24HrVolume\":%9.2f,"),  (header) ? "," : "", FlowCtr->Saved24hrFlowVolume);
        if bitRead (MySettings.MQTT_send_bit_mask, 10) ResponseAppend_P(PSTR("%s\"Current1HrVolume\":%9.2f,"),   (header) ? "," : "", FlowCtr->Current1hrFlowVolume);
        if bitRead (MySettings.MQTT_send_bit_mask, 11) ResponseAppend_P(PSTR("%s\"Current24HrVolume\":%9.2f,"),  (header) ? "," : "", FlowCtr->Current24hrFlowVolume);
        if bitRead (MySettings.MQTT_send_bit_mask, 8) ResponseAppend_P(PSTR("%s\"FlowUnits\":\"%s\","),    (header) ? "," : "", FlowCtr->Current_Units);
        if bitRead (MySettings.MQTT_send_bit_mask, 9) ResponseAppend_P(PSTR("%s\"VolumeUnits\":\"%s\","),  (header) ? "," : "", FlowCtr->Current_Volume_Units);
        if bitRead (MySettings.MQTT_send_bit_mask, 12) ResponseAppend_P(PSTR("\"ExcessFlow\":%s"), (FlowCtr->WeHaveExcessFlow) ? "true" : "false");    
        header = true;

// no flow Domoticz option done here as we are not sure what one may needed for Domoticz....
#ifdef USE_DOMOTICZ    
#endif // USE_DOMOTICZ

        if ((0 == TasmotaGlobal.tele_period) && (Settings->flag3.counter_reset_on_tele))
        {
          AddLog( LOG_LEVEL_INFO, PSTR("%s: "), " Domoticz not implemented !");
        }

#ifdef USE_WEBSERVER
      }
      else
      { 
        float  freq = (1.0 / ((float) flow_pulse_period / 1000000.0)); 
        if ( freq <= 0.0) freq = 0.0;
        WSContentSend_PD(PSTR("{s}" D_FLOW_PERIOD "{m}%9.4f %s{e}"), (float) flow_pulse_period / 1000000.0, D_UNIT_SECOND);
        WSContentSend_PD(PSTR("{s}" D_Flow_Frequency "{m}%9.2f %s{e}"), freq, D_UNIT_HZ);
        if (MySettings.flow_type == 0)            // Check for type 0 sensor
        {
          WSContentSend_PD(PSTR("{s}" D_FLOW_RATE "{m}%9.2f %s{e}"), (FlowCtr->CurrentFlow * MySettings.flow_rate_factor), FlowCtr->Current_Units);
          WSContentSend_PD(PSTR("{s}" D_FLOW_COUNT "{m}%lu{e}"), current_pulse_counter);
          WSContentSend_PD(PSTR("{s}" D_Flow_Factor "{m}%9.2f{e}"),  MySettings.flow_rate_factor);  
        }
        else                                      // We have Type 1 or 2 flow sensor, then we will send K & Offset                
        {
          WSContentSend_PD(PSTR("{s}" D_FLOW_RATE "{m}%9.2f %s{e}"), FlowCtr->CurrentFlow, FlowCtr->Current_Units);
          WSContentSend_PD(PSTR("{s}" D_Flow_K "{m}%9.3f{e}"), MySettings.flow_k);
          WSContentSend_PD(PSTR("{s}" D_Flow_Offset "{m}%9.3f{e}"), MySettings.flow_offset);
        }
        WSContentSend_PD(PSTR("{s}" " This Flow{m}%9.0f %s{e}"),  FlowCtr->VolumePerFlow,   FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" " Current 1Hr Flow{m}%9.0f %s{e}"),  FlowCtr->Current1hrFlowVolume,   FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" " Current 24Hr Flow{m}%9.0f %s{e}"), FlowCtr->Current24hrFlowVolume, FlowCtr->Current_Volume_Units);  
        WSContentSend_PD(PSTR("{s}" " Last 1Hr Flow{m}%9.0f %s{e}"),  FlowCtr->Saved1hrFlowVolume,   FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" " Last 24Hr Flow{m}%9.0f %s{e}"), FlowCtr->Saved24hrFlowVolume, FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "We Have Flow" "{m}%d{e}"), (int8_t) FlowCtr->WeHaveFlow);
        WSContentSend_PD(PSTR("{s}" "Flow Over Threshold" "{m}%d{e}"), (int8_t) FlowCtr->WeHaveFlowOverThreshold);
        WSContentSend_PD(PSTR("{s}" "Excess Flow" "{m}%d{e}"), (int8_t) FlowCtr->WeHaveExcessFlow );

#endif // end of: USE_WEBSERVER
      }
    }
  }
  if (header)
  {
    ResponseJsonEnd();
  }
}   // end of: void FlowShow(bool json)


/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

bool Xsns125Cmnd(void)
{
  char argument[XdrvMailbox.data_len];
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "ArgC", ArgC());
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Payload", XdrvMailbox.payload );
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Payload Length", XdrvMailbox.data_len );
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Arg 1", strtol(ArgV(argument, 1), nullptr, 10) );
  AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Arg 2", strtol(ArgV(argument, 2), nullptr, 10) );
  //AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Arg 3", strtol(ArgV(argument, 3), nullptr, 10) );
  //AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %u"), "Index", XdrvMailbox.index );

  if (ArgC() > 1)                                       // we always expect a 2nd parameter
  {
    switch (XdrvMailbox.payload)
    {
    case 1: // Flow Type
                                                        // check range of type, 0-->2 only
      if (((uint8_t)strtol(ArgV(argument, 2), nullptr, 10) >= 0) && ((uint8_t)strtol(ArgV(argument, 2), nullptr, 10) <= 2))
      {
        MySettings.flow_type = (uint8_t) strtol(ArgV(argument, 2), nullptr, 10);
        AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Flow Type", MySettings.flow_type);
      }
      break;

    case 2: // Flow Factor
      MySettings.flow_rate_factor = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 2, Flow Factor", MySettings.flow_rate_factor);
      break;

    case 3: // Flow K
      MySettings.flow_k = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 3, Flow K", MySettings.flow_k);
      break;

    case 4: // Flow Offset
      MySettings.flow_offset = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 4, Flow Offset", MySettings.flow_offset);
      break;

    case 5: // Flow Units
      MySettings.flow_units = (uint8_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 5, Flow Units", MySettings.flow_units);
      break;

    case 6: // Excess flow threshold max count
      MySettings.flow_threshold_max = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 6, Max Flow", MySettings.flow_threshold_max);
      break;

    case 7:   // Flow Threshold Time in second's (convert to miliseconds)
      MySettings.flow_threshold_time = (60000) * (uint32_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 7, Max Flow Time", MySettings.flow_threshold_time);
      break;

    case 8:  // Current Sample Interval in second's, lower limit to 10 seconds
      if ((uint16_t) strtol(ArgV(argument, 2), nullptr, 10) == 0)   MySettings.current_sending_interval = 1;
      else MySettings.current_sending_interval = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 8, Sample Interval", MySettings.current_sending_interval);
      break; 

    case 9:   // MQTT Bit Mask, 16 bits
      MySettings.MQTT_send_bit_mask = (uint16_t) strtol(ArgV(argument, 2), nullptr, 16);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: 0x%04x"), "Case 9, MQTT Bit Mask", MySettings.MQTT_send_bit_mask);
      break;

    case 10: // Max Flow reset time in second's (convert to microseconds)
      if (strtol(ArgV(argument, 2), nullptr, 10) <= 10 ) MySettings.max_flow_reset_time = 10000000;
      else MySettings.max_flow_reset_time = (1000000) * strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 10, Max Flow Reset Time", MySettings.max_flow_reset_time);
      break;
      
    case 11: // Max Flo Rate
      MySettings.max_flow_rate = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 11, Max Flow Rate", MySettings.max_flow_rate);
      break;

    case 12: // Flow Debounce time in MS
      MySettings.flow_debounce = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 12, Flow Debounce", MySettings.flow_debounce);
      break;

    case 13: // Flow Debounce Low time in MS
      MySettings.flow_debounce_low = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 13, Flow Debounce Low", MySettings.flow_debounce_low);
      break;

    case 14: // Flow Debounce High time in MS
      MySettings.flow_debounce_high = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 14, Flow Debounce High", MySettings.flow_debounce_high);
      break;
    }
  }

// build a JSON response
  Response_P(PSTR("{\"" D_FLOWMETER_NAME "\":{\"Flow Type\":%d"), MySettings.flow_type);
  ResponseAppend_P(PSTR(",\"Flow Rate Factor\":%7.2f"), MySettings.flow_rate_factor);
  ResponseAppend_P(PSTR(",\"Flow K\":%7.2f"), MySettings.flow_k);
  ResponseAppend_P(PSTR(",\"Flow Offset\":%7.2f"), MySettings.flow_offset);
  ResponseAppend_P(PSTR(",\"Flow Units\":%d"), MySettings.flow_units);
  ResponseAppend_P(PSTR(",\"Flow Threshold_Max\":%7.2f"), MySettings.flow_threshold_max);
  ResponseAppend_P(PSTR(",\"Flow Threshold Time\":%u"), MySettings.flow_threshold_time);
  ResponseAppend_P(PSTR(",\"Flow Sample Interval\":%u"), MySettings.current_sending_interval);
  char hex_data[8];
  sprintf(hex_data, "%04x", MySettings.MQTT_send_bit_mask);
  ResponseAppend_P(PSTR(",\"MQTT Bit Mask\":\"0x%s\""), hex_data );
  ResponseAppend_P(PSTR(",\"Max Flow Reset Time\":%u"), MySettings.max_flow_reset_time);
  ResponseAppend_P(PSTR(",\"Max Flow Rate\":%7.2f"), MySettings.max_flow_rate);
  ResponseAppend_P(PSTR(",\"Flow Debounce \":%u"), MySettings.flow_debounce);
  ResponseAppend_P(PSTR(",\"Flow Debounce Low \":%u"), MySettings.flow_debounce_low);
  ResponseAppend_P(PSTR(",\"Flow Debounce High \":%u"), MySettings.flow_debounce_high);
  ResponseJsonEndEnd();
  return true;

}   // end of bool Xsns125Cmnd(void)


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns125(uint8_t function)
{
  bool result = false;

  if (FlowCtr->counter)
  {  
    switch (function)
    {
    case FUNC_EVERY_SECOND:
      FlowCtrFlowEverySecond();
      break;
    case FUNC_JSON_APPEND:
      FlowCtrShow(1);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      FlowCtrShow(0);
      break;
#endif    // end of USE_WEBSERVER

    case FUNC_SAVE_BEFORE_RESTART:
    case FUNC_SAVE_AT_MIDNIGHT:
      FlowCtrSaveState();
      break;

    case FUNC_COMMAND:
    case FUNC_COMMAND_SENSOR:
      if (XSNS_125 == XdrvMailbox.index)
      {
        result = Xsns125Cmnd();
      }
      break;
    }
  }
  else
  {
    switch (function)
    {
    case FUNC_INIT:
      FlowCtrInit();
      break;

    case FUNC_PIN_STATE:
      result = FlowCtrPinState();
      break;

    }
  }
  return result;
}

#endif    // end of USE_FLOW

/* ************************* The Very End ************************ */
