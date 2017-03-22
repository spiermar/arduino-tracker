// GPS Tracker project with Adafruit IO
// Author: Martin Spier

#include "Arduino_Tracker.h"

// Alarm pins
const int ledPin = LEAD_PIN;

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// the logging file
File logfile;

// FONA instance & configuration
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);     // FONA software serial connection.
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);                 // FONA library connection.

uint8_t txFailures = 0;                                       // Count of how many publish failures have occured in a row.
uint8_t gpsFixFailures = 0;                                   // Count of how many GPS fix failures have occured in a row.
uint8_t gprsFailures = 0;                                     // Count of how many GPS fix failures have occured in a row.
uint8_t loopFailures = 0;                                     // Count of how many general failures have occured in a row.

float latitude, longitude, speed_kph, heading, altitude;
uint8_t year, month, date, hr, min, sec;
uint16_t vbat;

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

int8_t cellularConnect() {
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

    // Reset everything if too many GPRS connect failures occured in a row.
    if (gprsFailures >= MAX_GPRS_FAILURES) {
      Serial.println(F("Failed to turn GPRS on, aborting..."));
      return -1;
    }
    Serial.println(F("Retrying GPRS in 10 seconds..."));

    delay(10000);  // wait 10 seconds
  }
  Serial.println(F("Connected to Cellular!"));

  return 0;
}

void sdLog() {
  // initialize the SD card
  Serial.println(F("Initializing SD card..."));

  // see if the card is present and can be initialized
  if (!SD.begin(chipSelect)) {
    Serial.println(F("Card failed, or not present. Aborting."));
    // don't do anything else
    return;
  }

  Serial.println(F("SD card initialized."));

  // make a string for assembling the data to log:
  char sendbuffer[64];
  char *p = sendbuffer;

  // add date and time
  sprintf (p, "%u-%u-%u %u:%u:%u", year, month, date, hr, min, sec);
  p += strlen(p);
  p[0] = ','; p++;

  // add speed value
  dtostrf(speed_kph, 2, 2, p);
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

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  logfile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (logfile) {
    logfile.println(sendbuffer);
    logfile.close();
    // print to the serial port too:
    Serial.println(sendbuffer);
  } else { // if the file isn't open, pop up an error
    Serial.println(F("Error opening datalog.txt."));
  }

  return;
}

int8_t getGPSFix() {
  Serial.println(F("Trying to get a GPS fix... "));

  while (!fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude)) { // getGPS will return true for 3D fix
    Serial.println(F("Waiting for FONA GPS 3D fix..."));
    gpsFixFailures++;

    // Reset everything if too many GPS fix failures occured in a row.
    if (gpsFixFailures >= MAX_GPS_FIX_FAILURES) {
      Serial.println(F("Too many GPS fix failures, aborting..."));
      return -1;
    }
    Serial.println(F("Retrying GPS fix in 10 seconds..."));

    delay(10000);  // wait 10 seconds
  }
  Serial.println(F("FONA GPS 3D fix acquired!"));
  gpsFixFailures = 0;

  // Grab battery reading
  fona.getBattPercent(&vbat);

  // Grab time
  fona.readRTC(&year, &month, &date, &hr, &min, &sec);

  return 0;
}

void setup() {

  // Initialize serial output.
  Serial.begin(115200);
  Serial.println(F("Adafruit IO & FONA808 Tracker"));

  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(chipSelect, OUTPUT);

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
  if (!(getGPSFix() < 0)) {
    // Log to SD card
    sdLog();
    // Connect to cellular
    if (!(cellularConnect() < 0)) {
        loopFailures = 0;
    } else {
      loopFailures++;
    }
  } else {
    loopFailures++;
  }

  // Use the watchdog to simplify retry logic and make things more robust.
  Watchdog.enable(8000);

  // Disable GPS.
  // TODO: Check if it was really disabled
  Watchdog.reset();
  fona.enableGPS(false);

  // Disable GPRS
  // TODO: Check if it was really disabled
  Watchdog.reset();
  fona.enableGPRS(false);

  // Put FONA in sleep module
  // TODO: Wait for Ok and retry
  Watchdog.reset();
  fonaSS.println("AT+CSCLK=1");

  // Disable Watchdog for delay
  Watchdog.disable();

  if (loopFailures >= MAX_LOOP_FAILURES) {
    halt(F("Too many failures, resetting..."));
  }

  // Wait 60 seconds
  delay(PUBLISH_INTERVAL * 60000);
}
