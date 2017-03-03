// GPS Tracker project with Adafruit IO
// Author: Martin Spier

#include "Arduino_Tracker.h"

// Alarm pins
const int ledPin = LEAD_PIN;

// FONA instance & configuration
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);     // FONA software serial connection.
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);                 // FONA library connection.

// Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
Adafruit_MQTT_FONA mqtt(&fona, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

uint8_t txFailures = 0;                                       // Count of how many publish failures have occured in a row.
uint8_t mqttFailures = 0;                                     // Count of how many MQTT connect failures have occured in a row.
uint8_t gpsFixFailures = 0;                                   // Count of how many GPS fix failures have occured in a row.
uint8_t gprsFailures = 0;                                     // Count of how many GPS fix failures have occured in a row.

// Halt function called when an error occurs.  Will print an error and stop execution while
// doing a fast blink of the LED.  If the watchdog is enabled it will reset after 8 seconds.
void halt(const __FlashStringHelper *error) {
  Serial.println(error);
  Watchdog.enable(8000);
  Watchdog.reset();
  while (1) {
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void mqttConnect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    Serial.println(F("MQTT already connected... "));
    return;
  }

  Serial.println(F("Connecting to MQTT... "));

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    mqttFailures++;
    // Reset everything if too many MQTT connect failures occured in a row.
    if (mqttFailures >= MAX_MQTT_FAILURES) {
      halt(F("Too many MQTT connect failures, resetting..."));
    }
    Serial.println(F("Retrying MQTT connection in 5 seconds..."));
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println(F("MQTT Connected!"));
  mqttFailures = 0;
}

void cellularConnect() {
  Watchdog.enable(8000);
  Watchdog.reset();

  // Getting out of sleep mode
  fonaSS.println("AT+CSCLK=0");

  // Wait for FONA to connect to cell network (up to 8 seconds, then watchdog reset).
  Serial.println(F("Checking for network..."));
  Watchdog.reset();
  while (fona.getNetworkStatus() != 1) {
    fonaSS.println("AT+CSCLK=0");
    delay(500);
  }

  // Disable GPRS
  Watchdog.reset();
  Serial.println(F("Disabling GPRS"));
  fona.enableGPRS(false);

  // Wait a little bit
  Watchdog.reset();
  delay(2000);

  // Start the GPRS data connection.
  Serial.println(F("Enabling GPRS"));
  Watchdog.disable();
  while (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to turn GPRS on..."));
    gprsFailures++;

    // Reset everything if too many MQTT connect failures occured in a row.
    if (gprsFailures >= MAX_GPRS_FAILURES) {
      halt(F("Failed to turn GPRS on, resetting..."));
    }
    Serial.println(F("Retrying GPRS in 10 seconds..."));

    delay(10000);  // wait 10 seconds
  }
  Serial.println(F("Connected to Cellular!"));
}

// Serialize the lat, long, altitude to a CSV string that can be published to the specified feed.
void logTracker(float speed, float latitude, float longitude, float altitude, uint16_t vbat) {
  // Initialize a string buffer to hold the data that will be published.

  char sendbuffer[48];
  char *p = sendbuffer;

  // add speed value
  dtostrf(speed, 2, 2, p);
  p += strlen(p);

  // add vbat value
  sprintf (p, ":%u", vbat);
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
  dtostrf(altitude, 2, 2, p);
  p += strlen(p);

  // null terminate
  p[0] = 0;

  // Feeds configuration
  Adafruit_MQTT_Publish publishFeed = Adafruit_MQTT_Publish(&mqtt, AIO_FEED);

  // Finally publish the string to the feed.
  Serial.println(F("Publishing tracker information: "));
  Serial.println(sendbuffer);
  if (!publishFeed.publish(sendbuffer)) {
    Serial.println(F("Publish failed!"));
    txFailures++;
    // Reset everything if too many transmit failures occured in a row.
    if (txFailures >= MAX_TX_FAILURES) {
      halt(F("Connection lost, resetting..."));
    }
  }
  else {
    Serial.println(F("Publish succeeded!"));
    txFailures = 0;
  }
}

void getGPSFix() {
  float latitude, longitude, speed_kph, heading, altitude;
  uint16_t vbat;

  Serial.println(F("Trying to get a GPS fix... "));

  while (!fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude)) { // getGPS will return true for 3D fix
    Serial.println(F("Waiting for FONA GPS 3D fix..."));
    gpsFixFailures++;

    // Reset everything if too many MQTT connect failures occured in a row.
    if (gpsFixFailures >= MAX_GPS_FIX_FAILURES) {
      halt(F("Too many GPS fix failures, resetting..."));
    }
    Serial.println(F("Retrying GPS fix in 10 seconds..."));

    delay(10000);  // wait 10 seconds
  }
  Serial.println(F("FONA GPS 3D fix acquired!"));
  gpsFixFailures = 0;

  // Grab battery reading
  fona.getBattPercent(&vbat);

  // Log the current location to the path feed
  logTracker(speed_kph, latitude, longitude, altitude, vbat);
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

  // Set GPRS network settings.
  fona.setGPRSNetworkSettings(F(FONA_APN));
  // fona.setGPRSNetworkSettings(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD));

  // Wait a little bit to make sure settings are in effect.
  delay(2000);
}

void loop() {
  // Connect to cellular
  cellularConnect();

  // Connect to MQTT server.
  mqttConnect();

  // Use the watchdog to simplify retry logic and make things more robust.
  Watchdog.enable(8000);

  // Enable GPS
  Watchdog.reset();
  fona.enableGPS(true);

  // Wait a little bit to make sure GPS is enabled.
  Watchdog.reset();
  delay(5000);

  // Disabling watchdog
  Watchdog.disable();

  // Grab a GPS reading.
  getGPSFix();

  // Use the watchdog to simplify retry logic and make things more robust.
  Watchdog.enable(8000);

  // Disconnect MQTT connection.
  Watchdog.reset();
  mqtt.disconnect();

  // Disable GPS.
  Watchdog.reset();
  fona.enableGPS(false);

  // Disable GPRS
  Watchdog.reset();
  fona.enableGPRS(false);

  // Put FONA in sleep module
  Watchdog.reset();
  fonaSS.println("AT+CSCLK=1");

  // Disable Watchdog for delay
  Watchdog.disable();

  // Wait 60 seconds
  delay(PUBLISH_INTERVAL * 60000);
}
