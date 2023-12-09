/*
  xsns_125_flow_counter.ino - Flow sensors (water meters... sensor support for Tasmota)

  tom@lafleur.us
  Copyright (C) 2022, 2023  Tom Lafleur and Theo Arends

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
 *  -----------  ---   ----------------------------------------------------------
 *   1-Oct-2021  1.0   TRL - first build
 *   3-Apr-2022  1.1   TRL - refactoring of code base
 *   5-Apr-2022  1.1a  TRL - added check for sufficient flow change
 *   7-Apr-2022  1.2   TRL - Local Data-struct was change to dynamic
 *   7-Apr-2022  1.2a  TRL - Moved MySettings to settings.h in base code (removed, See 1.4)
 *   1-Aug-2022  1.3   TRL - Moved to 12.1.0.2, Moved MySettings back to local space, not in settings.h for now (removed, See 1.4)
 *  19-Dec-2022  1.3a  TRL - Moved to 12.3.1.1
 *  16-Jan-2023  1.4   TRL - added save setings to flash file system
 *  17-Jan-2023  1.4a  TRL - change some variables names, option for defaults settings to be in --> user_config_override.h
 *
 *
 *  Notes:  1)  Tested with TASMOTA  12.3.1.1
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
    may or may not be suitable for large volume industrial meters.

    There are two basic type of water flow meters, some that produce a pulse per unit of flow,
    and K-Offset flow meters.

    Unit per flow meter, give a pulse per unit of flow, typical devices give 1 to 100 gal/pulse or 
    a pulse per cubic feet of water. One "unit" of of water is 100 ccf or 748.052 gals.

    Almost all of the turbine type flow sensors used in irrigation, use two calibration factors
    specified: a “K” factor and an “Offset”.

    During calibration the manufacturer measures the pulse rate outputs for a number of precise flow rates.
    These are plotted, but since the turbine has some friction, the graph will not be
    linear especially at the low end and a linear regression is done to get a best fit straight line.
    The “K” factor represents the slope of the fitted line and has a dimension of pulses per unit volume moved.
    Offset represents the small amount of liquid flow required to start the turbine moving.
    You can assume that if any pulses are arriving at all, at least the offset volume of liquid is moving.

    There does not seem to be a standard for how K factor flow meters are presented.
    Flow sensors output a pulse stream at a frequency proportional to the flow volume as calibrated,
    With some sensors like CST, RainBird, you multiply the pulse frequency by the K factor to obtain a volume rate.
    Others however like Badger, require you to divide the pulse frequency by K.

    So there are two basic type of K-Offset flow sensors, CST and many other are of type = 1,
    Some like Badger are of type = 2. So read the vendors data sheet!

    Frequency = (Gallons per Minute / K ) – Offset  or  = (Gallons per Minute * K ) – Offset
    We are measuring pulse frequency so turning the equation around:
    Gallons per minute = (Frequency + Offset) * K  or  = (Frequency + offset) / K

    // flow meter type
    FlowCtr_type    0   pulse per unit (GPM....)
                    1   K-Offset    flowrate = (freq + offset) * K  --> freq = (PPM / K) - offset
                    2   K-Offset    flowrate = (freq + offset) / K  --> freq = (PPM * K) - offset

    // unit per pulse from flow meter
    FlowCtr_rate_factor                             flow_units
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
 * We will typical limit the flow range to be about .25gpm to 60gpm for a 1 GPM sensor
 *  We will reset flow period timer at 4 minutes or so if no pulses...
 * 
 * We also sense for excessive flow, we do this by setting a excessive flow limit
 *  and the amount of time it takes to be over this limit.
 *   
 * Most of these settings are changeable from commands to the device.
 *
 *
 *    https://www.petropedia.com/definition/7578/meter-k-factor
 *    https://instrumentationtools.com/flow-meter-k-factor-and-calculations/
 *    https://www.creativesensortechnology.com/copy-of-pct-120
 * 
 *
 *  Typical I/O Pins...
 *    GPIO13  Heartbeat
 *    GPIO14  H2O Flow Pulse
 *    GPIO22  SCL                   // not use here
 *    GPIO23  SDA                   // not use here
 *    GPIO32  Flow LED
 *    GPIO36  ADC-1 for pressure sensor (need berry code)
 * 
 * 
 * 
 *  File system..
 *    on code load
 *
************************************************************************************** */

/* **************************************************************************************
  Xsns127Cmnd:

  format: Sensor125 1,2,3

      0            0   set defaults to file system, reset driver   
     
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
      3     K              // K value from device, float
      4     Offset         // Offset value from device, float
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
      

  **************************************************************************************
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
        15      Message's
*/

/* ************************************************************************************** */
// -------------------->  Flow Sensor Defaults (X125)  <-------------------------------

// define here or in --> user_config_override.h
 
/*
 #define xFlowCtr_type                          1         // Current type of flow sensor, 0 = flow per unit,  1,2 =  K-Offset
 #define xFlowCtr_units                         0         // Current flow units
 #define xFlowCtr_debounce_low                  0         // Current debounce values...
 #define xFlowCtr_debounce_high                 0 
 #define xFlowCtr_debounce                      0 
 #define xFlowCtr_MQTT_bit_mask            0xffff         // MQTT Bit Mask, Controls what we send via MQTT
 #define xFlowCtr_current_send_interval        10         // In second's
 #define xFlow_threshold_reset_time     (20 * 60 * 1000)  // Excessive flow threshold timeout, in miliseconds (20 Min)  
 #define xFlowCtr_max_flow_rate              60.0f        // Sensor Max Flow rate in units of flow...
 #define xFlowCtr_threshold_max              20.0f        // Excessive flow threshold in units of flow
 #define xFlowCtr_rate_factor                 1.0f        // Current Rate Factor
 #define xFlowCtr_k                          .153f        // For K-Offset flow sensor (--> CST 1in ELF sensor)
 #define xFlowCtr_offset                    1.047f        // Current Offset
*/

