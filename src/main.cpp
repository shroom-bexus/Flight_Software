//   ___ _  _ ___  ___   ___  __  __
//  / __| || | _ \/ _ \ / _ \|  \/  |
//  \__ \ __ |   / (_) | (_) | |\/| |
//  |___/_||_|_|_\\___/ \___/|_|  |_|
// -----------------------------------
// BEXUS - Student Balloon Experiment

#include <Arduino.h>

#include "max31865.h"

void setup() {
// write your initialization code here
    Serial.begin(115200);

    init_temp();
}

void loop() {
// write your code here
    update_temp();
    Serial.println(get_temp(TempSensor::TEMP_1));
    delay(500);
}