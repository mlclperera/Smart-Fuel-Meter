#include <ESP8266WiFi.h>
#include <OneWire.h>

const char* ssid = "SltWiFi";  //192.168.1.19 - Home    
const char* password = "cHlaK@#1";  

OneWire  ds(0); // D3
WiFiServer server(80);

#define TRIG_PIN 5 //RX - D1
#define ECHO_PIN 4 // TX - D2

int distance = 0;
float liter = 0;
const float pi = 3.14;
const float r = 28.5;
const long vol = 204140; // Total Volume when barrel in filled (in Cubic Centimerets)

void setup()
{
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.begin(115200);
  delay(10);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.print("Use this URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void loop()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  const unsigned long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration / 29 / 2;
  distance = distance - 20;
  liter = pi * r * r * distance;

  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  while (!client.available()) {
    delay(1);
  }

  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  //now the DS18b20
  if ( !ds.search(addr))
  {
    ds.reset_search();
    delay(250);
    return;
  }

  if (OneWire::crc8(addr, 7) != addr[7])
  {
    Serial.println("CRC is not valid!");
    return;
  }

  // the first ROM byte indicates which chip
  switch (addr[0])
  {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  //delay(1000);
  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  // Temperature Monitoring Starts
  for ( i = 0; i < 9; i++)
  {
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10)
    {
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  }

  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;

  int volume = (22 / 7) * 0.57 * 0.57 * (89 - distance) / 100 * 1000; // V = pi*r*r*h

  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<meta http-equiv=\"refresh\" content=\"5\">"); //refresh every 5 seconds
  client.println("<title>Smart Fuel Meter</title>");
  client.println("<h1 style='font-size: 40px;color: #3498DB;font-style: italic;'>Smart Fuel Meter</h1>");
  client.println("<h3 style='color:saddlebrown; font-weight: bold;'>Temperature of the Tank</h3>");
  client.print("Temperature in Celsius = ");
  client.println(celsius);
  client.println("<br><br>");
  client.print("Temperature in Fahrenheit = ");
  client.println(fahrenheit);
  client.println("<br><br>");

  if (celsius >= 25 && celsius <= 30) {
    client.println("<img src=\"https://www.clipartmax.com/png/middle/163-1630195_cold-low-temperature-thermometer-weather-icon-icon-cold-thermometer.png\" alt=\"Temperature Gauge 01\" height=\"100\" width=\"100\">");

  } else if (celsius >= 30 && celsius <= 35) {
    client.println("<img src=\"https://www.freeiconspng.com/thumbs/temperature-icon-png/temperature-icon-png-25.png\" alt=\"Temperature Gauge 02\" height=\"100\" width=\"100\">");

  } else if (celsius >= 35) {
    client.println("<img src=\"https://media.istockphoto.com/vectors/fever-sad-face-thermometer-at-high-temperature-thin-line-icon-health-vector-id1281707302\" alt=\"Temperature Gauge 03\" height=\"100\" width=\"100\">");
  }
  client.println("</html>");
  // End of Temperature monitoring

  // Getting current volume of tank
  double remain;
  remain = vol - liter;
  client.println("<h3 style='color:saddlebrown; font-weight: bold;'>Remaining Fuel of the Tank</h3>");
  client.print("Remaining Volume of Tank (ccc) : ");
  client.println(remain);
  client.println("<br><br>");

  if (remain >= 163592 && vol >= remain) { // 100% - Full
    client.println("<img src=\"https://image.similarpng.com/very-thumbnail/2020/04/battery-100-charging-load-status-png.png\" alt=\"Temperature Gauge 01\" height=\"100\" width=\"100\">");
//    Serial.print("Remaining volume (ccc) : ");
//    Serial.println(remain);

  } else if (remain <= 163592 && remain >= 122484) { // 80%
    client.println("<img src=\"https://image.similarpng.com/very-thumbnail/2020/04/battery-80-charging-load-status-png.png\" alt=\"Temperature Gauge 02\" height=\"100\" width=\"100\">");
//    Serial.print("Remaining volume (ccc) : ");
//    Serial.println(remain);

  } else if (remain <= 122484 && remain >= 81656) { // 60%
    client.println("<img src=\"https://image.similarpng.com/very-thumbnail/2020/04/battery-60-charging-load-status-png.png\" alt=\"Temperature Gauge 03\" height=\"100\" width=\"100\">");
//    Serial.print("Remaining volume (ccc) : ");
//    Serial.println(remain);

  } else if (remain <= 81656 && remain >= 40828) { // 40%
    client.println("<img src=\"https://image.similarpng.com/very-thumbnail/2020/04/battery-40-charging-load-status-png.png\" alt=\"Temperature Gauge 04\" height=\"100\" width=\"100\">");
    client.println("<h4 style='color:#ff0000; font-weight: bold;'>Need to Fill Your Tank </h3>");
//    Serial.print("Remaining volume (ccc) : ");
//    Serial.println(remain);

  } else if (remain <= 40828 && remain >= 20424) { // 20%
    client.println("<img src=\"https://image.similarpng.com/very-thumbnail/2020/04/battery-20-charging-load-status-png.png\" alt=\"Temperature Gauge 05\" height=\"100\" width=\"100\">");
    client.println("<h4 style='color:#ff0000; font-weight: bold;'>Tank is going to Empty </h3>");
//    Serial.print("Remaining volume (ccc) : ");
//    Serial.println(remain);

  } else if (remain <= 20424 && remain >= 0) { // 0% - Empty
    client.println("<img src=\"https://image.similarpng.com/thumbnail/2021/06/Battery-Charging-icon-on-transparent-background-PNG.png\" alt=\"Temperature Gauge 06\" height=\"100\" width=\"100\">");
    client.println("<h4 style='color:#ff0000; font-weight: bold;'>Tank is Empty</h3>");
//    Serial.print("Remaining volume (ccc) : ");
//    Serial.println(remain);

  }
  else {
    client.println("<img src=\"https://www.pngfind.com/pngs/m/637-6375883_how-to-fix-a-404-error-404-error.png\" alt=\"Temperature Gauge 10\" height=\"100\" width=\"100\">");
    client.println("<h4 style='color:#ff0000; font-weight: bold;'>Undefined Volume Calculation</h3>");
//    Serial.print("Remaining volume (ccc) : ");
//    Serial.println(remain);
  }

  client.println("</html>");

  delay(1);
  Serial.println("Client disconnected");
  Serial.println("");
}
