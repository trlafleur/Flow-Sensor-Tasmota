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
 *  7-Apr-2022  1.2   TRL - Local Data-struct was change to dynamic
 *  7-Apr-2022  1.2a  TRL - Moved MySettings to settings.h in base code
 *
 *  Notes:  1)  Tested with TASMOTA  11.0.0.3
 *          2)  ESP32, ESP32S3
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
    FlowCtr_type    0   pulse per unit (GPM....)
                    1   K-Offset    flowrate = (freq + offset) * K  --> freq = (PPM / K) - offset
                    2   K-Offset    flowrate = (freq + offset) / K  --> freq = (PPM * K) - offset

    // unit per pulse from flow meter
    FlowCtr_rate_factor                                flow_units
                0.1   0.1 gal per minute            GPM
                1       1 gal per minute            GPM
                10      10 gal per minute           GPM
                100     100 gal gal per minute      GPM
                7.48052 1 cubic feet                Cft
                74.8052 10 cubic feet               Cft
                748.052 100 cubic feet  (unit)      Cft
                1       1 cubic meter               M3
                1       1 litres                    LM

    FlowCtr_units
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

      1     type   0   pulse per unit (GPM....)
                   1   K-Offset    flowrate = (freq + offset) * K  --> freq = (PPM / K)
                   2   K-Offset    flowrate = (freq + offset) / K  --> freq = (PPM * K)

      2     rate_factor    // unit per pulse from flow meter examples, float
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
      10    Max Flow Rate                                 // Max flow rate for this sensor
      11    Debounce 0 = off, 1 = on
      12    Debounce Low Time in MS
      13    Debounce High Time in MS


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
Changes made to Tasmota base code... See integration notes...

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

moved MySettings to settings.h
changes settings.h at 726 and 782
set defaults in Settings.ino at line 1231
*/


#ifdef USE_FLOW
#define XSNS_125 125

#ifndef DEBUG
  #define DEBUG
#endif

/*********************************************************************************************\
 * Flow sensors for Water meters, units per minute or K-Offset types...
\*********************************************************************************************/

// will need to move these to Tasmota base code at a later time...
#define D_FLOW_RATE1  "Flow Rate"
#define D_FLOW_COUNT  "Flow Pulse Count"
#define D_FLOW_PERIOD "Flow Period"
#define D_Flow_Factor "Flow Factor"
#define D_FlowCtr_k      "Flow K"
#define D_FlowCtr_offset "Flow Offset"
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
// these settings are save between re-boots
//    "Settings->"  this structure is now renamed and is in Tasmota base code settings.h  <---------- TRL

// xsns125 Flow Counter varables in settings.h

// struct MYSETTINGS
// {
//   uint8_t  FlowCtr_type =                        0;    // Current type of flow sensor, 0 = flow per unit,  1,2 = K-Offset
//   uint8_t  FlowCtr_units =                       0;    // Current flow units
//   uint16_t FlowCtr_debounce_low =                0;    // Current debounce values...
//   uint16_t FlowCtr_debounce_high =               0;
//   uint16_t FlowCtr_debounce =                    0;
//   uint16_t FlowCtr_MQTT_bit_mask =          0xffff;    // MQTT Bit Mask, Controls what we send
//   uint16_t FlowCtr_current_send_interval =        10;  // in seconds
//   uint32_t flow_threshold_reset_time =    5 * 60 * 1000; // Excessive flow threshold timeout, in milliseconds (20 Min)  
//   float    FlowCtr_max_flow_rate  =           60.0;    // Sensor Max Flow rate in units of flow...
//   float    FlowCtr_threshold_max =            20.0;    // Excessive flow threshold in units of flow
//   float    FlowCtr_rate_factor =               1.0;    // Current Rate Factor
//   float    FlowCtr_k =                        .153;    // For K-Offset flow sensor (--> CST 1in ELF sensor)
//   float    FlowCtr_offset =                  1.047;    // Current Offset
// } MySettings;


/* ******************************************************** */
// Our Local global variables...
volatile uint32_t flow_pulse_period;     // in microseconds
volatile uint32_t current_pulse_count;
volatile bool     FlowLedState  = false; // LED toggles on every pulse 