/* ************************************************************************************** 
Changes made to Tasmota base code... 
See integration notes...
12.3.1.1

include/tasmota_template.h 
(must be at end of structure)
line 208 
GPIO_FLOW, GPIO_FLOW_NP, GPIO_FLOW_LED,        // Flow Sensor xsns_125      //  <---------------  TRL

line 459
D_SENSOR_FLOW "|" D_SENSOR_FLOW "_n|" D_SENSOR_FLOW_LED "|"    // Flow xsns_125  // <---------------  TRL

line 542
#ifdef USE_FLOW
  AGPIO(GPIO_FLOW),                            // Flow xsns_125             // <---------------  TRL
  AGPIO(GPIO_FLOW_NP),
  AGPIO(GPIO_FLOW_LED),
#endif

tasmota/language/en_GB.h
at line 921
#define D_SENSOR_FLOW          "H2O Flow"      // Flow xsns_125             // <---------------  TRL
#define D_SENSOR_FLOW_N        "H2O Flow N"
#define D_SENSOR_FLOW_LED      "H2O Flow Led"

*/

/* ******************************************************** */
#ifdef USE_FLOW
  #define XSNS_125  125

#ifndef DEBUG_MyFlow
  #define DEBUG_MyFlow
#endif

#ifdef ESP32                       // ESP32 only.

/*********************************************************************************************\
 * Flow sensors for Water meters, units per minute or K-Offset types...
\*********************************************************************************************/

// will should move these to Tasmota base code at a later time to support multiple language
//#define D_FLOW_RATE1              "Flow Rate"
#define D_FLOW_COUNT              "Flow Pulse Count"
#define D_FLOW_PERIOD             "Flow Period"
#define D_Flow_Factor             "Flow Factor"
#define D_FlowCtr_k               "Flow K"
#define D_FlowCtr_offset          "Flow Offset"
#define D_Flow_Frequency          "Flow Frequency"

#define D_FLOWMETER_NAME          "Flow_Meter"
//#define D_PRFX_FLOW               "Flow"
//#define D_CMND_FLOW_TYPE          "Type"
//#define D_CMND_FLOW_RATE          "Flow_Rate"
//#define D_CMND_FLOW_DEBOUNCE      "Debounce"
//#define D_CMND_FLOW_DEBOUNCELOW   "Debounce_Low"
//#define D_CMND_FLOW_DEBOUNCEHIGH  "Debounce_High"

#define D_GPM "GPM"   // 0
#define D_CFT "Cft"   // 1
#define D_M3  "M3"    // 2
#define D_LM  "lM"    // 3

#define D_GAL "GAL"   // 0
#define D_CF  "CF"    // 1
#define D_CM  "CM"    // 2
#define D_L   "L"     // 3


/* ******************************************************** */
// Format  1.3a = 0x01 03 01, 1.4 0x01 04 00
const uint32_t  MyFlow_Settings_VERSION = 0x010401;       // Latest settings version)
const char      Flow_SW_Version[8] = "1.4a";

// this is the current settings values from filesystem (44 bytes)
struct MYSETTINGS
{
  uint32_t crc32;                                                               // To detect file changes
  uint32_t version;                                                             // To detect settings file changes
  uint8_t  FlowCtr_type =                   xFlowCtr_type;                      // Current type of flow sensor, 0 = flow per unit,  1, 2 = K-Offset
  uint8_t  FlowCtr_units =                  xFlowCtr_units;                     // Current flow units
  uint16_t FlowCtr_debounce_low =           xFlowCtr_debounce_low;              // Current debounce values...
  uint16_t FlowCtr_debounce_high =          xFlowCtr_debounce_high;
  uint16_t FlowCtr_debounce =               xFlowCtr_debounce;
  uint16_t FlowCtr_MQTT_bit_mask =          xFlowCtr_MQTT_bit_mask;             // MQTT Bit Mask, Controls what we send
  uint16_t FlowCtr_current_send_interval =  xFlowCtr_current_send_interval;     // in seconds
  uint32_t Flow_threshold_reset_time =      xFlow_threshold_reset_time;         // Excessive flow threshold timeout, in milliseconds (20 Min)  
  float    FlowCtr_max_flow_rate  =         xFlowCtr_max_flow_rate;             // Sensor Max Flow rate in units of flow...
  float    FlowCtr_threshold_max =          xFlowCtr_threshold_max;             // Excessive flow threshold in units of flow
  float    FlowCtr_rate_factor =            xFlowCtr_rate_factor;               // Current Rate Factor
  float    FlowCtr_k =                      xFlowCtr_k;                         // For K-Offset flow sensor (--> CST 1in ELF sensor)
  float    FlowCtr_offset =                 xFlowCtr_offset;                    // Current Offset (--> CST 1in ELF sensor)
} MySettings;


/* ******************************************************** */
// Global structure's containing non-saved variables

// Our Local global volatile variables...
volatile DRAM_ATTR uint32_t flow_pulse_period;        // in microseconds
volatile DRAM_ATTR uint32_t flow_current_pulse_count;
volatile DRAM_ATTR bool     flow_led_state  = false;  // LED toggles on every pulse 

struct FLOWCTR
{
  bool        WeHaveFlow;                // true if we have started a new flow
  bool        WeHaveFlowOverThreshold;   // true if current flow exceed threshold
  bool        WeHaveExcessFlow;          // true if we have exceed threshold and we have exceeded Flow_threshold_reset_time
  char        Current_Units[8];          // Default units...
  char        Current_Volume_Units[8];
  uint8_t     no_pullup;                 // Counter input pullup flag (1 = No pullup)
  uint8_t     pin_state;                 // LSB0..3 Last state of counter pin; LSB7==0 IRQ is FALLING, LSB7==1 IRQ is CHANGE
  uint32_t    timer;                     // Last flow time in microseconds
  uint32_t    timer_low_high;            // Last low/high flow counter time in microseconds
  uint32_t    OneMinute;                 // event timers....
  uint32_t    OneHour;                   // event timers....
  uint32_t    OneDay;                    // event timers....
  uint32_t    SendingRate;               // Current sample rate count
  uint32_t    LastPulseCount;            // Last pules count,use to check for new flow
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
} FlowCtr;


