// Libraries
#include <Arduino.h>
#include <Adafruit_SleepyDog.h>
#include <SoftwareSerial.h>
#include <Adafruit_FONA.h>
#include <SD.h>
#include <SPI.h>

#define LEAD_PIN             6
#define CHIP_SELECT          10

// FONA pins configuration
#define FONA_RX              2   // FONA serial RX pin (pin 2 for shield).
#define FONA_TX              3   // FONA serial TX pin (pin 3 for shield).
#define FONA_RST             4   // FONA reset pin (pin 4 for shield)

// FONA GPRS configuration
#define FONA_APN             ""  // APN used by cell data service (leave blank if unused)

#define MAX_GPS_FIX_FAILURES 24 // Maximum number of GPS fix failures in a row before sleeping.
#define MAX_GPRS_FAILURES    6  // Maximum number of GPRS enable failures in a row before sleeping.
#define MAX_HTTP_FAILURES    6  // Maximum number of HTTP post failures in a row before sleeping.
#define MAX_LOOP_FAILURES    6  // Maximum number general failures in a row before resetting the whole sketch.

#define PUBLISH_INTERVAL     (int)5 // Publish interval in minutes.

#define HTTP_POST_URL        ""
