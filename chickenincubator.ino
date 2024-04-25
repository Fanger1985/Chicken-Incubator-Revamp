#include <WiFi.h>
#include <DHT.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Preferences.h>

Preferences preferences; // Preferences object for storing persistent data

// Constants
const char* ssid = "SpectrumSetup-DD";  // your network SSID
const char* password = "jeansrocket543";  // your network password

// DHT11 sensor setup
#define DHTPIN 4  // Change to your preferred GPIO pin
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Web server setup
WebServer server(80);

// Data arrays for the graph
const int maxDataPoints = 100;
float temperatureData[maxDataPoints];
float humidityData[maxDataPoints];
int dataCount = 0;

// Function to read sensor data
void readSensorData() {
  float temperature = dht.readTemperature(true); // Read temperature in Fahrenheit
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor");
    return;
  }

  // Add data to the arrays
  if (dataCount < maxDataPoints) {
    temperatureData[dataCount] = temperature;
    humidityData[dataCount] = humidity;
    dataCount++;
  } else {
    // Shift data to make room for new data points
    for (int i = 0; i < maxDataPoints - 1; i++) {
      temperatureData[i] = temperatureData[i + 1];
      humidityData[i] = humidityData[i + 1];
    }
    temperatureData[maxDataPoints - 1] = temperature;
    humidityData[maxDataPoints - 1] = humidity;
  }
}

// Function to generate the graph data in JSON format
String generateGraphData() {
  DynamicJsonDocument doc(1024);

  JsonArray tempArray = doc.createNestedArray("temperature");
  JsonArray humArray = doc.createNestedArray("humidity");

  for (int i = 0; i < dataCount; i++) {
    tempArray.add(temperatureData[i]);
    humArray.add(humidityData[i]);
  }

  String output;
  serializeJson(doc, output);
  return output;
}

// Function to get the current Unix timestamp
unsigned long getUnixTime() {
  return (unsigned long)time(NULL);
}

int daysSinceStart() {
  unsigned long startDate = preferences.getULong("start_date", 1713160000); // Correct Unix timestamp
  unsigned long currentDate = getUnixTime(); // Current Unix timestamp
  
  unsigned long secondsInDay = 86400; // 24 hours * 60 minutes * 60 seconds
  return (currentDate - startDate) / secondsInDay; // Calculate full days since start
}

void handleRoot() {
  String response = "<html><head>";
  response += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"; // Responsive viewport
  response += "<style>";
  response += "body { background-color: #FFF5B7; font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; overflow: hidden; }"; // No scrollbars
  response += ".container { padding: 3%; margin: auto; border-radius: 10px; box-shadow: 4px 4px 10px #D1D1D1, -4px -4px 10px #FFFFFF; max-width: 90vw; }"; // Reduced padding and margins
  response += "h1 { color: #FFC107; }";
  response += "p { font-size: 3vw; }"; // Smaller font size for better fit
  response += "</style>";
  response += "</head><body>";
  response += "<div class='container'>";
  response += "<h1>Chicken Incubation Monitor - Cheep Cheep</h1>";
  response += "<p id='temperature'>Current Temperature: " + String(dht.readTemperature(true)) + " F</p>"; // Display temperature
  response += "<p id='humidity'>Current Humidity: " + String(dht.readHumidity()) + " %</p>"; // Display humidity
  response += "<p>Started 4/25/2024</p>"; // Display the fixed start date
  response += "<h2>Temperature and Humidity Graph</h2>";
  response += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  response += "<canvas id='tempHumGraph' style='max-width: 90vw; max-height: 50vh;'></canvas>"; // Responsive canvas size

  // JavaScript for fetching sensor data every 60 seconds
  response += "<script>";
  response += "function fetchSensorData() {";
  response += "  fetch('/graphData').then((response) => response.json()).then((data) => {";
  response += "    document.getElementById('temperature').innerText = `Current Temperature: ${data.temperature[data.temperature.length - 1]} F`;";
  response += "    document.getElementById('humidity').innerText = `Current Humidity: ${data.humidity[data.humidity.length - 1]} %`;";
  response += "    const ctx = document.getElementById('tempHumGraph').getContext('2d');";
  response += "    const chartConfig = {";
  response += "      type: 'line',";
  response += "      data: {";
  response += "        labels: data.temperature.map((_, index) => index + 1),"; // Dynamic labels
  response += "        datasets: [";
  response += "          {";
  response += "            label: 'Temperature (°F)',";
  response += "            data: data.temperature,";
  response += "            borderColor: 'red',";
  response += "            fill: false,";
  response += "            tension: 0.4";
  response += "          },";
  response += "          {";
  response += "            label: 'Humidity (%)',";
  response += "            data: data.humidity,";
  response += "            borderColor: 'blue',";
  response += "            fill: false,";
  response += "            tension: 0.4";
  response += "          }";
  response += "        ],";
  response += "      },";
  response += "    };";
  response += "    new Chart(ctx, chartConfig);"; // Initialize the graph
  response += "  }).catch((error) => console.error('Error fetching sensor data:', error));"; // Error handling
  response += "}";
  response += "setInterval(fetchSensorData, 60000);"; // Fetch data every 60 seconds
  response += "fetchSensorData();"; // Initial fetch
  response += "</script>";

  response += "</div>"; 
  response += "</body></html>";

  server.send(200, "text/html", response);
}

// Function to handle the graph data endpoint
void handleGraphData() {
  String graphData = generateGraphData();
  server.send(200, "application/json", graphData);
}

void setup() {
  Serial.begin(115200);
  dht.begin(); // Initialize the DHT sensor
  
  // Initialize Preferences for persistent data
  preferences.begin("incubation", false); // 'false' allows read-write
  
  // Set the start date only if not already set
  if (!preferences.isKey("start_date")) {
    time_t correctStartDate = 1713160000; // Unix timestamp for 4/25/2024 at 1 PM EST
    preferences.putULong("start_date", correctStartDate); // Store the correct start date
    Serial.println("Start date set.");
  }

  // Log the stored start date to ensure it's correct
  unsigned long storedStartDate = preferences.getULong("start_date", 0);
  Serial.print("Stored Start Date (Unix Timestamp): ");
  Serial.println(storedStartDate);
  
  // Connect to WiFi
  WiFi.begin(ssid, password); // Start connecting to the specified network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000); // Wait for connection
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // Output the IP address
  
  // Set up server routes for the web server
  server.on("/", handleRoot); // Main route
  server.on("/graphData", handleGraphData); // Graph data route
  
  server.begin(); // Start the web server
  Serial.println("Server started");
}


// Loop function
void loop() {
  server.handleClient();
  readSensorData();  // Read sensor data periodically
  delay(5000);  // Adjust the delay as needed to control data collection rate
}