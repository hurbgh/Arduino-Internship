# 2025 Summer Internship at interlab code
The files bt_spp_acceptor and turnLedOnFromPhone are files made from me reinstalling ESP-IDF to make sure it works correctly. The files screenDisplay, and pmsArrayWorks are code I wrote first to learn how to read from a sensor and print to the LCD screen before moving on to multiplexorPrintCheck which contains that actual code to check all the sensors connected to a multiplexor and print the results obtained from the sensors connected to the multiplexor.

## Sensor Testing Documentation
The following are diagrams to help understand how the multiplexorPrintCheck program works.

MH-Z16 CO2 sensor state diagram:
![State diagram of the co2Sensor function.](./images/co2Sensor_function_state_diagram.jpg "co2Sensor state diagram")

PMS7003 sensor state diagram:
![State diagram of pmsSensor function.](./images/pmsSensor_function_state_diagram.jpg "pms state diagram")

Getting PM data sequence diagrams:
![Sequence diagram for getting data from PMS7003](./images/loop_gets_PM_sensor_data.jpg "Sequence diagram of getting data from PM sensor")
![Sequence diagram for getting connection failed from PMS7003](./images/loop_timeout_PM_sensor_data.jpg "Sequence diagram of getting connection failed from PM sensor")

Getting CO2 data sequence diagrams:
![Sequence diagram for getting data from MH-Z16](./images/loop_gets_CO2_sensor_data.jpg "Sequence diagram of getting data from MH-Z16 sensor")
![Sequence diagram for getting connection failed from MH-Z16](./images/loop_timeout_CO2_sensor_data.jpg "Sequence diagram of getting connection failed from MH-Z16 sensor")

### How it works:
The pmsSensor() is repeatedly called by the loop() to read the PM sensors. It progresses through each step of reading the sensor by using boolean variables and if statements to guide it. On the other hand, the co2Sensor() is not repeatedly called. It is called only once and returns with connection failed or the actual data. The program goes to a port, gets data, prints it, then moves to the next port. Depending on the port how it gets sensor data is different because those ports are intended for specific types of sensors and so which port it selects affects its approach becase the two sensors work differently.