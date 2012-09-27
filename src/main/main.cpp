/* Justin Charette
 * Jennifer *Whatsherface* Garner
 *
 *
 * Errors:
 *      1. no log file
 *          causes:
 *              - no sdcard
 *              - no sdcard filesystem
 *              - sdcard is full
 *          error type:
 *              fatal
 *          response:
 *              crash to error loop
 *  
 *      2. cannot read pH
 *          causes:
 *              - no sensor
 *              - serial comms fail
 *              - sensor malfunctions
 *          error type:
 *              nonfatal
 *          response:
 *              log error token to sdcard instead of measurement.
 *              flag error for user diagnostic check.
 *
 *      3. cannot read temperature
 *          causes:
 *              - no sensor
 *              - one wire comms fail
 *              - sensor malfunctions
 *          error type:
 *              nonfatal
 *          response:
 *              log error token to sdcard instead of measurement.
 *              flag error for user diagnostic check.
 */


#include <Arduino.h>
#include <AtlasPH.h>
#include <ds3231rtc.h>
#include <OneWire.h>
#include <PString.h>
#include <SD.h>
#include <stdlib.h>
#include <Wire.h>
#include <avr/pgmspace.h>


/* datatypes ********************************************************/

struct error_t {
    int8_t      temp_sensor;
    int8_t      ph_sensor;
    int8_t      sdcard;
    int8_t      filesystem;
    int8_t      logfile;
};

struct timer_t {
    unsigned long then;
    unsigned long period;
};

/* prototypes *******************************************************/

boolean check_timer_elapsed (timer_t& timer_);

void    error_loop          (void);

void    loop                (void);

void    measure_and_record  (Print& out);

void    print_two_digits    (Print& out, uint8_t value);

void    serialEvent         (void);
void    setup               (void);

int8_t  read_temperature    (char reading[7]);
int8_t  read_ph             (char reading[6]);
void    record_headers      (Print& out);
void    reset_timer         (timer_t& timer_);


/* global variables *************************************************/

const int8_t TEMP_ONEWIRE_PIN   = 2;
const int8_t PH_RX_PIN          = 3;
const int8_t PH_TX_PIN          = 4;
const int8_t SD_CS_PIN          = 10;
const int8_t ERROR_LED_PIN      = 13;

/* temperature error flags */
const int8_t ERRT_NONE          = 0;
const int8_t ERRT_NO_DEVICE     = 1;
const int8_t ERRT_INVALID_CRC   = 2;
const int8_t ERRT_UNRECOGNIZED  = 3;
const int8_t ERRT_1WIRE_BUSY    = 4;
const int8_t ERRT_CONVERT       = 5;

/* pH error flags */
/* use AtlasPH return value constants. */
const int8_t ERRP_UNKNOWN_VERSION   = 4;

/* sdcard error flags */
const int8_t ERRSD_NO_CARD      = 1;
const int8_t ERRSD_NO_FILESYS   = 2;


/* temperature error messages for log file. 6 chars max. */
const char tempErrMsg_none[]    PROGMEM =   "";             // 0    errt none
const char tempErrMsg_no_dev[]  PROGMEM =   "no dev";       // 1    errt no device
const char tempErrMsg_crc[]     PROGMEM =   "crc  x";       // 2    errt invalid crc
const char tempErrMsg_dev[]     PROGMEM =   "dev ??";       // 3    errt unrecognized
const char tempErrMsg_busy[]    PROGMEM =   "busy x";       // 4    errt 1wire busy
const char tempErrMsg_adc[]     PROGMEM =   "ADC  x";       // 5    errt convert

PGM_P tempErrMsgs[] PROGMEM = {
    tempErrMsg_none,    // 0    errt none
    tempErrMsg_no_dev,  // 1    errt no device
    tempErrMsg_crc,     // 2    errt invalid crc
    tempErrMsg_dev,     // 3    errt unrecognized
    tempErrMsg_busy,    // 4    errt 1wire busy
    tempErrMsg_adc      // 5    errt convert
};