/* ******************************************************** */
// Forward declarations...
void FlowCtrCheckExcessiveFlow(void);
void FlowCtrBoundsCheck(void);
void FlowCtrFlowEverySecond(void);
void IRAM_ATTR FlowCtrIsr(void);
bool FlowCtrPinState(void);
void FlowCtrInit(void);
void FlowCtrBoundsCheck(void);
void FlowCtrCheckExcessiveFlow(void);
void FlowCtrSaveState(void);
void FlowCtrShow(bool json);
bool Xsns125Cmnd(void);
bool Xsns125(uint32_t function);
uint32_t FlowSettingsCrc32(void);
void FlowSettingsDefault(void);
void FlowSettingsDelta(void);
void FlowSettingsLoad(void);
void FlowSettingsSave(void);


/* ******************************************************** */
/* ********************** ISR ***************************** */
/* ******************************************************** */
void IRAM_ATTR FlowCtrIsr(void)
{
  volatile uint32_t flow_current_time_ISR = micros();
  volatile uint32_t flow_debounce_time;

  if (FlowCtr.pin_state)
  {
    // handle low and high debounce times when configured
    if (digitalRead(Pin(GPIO_FLOW)) == bitRead(FlowCtr.pin_state, 0))
    {
      // new pin state to be ignored because debounce Time was not met during last IRQ
      return;
    }
    flow_debounce_time = flow_current_time_ISR - FlowCtr.timer_low_high;
    if bitRead (FlowCtr.pin_state, 0)
    {
      // last valid pin state was high, current pin state is low
      if (flow_debounce_time <= MySettings.FlowCtr_debounce_high * 1000)
        return;
    }
    else
    {
      // last valid pin state was low, current pin state is high
      if (flow_debounce_time <= MySettings.FlowCtr_debounce_low * 1000)
        return;
    }
    // passed debounce check, save pin state and timing
    FlowCtr.timer_low_high = flow_current_time_ISR;
    FlowCtr.pin_state ^= 1;
    // do not count on rising edge
    if bitRead (FlowCtr.pin_state, 0)
      {
        return;
      }
  }   // end of (FlowCtr.pin_state)

  flow_debounce_time = flow_current_time_ISR - FlowCtr.timer;
  if (flow_debounce_time > (MySettings.FlowCtr_debounce * 1000))
  {
    FlowCtr.timer = flow_current_time_ISR;
    flow_pulse_period = flow_debounce_time;          // pulse period
    flow_current_pulse_count++;                 // pulse count
  }

  // Optional external LED to show each pulse
 if (PinUsed(GPIO_FLOW_LED)) 
  {   
     if (flow_led_state) 
      {
        flow_led_state = false;
        digitalWrite(Pin(GPIO_FLOW_LED), 0);
      }
    else
      {
        flow_led_state = true;
        digitalWrite(Pin(GPIO_FLOW_LED), 1);
      }
  }
}   // end of void IRAM_ATTR FlowCtrIsr(void)


/* ******************************************************** */
uint32_t FlowSettingsCrc32(void)
{
      // Use Tasmota CRC calculation function
  return GetCfgCrc32( (uint8_t*) &MySettings +4, sizeof(MySettings) -4);
}

/* ******************************************************** */
void FlowSettingsDefault(void) 
{
  // Initial default values in case settings file is not found
  AddLog(LOG_LEVEL_INFO, PSTR("X125-flow0: Setting Defaults"));

  MySettings.crc32 =                          0;                                  // To detect file corruption 
  MySettings.version =                        MyFlow_Settings_VERSION;            // Settings versions                                                  // To detect driver function changes
  MySettings.FlowCtr_type =                   xFlowCtr_type;                      // Current type of flow sensor, 0 = flow per unit,  1, 2 = K-Offset  // uint8_t <------------- TRL
  MySettings.FlowCtr_units =                  xFlowCtr_units;                     // Current flow units     // uint8_t
  MySettings.FlowCtr_debounce_low =           xFlowCtr_debounce_low;              // Current debounce values...
  MySettings.FlowCtr_debounce_high =          xFlowCtr_debounce_high;
  MySettings.FlowCtr_debounce =               xFlowCtr_debounce;
  MySettings.FlowCtr_MQTT_bit_mask =          xFlowCtr_MQTT_bit_mask;             // MQTT Bit Mask, Controls what we send
  MySettings.FlowCtr_current_send_interval =  xFlowCtr_current_send_interval;     // in seconds
  MySettings.Flow_threshold_reset_time =      xFlow_threshold_reset_time;         // Excessive flow threshold timeout, in milliseconds (20 Min)  
  MySettings.FlowCtr_max_flow_rate  =         xFlowCtr_max_flow_rate;             // Sensor Max Flow rate in units of flow...
  MySettings.FlowCtr_threshold_max =          xFlowCtr_threshold_max;             // Excessive flow threshold in units of flow
  MySettings.FlowCtr_rate_factor =            xFlowCtr_rate_factor;               // Current Rate Factor
  MySettings.FlowCtr_k =                      xFlowCtr_k;                         // For K-Offset flow sensor (--> CST 1in ELF sensor)
  MySettings.FlowCtr_offset =                 xFlowCtr_offset;                    // Current Offset (--> CST 1in ELF sensor)
}


/* ******************************************************** */
void FlowSettingsDelta(void) 
{
  if (MySettings.version != MyFlow_Settings_VERSION)                              // Fix version dependent changes
  {     
    // Set current version and save settings file
    MySettings.version = MyFlow_Settings_VERSION;
    FlowSettingsSave();
  }
}


