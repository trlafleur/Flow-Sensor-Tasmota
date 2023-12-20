Flow Sensor.


This Tasmota sensor driver is designed for residential-style water meters use in whole-house water meters and irrigation flow sensors.

There are basic two types of common flow sensors, units per minute (i.e.: GPM) and K-offset meters. GPM sensors are common in whole-house meters that are supplied by the local water company. K-offset sensors are common in irrigation systems.

Unit per minute meters, give a pulse per unit of flow, typical devices give 1 to 100 gal per pulse or 
a pulse per cubic feet of water. Some are in Cubic feet of water, some are in Units of water, or 1 unit = 100cf = 748.052 gals... Metric meters are very similar.

Almost all of the turbine-type flow sensors used in irrigation, use one or two calibration factors
specified: a K factor and Offset.

During calibration, the manufacturer measures the pulse rate outputs for several precise flow rates. These are plotted, but since the turbine has some friction, the graph will not be linear, especially at the low end and a linear regression is done to get a best-fit straight line.
The K factor represents the slope of the fitted line and has a dimension of pulses per unit volume moved. The offset represents the small amount of liquid flow required to start the turbine moving. You can assume that if any pulses are arriving at all, at least the offset volume of liquid is moving.

The sensor is essentially a 6 to 20-ohm switch with a less than 1ma leakage current. With no flow running (the impeller not turning), the sensor will appear to the controller input as a small current load. When the impeller is turning, it appears as a quick series of 5 ms short circuits. 

There does not seem to be a standard for how K-factor flow meters are presented.
Sensors output a pulse stream at a frequency proportional to the flow volume as calibrated.
With the more common sensors like CST and Rainbird, you multiply the pulse frequency by the K factor to obtain a volume rate. Others however like Badger, require you to divide the pulse frequency by K. So one must read the vendor's datasheet!

~~~
Frequency =  (Gallons per Minute / K ) - Offset 
Frequency  = (Gallons per Minute * K ) - Offset
~~~
We are measuring pulse frequency, we turn the equation around, and we get:
~~~
Gallons per minute = (Frequency + Offset) * K		CTS, Rainbird
Gallons per minute = (Frequency + offset) / K		Badger
~~~
In the schematic is the interface circuit that I use to interface the sensor to the ESP32.
The comparator with some hysteresis is used to sense the pulse change and is cleaned up by the two TTL inverters. R4 and C3 provide the 1st level of debouncing of the pulse from the sensor.  Another option is to use an optocoupler. In the photo, is the ESP32 mounted in a waterproof enclosure on a motherboard that provided power to the processor.

The k-Offset sensor requires power to operate, this is done via a bias resistor giving proper operating current to the device. Such devices require about 5 to 100 to operate at some minimal voltage. They produce a 5ms pulse at a rate from 1 to 200Hz.

Pulse per unit of flow sensors generally uses a small reed switch to generate a pulse, some use a hall-affect sensor that requires power.  

[FlowSensorSchematic.pdf](https://github.com/trlafleur/Flow-Sensor-Tasmota/files/8422354/FlowSensorSchematic.pdf)


Some general information:

~~~
Flow meter type:
       0	Pulse per unit (GPM.)
       1 	K-Offset flowrate = (freq + offset) * K --> freq = (PPM / K) - offset
       2 	K-Offset flowrate = (freq + offset) / K --> freq = (PPM * K) - offset
            
Unit per pulse from flow meter:

Rate_factor flow units:
1         1 gal per minute GPM
10        10 gal per minute GPM
100       100 gal gal per minute GPM
7.48052   1 cubic feet Cft (CF)
74.8052   10 cubic feet Cft
748.052   100 cubic feet Cft (Unit or CCF)
1         1 cubic meter M3
1         1 litters LM


Some information on the 1 GPM water meter and flow rates.
                       Period   Freq in Hz
 -------------------------------------------------------------
 1 Pulse    1 gal   60 sec    .01667 Hz
 60 Pulse   60 gal  1 sec          1 Hz
 30 Pulse   30 gal  2 sec         .5 Hz
 10 Pulse   10 gal  6 sec     .16667 Hz
 5 Pulse    5 gal   12 sec    .08333 Hz
 2 Pulse    2 gal   2 sec     .03333 Hz
 .5 Pulse   .5 gal  120 sec   .00833 Hz
~~~
Some reference information:
~~~
 https://www.petropedia.com/definition/7578/meter-k-factor
 https://instrumentationtools.com/flow-meter-k-factor-and-calculations/
 https://www.creativesensortechnology.com/copy-of-pct-120
~~~
Examples:

For a 1 GPM water meter:

At a 1-GPM flow rate, it takes 1 minute for one pulse from the sensor,
So for a 0.25-GPM minimum rate, it takes 4 minutes between pulses...
We will typically limit the flow range to about .25gpm to 60gpm for a 1-GPM sensor.
~~~
We set the sensor to type = 0.
Set the Flow Factor to 1.
Set max flow rate to 60 (per manufacture spec sheet)
Set Flow Units to 0 (GPM)
~~~

For a K-Offset sensor:  (For a CST ELF,  1in sensor, 0.20 to 20 GPM)
~~~
We set the sensor type to 1 
Set K to: 0.153
Set offset to: 1.047
Set max flow rate to 20 (per manufacture spec sheet)
Set Flow Units to 0 (GPM)
~~~


We also sense excessive flow, we do this by setting an excessive flow limit and the amount of time to be over this limit.
 
Most of these settings are changeable from commands to the device.



Sensor Commands:

~~~
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
                        1       1 litre                    LM
      3     K              // K value from the device, float
      4     Offset         // Offset value from device, float
      5     Flow_units
                        0 = GPM     GAL Gallons per minute
                        1 = Cft     CF  Cubic Feet per minute
                        2 = M3      CM  Cubic Metre per minute
                        3 = LM      L   Litres per minutes
      6     Excess Flow Threshold                         // flow rate at which to trigger an excess flow
      7     Excess Flow Threshold Time                    // time in seconds to report excessive flow
      8     Current Send Interval in seconds              // How often do we send MQTT information when we have a flow 
      9     MQTT BitMask, 16 bits                         // used to enable/disable MQTT messages
      10    Max Flow Rate                                 // Max flow rate for this sensor
      11    Debounce 0 = off, 1 = on
      12    Debounce Low Time in MS
      13    Debounce High Time in MS
      14    Set Flow Units to any text 1-7 char 
      15    Set Flow Volume Units to any text 1-7 char
       
----------------------------------
MQTT send bit mask: We use this to enable/disable MQTT messages
Default = 0xFFFF
Bit:
0 	FlowCount 
1 	Flow
2 	FlowPeriod
3 	Last 1hrVolume
-
4 	Last 24hrVolume
5 	RateFactor
6 	K
7 	OffSet
-
8 	FlowUnits
9 	VolumeUnits
10	Current1hrVolume
11 	Current24hrVolume
-
12 	Excess flow flag
13 	VolumePerFlow
14
15 	Message (Not Used Yet)


Flow Units:
0 	GPM Gallons per minute <--- defaults
1	CFT Cubic Feet per minute
2 	M3 Cubic Meters per minute
3 	LM Liters per minute
~~~