// local varables...
struct FLOWCTR
{
  uint8_t     no_pullup;                 // Counter input pullup flag (1 = No pullup)
  uint8_t     pin_state;                 // LSB0..3 Last state of counter pin; LSB7==0 IRQ is FALLING, LSB7==1 IRQ is CHANGE
  uint32_t    timer;                     // Last flow time in microseconds
  uint32_t    timer_low_high;            // Last low/high flow counter time in micro seconds
  uint32_t    OneMinute;                 // 
  uint32_t    OneHour;                   // event timers....
  uint32_t    OneDay;                    //
  uint32_t    SendingRate;               // Current sample rate count
  uint32_t    LastPulseCount;            // use to check for new flow
  uint32_t    CurrentTime;               // Current time 
  uint32_t    LastPulseTime;             // Last pulse time     
  uint32_t    ExcessiveFlowStartTime;    // Time that this flow started...
  float       Saved1hrFlowVolume;        // saved volume for the last 1hr period
  float       Saved24hrFlowVolume;       // saved volume for the last 24hr period
  float       Current1hrFlowVolume;      // current run-rate volume for this 1hr period
  float       Current24hrFlowVolume;     // current run-rate volume for this 24hr period
  float       VolumePerFlow;             // Volume for current flow
  float       OldFlowRate;
  float       CurrentFlow;
  float       Freq;
  char        Current_Units[8];           // Default units...
  char        Current_Volume_Units[8];
  bool        WeHaveFlow;                 // true if we have started a new flow
  bool        WeHaveFlowOverThreshold;    // true if current flow exceed threshold
  bool        WeHaveExcessFlow;           // true if we have exceed threshold and we have exceeded Flow_threshold_reset_time
} ;
static struct FLOWCTR *FlowCtr = nullptr;