/* ******************************************************** */
void FlowSettingsLoad(void) 
{
  // Called from FUNC_PRE_INIT once at restart
  // Initial default values in case file is not found
  FlowSettingsDefault();

  // Try to load file --> /.snsset125
  char filename[20];
  // Use for sensors:
  snprintf_P(filename, sizeof(filename), PSTR(TASM_FILE_SENSOR), XSNS_125);
  AddLog(LOG_LEVEL_INFO, PSTR("X125-flow1: About to load settings from file: %s"), filename);

#ifdef USE_UFILESYS
  if (TfsLoadFile(filename, (uint8_t*)&MySettings, sizeof(MySettings))) 
  {
    AddLog(LOG_LEVEL_INFO, PSTR("X125-flow1: Settings loaded from file: %s"), filename);
    // Fix possible setting deltas
    FlowSettingsDelta();
  } else 
  {
    // File system not ready: No flash space reserved for file system or file not found
    AddLog(LOG_LEVEL_INFO, PSTR("X125-flow1: ERROR File system not ready or file not found!, using defaults"));
    // try saving defaults
    FlowSettingsSave();
    // will use default values in case file is not found error
  }
#else
  AddLog(LOG_LEVEL_INFO, PSTR("X125-flow1: ERROR File system not enabled!"));
#endif  // USE_UFILESYS

  MySettings.crc32 = FlowSettingsCrc32();
}


/* ******************************************************** */
void FlowSettingsSave(void) 
{
  // Called from FUNC_SAVE_SETTINGS every SaveData second and at restart
  if (FlowSettingsCrc32() != MySettings.crc32) 
  {
    // Try to save file
    MySettings.crc32 = FlowSettingsCrc32();

    char filename[20];
    // Use for sensors:
    snprintf_P(filename, sizeof(filename), PSTR(TASM_FILE_SENSOR), XSNS_125);
    AddLog(LOG_LEVEL_INFO, PSTR("X125-Flow2: About to save settings to file: %s"), filename);

#ifdef USE_UFILESYS
    if (!TfsSaveFile(filename, (const uint8_t*)&MySettings, sizeof(MySettings))) 
    {
      // File system not ready: No flash space reserved for file system
      AddLog(LOG_LEVEL_INFO, PSTR("X125-Flow2: ERROR File system not ready or unable to save file!"));
    }
#else
    AddLog(LOG_LEVEL_INFO, PSTR("X125-Flow2: ERROR File system not enabled!"));
#endif  // USE_UFILESYS
  }
}


/* ******************************************************** */
bool FlowCtrPinState(void)
{
  if ((XdrvMailbox.index >= AGPIO(GPIO_FLOW_NP)) && (XdrvMailbox.index < (AGPIO(GPIO_FLOW_NP))))
  {
    bitSet(FlowCtr.no_pullup, XdrvMailbox.index - AGPIO(GPIO_FLOW_NP));
    XdrvMailbox.index -= (AGPIO(GPIO_FLOW_NP) - AGPIO(GPIO_FLOW));
    return true;
  }
  return false;
}


/* ******************************************************** */
void FlowCtrInit(void)
{
  if (PinUsed(GPIO_FLOW))                                                             // do we need GPIO_FLOW_N ?? GPIO_FLOW_NP
  {   
    pinMode(Pin(GPIO_FLOW), bitRead(FlowCtr.no_pullup, 0) ? INPUT : INPUT_PULLUP);
    if ((MySettings.FlowCtr_debounce_low == 0) && (MySettings.FlowCtr_debounce_high == 0))
    {
      FlowCtr.pin_state = 0;
      attachInterrupt(Pin(GPIO_FLOW), FlowCtrIsr, FALLING);
    }
    else
    {
      FlowCtr.pin_state = 0x8f;
      attachInterrupt(Pin(GPIO_FLOW), FlowCtrIsr, CHANGE);
    }

  // Lets setup display units ....
   switch (MySettings.FlowCtr_units)  
    {
      default:
      case 0:
        strcpy(FlowCtr.Current_Units, D_GPM);
        strcpy(FlowCtr.Current_Volume_Units, D_GAL);
        break;

      case 1:
        strcpy(FlowCtr.Current_Units, D_CFT);
        strcpy(FlowCtr.Current_Volume_Units, D_CF);
        break;

      case 2:
        strcpy(FlowCtr.Current_Units, D_M3);
        strcpy(FlowCtr.Current_Volume_Units, D_CM);
        break;

      case 3:
        strcpy(FlowCtr.Current_Units, D_LM);
        strcpy(FlowCtr.Current_Volume_Units, D_L);
        break;
     }

    // pre set local working variables
    flow_current_pulse_count =                 0;    // set counts to zero on reboot...
    flow_pulse_period  =                       0;    // set period to zero on reboot...
    FlowCtr.ExcessiveFlowStartTime =    micros();    // get current time...
    FlowCtr.WeHaveFlow =                   false;    //
    FlowCtr.WeHaveFlowOverThreshold =      false;    //
    FlowCtr.WeHaveExcessFlow =             false;    //
    FlowCtr.LastPulseTime =                    0;    // Last pulse time
    FlowCtr.SendingRate =                      0;    // Current sample rate count 
    FlowCtr.Current1hrFlowVolume =          0.0f;    // current run-rate volume for this 1hr period
    FlowCtr.Current24hrFlowVolume =         0.0f;    // current run-rate volume for this 24hr period
    FlowCtr.VolumePerFlow  =                0.0f;    // Volume for current flow
    FlowCtr.OneHour =                          0;    // event timers....
    FlowCtr.OneDay =                           0;    //
    FlowCtr.OneMinute =                        0;    //
    FlowCtr.Freq =                          0.0f;    // Current frequency of pulses

  }   // end of: if (PinUsed(GPIO_FLOW))

  // This is an optional LED indicator of flow pulse's from the sensor
  if (PinUsed(GPIO_FLOW_LED)) 
  {
       flow_led_state  =  false;
       pinMode(Pin(GPIO_FLOW_LED), OUTPUT);           // set pin to output
       digitalWrite(Pin(GPIO_FLOW_LED), 0);           // turn off led for now
  }

}   // end of: void FlowInit(void)


