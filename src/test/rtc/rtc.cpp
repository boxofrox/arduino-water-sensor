

/*
This software was made to demonstrate how to quickly get your Atlas Scientific product running on the Arduino platform.
An Arduino Duemilanove board was used to test this code.
This code was written in the Arudino 1.0 IDE
Modify the code to fit your system.
Code efficacy was NOT considered, this is a demo only.
The soft serial port TX line goes to the RX pin.
The soft serial port RX line goes to the TX pin.
Make sure you also connect to power and GND pins to power and a common ground.
Data is received and re-sent through the Arduinos hardware UART TX line.
Open TOOLS > serial monitor, set the serial monitor to the correct serial port and set the baud rate to 38400.
Remember, select carriage return from the drop down menu next to the baud rate selection; not "both NL & CR".
The data from the Atlas Scientific product will come out on the serial monitor.
Type in a command in the serial monitor and the Atlas Scientific product will respond.
*/




#include <Arduino.h>
#include <Wire.h>
#include <ds3231rtc.h>

/* global variables ***********************************************************/

/* real time clock */
DS3231RTC       rtcCom;


/* prototypes *****************************************************************/

void    setup               (void);
void    loop                (void);
void    print_date_time     (const DateTime& time);
void    print_two_digits    (Stream& out, uint8_t value);


void setup (void) {
    DateTime    time;

    Serial.begin(38400);
    Wire.begin();
    rtcCom.begin();
    

    Serial.println( "Date/Time: uninitialized" );
    print_date_time( time );
    Serial.println();

    time = rtcCom.now();

    Serial.println( "Date/Time: now()" );
    print_date_time( time );
    Serial.println();


    /* compiled time */
    Serial.println( "Date/Time: compiled" );
    print_date_time( DateTime( __DATE__, __TIME__ ) ); 
    Serial.println();

    /* set the RTC time. */
    rtcCom.adjust( DateTime( __DATE__, __TIME__ ) );

    for (uint8_t a = 0; a < 10; a++) {
        delay( 1000 );

        time = rtcCom.now();
        Serial.println( "Date/Time: now()" );
        print_date_time( time );
        Serial.println();
    }
}
 
 
void loop (void) {
}

void print_date_time (const DateTime& time) {
    Serial.print( " * " );
    print_two_digits( Serial, time.month() );

    Serial.print( "-" );
    print_two_digits( Serial, time.date() );
    
    Serial.print( "-" );
    print_two_digits( Serial, time.year() );

    Serial.print( " " );
    print_two_digits( Serial, time.hour() );

    Serial.print( ":" );
    print_two_digits( Serial, time.minute() );

    Serial.print( ":" );
    print_two_digits( Serial, time.second() );
}

void print_two_digits (Stream& out, uint8_t value) {
    if (value < 10) {
        out.print( '0' );
        out.print( value );
    }
    else
        out.print( value );
}

// vim: set ft=cpp :
