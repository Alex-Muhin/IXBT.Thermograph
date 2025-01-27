# IXBT.Thermograph
The device is intended to track ambient temperature with several wired sensors 18B20.  
Measurements are saved to SD card as CSV file.
## Assembling
Thermograph is built of two boards: any kind of Arduino (Amtel 328); and microSD card reader.  
Current script uses pin D10 as CS for SD card reader and D4 for OneWire bus. The bus (D4) is prepulled with ~2.5 kOhm resistor to 3.3v (all the 18B20 sensors are powered with 3.3v ). 
## Configuring
After being turned on the device is checking SD card and sensors. Then it tries to read SENSOR.CFG file (see below) and add uregistres sensors in it. 
In SENSORS.CFG you can define:
```
P:<int>	  - measurement period in seconds between 1 and 1800 sec (f.e. P:120 means "every two minutes")  
D:<char>  - decimal point symbol ("." or "," only)  
C:<char>  - CSV column delimiter. Only ",", ";" and "	" (tabulation) are allowed  
```
Below sensor's addreses are enumerated. The addresses are listed in the order in which the thermograph detected the newly connected sensors.   
F.e.:  
```
S:0030-0001-43da-9ef0  
S:1201-0000-1f67-0028  
```
The device can serve up to ten sensors (you may increase MAX_SENSORS constant if you need more).   
## CSV
On each session the new Wxxxxxxx.csv file is being created (W0000000.CSV, W0000001.CSV... ect.)  
Number of columns in the CSV corresponds to number of connected sensors. Column headings consist of "S:" and sensor position in SENSORS.CFG.  
So it's thoughtful to register all of your sensors before usage: just tag all of them sequntially (1,2,3,...10) and then connect one by one to unpowered device and turn it on. The SENSORS.CSV file will be filled with your sensors' addresses in order of marks you made. Later you'll be able to chage the sequence.