/* ******************************************************** */
/* ****************** Main Loop *************************** */
/* ******************************************************** */
void FlowCtrFlowEverySecond(void)
{

#ifdef DEBUG_MyFlow
  TasmotaGlobal.seriallog_timer = 600;               // DEBUG Tool   <---------- TRL
#endif

  FlowCtr.CurrentTime = millis();                    // get current time from system
  FlowCtr.OneMinute++;                               // advance our event timers...
  FlowCtr.OneHour++;
  FlowCtr.OneDay++;
  FlowCtr.SendingRate++;


  if (PinUsed(GPIO_FLOW))                             // check to make sure we have a pin assigned
  {
    // check if it's time to do 1 minute task updates of volumes
    if (FlowCtr.OneMinute >= 60)                                         // 60 sec
    {
      FlowCtr.OneMinute = 0;
      FlowCtr.OldFlowRate = FlowCtr.CurrentFlow;
      if (flow_current_pulse_count > FlowCtr.LastPulseCount)             // check to see if we have a new flow pulse
      {                                                                  // yes, we have a flow...
        FlowCtr.Freq = (1.0 / ((float) flow_pulse_period / 1000000.0));  // flow_pulse_period is in microseconds
        if (FlowCtr.Freq < 0.0f) FlowCtr.Freq = 0.0f;

        // Calculate current flow rate based on sensor type
        switch (MySettings.FlowCtr_type)
        {
          default:
           case 0:   // we have Type 0 flow sensor, units per minute (ie: GPM..)
              FlowCtr.CurrentFlow = ((flow_current_pulse_count - FlowCtr.LastPulseCount) * MySettings.FlowCtr_rate_factor); 
              AddLog( LOG_LEVEL_DEBUG, PSTR("%s:, %9.2f,  %u"), " flow rate, period !",  FlowCtr.CurrentFlow, flow_pulse_period);
           break;

          case 1:   // we have Type 1 flow sensor, K-Offset *  flowrate = (freq + offset) * K  --> freq = (PPM / K) - offset   // MySettings.FlowCtr_rate_factor
              FlowCtr.CurrentFlow = ((( FlowCtr.Freq + MySettings.FlowCtr_offset ) *  MySettings.FlowCtr_k) * MySettings.FlowCtr_rate_factor);
           break;

          case 2:   // we have Type 2 flow sensor, K-Offset /  flowrate = (freq + offset) / K  --> freq = (PPM * K) - offset
              FlowCtr.CurrentFlow = ((( FlowCtr.Freq + MySettings.FlowCtr_offset ) /  MySettings.FlowCtr_k) * MySettings.FlowCtr_rate_factor);
          break;
        }   // end of: switch (MySettings.FlowCtr_type) 

        AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %8.4f,  %u,  %8.4fHz"), "Flow rate", FlowCtr.CurrentFlow, flow_pulse_period, FlowCtr.Freq);
        FlowCtrBoundsCheck();                                            // let make sure flow is reasonable...

        FlowCtr.Current1hrFlowVolume  += FlowCtr.CurrentFlow ;         // update flow volumes, this give us flow per minute
        FlowCtr.Current24hrFlowVolume += FlowCtr.CurrentFlow ;
        FlowCtr.VolumePerFlow         += FlowCtr.CurrentFlow ;

        FlowCtr.WeHaveFlow = true;
        FlowCtr.LastPulseCount = flow_current_pulse_count;                // save current pulse count

      }   // end of: if (flow_current_pulse_count > FlowCtr.LastPulseCount)
      else                                                                // no change in pulse count
      {
        FlowCtr.WeHaveFlow =   false;
        FlowCtr.CurrentFlow =    0.0f;                                    // so lets reset counters 
        FlowCtr.VolumePerFlow =  0.0f;
        FlowCtr.Freq =           0.0f;                  
        flow_pulse_period =        0;
      }
    }   // end of: if (FlowCtr.OneMinute >= 60)

    // check if it's time to update 1hr flow rate
    if (FlowCtr.OneHour >= 3600)                                 // 1hr = 3600 sec
    {
      FlowCtr.OneHour = 0;
      FlowCtr.Saved1hrFlowVolume = FlowCtr.Current1hrFlowVolume; 
      FlowCtr.Current1hrFlowVolume = 0.0f;                       // reset current 1hr flow volume
    }

    // check if it's time to update 24hr flow rate
    if (FlowCtr.OneDay >= 86400)                                 // 24hr = 86400 sec
    {
      FlowCtr.OneDay = 0;
      FlowCtr.Saved24hrFlowVolume = FlowCtr.Current24hrFlowVolume;
      FlowCtr.Current24hrFlowVolume = 0.0f;                      // reset current 24hr flow volume
    }

    FlowCtrCheckExcessiveFlow();                                 // check for excessive flow over our threshold
  
    // We are sending MQTT data only if we had a sufficient flow change in this period, with a minimum send rate...
    // As flow sensor have lots of jitter, we will round the flow values to one decimal point for our test of "sufficient flow change".
    float OFR = round(FlowCtr.OldFlowRate * 10) / 10;
    float CF  = round(FlowCtr.CurrentFlow * 10) / 10;

    AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %8.4f, %8.4f,  %u"), "Flow rate MQTT Check", FlowCtr.CurrentFlow, FlowCtr.OldFlowRate, FlowCtr.SendingRate);
    if  ((FlowCtr.CurrentFlow != 0) && ((OFR != CF) && (FlowCtr.SendingRate >= MySettings.FlowCtr_current_send_interval)))
    {
    AddLog( LOG_LEVEL_DEBUG, PSTR("%s: %8.4f, %8.4f,  %u"), "Flow rate in MQTT Send", FlowCtr.CurrentFlow, FlowCtr.OldFlowRate, FlowCtr.SendingRate);
      FlowCtr.SendingRate = 0;                                   // reset sending rate...
      ResponseClear();
      Response_P(PSTR("{\"FLOW\":{"));
      if (MySettings.FlowCtr_type == 0)
      {
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 0)  ResponseAppend_P(PSTR("\"FlowPulseCount\":%lu,"), flow_current_pulse_count );
      }
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 1)  ResponseAppend_P(PSTR("\"Rate\":%9.2f,"), (float)      FlowCtr.CurrentFlow);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 10) ResponseAppend_P(PSTR("\"Current1HrVolume\":%9.2f,"),  FlowCtr.Current1hrFlowVolume);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 11) ResponseAppend_P(PSTR("\"Current24HrVolume\":%9.2f,"), FlowCtr.Current24hrFlowVolume);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 14) ResponseAppend_P(PSTR("\"VolumeThisFlow\":%9.2f,"),    FlowCtr.VolumePerFlow);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 12) ResponseAppend_P(PSTR("\"ExcessFlow\":%s"), (FlowCtr.WeHaveExcessFlow) ? "true" : "false");

      ResponseJsonEndEnd();
      MqttPublishPayloadPrefixTopicRulesProcess_P(STAT, D_RSLT_SENSOR, (char*) TasmotaGlobal.mqtt_data.c_str());
      ResponseClear();

    }   // end of:  if  ((OFR != CF) && (SendingRate >= MySettings.FlowCtr_current_send_interval))
  }   // end of: if (PinUsed(GPIO_FLOW))               
}   // end of: FlowCtrFlowEverySecond(void)
 