// Forward declarations...
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
      if (debounce_time <= Settings->FlowCtr_debounce_high * 1000)
        return;
    }
    else
    {
      // last valid pin state was low, current pin state is high
      if (debounce_time <= Settings->FlowCtr_debounce_low * 1000)
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
  if (debounce_time > (Settings->FlowCtr_debounce * 1000))
  {
    FlowCtr->timer = CurrentTimeISR;
    flow_pulse_period = debounce_time;          // pulse period
    current_pulse_count++;                      // pulse count
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
    FlowCtr = (struct FLOWCTR*) calloc(1,sizeof(struct FLOWCTR));     // instantiated of FlowCtr struc

    if (!FlowCtr)                                                     // check to make sure we have allocated memory!
    {
      AddLog(LOG_LEVEL_DEBUG, PSTR("Flow Sensor: out of memory"));
      return;
    }
    
    pinMode(Pin(GPIO_FLOW), bitRead(FlowCtr->no_pullup, 0) ? INPUT : INPUT_PULLUP);
    if ((Settings->FlowCtr_debounce_low == 0) && (Settings->FlowCtr_debounce_high == 0))
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
  switch (Settings->FlowCtr_units)  
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

    current_pulse_count =                 0;    // set counts to zero on reboot...
    flow_pulse_period  =                  0;    // set period to zero on reboot...
    FlowCtr->WeHaveFlow =             false;
    FlowCtr->WeHaveFlowOverThreshold =false;
    FlowCtr->WeHaveExcessFlow =       false;
    FlowCtr->LastPulseTime =              0;    // Last pulse time
    FlowCtr->SendingRate =                0;    // Current sample rate count 
    FlowCtr->Current1hrFlowVolume =     0.0;    // current run-rate volume for this 1hr period
    FlowCtr->Current24hrFlowVolume =    0.0;    // current run-rate volume for this 24hr period
    FlowCtr->VolumePerFlow  =           0.0;    // Volume for current flow
    FlowCtr->OneHour =                    0;    // event timers....
    FlowCtr->OneDay =                     0;
    FlowCtr->OneMinute =                  0;
    FlowCtr->Freq =                       0;
              

    // // lets set global varables to defaults, ---> this is now set in settings.ino and is here just for information
    // Settings->FlowCtr_type =                        0;        // Current type of flow sensor, 0 = flow per unit,  1,2 = K-Offset
    // Settings->FlowCtr_units =                       0;        // Current flow units
    // Settings->FlowCtr_debounce_low =                0;        // Current debounce values...
    // Settings->FlowCtr_debounce_high =               0;
    // Settings->FlowCtr_debounce =                    0;
    // Settings->FlowCtr_MQTT_bit_mask =          0xffff;        // MQTT Bit Mask, Controls what we send
    // Settings->FlowCtr_current_send_interval =      10;        // in seconds
    // Settings->Flow_threshold_reset_time =  20 * 60 * 1000;    // Excessive flow threshold timeout, in miliseconds (20 Min) 
    // Settings->FlowCtr_max_flow_rate  =           60.0;        // Sensor Max Flow rate in units of flow...
    // Settings->FlowCtr_threshold_max =            20.0;        // Excessive flow threshold in units of flow
    // Settings->FlowCtr_rate_factor =               1.0;        // Current Rate Factor
    // Settings->FlowCtr_k =                        .153;        // For K-Offset flow sensor (--> CST 1in ELF sensor)
    // Settings->FlowCtr_offset =                  1.047;        // Current Offset

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
/* ******************************************************** */
void FlowCtrFlowEverySecond(void)
{
  TasmotaGlobal.seriallog_timer = 600;                // DEBUG Tool   <---------- TRL

  FlowCtr->CurrentTime = millis();                    // get current time from system
  FlowCtr->OneMinute++;
  FlowCtr->OneHour++;                                 // advance our event timers...
  FlowCtr->OneDay++;
  FlowCtr->SendingRate++;

  if (PinUsed(GPIO_FLOW))                             // check to make sure we have a pin assigned
  {
    // check if it's time to do 1 minute task updates of volumes
    if (FlowCtr->OneMinute >= 60)                                         // 60 sec
    {
      FlowCtr->OneMinute = 0;
      FlowCtr->OldFlowRate = FlowCtr->CurrentFlow;
      if (current_pulse_count > FlowCtr->LastPulseCount)                  // check to see if we have a new flow pulse
      {                                                                   // yes, we have a flow...
        FlowCtr->Freq = (1.0 / ((float) flow_pulse_period / 1000000.0));  // flow_pulse_period is in microseconds
        if (FlowCtr->Freq < 0.0) FlowCtr->Freq = 0.0;

        // Calculate current flow rate based on sensor type
        switch (Settings->FlowCtr_type)
        {
          default:
           case 0:   // we have Type 0 flow sensor, units per minute (ie: GPM..)
              FlowCtr->CurrentFlow = ((current_pulse_count - FlowCtr->LastPulseCount) * Settings->FlowCtr_rate_factor); 
              AddLog( LOG_LEVEL_DEBUG, PSTR("%s:, %9.2f,  %u"), " flow rate, period !",  FlowCtr->CurrentFlow, flow_pulse_period);
           break;

          case 1:   // we have Type 1 flow sensor, K-Offset *  flowrate = (freq + offset) * K  --> freq = (PPM / K) - offset   // Settings->FlowCtr_rate_factor
              FlowCtr->CurrentFlow = ((( FlowCtr->Freq + Settings->FlowCtr_offset ) *  Settings->FlowCtr_k) * Settings->FlowCtr_rate_factor);
           break;

          case 2:   // we have Type 2 flow sensor, K-Offset /  flowrate = (freq + offset) / K  --> freq = (PPM * K) - offset
              FlowCtr->CurrentFlow = ((( FlowCtr->Freq + Settings->FlowCtr_offset ) /  Settings->FlowCtr_k) * Settings->FlowCtr_rate_factor);
          break;
        }   // end of: switch (Settings->FlowCtr_type) 

        AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %8.4f,  %u,  %8.4fHz"), "Flow rate", FlowCtr->CurrentFlow, flow_pulse_period, FlowCtr->Freq);
        FlowCtrBoundsCheck();                                            // let make sure flow is reasonable...

        FlowCtr->Current1hrFlowVolume  += FlowCtr->CurrentFlow ;         // update flow volumes, this give us flow per minute
        FlowCtr->Current24hrFlowVolume += FlowCtr->CurrentFlow ;
        FlowCtr->VolumePerFlow         += FlowCtr->CurrentFlow ;

        FlowCtr->WeHaveFlow = true;
        FlowCtr->LastPulseCount = current_pulse_count;                    // save current pulse count

      }   // end of: if (current_pulse_count > FlowCtr->LastPulseCount)
      else                                                                // no change in pulse count
      {
        FlowCtr->WeHaveFlow =   false;
        FlowCtr->CurrentFlow =    0.0;                                    // so lets reset counters 
        FlowCtr->VolumePerFlow =  0.0;
        FlowCtr->Freq =           0.0;                  
        flow_pulse_period =         0;
      }
    }   // end of: if (FlowCtr->OneMinute >= 60)

    // check if it's time to update 1hr flow rate
    if (FlowCtr->OneHour >= 3600)                                 // 1hr = 3600 sec
    {
      FlowCtr->OneHour = 0;
      FlowCtr->Saved1hrFlowVolume = FlowCtr->Current1hrFlowVolume; 
      FlowCtr->Current1hrFlowVolume = 0.0;                        // reset current 1hr flow volume
    }

    // check if it's time to update 24hr flow rate
    if (FlowCtr->OneDay >= 86400)                                 // 24hr = 86400 sec
    {
      FlowCtr->OneDay = 0;
      FlowCtr->Saved24hrFlowVolume = FlowCtr->Current24hrFlowVolume;
      FlowCtr->Current24hrFlowVolume = 0.0;                       // reset current 24hr flow volume
    }

    FlowCtrCheckExcessiveFlow();                                  // check for excessive flow over our threshold
  
    // We are sending MQTT data only if we had a sufficient flow change in this period, with a minium send rate...
    // As flow sensor have lots of jitter, we will round the flow values to one decimal point for our test of "sufficient flow change".
    float OFR = round(FlowCtr->OldFlowRate * 10) / 10;
    float CF  = round(FlowCtr->CurrentFlow * 10) / 10;

    AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %8.4f, %8.4f,  %u"), "Flow rate MQTT Check", FlowCtr->CurrentFlow, FlowCtr->OldFlowRate, FlowCtr->SendingRate);
    if  ((FlowCtr->CurrentFlow != 0) && ((OFR != CF) && (FlowCtr->SendingRate >= Settings->FlowCtr_current_send_interval)))
    {
    AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %8.4f, %8.4f,  %u"), "Flow rate in MQTT Send", FlowCtr->CurrentFlow, FlowCtr->OldFlowRate, FlowCtr->SendingRate);
      FlowCtr->SendingRate = 0;                                   // reset sending rate...
      ResponseClear();
      Response_P(PSTR("{\"FLOW\":{"));
      if (Settings->FlowCtr_type == 0)
      {
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 0)  ResponseAppend_P(PSTR("\"FlowPulseCount\":%lu,"), current_pulse_count );
      }
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 1)  ResponseAppend_P(PSTR("\"Rate\":%9.2f,"), (float) FlowCtr->CurrentFlow);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 10) ResponseAppend_P(PSTR("\"Current1HrVolume\":%9.2f,"),  FlowCtr->Current1hrFlowVolume);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 11) ResponseAppend_P(PSTR("\"Current24HrVolume\":%9.2f,"), FlowCtr->Current24hrFlowVolume);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 14) ResponseAppend_P(PSTR("\"VolumeThisFlow\":%9.2f,"), FlowCtr->VolumePerFlow);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 12) ResponseAppend_P(PSTR("\"ExcessFlow\":%s"), (FlowCtr->WeHaveExcessFlow) ? "true" : "false");

      ResponseJsonEndEnd();
      MqttPublishPayloadPrefixTopicRulesProcess_P(STAT, D_RSLT_SENSOR, (char*) TasmotaGlobal.mqtt_data.c_str());
      ResponseClear();

    }   // end of:  if  ((OFR != CF) && (SendingRate >= Settings->FlowCtr_current_send_interval))

  }   // end of: if (PinUsed(GPIO_FLOW))                
}   // end of: FlowCtrFlowEverySecond(void)
 

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
      if ( FlowCtr->CurrentFlow >= Settings->FlowCtr_max_flow_rate)
      {  
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " Flow rate over max !",  FlowCtr->CurrentFlow);
        FlowCtr->CurrentFlow = Settings->FlowCtr_max_flow_rate;                                               // set to max
      }

      // check for possible flow over threshold 
      if ( FlowCtr->CurrentFlow >= Settings->FlowCtr_threshold_max) 
      {
        if (FlowCtr->WeHaveFlowOverThreshold == false)  FlowCtr->ExcessiveFlowStartTime = millis();           // set timer
        FlowCtr->WeHaveFlowOverThreshold = true;
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " We have a flow over threshold !", FlowCtr->CurrentFlow);
      }
       else FlowCtr->WeHaveFlowOverThreshold = false;
}   // end of: void FlowCtrBoundsCheck(void)


