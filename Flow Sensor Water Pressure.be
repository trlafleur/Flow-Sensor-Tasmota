

#- This is using a pressure sensor to check the current water pressure in our system.
   The sensor measures 0-100PSI, using the Analog to Digital Converter
   in the ESP32 on a pin (A4), The ATD is 12bits, 0-4095 from 0v to 3.3v.
   We will use it in the 0 to 2.5V range as specified in the ESP32 reference document.
   Tasmota setting for the ATD are 12bits with ADC_ATTEN_DB set to 11.
   This ATD converter is terribly inaccurate, I do not recommend it for anything serious.
   It's also not very linear at the bottom or top of its range and has
   a large offset; For us, this is not a real issue as the sensor has a voltage 
   range of 0.5v for 0 PSI and 100 PSI at 4.5V and we will offset the reading as needed.
    
   We are interested in a change of water pressure, and the accuracy is not real important. 
 
   We scaled the input voltage by 10k/10k resistors with a .1uf to gnd,
   this give us a scaling factor of .50 so:
        5v      = 2.500v
        4.5V    = 2.250v
        2.5v    = 1.250v
        1.25v   = 0.625v
        0.5V    = 0.250v

   Sensor range is : 0.5V  4.5V = a 4v range as a linear voltage:
    0   psi = 0.5V * 0.5 = 0.25v = 138     after scalling
    50  psi = 2.5V * 0.5 = 1.25v = 1328
    100 psi = 4.5V * 0.5 = 2.25v = 2515
    
    We have measured the ATD in our ESP32 and found these settings to work for us... 
    Checking multiple devices, we found that they were sometimes off by 10% or more!
    
    AdcParam1 6,138,2515,0,100    # We are using the scaling function built in to Tasmota
     
        
        ESP32 A TO D Reference:
            https://deepbluembedded.com/esp32-adc-tutorial-read-analog-voltage-arduino/
            https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html
   
   We display the data on the web page and send it via MQTT.
 
   Open the web page for this device, select Console, then Manage File System
   Rename this Berry file to "autoexec.be", then upload it to the ESP32 file system. 
   Reboot Tasmota, this Berry file will run after re-booting.
   
 -#


 #- *************************************** -#
 #-
    CHANGE LOG:
 
    DATE         REV  DESCRIPTION
    -----------  ---  ----------------------------------------------------------
    17-Apr-2022  1.0  TRL - 1st release, water pressure
   
    
    Notes:  1)  Tested with 12.3.1.1(tasmota)
    
    
    ToDo:   1)

    tom@lafleur.us
 
-#


#- *************************************** -#
    #ifndef USE_ADC
    #define USE_ADC
    #endif
   
    #define USE_BERRY_DEBUG 
    

#- *************************************** -#
class FlowPressure : Driver
    
      var FlowPres_data
      #build an global array-->list to store sensor data for filtering
      static buf = []
     
    
def FlowPres()
#- *************************************** -#
  
    var MaxBuf = 5  # 60??
    #print ("\n")
 
    # Read Sensor data
    import json
    var sensors=json.load(tasmota.read_sensors())
    if !(sensors.contains('ANALOG')) return end
    var d = real(sensors['ANALOG']['Range1'] )
    #print("ATD: ", d)
    
    if (self.buf.size() >= MaxBuf) self.buf.pop(0) end      # remove oldest entry
    self.buf.push(d)                                        # add new sensor reading to list
    
                                           
    d = 0.0                                         # clear sum
    for i : 0 .. (self.buf.size() - 1 )             # let's sum all of the entrys in the array
     d = d + self.buf.item (i)
    end
    
    d = d / self.buf.size()                         # average the sensor data from the array
    #print("ATD: ", d)
    if (d < 0) d = 0 end
    self.FlowPres_data = [(d)]                    # return the data as PSI %
    return self.FlowPres_data
  end


#- *************************************** -#
  def every_second()
	if !self.FlowPres return nil end
	self.FlowPres()
  end


#- *************************************** -#
  def web_sensor()
    import string
    if !self.FlowPres_data return nil end               # exit if not initialized
    var msg = string.format("{s}Water Pressure {m}%7.2f PSI{e}", self.FlowPres_data[0])
    tasmota.web_send_decimal(msg)
  end


#- *************************************** -#
  def json_append()
    if !self.FlowPres_data return nil end
	import string
	var msg = string.format(",\"WaterPressure\":{\"PSI\":%.f}", self.FlowPres_data[0])
    tasmota.response_append(msg)
  end
  
end


#- *************************************** -#
FlowPressure = FlowPressure()
tasmota.add_driver(FlowPressure)

# for CB0454
#tasmota.cmd('AdcParam1 6,138,2515,0,100')
# for 286814
tasmota.cmd('AdcParam1 6,175,2770,0,100')
#- ************ The Very End ************* -#