/* ******************************************************** */
void FlowCtrBoundsCheck(void)                 // Lets do a bounds check
{
      if (FlowCtr.CurrentFlow < 0.0f)                 
      {
        FlowCtr.WeHaveFlow = false;
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " Neg flow rate, So we set it to zero!",  FlowCtr.CurrentFlow);
        FlowCtr.CurrentFlow = 0.0f;
      }
    
      // Lets Check that we don't get unreasonable large flow value.
      // could happen when long's wraps or false interrupt triggered
      if ( FlowCtr.CurrentFlow >= MySettings.FlowCtr_max_flow_rate)
      {  
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " Flow rate over max!",  FlowCtr.CurrentFlow);
        FlowCtr.CurrentFlow = MySettings.FlowCtr_max_flow_rate;                                               // set to max
      }

      // check for possible flow over threshold 
      if ( FlowCtr.CurrentFlow >= MySettings.FlowCtr_threshold_max) 
      {
        if (FlowCtr.WeHaveFlowOverThreshold == false)  FlowCtr.ExcessiveFlowStartTime = millis();           // set timer
        FlowCtr.WeHaveFlowOverThreshold = true;
        AddLog( LOG_LEVEL_INFO, PSTR("%s: %9.2f"), " We have a flow over threshold!", FlowCtr.CurrentFlow);
      }
       else FlowCtr.WeHaveFlowOverThreshold = false;
}   // end of: void FlowCtrBoundsCheck(void)


/* ******************************************************** */
// This will check to see if we have a flow over our threshold rate for a preset amount of time..
// We may have a stuck valve or ??. If MySettings.flow_threshold_reset_time = 0, we will skip this 
void FlowCtrCheckExcessiveFlow(void)
{
  if ( MySettings.Flow_threshold_reset_time > 0)                   // skip Excessive Flow test if zero
  {
    if ( (FlowCtr.CurrentTime >= (FlowCtr.ExcessiveFlowStartTime + MySettings.Flow_threshold_reset_time) ) && (FlowCtr.WeHaveFlowOverThreshold == true) )
    {
      FlowCtr.WeHaveExcessFlow = true;
      AddLog( LOG_LEVEL_INFO, PSTR("%s: %7.2f, %u"), " We have Excess Flow over time!",  FlowCtr.CurrentFlow, MySettings.Flow_threshold_reset_time);
    } 
    else
    {
      FlowCtr.WeHaveExcessFlow = false;                            // reset excessive flow flag
      FlowCtr.ExcessiveFlowStartTime = FlowCtr.CurrentTime;        // reset start time
    }
  } 
  else
  {                                                                                             
    FlowCtr.WeHaveExcessFlow = false;                              // reset excessive flow flag
    FlowCtr.ExcessiveFlowStartTime = FlowCtr.CurrentTime;          // reset start time
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

  //AddLog(LOG_LEVEL_INFO, PSTR("In FlowCtrShow"));
    if (PinUsed(GPIO_FLOW))
    {
      if (json)
      {
        if (!header) ResponseAppend_P(PSTR(",\"FLOW\":{"));

        if (MySettings.FlowCtr_type == 0)                  // if we have Type 0 flow sensor.
        {
          if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 0) ResponseAppend_P(PSTR("%s\"FlowPulseCount\":%lu,"), (header) ? "," : "", flow_current_pulse_count);
        }
        else                                             // if we have Type 1 or 2 flow sensor.
        {
          if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 6) ResponseAppend_P(PSTR("%s\"K\":%9.4f,"),            (header) ? "," : "", MySettings.FlowCtr_k);
          if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 7) ResponseAppend_P(PSTR("%s\"Offset\":%9.4f,"),       (header) ? "," : "", MySettings.FlowCtr_offset);
          if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 2) ResponseAppend_P(PSTR("%s\"FlowPeriod\":%9.4f,"),   (header) ? "," : "", (float) flow_pulse_period / 1000000.0);
        }
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 1)  ResponseAppend_P(PSTR("%s\"Rate\":%9.4f,"),          (header) ? "," : "", FlowCtr.CurrentFlow);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 5)  ResponseAppend_P(PSTR("%s\"RateFactor\":%9.4f,"),    (header) ? "," : "", MySettings.FlowCtr_rate_factor); 
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 14) ResponseAppend_P(PSTR("%s\"VolumeThisFlow\":%9.2f,"),(header) ? "," : "", FlowCtr.VolumePerFlow);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 3)  ResponseAppend_P(PSTR("%s\"1HrVolume\":%9.2f,"),     (header) ? "," : "", FlowCtr.Saved1hrFlowVolume);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 4)  ResponseAppend_P(PSTR("%s\"24HrVolume\":%9.2f,"),    (header) ? "," : "", FlowCtr.Saved24hrFlowVolume);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 10) ResponseAppend_P(PSTR("%s\"Current1HrVolume\":%9.2f,"),   (header) ? "," : "", FlowCtr.Current1hrFlowVolume);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 11) ResponseAppend_P(PSTR("%s\"Current24HrVolume\":%9.2f,"),  (header) ? "," : "", FlowCtr.Current24hrFlowVolume);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 8)  ResponseAppend_P(PSTR("%s\"FlowUnits\":\"%s\","),    (header) ? "," : "", FlowCtr.Current_Units);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 9)  ResponseAppend_P(PSTR("%s\"VolumeUnits\":\"%s\","),  (header) ? "," : "", FlowCtr.Current_Volume_Units);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 15) ResponseAppend_P(PSTR("%s\"SW-Version\":\"%s\","),  (header) ? "," : "", Flow_SW_Version);
        if bitRead (MySettings.FlowCtr_MQTT_bit_mask, 12) ResponseAppend_P(PSTR("\"ExcessFlow\":%s"), (FlowCtr.WeHaveExcessFlow) ? "true" : "false");    
        header = true;

