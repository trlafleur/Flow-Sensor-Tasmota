Flow Sensor Integration into Tasmota

In order to minimize changes to Tasmota for development, only a few addition were made to TASMOTA-32  Ver: 11.0.0.3

~~~
tasmota/tasmota_template.h
At line 186
GPIO_FLOW, GPIO_FLOW_NP, GPIO_FLOW_LED, // Flow Sensor X125 // <--------------- TRL

At line 409
D_SENSOR_FLOW "|" D_SENSOR_FLOW "_n" D_SENSOR_FLOW_LED "|" // <--------------- TRL

At line 458
#ifdef USE_FLOW
AGPIO(GPIO_FLOW), // Flow xsns_125 // <--------------- TRL
AGPIO(GPIO_FLOW_NP),
AGPIO(GPIO_FLOW_LED),
#endif

tasmota/language/en_GB.h
At line 859
#define D_SENSOR_FLOW "H2O Flow" // <--------------- TRL
#define D_SENSOR_FLOW_N "H2O Flow N"
#define D_SENSOR_FLOW_LED "H2O Flow Led"
~~~

These item were added to settings.h. A global change in this file from "MySettings." To "Settings->" was made. ~40 bytes need to be allocated. Also one neeed to comment or remove structure MySettings.
This was moved in Tasomota 11.0.0.3 to line ~784 and changes were made to 'free space' to make room for this data
~~~
// xsns125 Flow Counter variables in settings.h

 // we need ~ 40 bytes see -->  free_ea6[24];                          <--------------------- TRL
  uint8_t  FlowCtr_type;                  // Current type of flow sensor, 0 = flow per unit,  1,2 = K-Offset
  uint8_t  FlowCtr_units;                 // Current flow units
  uint16_t FlowCtr_debounce_low;          // Current debounce values...
  uint16_t FlowCtr_debounce_high;
  uint16_t FlowCtr_debounce;
  uint16_t FlowCtr_MQTT_bit_mask;         // MQTT Bit Mask, Controls what we send
  uint16_t FlowCtr_current_send_interval; // in seconds
  uint32_t Flow_threshold_reset_time;     // Excessive flow threshold timeout, in miliseconds (20 Min)  
  float    FlowCtr_max_flow_rate;         // Sensor Max Flow rate in units of flow...
  float    FlowCtr_threshold_max;         // Excessive flow threshold in units of flow
  float    FlowCtr_rate_factor;           // Current Rate Factor
  float    FlowCtr_k;                     // For K-Offset flow sensor (--> CST 1in ELF sensor)
  float    FlowCtr_offset;                // Current Offset
  
~~~

// This was added to settings.ino at line ~1230

~~~
// lets set global varables to defaults for Flow Sensor125             // <------------------------- TRL
    Settings->FlowCtr_type =                    xFlowCtr_type;                 // Current type of flow sensor, 0 = flow per unit,  1,2 = K-Offset
    Settings->FlowCtr_units =                   xFlowCtr_units;                // Current flow units
    Settings->FlowCtr_debounce_low =            xFlowCtr_debounce_low;         // Current debounce values...
    Settings->FlowCtr_debounce_high =           xFlowCtr_debounce_high;
    Settings->FlowCtr_debounce =                xFlowCtr_debounce;
    Settings->FlowCtr_MQTT_bit_mask =           xFlowCtr_MQTT_bit_mask;        // MQTT Bit Mask, Controls what we send
    Settings->FlowCtr_current_send_interval =   xFlowCtr_current_send_interval;// in seconds
    Settings->Flow_threshold_reset_time =       xFlow_threshold_reset_time;    // Excessive flow threshold timeout, in miliseconds (20 Min)  
    Settings->FlowCtr_max_flow_rate  =          xFlowCtr_max_flow_rate;        // Sensor Max Flow rate in units of flow...
    Settings->FlowCtr_threshold_max =           xFlowCtr_threshold_max;        // Excessive flow threshold in units of flow
    Settings->FlowCtr_rate_factor =             xFlowCtr_rate_factor;          // Current Rate Factor
    Settings->FlowCtr_k =                       xFlowCtr_k;                    // For K-Offset flow sensor (--> CST 1in ELF sensor)
    Settings->FlowCtr_offset =                  xFlowCtr_offset;               // Current Offset

~~~
This was added to my user_config_override.h, it should be move to my_user_config.h
~~~
 #define xFlowCtr_type                          1         // Current type of flow sensor, 0   flow per unit,  1,2   K-Offset
 #define xFlowCtr_units                         0         // Current flow units
 #define xFlowCtr_debounce_low                  0         // Current debounce values...
 #define xFlowCtr_debounce_high                 0 
 #define xFlowCtr_debounce                      0 
 #define xFlowCtr_MQTT_bit_mask            0xffff         // MQTT Bit Mask, Controls what we send
 #define xFlowCtr_current_send_interval        30         // in seconds
 #define xFlow_threshold_reset_time     (20 * 60 *1000)     // Excessive flow threshold timeout, in miliseconds (20 Min)  
 #define xFlowCtr_max_flow_rate              60.0         // Sensor Max Flow rate in units of flow...
 #define xFlowCtr_threshold_max              20.0         // Excessive flow threshold in units of flow
 #define xFlowCtr_rate_factor                 1.0         // Current Rate Factor
 #define xFlowCtr_k                          .153         // For K-Offset flow sensor (--> CST 1in ELF sensor)
 #define xFlowCtr_offset                    1.047         // Current Offset
~~~




The flowing item may need to be integrated into the base code, but now reside in the sensor code.

~~~
// We made need to move these to Tasmota base code at a later time...
#define D_FLOW_RATE1 "Flow Rate"
#define D_FLOW_COUNT "Flow Pulse Count"
#define D_FLOW_PERIOD "Flow Period"
#define D_Flow_Factor "Flow Factor"
#define D_Flow_K "Flow K"
#define D_Flow_Offset "Flow Offset"
#define D_Flow_Frequency "Flow Frequency"

#define D_FLOWMETER_NAME "Flow_Meter"
#define D_PRFX_FLOW "Flow"
#define D_CMND_FLOW_TYPE "Type"
#define D_CMND_FLOW_RATE "Flow_Rate"
#define D_CMND_FLOW_DEBOUNCE "Debounce"
#define D_CMND_FLOW_DEBOUNCELOW "Debounce_Low"
#define D_CMND_FLOW_DEBOUNCEHIGH "Debounce_High"

#define D_GPM "GPM" // 0
#define D_CFT "Cft" // 1
#define D_M3 "M3" // 2
#define D_LM "lM" // 3

#define D_GAL "GAL" // 0
#define D_CF "CF" // 1
#define D_CM "CM" // 2
#define D_L "L" // 3

#define D_UNIT_HZ "Hz"
~~~
