// GPS Tracker project with Adafruit IO
// Author: Martin Spier

// Libraries
#include <Arduino.h>
#include <Adafruit_SleepyDog.h>
#include <SoftwareSerial.h>
#include "Adafruit_FONA.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// Alarm pins
const int ledPin = 6;

// Latitude & longitude for distance measurement
float latitude, longitude, speed_kph, heading, altitude;

// FONA pins configuration
#define FONA_RX              2   // FONA serial RX pin (pin 2 for shield).
#define FONA_TX              3   // FONA serial TX pin (pin 3 for shield).
#define FONA_RST             4   // FONA reset pin (pin 4 for shield)

// FONA GPRS configuration
#define FONA_APN             "wholesale"  // APN used by cell data service (leave blank if unused)
#define FONA_USERNAME        ""  // Username used by cell data service (leave blank if unused).
#define FONA_PASSWORD        ""  // Password used by cell data service (leave blank if unused).

// Adafruit IO configuration
#define AIO_SERVER           "io.adafruit.com"  // Adafruit IO server name.
#define AIO_SERVERPORT       1883  // Adafruit IO port.
#define AIO_USERNAME         "mspier"  // Adafruit IO username (see http://accounts.adafruit.com/).
#define AIO_KEY              "91ad2dfb5578457ead8c903353edd2a7"  // Adafruit IO key (see settings page at: https://io.adafruit.com/settings).

#define MAX_TX_FAILURES      3  // Maximum number of publish failures in a row before resetting the whole sketch.

// FONA instance & configuration
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);     // FONA software serial connection.
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);                 // FONA library connection.

// Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
Adafruit_MQTT_FONA mqtt(&fona, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

uint8_t txFailures = 0;                                       // Count of how many publish failures have occured in a row.

// Feeds configuration
Adafruit_MQTT_Publish tracker_feed = Adafruit_MQTT_Publish(&mqtt, "mspier/feeds/tracker/csv");

// Halt function called when an error occurs.  Will print an error and stop execution while
// doing a fast blink of the LED.  If the watchdog is enabled it will reset after 8 seconds.
void halt(const __FlashStringHelper *error) {
  Serial.println(error);
  while (1) {
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.println("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}

// Serialize the lat, long, altitude to a CSV string that can be published to the specified feed.
void logTracker(float speed, float latitude, float longitude, float altitude, Adafruit_MQTT_Publish& publishFeed) {
  // Initialize a string buffer to hold the data that will be published.

  char sendbuffer[120];
  char *p = sendbuffer;

  // add speed value
  dtostrf(speed, 2, 6, p);
  p += strlen(p);
  p[0] = ','; p++;

  // concat latitude
  dtostrf(latitude, 2, 6, p);
  p += strlen(p);
  p[0] = ','; p++;

  // concat longitude
  dtostrf(longitude, 3, 6, p);
  p += strlen(p);
  p[0] = ','; p++;

  // concat altitude
  dtostrf(altitude, 2, 6, p);
  p += strlen(p);

  // null terminate
  p[0] = 0;

  // Finally publish the string to the feed.
  Serial.println(F("Publishing tracker information: "));
  Serial.println(sendbuffer);
  if (!publishFeed.publish(sendbuffer)) {
    Serial.println(F("Publish failed!"));
    txFailures++;
  }
  else {
    Serial.println(F("Publish succeeded!"));
    txFailures = 0;
  }
}

void setup() {

  // Initialize serial output.
  Serial.begin(115200);
  Serial.println(F("Adafruit IO & FONA808 Tracker"));

  // Set alarm components
  pinMode(ledPin, OUTPUT);

  // Initialize the FONA module
  Serial.println(F("Initializing FONA....(may take 10 seconds)"));
  fonaSS.begin(4800);
  if (!fona.begin(fonaSS)) {
    halt(F("Couldn't find FONA"));
  }
  fonaSS.println("AT+CMEE=2");
  Serial.println(F("FONA is OK"));

  // Use the watchdog to simplify retry logic and make things more robust.
  // Enable this after FONA is intialized because FONA init takes about 8-9 seconds.
  Watchdog.enable(8000);
  Watchdog.reset();

  // Wait for FONA to connect to cell network (up to 8 seconds, then watchdog reset).
  Serial.println(F("Checking for network..."));
  while (fona.getNetworkStatus() != 1) {
   delay(500);
  }

  // Enable GPS.
  fona.enableGPS(true);

  // Start the GPRS data connection.
  Watchdog.reset();
  //fona.setGPRSNetworkSettings(F(FONA_APN));
  fona.setGPRSNetworkSettings(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD));
  delay(4000);
  Watchdog.reset();
  Serial.println(F("Enabling GPRS"));
  if (!fona.enableGPRS(true)) {
    halt(F("Failed to turn GPRS on, resetting..."));
  }
  Serial.println(F("Connected to Cellular!"));

  // Wait a little bit to stabilize the connection.
  Watchdog.reset();
  delay(3000);

  // Now make the MQTT connection.
  int8_t ret = mqtt.connect();
  if (ret != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    halt(F("MQTT connection failed, resetting..."));
  }
  Serial.println(F("MQTT Connected!"));

  Watchdog.reset();

  Serial.print("Initializing SD card...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output
  // or the SD library functions will not work.
   pinMode(10, OUTPUT);

  if (!SD.begin(10)) {
    Serial.println("Initialization failed!");
    return;
  }
  Serial.println("Initialization done.");
}

void loop() {

  // Watchdog reset at start of loop--make sure everything below takes less than 8 seconds in normal operation!
  Watchdog.enable(8000);
  Watchdog.reset();

  // Reset everything if disconnected or too many transmit failures occured in a row.
  if (!fona.TCPconnected() || (txFailures >= MAX_TX_FAILURES)) {
    halt(F("Connection lost, resetting..."));
  }

  // Grab a GPS reading.
  float latitude, longitude, speed_kph, heading, altitude;
  fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);

  // Grab battery reading
  uint16_t vbat;
  fona.getBattPercent(&vbat);

  // Log the current location to the path feed, then reset the counter.
  logTracker(speed_kph, latitude, longitude, altitude, tracker_feed);

  // Disable Watchdog for delay
  Watchdog.disable();

  // Wait 60 seconds
  delay(60000);

}