// no flow Domoticz option done here as we are not sure what one may needed for Domoticz....
#ifdef USE_DOMOTICZ    
#endif // USE_DOMOTICZ

        {
          AddLog( LOG_LEVEL_DEBUG, PSTR("%s: "), " Domoticz not implemented!");
        }

#ifdef USE_WEBSERVER
      }   // end of:  if (json)
      else
      { 
        WSContentSend_PD(PSTR("{s}" D_FLOW_RATE "{m}%9.2f %s{e}"), (FlowCtr.CurrentFlow), FlowCtr.Current_Units);
        if (MySettings.FlowCtr_type == 0)      // Check for type 0 sensor
        {
          WSContentSend_PD(PSTR("{s}" D_FLOW_COUNT "{m}%lu{e}"), flow_current_pulse_count);
        }
        else                                    // If we have Type 1 or 2 flow sensor, then we will send K & Offset                
        {
          WSContentSend_PD(PSTR("{s}" D_FLOW_PERIOD "{m}%9.4f %s{e}"), (float) flow_pulse_period / 1000000.0, D_UNIT_SECOND);
          WSContentSend_PD(PSTR("{s}" D_Flow_Frequency "{m}%9.2f %s{e}"), FlowCtr.Freq, D_UNIT_HERTZ);
          WSContentSend_PD(PSTR("{s}" D_FlowCtr_k "{m}%9.3f{e}"),       MySettings.FlowCtr_k);
          WSContentSend_PD(PSTR("{s}" D_FlowCtr_offset "{m}%9.3f{e}"),  MySettings.FlowCtr_offset);
        }

        WSContentSend_PD(PSTR("{s}" D_Flow_Factor "{m}%9.2f{e}"),       MySettings.FlowCtr_rate_factor); 
        WSContentSend_PD(PSTR("{s}" "This Flow{m}%9.0f %s{e}"),         FlowCtr.VolumePerFlow,   FlowCtr.Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "Current 1Hr Flow{m}%9.0f %s{e}"),  FlowCtr.Current1hrFlowVolume,   FlowCtr.Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "Current 24Hr Flow{m}%9.0f %s{e}"), FlowCtr.Current24hrFlowVolume, FlowCtr.Current_Volume_Units);  
        WSContentSend_PD(PSTR("{s}" "Last 1Hr Flow{m}%9.0f %s{e}"),     FlowCtr.Saved1hrFlowVolume,   FlowCtr.Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "Last 24Hr Flow{m}%9.0f %s{e}"),    FlowCtr.Saved24hrFlowVolume, FlowCtr.Current_Volume_Units);
        WSContentSend_PD(PSTR("{s}" "We Have Flow{m}%d{e}"), (int8_t)   FlowCtr.WeHaveFlow);
        WSContentSend_PD(PSTR("{s}" "Flow Over Threshold{m}%d{e}"), (int8_t) FlowCtr.WeHaveFlowOverThreshold);
        WSContentSend_PD(PSTR("{s}" "Excess Flow{m}%d{e}"), (int8_t)    FlowCtr.WeHaveExcessFlow );
        WSContentSend_PD(PSTR("{s}" "Sensor Type{m}%d{e}"), (int8_t)    MySettings.FlowCtr_type ); 

#endif      // end of: USE_WEBSERVER
      }   // end of: if (json) else
    }   // end of: if (PinUsed(GPIO_FLOW))
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
    case 0: // Save defaults to filesystem
      FlowSettingsDefault();
      AddLog(LOG_LEVEL_INFO, PSTR("%s: "), "X125-Flow: Case 0, Save defaults to filesystem");
      break;
      
    case 1: // Flow Type
       // check range of type, 0-->2 only
      if (((uint8_t)strtol(ArgV(argument, 2), nullptr, 10) >= 0) && ((uint8_t)strtol(ArgV(argument, 2), nullptr, 10) <= 2)) // uint8_t
      {
        MySettings.FlowCtr_type = (uint8_t) strtol(ArgV(argument, 2), nullptr, 10);  // uint8_t
        AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 1, Flow Type", MySettings.FlowCtr_type);
      }
      break;

    case 2: // Flow Factor
      MySettings.FlowCtr_rate_factor = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "X125-Flow: Case 2, Flow Factor", MySettings.FlowCtr_rate_factor);
      break;

    case 3: // Flow K
      MySettings.FlowCtr_k = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "X125-Flow: Case 3, Flow K", MySettings.FlowCtr_k);
      break;

    case 4: // Flow Offset
      MySettings.FlowCtr_offset = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "X125-Flow: Case 4, Flow Offset", MySettings.FlowCtr_offset);
      break;

    case 5: // Flow Units
      MySettings.FlowCtr_units = (uint8_t) strtol(ArgV(argument, 2), nullptr, 10); // uint8_t
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 5, Flow Units", MySettings.FlowCtr_units);
      break;

    case 6: // Excess flow threshold max count
      MySettings.FlowCtr_threshold_max = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %7.3f"), "X125-Flow: Case 6, Max Flow", MySettings.FlowCtr_threshold_max);
      break;

    case 7:   // Flow Threshold Time in second's (convert to milliseconds)
      MySettings.Flow_threshold_reset_time = (1000) * (uint32_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 7, Max Flow Time", MySettings.Flow_threshold_reset_time);
      break;

    case 8:  // Current Sample Interval in second's, lower limit to 10 seconds
      if ((uint16_t) strtol(ArgV(argument, 2), nullptr, 10) == 0)   MySettings.FlowCtr_current_send_interval = 1;
      else MySettings.FlowCtr_current_send_interval = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 8, Sample Interval", MySettings.FlowCtr_current_send_interval);
      break; 

    case 9:   // MQTT Bit Mask, 16 bits
      MySettings.FlowCtr_MQTT_bit_mask = (uint16_t) strtol(ArgV(argument, 2), nullptr, 16);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: 0x%04x"), "X125-Flow: Case 9, MQTT Bit Mask", MySettings.FlowCtr_MQTT_bit_mask);
      break;
      
    case 10: // Max Flow Rate
      MySettings.FlowCtr_max_flow_rate = CharToFloat(ArgV(argument, 2));
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 10, Max Flow Rate", MySettings.FlowCtr_max_flow_rate);
      break;

    case 11: // Flow Debounce time in MS
      MySettings.FlowCtr_debounce = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 11, Flow Debounce", MySettings.FlowCtr_debounce);
      break;

    case 12: // Flow Debounce Low time in MS
      MySettings.FlowCtr_debounce_low = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 12, Flow Debounce Low", MySettings.FlowCtr_debounce_low);
      break;

    case 13: // Flow Debounce High time in MS
      MySettings.FlowCtr_debounce_high = (uint16_t) strtol(ArgV(argument, 2), nullptr, 10);
      AddLog(LOG_LEVEL_INFO, PSTR("%s: %u"), "X125-Flow: Case 13, Flow Debounce High", MySettings.FlowCtr_debounce_high);
      break;

      // we need to save MySettings to file system, then re-init driver...
      FlowSettingsSave();
      FlowCtrInit();
    }
  }