/* pH error error messages for log file. 5 chars max. */
const char phErrMsg_none[]      PROGMEM =   "";             // 0 success
const char phErrMsg_bad_resp[]  PROGMEM =   "huh? ";        // 1 bad response
const char phErrMsg_no_resp[]   PROGMEM =   "helo?";        // 2 no response
const char phErrMsg_check[]     PROGMEM =   "check";        // 3 check sensor
const char phErrMsg_version[]   PROGMEM =   "ver? ";        // 4 unknown version

PGM_P phErrMsgs[] PROGMEM = {
    phErrMsg_none,      // 0 success
    phErrMsg_bad_resp,  // 1 bad response
    phErrMsg_no_resp,   // 2 no response
    phErrMsg_check,     // 3 check sensor
    phErrMsg_version    // 4 unknown version
};

/* real time clock */
DS3231RTC       rtcCom;
/* temperature sensor */
OneWire         tempCom( TEMP_ONEWIRE_PIN );
uint8_t         tempAddr[8];
/* ph sensor */
AtlasPH         phCom( PH_RX_PIN, PH_TX_PIN );
/* sd card */
File            logFile;

/* sampling timer. */
timer_t         timer;

const unsigned long SAMPLING_PERIOD_IN_MILLIS       = 1000;

/* error status */
error_t         errorFlags;

/* functions ********************************************************/


/* error_loop {{{
 * 
 * sit here until user resets the device.
 */
void error_loop (void) {
    digitalWrite( ERROR_LED_PIN, HIGH );
    for (;;) {
        Serial.println( F("fatal error") );
        delay( 2000 );
    }
} //}}}


/* find_temp_sensor_address {{{
 *
 * find one temperature sensor on the one wire bus and save its
 * address for later use.
 */
boolean find_temp_sensor_address (void) {
#if 1
    while (tempCom.search( tempAddr )) {
        /* validate address CRC. */
        if (OneWire::crc8( tempAddr, 7 ) != tempAddr[7]) {
            /* redundant (?) check to make sure our address isn't garbage.
             * using OneWire::crc8 because we are checking the CRC of the
             * specific address.
             */
            /* Debugging */
            Serial.print( F("error: temp: invalid crc: addr ") );
            for (uint8_t a = 0; a < 8; a++) { Serial.write( tempAddr[a] ); }
            Serial.println();
            errorFlags.temp_sensor = ERRT_INVALID_CRC;
            /* check next address. */
            continue;
        }

        /* validate device type/classification. */
        if (tempAddr[0] != 0x10 && tempAddr[0] != 0x28) {
            /* Debugging */
            Serial.print( F("error: temp: wrong device: addr ") );
            for (uint8_t a = 0; a < 8; a++) { Serial.write( tempAddr[a] ); }
            Serial.println();
            errorFlags.temp_sensor = ERRT_UNRECOGNIZED;
            /* check next address. */
            continue;
        }

        /* success. */
        {
            /* clear any errors. */
            errorFlags.temp_sensor = ERRT_NONE;
            Serial.print( F("* temp sensor found ") );
            for (uint8_t a = 0; a < 8; a++) { Serial.print( tempAddr[a], HEX ); }
            Serial.println();
            return true;
        }
    }

    /* no working temperature sensor found. */

    /* reset address search for a later try? */
    tempCom.reset_search();
    /* keep error flag if already set. */
    if (!errorFlags.temp_sensor)
        /* activate error flag for temp sensor. */
        errorFlags.temp_sensor = ERRT_NO_DEVICE;
#endif
    return false;
} //}}}


/* get_temp {{{
 */
