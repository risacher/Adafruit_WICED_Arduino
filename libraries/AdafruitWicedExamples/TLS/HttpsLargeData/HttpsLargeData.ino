/*********************************************************************
 This is an example for our Feather WIFI modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example connects to a secure HTTP server and requests a large
 * text file (10KB, 100KB, 1MB). The sketch also computes the
 * checksum of the content to optionally compare with one calculated via
 * another python tool provided in the same folder as this code.
 *
 * To run this sketch:
 *
 * 1. Change SSID/Pass
 * 2. Choose the file to download by changing FILE_ID
 * 3. Compile and run the sketch
 *
 * */

#include <adafruit_feather.h>
#include <adafruit_http.h>
#include <adafruit_crc32.h>

#define WLAN_SSID             "yourSSID"
#define WLAN_PASS             "yourPassword"

#define SERVER                "adafruit-download.s3.amazonaws.com"
#define HTTPS_PORT            443

int ledPin = PA15;

// Change the FILE_ID to indicate the file to
// load from the Amazon S3 Server
#define FILE_ID    1

// S3 server files
const char * file_arr[] =
{
    [0] = "/text_10KB.txt"  ,
    [1] = "/text_100KB.txt" ,
    [2] = "/text_1MB.txt"   ,
};

const char * url = file_arr[FILE_ID];

// Use the HTTP class
AdafruitHTTP http;

// Use CRC32 class to compute checksum
AdafruitCRC32 crc32;

uint32_t datacount = 0;
int      time_start;
int      time_duration;

/**************************************************************************/
/*!
    @brief  TCP/HTTP received callback
*/
/**************************************************************************/
void receive_callback(void)
{
  // skip if byte is not available yet
  if ( !http.available() ) return;

  // HTTP Reponse header is not parsed
  // respParseHeader() will detect and save Status & Content-Length
  if ( http.respStatus() == 0 )
  {
    http.respParseHeader();
  }

  if (http.respStatus() == HTTP_STATUS_RESPONSE_OK)
  {
    while( http.available() && (datacount < http.respContentLength()) )
    {
      int c = http.read();
      datacount++;
      crc32.compute((char)c);
    }

    // all data is received, disconnect right away to compute speed
    if (datacount >= http.respContentLength())
    {
      time_duration = millis() - time_start;
      disconnect_server();
    }
  }
}

/**************************************************************************/
/*!
    @brief  TCP/HTTP disconnect callback
*/
/**************************************************************************/
void disconnect_server(void)
{
  Serial.println("Total bytes received (including headers):");
  Serial.printf(" - %d bytes in %.02f seconds\r\n", http.byteRead(), time_duration/1000.0F);
  Serial.printf(" - Speed ~ %.02f KB/s", ((float) http.byteRead()) / time_duration );
  Serial.println();

  Serial.print("Total data count: ");
  Serial.print( datacount );
  Serial.print(" CRC32: ");
  Serial.println( crc32.crc );

  http.stop();

  Serial.println();
  Serial.println("------------");
  Serial.println("DISCONNECTED");
  Serial.println("------------");
  Serial.println();

}

/**************************************************************************/
/*!
    @brief  The setup function runs once when reset the board
*/
/**************************************************************************/
void setup()
{
  Serial.begin(115200);

  // Wait for the Serial Monitor to open
  while (!Serial)
  {
    /* Delay required to avoid RTOS task switching problems */
    delay(1);
  }

  Serial.println("HTTPS Large Data Example\r\n");

  // Print all software versions
  Feather.printVersions();

  while ( !connectAP() )
  {
    delay(500); // delay between each attempt
  }

  // Connected: Print network info
  Feather.printNetwork();

  // Tell the HTTP client to auto print error codes and halt on errors
  http.err_actions(true, true);

  // Set up callbacks
  http.setReceivedCallback(receive_callback);

  // Start a secure connection
  Serial.printf("Connecting to '%s' port %d ... ", SERVER, HTTPS_PORT );
  http.connectSSL(SERVER, HTTPS_PORT); // Will halt if an error occurs
  Serial.println("OK");

  // Make a HTTP request
  http.addHeader("Accept", "text/html");
  http.addHeader("Connection", "keep-alive");

  Serial.printf("Requesting '%s' ... ", url);
  http.get(SERVER, url); // Will halt if an error occurs
  Serial.println("OK");

  time_start = millis();
}

/**************************************************************************/
/*!
    @brief  The loop function runs over and over again forever
*/
/**************************************************************************/
void loop()
{
  togglePin(ledPin);
  delay(250);
}

/**************************************************************************/
/*!
    @brief  Connect to defined Access Point
*/
/**************************************************************************/
bool connectAP(void)
{
  // Attempt to connect to an AP
  Serial.print("Please wait while connecting to: '" WLAN_SSID "' ... ");

  if ( Feather.connect(WLAN_SSID, WLAN_PASS) )
  {
    Serial.println("Connected!");
  }
  else
  {
    Serial.printf("Failed! %s (%d)", Feather.errstr(), Feather.errnum());
    Serial.println();
  }
  Serial.println();

  return Feather.connected();
}