// build a JSON response
  Response_P(PSTR("{\"" D_FLOWMETER_NAME "\":{\"Flow Type\":%d"),  MySettings.FlowCtr_type);
  ResponseAppend_P(PSTR(",\"Flow Rate Factor\":%7.2f"),            MySettings.FlowCtr_rate_factor);
  ResponseAppend_P(PSTR(",\"Flow K\":%7.2f"),                      MySettings.FlowCtr_k);
  ResponseAppend_P(PSTR(",\"Flow Offset\":%7.2f"),                 MySettings.FlowCtr_offset);
  ResponseAppend_P(PSTR(",\"Flow Units\":%d"),                     MySettings.FlowCtr_units);
  ResponseAppend_P(PSTR(",\"Flow Threshold_Max\":%7.2f"),          MySettings.FlowCtr_threshold_max);
  ResponseAppend_P(PSTR(",\"Flow Threshold Time\":%u"),            MySettings.Flow_threshold_reset_time);
  ResponseAppend_P(PSTR(",\"Flow Sample Interval\":%u"),           MySettings.FlowCtr_current_send_interval);
  char hex_data[8];
  sprintf(hex_data, "%04x",                                        MySettings.FlowCtr_MQTT_bit_mask);
  ResponseAppend_P(PSTR(",\"MQTT Bit Mask\":\"0x%s\""), hex_data );
  ResponseAppend_P(PSTR(",\"Max Flow Rate\":%7.2f"),               MySettings.FlowCtr_max_flow_rate);
  ResponseAppend_P(PSTR(",\"Flow Debounce \":%u"),                 MySettings.FlowCtr_debounce);
  ResponseAppend_P(PSTR(",\"Flow Debounce Low \":%u"),             MySettings.FlowCtr_debounce_low);
  ResponseAppend_P(PSTR(",\"Flow Debounce High \":%u"),            MySettings.FlowCtr_debounce_high);
  ResponseJsonEndEnd();

  return true;
}   // end of bool Xsns125Cmnd(void)


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns125(uint32_t function)
{
  bool result = false;

  switch (function)                   // do functions as needed...
  {   
    case FUNC_SAVE_SETTINGS:
      AddLog(LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Save Settings"));
      FlowSettingsSave();
      break;

    case FUNC_PRE_INIT:
      AddLog(LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Pre-Init")); 
      //FlowSettingsLoad();           // moved to Init below
      break;
    
    case FUNC_INIT:                   // check if we need to do FlowInit
      AddLog(LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Init")); 
      //FlowSettingsSave();
      FlowSettingsLoad();
      FlowCtrInit();
      break;
  
    case FUNC_EVERY_SECOND:           // do FlowCtrFlowEverySecond()
      // AddLog( LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Every Second"));
      FlowCtrFlowEverySecond();
      break;

    case FUNC_EVERY_50_MSECOND:
      break;

    case FUNC_EVERY_100_MSECOND:
      break;

    case FUNC_JSON_APPEND:            // do FlowCtrShow(1)
      AddLog( LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Append"));
      FlowCtrShow(1);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:             // do FlowCtrShow(0)
      AddLog( LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Show"));
      FlowCtrShow(0);
      break;
#endif    // end of USE_WEBSERVER

    case FUNC_SAVE_BEFORE_RESTART:    // do FlowCtrSaveState()
    case FUNC_SAVE_AT_MIDNIGHT:
      AddLog( LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Save State"));
      FlowCtrSaveState();
      break;

    case FUNC_COMMAND:                // do Command()
    case FUNC_COMMAND_SENSOR:
    AddLog( LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Command"));
      if (XSNS_125 == XdrvMailbox.index)
      {
        result = Xsns125Cmnd();
      }
      break;

    case FUNC_PIN_STATE:
      result = FlowCtrPinState();     // do FlowCtrPinState()
      AddLog( LOG_LEVEL_DEBUG, PSTR("Debug: In Command Function Pin State"));
      break;

  }   // end of: switch (function)
  return result;
}   // end of: bool Xsns125(uint32_t function)

#endif    // end of USE_FLOW  #ifdef USE_FLOW

#endif    // end of ESP32
/* ************************* The Very End ************************ */
