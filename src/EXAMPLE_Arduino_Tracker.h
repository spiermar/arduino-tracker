// Libraries
#include <Arduino.h>
#include <Adafruit_SleepyDog.h>
#include <SoftwareSerial.h>
#include "Adafruit_FONA.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"
#include "SD.h"
#include "SPI.h"

#define LEAD_PIN             6

// FONA pins configuration
#define FONA_RX              2   // FONA serial RX pin (pin 2 for shield).
#define FONA_TX              3   // FONA serial TX pin (pin 3 for shield).
#define FONA_RST             4   // FONA reset pin (pin 4 for shield)

// FONA GPRS configuration
#define FONA_APN             ""  // APN used by cell data service (leave blank if unused)

// Adafruit IO configuration
#define AIO_SERVER           "io.adafruit.com"  // Adafruit IO server name.
#define AIO_SERVERPORT       1883  // Adafruit IO port.
#define AIO_USERNAME         ""  // Adafruit IO username (see http://accounts.adafruit.com/).
#define AIO_KEY              ""  // Adafruit IO key (see settings page at: https://io.adafruit.com/settings).
#define AIO_FEED             ""

#define MAX_TX_FAILURES      3  // Maximum number of publish failures in a row before resetting the whole sketch.
#define MAX_MQTT_FAILURES    6  // Maximum number of MQTT connect failures in a row before resetting the whole sketch.
#define MAX_GPS_FIX_FAILURES 12  // Maximum number of GPS fix failures in a row before resetting the whole sketch.
#define MAX_GPRS_FAILURES    6  // Maximum number of GPRS enable failures in a row before resetting the whole sketch.
#define MAX_LOOP_FAILURES    6  // Maximum number general failures in a row before resetting the whole sketch.

#define PUBLISH_INTERVAL     (int)5 // Publish interval in minutes.