/* ******************************************************** */
// this will check to see if we have a flow over our threshold rate for a preset amount of time..
// we may have a stuck valve or ?? If Settings->flow_threshold_reset_time = 0, we will skip this 
void FlowCtrCheckExcessiveFlow(void)
{
  if ( Settings->Flow_threshold_reset_time > 0)                      // skip Excessive Flow test if zero
  {
    if ( (FlowCtr->CurrentTime >= (FlowCtr->ExcessiveFlowStartTime + Settings->Flow_threshold_reset_time) ) && (FlowCtr->WeHaveFlowOverThreshold == true) )
    {
      FlowCtr->WeHaveExcessFlow = true;
      AddLog( LOG_LEVEL_INFO, PSTR("%s: %7.2f, %u"), " We have Excess Flow over time !",  FlowCtr->CurrentFlow, Settings->Flow_threshold_reset_time);
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

        if (Settings->FlowCtr_type == 0)                  // if we have Type 0 flow sensor.
        {
          if bitRead (Settings->FlowCtr_MQTT_bit_mask, 0) ResponseAppend_P(PSTR("%s\"FlowPulseCount\":%lu,"),   (header) ? "," : "", current_pulse_count);
        }
        else                                             // if we have Type 1 or 2 flow sensor.
        {
          if bitRead (Settings->FlowCtr_MQTT_bit_mask, 6) ResponseAppend_P(PSTR("%s\"K\":%9.4f,"),         (header) ? "," : "", Settings->FlowCtr_k);
          if bitRead (Settings->FlowCtr_MQTT_bit_mask, 7) ResponseAppend_P(PSTR("%s\"Offset\":%9.4f,"),    (header) ? "," : "", Settings->FlowCtr_offset);
          if bitRead (Settings->FlowCtr_MQTT_bit_mask, 2) ResponseAppend_P(PSTR("%s\"FlowPeriod\":%9.4f,"),  (header) ? "," : "", (float) flow_pulse_period / 1000000.0);
        }
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 1)  ResponseAppend_P(PSTR("%s\"Rate\":%9.4f,"),        (header) ? "," : "", FlowCtr->CurrentFlow);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 5)  ResponseAppend_P(PSTR("%s\"RateFactor\":%9.4f,"),(header) ? "," : "", Settings->FlowCtr_rate_factor); 
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 14) ResponseAppend_P(PSTR("\"VolumeThisFlow\":%9.2f,"),  (header) ? "," : "", FlowCtr->VolumePerFlow);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 3)  ResponseAppend_P(PSTR("%s\"1HrVolume\":%9.2f,"),   (header) ? "," : "", FlowCtr->Saved1hrFlowVolume);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 4)  ResponseAppend_P(PSTR("%s\"24HrVolume\":%9.2f,"),  (header) ? "," : "", FlowCtr->Saved24hrFlowVolume);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 10) ResponseAppend_P(PSTR("%s\"Current1HrVolume\":%9.2f,"),   (header) ? "," : "", FlowCtr->Current1hrFlowVolume);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 11) ResponseAppend_P(PSTR("%s\"Current24HrVolume\":%9.2f,"),  (header) ? "," : "", FlowCtr->Current24hrFlowVolume);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 8)  ResponseAppend_P(PSTR("%s\"FlowUnits\":\"%s\","),    (header) ? "," : "", FlowCtr->Current_Units);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 9)  ResponseAppend_P(PSTR("%s\"VolumeUnits\":\"%s\","),  (header) ? "," : "", FlowCtr->Current_Volume_Units);
        if bitRead (Settings->FlowCtr_MQTT_bit_mask, 12) ResponseAppend_P(PSTR("\"ExcessFlow\":%s"), (FlowCtr->WeHaveExcessFlow) ? "true" : "false");    
        header = true;

