/* Justin Charette
 */

#ifndef ATLAS_PH_H_
#define ATLAS_PH_H_

#include <Arduino.h>
#include <SoftwareSerial.h>

class AtlasPH {
public:
    static const int8_t SUCCESS                 = 0;
    static const int8_t E_BAD_RESPONSE          = 1;
    static const int8_t E_NO_RESPONSE           = 2;
    static const int8_t E_CHECK_SENSOR          = 3;

private:
    int8_t          rx_pin, tx_pin;
    SoftwareSerial  com;
    char            version_num[6], version_date[6];
    bool            has_version;

public:
    AtlasPH (int8_t rx_pin_, int8_t tx_pin_);

    /* return true if data is available in the serial buffer */
    bool available (void) { return com.available(); }
    
    /* return the pH reading into the dest variable if successful or return an
     * error code.
     * the pH string format is "##.##"
     */
    int8_t  read (char dest[6]);
    int8_t  read (float& dest);

    /* enable/disable the debugging leds on the pH Stamp uC. */
    void enable_leds (void);
    void disable_leds (void);

    /* set temperature in deg C for pH correction */
    void set_temperature (float temp);

    /* instruct pH Stamp uC to read pH continuously. */
    /* unsure how to effectively handle buffer overflows.
     * read_ph() is sufficient and can be called often instead of
     * relying on automatic, continuous measurements.
     */
    // void start_continuous (void);
    // void stop_continuous (void);

    /* put the pH Stamp uC into standby */
    void standby (void);

    /* reset the pH Stamp uC to original factory settings. */
    int8_t reset_to_factory (void);

    /* return the version number or date in the dest variable if successful, or
     * return an error code.  version number is expected to fit with 6 bytes,
     * but not expected to fill array.
     */
    int8_t get_version_number (char dest[6]);
    int8_t get_version_date (char dest[6]);

private:
    /* transfer the command to the pH Stamp uC, wait for a reply, and store the
     * reply in the dest variable.
     *
     * @param command    the command string to send to the pH Stamp uC.
     * @param dest       the character array to store the response in.
     * @param size       the length of the dest array.
     *
     * return 0, if successful, otherwise a non-zero error code.
     */
    int8_t transfer (const char command[], char dest[], size_t size);

    /* load the version information from the pH Stamp uC into the respective
     * member variables.
     */
    int8_t load_version_info (void);

    /* wait until serial data is available or timeout is exceeded.
     * return true if data available, false if timed out.
     */
    bool wait (unsigned long timeout_millis);
};

#endif