int8_t get_temperature (float& dest) {
    uint8_t data[12];

#if 1
    /* wait for 1wire bus to become available.  must do reset before
     * selecting the address.
     */
    if (!tempCom.reset()) {
        /* 1wire bus unavailable. */
        Serial.println( F("error: temp: bus is busy") );
        errorFlags.temp_sensor = ERRT_1WIRE_BUSY;
        return ERRT_1WIRE_BUSY;
    }

    /* select the temperature sensor we found during setup. */
    tempCom.select( tempAddr );

    /* start the conversion, with parasite power on at the end.  convert
     * command 44h.
     */
    tempCom.write( 0x44, 1 );

    /* wait for the OneWire bus to become avaiable.  this indicates that the
     * temperature sensor finished the conversion.
     * reset mostly just tells us wen we can start talking on 1wire... it might
     * mean sensor is there or something is broken... just using it as a delay
     * until the I/O pin goes high and we can continue...
     */
    if (!tempCom.reset()) {
        /* 1wire bus unavailable. */
        Serial.println( F("error: temp: convert failed") );
        errorFlags.temp_sensor = ERRT_CONVERT;
        return ERRT_CONVERT;
    }

    /* select the temperature sensor again. */
    tempCom.select( tempAddr );

    /* read the scratchpad.  read command is 0xBE. */
    tempCom.write( 0xBE );

    /* the memory scratchpad has 9 bytes.  byte 0 is LSB and byte 1 is MSB. */
    for (uint8_t a = 0; a < 9; a++)
        data[a] = tempCom.read();
    
    {
        uint8_t msb = data[1];
        uint8_t lsb = data[0];

        /* using bit-wise shifts to make a 2-byte number; or-ing 0's and LSB. */
        dest = ((msb << 8) | lsb);
        /* divide by 16 to convert number to degree's celcius. */
        dest /= 16;
    }

#endif
    return ERRT_NONE;
} //}}}


/* initialize_log_file {{{
 */
int8_t initialize_log_file (void) {
    return 0;
} //}}}


/* loop {{{
 */
void loop (void) {
    char buffer[33] = { '\0' };     // record here first, then print to Serial and Log
    PString stream( buffer, sizeof(buffer) );

    if (check_timer_elapsed( timer )) {
        reset_timer( timer );
        measure_and_record( stream );
        Serial.print( stream );
        logFile.print( stream );
        logFile.flush();
    }
} //}}}


/* measure_and_record {{{
 */
void measure_and_record (Print& out) {
    DateTime    time;
    float temperature;
    float ph;
    boolean has_temp = false, has_ph = false;

    /* read time from RTC. */
    time = rtcCom.now();

    /* read temperature. */
    {
        errorFlags.temp_sensor = get_temperature( temperature );

        switch (errorFlags.temp_sensor) {
            case ERRT_NONE:
                has_temp = true;
                break;
        }
    }
    
    /* read pH */
    {
        phCom.set_temperature( temperature );   
        errorFlags.ph_sensor = phCom.read( ph );

        switch (errorFlags.ph_sensor) {
            case AtlasPH::SUCCESS:
                has_ph = true;
                break;
            case AtlasPH::E_BAD_RESPONSE:
                Serial.println( F("error: pH: bad read response") );
                break;
            case AtlasPH::E_NO_RESPONSE:
                Serial.println( F("error: pH: no read response") );
                break;
        }
    }

    /* time to print data to stream. */

    /* print the date and time.
     * format (17 chars):  'MM-DD-YY,HH:MM:SS'
     */
    {
        /* month */
        print_two_digits( out, time.month() );
        out.print( '-' );

        /* day */
        print_two_digits( out, time.date() );
        out.print( '-' );

        /* year -- 2-digit version. */
        print_two_digits( out, (uint8_t)(time.year() - 2000) );
        out.print( ',' );

        /* hour */
        print_two_digits( out, time.hour() );
        out.print( ':' );

        /* minute */
        print_two_digits( out, time.minute() );
        out.print( ':' );

        /* second */
        print_two_digits( out, time.second() );
    }

    /* separator.  (1 char) */
    out.print( ',' );

    /* print the temperature.
     * format (6 chars):  '###.##' or 'errmsg'
     */
    {
        char buffer[7] = { '\0' };
        if (has_temp)
            dtostrf( temperature, 6, 2, buffer );
        else
            strncpy_P( buffer, (PGM_P)pgm_read_word(&(tempErrMsgs[errorFlags.temp_sensor])), 6 );
        out.print( buffer );
    }

    /* separator.  (1 char) */
    out.print( ',' );

    /* print the pH.
     * format (5 char):  '##.##' or 'error'
     */
    {
        char buffer[6] = { '\0' };
        if (has_ph)
            dtostrf( ph, 5, 2, buffer );
        else
            strncpy_P( buffer, (PGM_P)pgm_read_word(&(phErrMsgs[errorFlags.ph_sensor])), 5 );
        out.print( buffer );
    }
    
    /* end of line. (1 char) */
    out.println();

    /* 31 characters total. */
} //}}}


