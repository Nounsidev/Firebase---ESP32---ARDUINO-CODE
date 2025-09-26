SOLAR ENERGY LED CONTROL SYSTEM 
USING FIREBASE, ESP32, ARDUINO AND LORA

Devices use: Arduino Uno R3, ESP32 cp102, LoRa Ra02
Features: Read, write data - firebase; 
          Communicate multiple slave devices (Arduino) by a master device (ESP32) using LoRa;
          Control led and light sensor
          Read battery level using resistor (not the optimal way due to energy wasted on resistor and battery voltage drop follows non-linear curve)

Include in this repo: Hardware connection
                      Arduino Code
                      ESP32 Code 

Description: Data will be read from firebase every few seconds by ESP32. Data is continuously synchronised accross master and slave. 
Master will takes turn sending and receiving data from slave (Identify different device using an address, such as 0x01, 0x02).
Each slave will read battery level and send back to master. if a slave is in light sensor mode,  it will override the led state variable.
Master receive battery and led state (only in light sensor mode), check if any variables have changed and write to firebase. 
