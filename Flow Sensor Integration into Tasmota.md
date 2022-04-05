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

These item should be added to settings.h. A global change in this file from "MySettings." To "Settings->" need to be made. 40 bytes need to be allocated. Also one neeed to comment or remove structure MySettings.

~~~
// xsns125 Flow Counter variables in settings.h

struct MYSETTINGS
{
uint8_t flow_type = 0; // Current type of flow sensor, 0 = flow per unit, 1,2 = K-Offset
uint8_t flow_units = 0; // Current flow units
uint16_t flow_debounce_low = 0; // Current debounce values...
uint16_t flow_debounce_high = 0;
uint16_t flow_debounce = 0;
uint16_t MQTT_send_bit_mask = 0xffff; // MQTT Bit Mask, Controls what we send
uint16_t current_sending_interval = 10; // in seconds
uint32_t flow_threshold_time = 5 * 60 * 1000000; // Excessive flow threshold timeout, in microseconds (20 Min)
uint32_t max_flow_reset_time = 4 * 60 * 1000000; // Reset flow if no pulse within this window, in microseconds
float max_flow_rate = 60.0; // Sensor Max Flow rate in units of flow...
float flow_threshold_max = 20.0; // Excessive flow threshold in units of flow
float flow_rate_factor = 1.0; // Current Rate Factor
float flow_k = .153; // For K-Offset flow sensor (--> CST 1in ELF sensor)
float flow_offset = 1.047; // Current Offset
} MySettings;

static_assert(sizeof(MySettings) == 40, "MySettings Size is not 40 bytes");
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
