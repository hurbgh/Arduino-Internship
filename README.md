# AIT interlab internship
## Test sensor project
PMS7003 sensor state diagram
![State diagram of pmSensor() function](images/pmsSensor_function_state_diagram.jpg)

PMS7003 sensor sequence diagram
![Sequence diagram of pmsSensor()](images/loop_gets_PM_sensor_data_fixed.jpg)

MH-Z16 sensor state diagram
![State diagram of co2Sensor() function](images/co2Sensor_function_state_diagram.jpg)

MH-Z16 sensor sequence diagram
![Sequence diagram of co2Sensor() function](images/loop_gets_CO2_sensor_data_all_outcomes.jpg)

### How the testSensors program works
The program repeatedly uses loop to process each state when getting data from a port used by the PMS7003 sensor. On the other hand, for the MH-Z16 sensor it is straightforward. If it gets data it will store it and process it, else it just marks it as connection failed.