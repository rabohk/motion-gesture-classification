#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include<Wire.h>
#include <Ticker.h>

//web page
char webpage[] PROGMEM = R"=====(
<html>
<!-- Adding a data chart using Chart.js -->
<head>
  <script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.5.0/Chart.min.js'></script>
</head>
<body onload="javascript:init()">
<!-- Adding a slider for controlling data rate -->
<div>
  <input type="range" min="100" max="2000" value="500" id="dataRateSlider" oninput="sendDataRate()" />
  <label for="dataRateSlider" id="dataRateLabel">Rate: 0.2Hz</label>
</div>
<hr />
<div>
  <canvas id="line-chart" width="800" height="450"></canvas>
</div>
<!-- Adding a websocket to the client (webpage) -->
<script>
  var webSocket, dataPlot;
  var maxDataPoints = 100;
  function removeData(){
    dataPlot.data.labels.shift();
    dataPlot.data.datasets[0].data.shift();
    dataPlot.data.datasets[1].data.shift();
    dataPlot.data.datasets[2].data.shift();
  }
  function addData(label, data) {
    if(dataPlot.data.labels.length > maxDataPoints) removeData();
    dataPlot.data.labels.push(label);
    dataPlot.data.datasets[0].data.push(data.valuex);
    dataPlot.data.datasets[1].data.push(data.valuey);
    dataPlot.data.datasets[2].data.push(data.valuez);
    dataPlot.update();
  }
  function init() {
    webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');
    dataPlot = new Chart(document.getElementById("line-chart"), {
      type: 'line',
      data: {
        labels: [],
        datasets: [{
          data: [],
          label: "X",
          borderColor: "#3e95cd",
          fill: false
        },
        {
          data: [],
          label: "Y",
          borderColor: "#cd3e5b",
          fill: false
        },
        {
          data: [],
          label: "Z",
          borderColor: "#3ecd45",
          fill: false
        }]
      }
    });
    webSocket.onmessage = function(event) {
      var data = JSON.parse(event.data);
      console.log(event.data);
      var today = new Date();
      var t = today.getMinutes() + ":" + today.getSeconds();
      addData(t, data);
    }
  }
  function sendDataRate(){
    var dataRate = document.getElementById("dataRateSlider").value;
    webSocket.send(dataRate);
    dataRate = 1.0/(dataRate/1000.0);
    document.getElementById("dataRateLabel").innerHTML = "Rate: " + dataRate.toFixed(2) + "Hz";
  }
</script>
</body>
</html>
)=====";


Ticker timer;

char * ssid = "Mars"; // Put your WiFi SSID here
char * pass = "fatima1972!"; // Put your Wifi Password here

double t1[25][3]={
  {336.17, 1.72, 273.89},
{337.27, 1.97, 274.69},
{338.42, 3.01, 277.57},
{334.16, 356.21, 262.21},
{332.28, 350.72, 252.73},
{312.74, 338.88, 250.36},
{35.44, 58.06, 23.93},
{5.05, 0.09, 88.93},
{4.87, 343.57, 163.88},
{358.11, 327.39, 182.95},
{333.88, 310.91, 203.02},
{310.42, 300.95, 215.15},
{304.87, 306.65, 226.87},
{270.78, 280.86, 265.95},
{307.08, 79.86, 346.68},
{358.68, 60.92, 359.27},
{33.15, 41.85, 36.10},
{15.19, 9.72, 57.75},
{9.79, 357.51, 104.12},
{358.38, 334.68, 183.43},
{350.18, 328.28, 195.64},
{349.04, 345.73, 217.29},
{353.19, 354.80, 232.66},
{355.84, 358.76, 253.41}},t2[25][3]={0};
int MAX_t2 = 25;

const int MPU_addr = 0x68; // I2C address of the MPU-6050
// angles
double x; double y; double z;

boolean recording=true;
int stR=0;
int lastButtonState = LOW;
int buttonState;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; 

// running web server
ESP8266WebServer server;

// Adding a websocket to the server
WebSocketsServer webSocket = WebSocketsServer(81);


boolean wifiConnected = false;
//int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
uint32_t counter;

int minVal=265; int maxVal=402;

struct MPUtelemetry {
  char magic[5];
  unsigned long counter;
  unsigned long ms;
  int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
};

MPUtelemetry tm = {"tele",0,0,0,0,0,0,0,0};

void setup() {
  //client.wifiConnection(WIFISSID, PASSWORD);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D5, INPUT_PULLUP);

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0); // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);

  Serial.begin(115200);

  Serial.print("Packet size: ");
  Serial.println(sizeof(MPUtelemetry));

// Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected ");
  Serial.println(WiFi.localIP());

  server.on("/",[](){
    server.send_P(200,"text/html",webpage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  timer.attach(0.1,getData);
  counter = 0;
}

void loop() {
  if(recording==true && stR>=25){
    recording=false;
    Serial.print("\n\nRecording done: ");
    Serial.println(recording);
    stR=0;
  }

  boolean reading = digitalRead(D5);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) {
        recording = true;
        Serial.print("\n\nRecording: ");
        Serial.println(recording);
      }
    }
  }
  lastButtonState = reading;

  //**************************************************************************
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true); // request a total of 14 registers
  tm.AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  tm.AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  tm.AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  tm.Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  tm.GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  tm.GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  tm.GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  tm.counter = counter;
  counter++;

  int xAng = map(tm.AcX,minVal,maxVal,-90,90);
  int yAng = map(tm.AcY,minVal,maxVal,-90,90);
  int zAng = map(tm.AcZ,minVal,maxVal,-90,90);

  x= RAD_TO_DEG * (atan2(-yAng, -zAng)+PI);
  y= RAD_TO_DEG * (atan2(-xAng, -zAng)+PI);
  z= RAD_TO_DEG * (atan2(-yAng, -xAng)+PI);

  if(recording){
//    t1[stR][0]=x;
//    t1[stR][1]=y;
//    t1[stR][2]=z;
//
    Serial.print(""); Serial.print(x);
    Serial.print(", "); Serial.print(y);
    Serial.print(", "); Serial.println(z); 
    stR++;
  }else{
//    Serial.print("= ");
//    for(int i=1;i<MAX_t2-1;i++){
//      t2[i][0]=t2[i-1][0];
//      t2[i][1]=t2[i-1][1];
//      t2[i][2]=t2[i-1][2];
//    }
//    t2[MAX_t2-1][0]=x;
//    t2[MAX_t2-1][1]=y;
//    t2[MAX_t2-1][2]=z;
//    Serial.println(dwt());
  }

  tm.ms = millis();

  webSocket.loop();
  server.handleClient();
  delay(100);
}

void getData() {
//  Serial.println(bmp.readTemperature());
  String json = "{\"valuex\":";
  json += x;//val
  json += ",\"valuey\":";
  json += y;//val
  json += ",\"valuez\":";
  json += z;//val
  json += "}";
  webSocket.broadcastTXT(json.c_str(), json.length());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
//  if(type == WStype_TEXT){
//    float dataRate = (float) atof((const char *) &payload[0]);
//    timer.detach();
//    timer.attach(dataRate, getData);
//  }
}

int dwt(){
  int t1_size = sizeof(t1)/sizeof(t1[0]);
  int t2_size = sizeof(t2)/sizeof(t2[0]);
  double dpx[t1_size + 1][t2_size + 1];
  double dpy[t1_size + 1][t2_size + 1];
  double dpz[t1_size + 1][t2_size + 1];
  
  for (int i = 0; i < t1_size + 1; i++) {
    for (int j = 0; j < t2_size + 1; j++) {
      dpx[i][j] = 9999.0;
      dpy[i][j] = 9999.0;
      dpz[i][j] = 9999.0;
    }
  }

  dpx[0][0] = 0;
  dpy[0][0] = 0;
  dpz[0][0] = 0;

  for (int i = 1; i < t1_size + 1; i++) {
    for (int j = 1; j < t2_size + 1; j++) {
      dpx[i][j] = abs(t1[i - 1][0] - t2[j - 1][0])
          + min(dpx[i - 1][j], min(dpx[i - 1][j - 1], dpx[i][j - 1]));
      dpy[i][j] = abs(t1[i - 1][1] - t2[j - 1][1])
          + min(dpy[i - 1][j], min(dpy[i - 1][j - 1], dpy[i][j - 1]));
      dpz[i][j] = abs(t1[i - 1][2] - t2[j - 1][2])
          + min(dpz[i - 1][j], min(dpz[i - 1][j - 1], dpz[i][j - 1]));
       Serial.print("val: "); Serial.print(dpx[i][j]);
      Serial.print(", "); Serial.print(dpy[i][j]);
      Serial.print(", "); Serial.println(dpz[i][j]); 
    }
  }

  Serial.print("val: "); Serial.print(dpx[t1_size][t2_size]);
  Serial.print(", "); Serial.print(dpy[t1_size][t2_size]);
  Serial.print(", "); Serial.println(dpz[t1_size][t2_size]); 


  return (int)(dpx[t1_size][t2_size]+dpy[t1_size][t2_size]+dpz[t1_size][t2_size]);
}
