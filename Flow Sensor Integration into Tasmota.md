

Flow Sensor Integration into Tasmota


To minimize changes to Tasmota for development, only a few additions were made to TASMOTA-32  Ver: 12.3.1.1
and they are documented below.
~~~
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
 *   3-Apr-2022  1.1   TRL - refactoring of the code base
 *   5-Apr-2022  1.1a  TRL - added check for sufficient flow change
 *   7-Apr-2022  1.2   TRL - Local Data-struct was changed to dynamic
 *   7-Apr-2022  1.2a  TRL - Moved MySettings to settings.h in base code (removed, See 1.4)
 *   1-Aug-2022  1.3   TRL - Moved to 12.1.0.2, Moved MySettings back to local space, not in settings.h for now (removed, See 1.4)
 *  19-Dec-2022  1.3a  TRL - Moved to 12.3.1.1
 *  16-Jan-2023  1.4   TRL - added save settings to flash file system
 *  17-Jan-2023  1.4a  TRL - change some variables names, option for defaults settings to be in --> user_config_override.h
 *  13-Dec-2023  1.4b  TRL - Added 4 decimal digits to flow rate factor 0.0000, removed some dead code
 *  20-Dec-2023  1.4c  TRL - refactoring of code base, code cleanup
 *  20-DEC-2023  1.5   TRL - MOVE TO 13.3.0.1
 * 
 * 
 *  Notes:  1)  Tested with TASMOTA  13.3.0.1
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

    This program is for residential-type water and flow meters, 
    may or may not be suitable for large-volume industrial meters.

    There are two basic types of water flow meters, some produce a pulse per unit of flow,
    and K-Offset flow meters.

    Unit per flow meter gives a pulse per unit of flow, typical devices give 1 to 100 gal/pulse or 
    a pulse per cubic feet of water. One "unit" of water is 100 ccf or 748.052 gals.

    Almost all of the turbine-type flow sensors used in irrigation, use two calibration factors
    specified: a K factor and an Offset.

    During calibration, the manufacturer measures the pulse rate outputs for several precise flow rates.
    These are plotted, but since the turbine has some friction, the graph will not be
    linear, especially at the low end and a linear regression is done to get a best-fit straight line.
    The K factor represents the slope of the fitted line and has a dimension of pulses per unit volume moved.
    The offset represents the small amount of liquid flow required to start the turbine moving.
    You can assume that if any pulses are arriving at all, at least the offset volume of liquid is moving.

    There does not seem to be a standard for how K-factor flow meters are presented.
    Flow sensors output a pulse stream at a frequency proportional to the flow volume as calibrated,
    With some sensors like CST, RainBird, you multiply the pulse frequency by the K factor to obtain a volume rate.
    Others however like Badger, require you to divide the pulse frequency by K.

    So there are two basic types of K-Offset flow sensors, CST and many others are of type = 1,
    Some like Badger are of type = 2. So read the vendor's data sheet!

    Frequency = (Gallons per Minute / K )  Offset  or  = (Gallons per Minute * K )  Offset
    We are measuring pulse frequency so turning the equation around:
    Gallons per minute = (Frequency + Offset) * K  or  = (Frequency + offset) / K

    // flow meter type
    FlowCtr_type    0   pulses per unit (GPM....)
                    1   K-Offset    flowrate = (freq + offset) * K  --> freq = (PPM / K) - offset
                    2   K-Offset    flowrate = (freq + offset) / K  --> freq = (PPM * K) - offset

    // unit per pulse from the flow meter
    FlowCtr_rate_factor                             flow_units
                0.1   0.1 gal per minute            GPM
                1       1 gal per minute            GPM
                10      10 gal per minute           GPM
                100     100 gal gal per minute      GPM
                7.48052 1 cubic feet                Cft
                74.8052 10 cubic feet               Cft
                748.052 100 cubic feet  (unit)      Cft
                1       1 cubic meter               M3
                1       1 litre                     LM

    FlowCtr_units
                0 = GPM     Gallons per minute   <--- defaults
                1 = CFT     Cubic Feet per minute
                2 = M3      Cubic Metres per minute
                3 = LM      Litres per minutes

 *  Some information on the 1 GPM water meter and flow rates.
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
 * At a 1-GPM flow rate, it takes 1 minute for one pulse from the sensor,
 *  so for 0.25-GPM rate it takes 4 minutes between pulses...
 *
 * We will typically limit the flow range to about .25gpm to 60gpm for a 1 GPM sensor
 *  We will reset the flow period timer at 4 minutes or so if no pulses...
 * 
 * We also sense excessive flow, we do this by setting an excessive flow limit
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
                        1       1 litre                     LM
      3     K              // K value from the device, float
      4     Offset         // Offset value from device, float
      5     Flow_units
                        0 = GPM     GAL Gallons per minute
                        1 = Cft     CF  Cubic Feet per minute
                        2 = M3      CM  Cubic Metre per minute
                        3 = LM      L   Litres per minutes
      6     Excess Flow Threshold                         // flow rate at which to trigger an excess flow
      7     Excess Flow Threshold Time                    // time in seconds to report excessive flow
      8     Current Send Interval in seconds              // How often we send MQTT information when we have a flow 
      9     MQTT BitMask, 16 bits                         // used to enable/disable MQTT messages
      10    Max Flow Rate                                 // Max flow rate for this sensor
      11    Debounce 0 = off, 1 = on
      12    Debounce Low Time in MS
      13    Debounce High Time in MS
      14    Set Flow Units to any text 1-7 char 
      15    Set Flow Volume Units to any text 1-7 char
      

  **************************************************************************************
  MQTT_send_bit_mask:                    //We use this mask to enable/disable MQTT messages
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
 #define xFlowCtr_MQTT_bit_mask            0xffff         // MQTT BitMask, Controls what we send via MQTT
 #define xFlowCtr_current_send_interval        10         // In second's
 #define xFlow_threshold_reset_time     (20 * 60 * 1000)  // Excessive flow threshold timeout, in milliseconds (20 Min)  
 #define xFlowCtr_max_flow_rate              60.0f        // Sensor Max Flow rate in units of flow...
 #define xFlowCtr_threshold_max              20.0f        // Excessive flow threshold in units of flow
 #define xFlowCtr_rate_factor                 1.0f        // Current Rate Factor
 #define xFlowCtr_k                          .153f        // For K-Offset flow sensor (--> CST 1in ELF sensor)
 #define xFlowCtr_offset                    1.047f        // Current Offset