// no flow Domoticz option done here as we are not sure what one may needed for Domoticz....
#ifdef USE_DOMOTICZ    
#endif // USE_DOMOTICZ

        {
          AddLog( LOG_LEVEL_INFO, PSTR("%s: "), " Domoticz not implemented !");
        }

#ifdef USE_WEBSERVER
      }   // end of:  if (json)
      else
      { 
        WSContentSend_PD(PSTR("{s}" D_FLOW_RATE "{m}%9.2f %s{e}"), (FlowCtr->CurrentFlow), FlowCtr->Current_Units);
        if (Settings->FlowCtr_type == 0)            // Check for type 0 sensor
        {
          WSContentSend_PD(PSTR("{s}" D_FLOW_COUNT "{m}%lu{e}"), current_pulse_count);
        }
        else                                      // We have Type 1 or 2 flow sensor, then we will send K & Offset                
        {
          WSContentSend_PD(PSTR("{s}" D_FLOW_PERIOD "{m}%9.4f %s{e}"), (float) flow_pulse_period / 1000000.0, D_UNIT_SECOND);
          WSContentSend_PD(PSTR("{s}" D_Flow_Frequency "{m}%9.2f %s{e}"), FlowCtr->Freq, D_UNIT_HZ);
          WSContentSend_PD(PSTR("{s}" D_FlowCtr_k "{m}%9.3f{e}"), Settings->FlowCtr_k);
          WSContentSend_PD(PSTR("{s}" D_FlowCtr_offset "{m}%9.3f{e}"), Settings->FlowCtr_offset);
        }
        WSContentSend_PD(PSTR("{s}" D_Flow_Factor "{m}%9.2f{e}"),  Settings->FlowCtr_rate_factor); 
        WSContentSend_PD(PSTR("{s}" "This Flow{m}%9.0f %s{e}"),  FlowCtr->VolumePerFlow,   FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "Current 1Hr Flow{m}%9.0f %s{e}"),  FlowCtr->Current1hrFlowVolume,   FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "Current 24Hr Flow{m}%9.0f %s{e}"), FlowCtr->Current24hrFlowVolume, FlowCtr->Current_Volume_Units);  
        WSContentSend_PD(PSTR("{s}" "Last 1Hr Flow{m}%9.0f %s{e}"),  FlowCtr->Saved1hrFlowVolume,   FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "Last 24Hr Flow{m}%9.0f %s{e}"), FlowCtr->Saved24hrFlowVolume, FlowCtr->Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "We Have Flow" "{m}%d{e}"), (int8_t) FlowCtr->WeHaveFlow);
        WSContentSend_PD(PSTR("{s}" "Flow Over Threshold" "{m}%d{e}"), (int8_t) FlowCtr->WeHaveFlowOverThreshold);
        WSContentSend_PD(PSTR("{s}" "Excess Flow" "{m}%d{e}"), (int8_t) FlowCtr->WeHaveExcessFlow );

