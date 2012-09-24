/* Justin Charette
 */

#include "AtlasPH.h"
#include <stdlib.h>


/* local static variables *****************************************************/

/* command constants for pH Stamp uC. */
const char C_CAL_04[]   = "f\r";
const char C_CAL_07[]   = "s\r";
const char C_CAL_10[]   = "t\r";
const char C_CONT[]     = "c\r";
const char C_INFO[]     = "i\r";
const char C_LED_OFF[]  = "l0\r";
const char C_LED_ON[]   = "l1\r";
const char C_READ[]     = "r\r";
const char C_RESET[]    = "x\r";
const char C_STANDBY[]  = "e\r";
// no command word for temp

/* command response constants for pH Stamp uC.  abbreviated for 6 character
 * array.
 */
const char CR_CHECK[]   = "check";
const char CR_RESET[]   = "reset";



/* member functions ***********************************************************/

AtlasPH::AtlasPH (int8_t rx_pin_, int8_t tx_pin_) :
    rx_pin( rx_pin_ ),
    tx_pin( tx_pin_ ),
    com( rx_pin_, tx_pin_ ),
    has_version( false )
{
    for (uint8_t a = 0; a < sizeof(version_num); a++) {
        version_num[a] = '\0';
        version_date[a] = '\0';
    }

    com.begin( 38400 );
    disable_leds();             // turn off leds for faster readings.
    standby();                  // stop continuous reading mode.
}

int8_t AtlasPH::read (char dest[6]) {
    int8_t ret;
    ret = transfer( C_READ, dest, 6 );
    if (!ret)
        return ret;
    if (!strcmp( CR_CHECK, dest ))
        return E_CHECK_SENSOR;
    return SUCCESS;
}

int8_t AtlasPH::read (float& dest) {
    char buffer[6] = { '\0' };
    int8_t ret = transfer( C_READ, buffer, 6 );
    if (ret)
        return ret;
    if (!strcmp( CR_CHECK, buffer ))
        return E_CHECK_SENSOR;
    dest = (float)atof( buffer );
    return SUCCESS;
}

void AtlasPH::enable_leds (void) {
    com.print( C_LED_ON );
    com.print( '\r' );
}

void AtlasPH::disable_leds (void) {
    com.print( C_LED_OFF );
    com.print( '\r' );
}

void AtlasPH::set_temperature (float temp) {
    char buffer[6] = { '\0' };
    dtostrf( temp, 5, 2, buffer );  // convert float to string
    com.print( buffer );
    com.print( '\r' );
}

void AtlasPH::standby (void) {
    com.print( C_STANDBY );
}

int8_t AtlasPH::reset_to_factory (void) {
    char buffer[6] = { '\0' };
    transfer( C_RESET, buffer, 6 );
    if (0 == strcmp( CR_RESET, buffer ))
        return SUCCESS;
    return E_BAD_RESPONSE;
}

int8_t AtlasPH::get_version_number (char dest[6]) {
    int8_t ret;

    if (! has_version)
        if (0 != (ret = load_version_info()))
            return ret;

    strlcpy( dest, version_num, 6 );
    return SUCCESS;
}

int8_t AtlasPH::get_version_date (char dest[6]) {
    int8_t ret;

    if (! has_version)
        if (0 != (ret = load_version_info()))
            return ret;

    strlcpy( dest, version_date, 6 );
    return SUCCESS;
}


/* private member functions ***************************************************/

int8_t AtlasPH::transfer (const char command[], char dest[], size_t size) {
    uint8_t a, timeout = 1000;
    char b;

    com.write( command );

    if (!wait( 26 ))
        return E_NO_RESPONSE;

    a = 0;
    while (timeout--) {
        while (com.available()) {
            if (a == size-1)
                return E_BAD_RESPONSE;
            b = com.read();
            if (b != '\r')
                dest[a] = b;
            else {
                dest[a] = '\0';
                goto end_of_xfer;
            }
            a++;
        }
    }
end_of_xfer:

    dest[size-1] = '\0';    // ensure dest array is null terminated.
    return SUCCESS;
}

bool AtlasPH::wait (unsigned long timeout_millis) {
    unsigned long start_time, duration;

    start_time = millis();

    while (! com.available()) {
        duration = millis() - start_time;
        if (duration >= timeout_millis)
            return false;
    }
    return true;
}

int8_t AtlasPH::load_version_info (void) {
    char buffer[16] = { '\0' };
    int8_t ret;
    uint8_t a, b;

    if (SUCCESS != (ret = transfer( C_INFO, buffer, sizeof(buffer) )))
        return ret;

    /* info response is in buffer.  find version. */
    a = 0;
    while (buffer[a] != 'V') {
        a++;
        if (a == sizeof(buffer))
            return E_BAD_RESPONSE;
    }

    /* skip V of version string. */
    a++;

    /* found the V.  grab the version number. */
    b = 0;
    while (buffer[a] != ',') {
        version_num[b] = buffer[a];
        a++; b++;
        if (a == sizeof(buffer) || b == sizeof(version_num))
            return E_BAD_RESPONSE;
    }
    version_num[b] = '\0';  // null terminate version number string.

    /* found end of version number.  move to version date. */
    a++;

    /* copy version date. */
    b = 0;
    while (a != sizeof(buffer) && buffer[a]) {
        version_date[b] = buffer[a];
        a++; b++;
        if (b == sizeof(version_num))
            return E_BAD_RESPONSE;
    }
    version_date[b] = '\0'; // null terminate version date string.

    has_version = true;
    return SUCCESS;
}


