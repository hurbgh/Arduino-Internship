The files bt_spp_acceptor and turnLedOnFromPhone are files made from me reinstalling ESP-IDF to make sure it works correctly. The files screenDisplay, and pmsArrayWorks are code I wrote first to learn how to read from a sensor and print to the LCD screen before moving on to multiplexorPrintCheck which contains that actual code to check all the sensors connected to a multiplexor and print the results obtained from the sensors connected to the multiplexor.
The following are diagrams to help understand how the multiplexorPrintCheck program works.

PMS7003 sensor state diagram:


MH-Z16 CO2 sensor:


sequence diagram:
![State diagram of pmsSensor function.](https://github.com/hurbgh/Arduino-Internship/blob/304837b309fbb52de8e6ec2c414b0dd3824e27a5/images/pmsSensor_function_state_diagram.jpg)