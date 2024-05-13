/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

#if !DEVICE_CAN
#error [NOT_SUPPORTED] CAN not supported for this target
#endif

#define MAX_V 3.3
#define BRAKE_TOL 1 
#define RX_pin D10
#define TX_pin D2

AnalogIn HE1(A0);
AnalogIn HE2(A1);
AnalogIn brakes(A2);
InterruptIn Cockpit(D9);
DigitalIn Cockpit_D(D9);
DigitalOut RTDScontrol(D7);
DigitalOut test_led(LED1);
CAN can(RX_pin, TX_pin);

float HE1_V = 0;
float HE2_V = 0;
bool TS_Ready = false; //TODO write CAN code to alter 
bool Motor_On = false;

void runRTDS() {
    // RTDScontrol.write(true);
    // wait_us(3000000);
    // RTDScontrol.write(false);
    printf("RUNNING RTDS");
    test_led.write(true);
    wait_us(3000000);
    test_led.write(false);
    printf("FINISHED RTDS");
}

void cockpit_switch_high() {
    wait_us(10000);
    int signal = Cockpit_D.read();
    if(Cockpit_D.read() == 0) {
        return;
    }
    test_led.write(true);
    if (TS_Ready && brakes.read() >= BRAKE_TOL) {
        Motor_On = true;
    }
    wait_us(10000);
    test_led.write(false);
}

void check_start_conditions() {
    CANMessage msg;
    int counter = 0;
    while (Motor_On == false) {
        if(counter >= 10000) { 
            printf("Still Waiting \n"); 
            counter = 0;
        }

        printf("Cockpit_D: %d\n", Cockpit_D.read());
        if(brakes.read() > BRAKE_TOL) {
            printf("Brakes pressed.\n");
        }

        if(can.read(msg)) {
            printf("Message received: %d\n", msg.data[0]);
            // TODO recognize tractive signal and set TS_rdy if received
        }

    }
}

void cockpit_switch_low() {
    wait_us(10000);
    int signal = Cockpit_D.read();
    if(Cockpit_D.read() == 1) {
        return;
    }
    test_led.write(true);
    Motor_On = false;
    wait_us(10000);
    test_led.write(false);
}

int main()
{
    printf("Waiting for start conditions!\n");
    Cockpit.rise(&cockpit_switch_high);
    Cockpit.fall(&cockpit_switch_low);
    check_start_conditions();
    // Cockpit switch needs to be on, Brakes need to be on, tractive system rdy message needs to be sent
    // Two threads check for the start conditions TS_rdy and BSPD and shut down when they are met
    // First we check that if all three thread conditions are met, test_led 3 sec, and then begins to blink
    //voltage range from 0 to 3.3

    runRTDS();
    
    //continuously read analog pins multiply by 3.3V
    while(1) {
        //Throttle angle .5 to 4.5 originally
        //20 degrees
        //.5 to 2.5 V
        //TODO test HE1 and HE2 range so you can calculate it (Pedal box mock up broken we need that. Next year we not using these HE sensors fs)
        HE1_V = HE1.read() * MAX_V;
        HE2_V = HE2.read() * MAX_V;
        printf("HE1: %f \nHE2: %f\n", HE1_V, HE2_V);

        test_led.write(true);
        wait_us(500000);
        test_led.write(false);
        //Send CAN 50 hz
        //Calculate percent HE1 and percent HE2
        //Measure percent difference check 
        //implausibility if greater than 10% pedal travel diff for more than 100 ms.
        //Send CAN to stop power
        //How to check for short and open circuit? If 0V or 3.3V do implausibility.

        if(Motor_On == false) break;
    }

    main();
}