/* print_two_digits {{{
 */
void print_two_digits (Print& out, uint8_t value) {
    if (value < 10) {
        out.print( '0' );
        out.print( value );
    }
    else
        out.print( value );
} //}}}


/* setup {{{
 */
void setup (void) {
    Serial.begin( 38400 );

    pinMode( ERROR_LED_PIN, OUTPUT );
    digitalWrite( ERROR_LED_PIN, LOW );

    /* initialize the real time clock. */
    Wire.begin();
    rtcCom.begin();
    // rtcCom.adjust( DateTime( __DATE__, __TIME__ ) );

    /* get the OneWire address of the temperature sensor. */
    if (!find_temp_sensor_address()) {
        /* Debugging */
        Serial.println( F("error: temp: no sensor") );
    }

    /* try to detect pH sensor by retrieving version info. */
    {
        char version_num[6];
        int8_t ret;
        /* try to read version info at most five times.  the SoftwareSerial
         * library is glitchy and still doesn't read reliably.
         */
        for (uint8_t a = 0; a < 5; a++) {
            ret = phCom.get_version_number( version_num );
            if (!ret)
                break;
            delay( 26 );
        }
        if (AtlasPH::SUCCESS != ret) {
            /* Debugging */
            Serial.print( F("error: pH: cannot talk to sensor: ") );
            Serial.println( ret, HEX );
            errorFlags.ph_sensor = ret;
        }
        /* validate version number. */
        if (0 != strcmp( "3.4", version_num )) {
            /* Debugging */
            Serial.println( F("error: pH: unknown version") );
            errorFlags.ph_sensor = ERRP_UNKNOWN_VERSION;
        }

        Serial.println( F("* pH sensor found V3.4") );
    }

    /* try to detect sd card. */
    if (!SD.begin( SD_CS_PIN )) {
        Serial.println( F("error: sd: init failed") );
        errorFlags.sdcard = ERRSD_NO_CARD;
        error_loop();
    }
    else
        Serial.println( F("* sdcard is present") );

    /* try to open the log file. */
    {
        char filename[13] = "log-0000.csv";      // 8.3 filename
        uint8_t counter = 0;
        
        /* check for existing file. */
            /* yes? choose another filename and try again. */
            /* no? open file for logging. */

        /* generate unique filename. */
        while (SD.exists( filename )) {
            counter++;
            if (counter < 10) {
                itoa( counter, &(filename[7]), 10 );
            }
            else if (counter < 100) {
                itoa( counter, &(filename[6]), 10 );
            }
            else {
                Serial.println( F("error: sd: all files used") );
                error_loop();
            }
            filename[8] = '.';
        }

        Serial.print( F("* log file is ") );
        Serial.println( filename );

        logFile = SD.open( filename, FILE_WRITE );
    }

    /* initialize the sampling timer. */
    timer.period = SAMPLING_PERIOD_IN_MILLIS;
    timer.then = millis();

    /* print log headers. */
    record_headers( Serial );
    record_headers( logFile );
    logFile.flush();
} //}}}


/* record_headers {{{
 */
void record_headers (Print& out) {
    out.println( F("date,time,temp(C),pH") );
} //}}}


/* Timer Functions */

/* check_timer_elapsed {{{
 */
boolean check_timer_elapsed (timer_t& timer_) {
    unsigned long now = millis();

    if (timer_.period > (now - timer_.then))
        return false;

    timer_.then = now;
    return true;
} //}}}


/* reset_timer {{{
 */
void reset_timer (timer_t& timer_) {
    timer_.then = millis();
} //}}}


// vim: set et ts=4 sts=4 sw=4 sr fdm=marker :