#endif // end of: USE_WEBSERVER
      }   // end of: if (json) else
    }   // end of: if (PinUsed(GPIO_FLOW))
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
        Settings->FlowCtr_type = (uint8_t) strtol(ArgV(argument, 2), nullptr, 10);
        AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Flow Type", Settings->FlowCtr_type);
      }
      break;

    case 2: // Flow Factor
      Settings->FlowCtr_rate_factor = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 2, Flow Factor", Settings->FlowCtr_rate_factor);
      break;

    case 3: // Flow K
      Settings->FlowCtr_k = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 3, Flow K", Settings->FlowCtr_k);
      break;

    case 4: // Flow Offset
      Settings->FlowCtr_offset = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 4, Flow Offset", Settings->FlowCtr_offset);
      break;

    case 5: // Flow Units
      Settings->FlowCtr_units = (uint8_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 5, Flow Units", Settings->FlowCtr_units);
      break;

    case 6: // Excess flow threshold max count
      Settings->FlowCtr_threshold_max = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "Case 6, Max Flow", Settings->FlowCtr_threshold_max);
      break;

    case 7:   // Flow Threshold Time in second's (convert to milliseconds)
      Settings->Flow_threshold_reset_time = (1000) * (uint32_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 7, Max Flow Time", Settings->Flow_threshold_reset_time);
      break;

    case 8:  // Current Sample Interval in second's, lower limit to 10 seconds
      if ((uint16_t) strtol(ArgV(argument, 2), nullptr, 10) == 0)   Settings->FlowCtr_current_send_interval = 1;
      else Settings->FlowCtr_current_send_interval = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 8, Sample Interval", Settings->FlowCtr_current_send_interval);
      break; 

    case 9:   // MQTT Bit Mask, 16 bits
      Settings->FlowCtr_MQTT_bit_mask = (uint16_t) strtol(ArgV(argument, 2), nullptr, 16);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: 0x%04x"), "Case 9, MQTT Bit Mask", Settings->FlowCtr_MQTT_bit_mask);
      break;
      
    case 10: // Max Flo Rate
      Settings->FlowCtr_max_flow_rate = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 10, Max Flow Rate", Settings->FlowCtr_max_flow_rate);
      break;

    case 11: // Flow Debounce time in MS
      Settings->FlowCtr_debounce = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 11, Flow Debounce", Settings->FlowCtr_debounce);
      break;

    case 12: // Flow Debounce Low time in MS
      Settings->FlowCtr_debounce_low = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 12, Flow Debounce Low", Settings->FlowCtr_debounce_low);
      break;

    case 13: // Flow Debounce High time in MS
      Settings->FlowCtr_debounce_high = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "Case 13, Flow Debounce High", Settings->FlowCtr_debounce_high);
      break;
    }
  }

