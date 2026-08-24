//   ___ _  _ ___  ___   ___  __  __
//  / __| || | _ \/ _ \ / _ \|  \/  |
//  \__ \ __ |   / (_) | (_) | |\/| |
//  |___/_||_|_|_\\___/ \___/|_|  |_|
// -----------------------------------
// BEXUS - Student Balloon Experiment

#include <Arduino.h>

#include "max31865.h"
#include "wsen_pads.h"

void setup() {
// write your initialization code here
    Serial.begin(115200);

    max31865_init();
    wsen_pads_init();
}

void loop() {
// write your code here
    max31865_update();
    wsen_pads_update();


    Serial.print("max31865 1 temperature: ");
    Serial.println(max31865_get_temperature(TempSensor::TEMP_1));
    Serial.print("wsen_pads teperature: ");
    Serial.println(wsen_pads_get_temperature());
    Serial.print("wsen_pads pressure: ");
    Serial.println(wsen_pads_get_pressure());

    delay(500);
}