*/

/* ************************************************************************************** 
Changes made to Tasmota base code... 
See integration notes...
13.3.0.1

include/tasmota_template.h 
(must be at end of structure)
line 219 
GPIO_FLOW, GPIO_FLOW_LED,                    // Flow Sensor xsns_125       // <---------------  TRL

line 483
D_SENSOR_FLOW "|"  D_SENSOR_FLOW_LED "|"     // Flow xsns_125              // <---------------  TRL

line 570
#ifdef USE_FLOW
  AGPIO(GPIO_FLOW),                           // Flow xsns_125             // <---------------  TRL
  AGPIO(GPIO_FLOW_LED),
#endif

tasmota/language/en_GB.h
at line 961
#define D_SENSOR_FLOW          "H2O Flow"      // Flow xsns_125             // <---------------  TRL
#define D_SENSOR_FLOW_LED      "H2O Flow Led"

tasmota/user_config_override.h

#ifndef USE_FLOW       // My flow meter x125
// define here or in xsns_125_flow_counter.ino
  #define USE_FLOW
  #define xFlowCtr_type                          0         // Current type of flow sensor, 0 = flow per unit,  1, 2 = K-Offset
  #define xFlowCtr_units                         0         // Current flow units
  #define xFlowCtr_debounce_low                  0         // Current debounce values...
  #define xFlowCtr_debounce_high                 0 
  #define xFlowCtr_debounce                      0 
  #define xFlowCtr_MQTT_bit_mask            0xffff         // MQTT BitMask, Controls what we send via MQTT
  #define xFlowCtr_current_send_interval        10         // in seconds
  #define xFlow_threshold_reset_time     (20 * 60 *1000)   // Excessive flow threshold timeout, in milliseconds (20 Min)  
  #define xFlowCtr_max_flow_rate              60.0f        // Sensor Max Flow rate in units of flow...
  #define xFlowCtr_threshold_max              20.0f        // Excessive flow threshold in units of flow
  #define xFlowCtr_rate_factor                 1.0f        // Current Rate Factor
  #define xFlowCtr_k                          .153f        // For K-Offset flow sensor (--> CST 1in ELF sensor)
  #define xFlowCtr_offset                    1.047f        // Current Offset

#endif

*/