// build a JSON response
  Response_P(PSTR("{\"" D_FLOWMETER_NAME "\":{\"Flow Type\":%d"), Settings->FlowCtr_type);
  ResponseAppend_P(PSTR(",\"Flow Rate Factor\":%7.2f"), Settings->FlowCtr_rate_factor);
  ResponseAppend_P(PSTR(",\"Flow K\":%7.2f"), Settings->FlowCtr_k);
  ResponseAppend_P(PSTR(",\"Flow Offset\":%7.2f"), Settings->FlowCtr_offset);
  ResponseAppend_P(PSTR(",\"Flow Units\":%d"), Settings->FlowCtr_units);
  ResponseAppend_P(PSTR(",\"Flow Threshold_Max\":%7.2f"), Settings->FlowCtr_threshold_max);
  ResponseAppend_P(PSTR(",\"Flow Threshold Time\":%u"), Settings->Flow_threshold_reset_time);
  ResponseAppend_P(PSTR(",\"Flow Sample Interval\":%u"), Settings->FlowCtr_current_send_interval);
  char hex_data[8];
  sprintf(hex_data, "%04x", Settings->FlowCtr_MQTT_bit_mask);
  ResponseAppend_P(PSTR(",\"MQTT Bit Mask\":\"0x%s\""), hex_data );
  ResponseAppend_P(PSTR(",\"Max Flow Rate\":%7.2f"), Settings->FlowCtr_max_flow_rate);
  ResponseAppend_P(PSTR(",\"Flow Debounce \":%u"), Settings->FlowCtr_debounce);
  ResponseAppend_P(PSTR(",\"Flow Debounce Low \":%u"), Settings->FlowCtr_debounce_low);
  ResponseAppend_P(PSTR(",\"Flow Debounce High \":%u"), Settings->FlowCtr_debounce_high);
  ResponseJsonEndEnd();
  return true;

}   // end of bool Xsns125Cmnd(void)


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns125(uint8_t function)
{
  bool result = false;

  if (FUNC_INIT == function)    // check if we need to do FlowInit
  {
    FlowCtrInit();
  }
  else if (FlowCtr)             // check that we have allocated struct
  {
    switch (function)           // do functions as needed...
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

    case FUNC_PIN_STATE:
      result = FlowCtrPinState();
      AddLog( LOG_LEVEL_DEBUG, PSTR("%s:"), " In Function Pin State");
      break;
    }   // end of: switch (function)
  }   // end of: else if (FlowCtr)
  return result;
}   // end of: bool Xsns125(uint8_t function)

#endif    // end of USE_FLOW

/* ************************* The Very End ************************ */
