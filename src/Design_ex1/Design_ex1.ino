#include <string.h>
#include <mpu6050_esp32.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
//define states
const uint8_t ON = 0; //colon on
const uint8_t OFF = 1; //colon off

//for changing-mode button 45
const int BUTTON_mode = 45;
uint8_t prev_button_state = 1;
const uint8_t ZERO = 0;
const uint8_t ONE = 1;
const uint8_t TWO = 2;
const uint8_t THREE = 3;
uint8_t button_state = ZERO;


//for changing-display button 39
const int BUTTON_display = 39;
const uint8_t ALWAYS = 0;
const uint8_t WAIT_ONE = 1;
const uint8_t MOTION = 2;
const uint8_t BLACK = 3;
const uint8_t WAIT_TWO = 4;
uint8_t prev_button = 1;
uint8_t button_display_state = ALWAYS;
int store[7];

//Some constants and some resources:
const int RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
const int GETTING_PERIOD = 2000; //periodicity of getting a number fact.
const int BUTTON_TIMEOUT = 1000; //button timeout in milliseconds
const uint16_t IN_BUFFER_SIZE = 1000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response_buffer[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response

const uint8_t LOOP_PERIOD = 10; //milliseconds
uint32_t primary_timer = 0;

const uint8_t INIT = 0;
const uint8_t PER_MIN = 1;
uint8_t query_state = INIT;

char network[] = "EECS_Labs";
char password[] = "";

uint8_t scanning = 0;//set to 1 if you'd like to scan for wifi networks (see below):
uint8_t channel = 1; //network channel on 2.4 GHz
byte bssid[] = {0x04, 0x95, 0xE6, 0xAE, 0xDB, 0x41}; //6 byte MAC address of AP you're targeting.

// BUTTON 45; //pin connected to button
uint8_t state = 0;  //system state (feel free to use)
uint8_t num_count; //variable for storing the number of times the button has been pressed before timeout
unsigned long timer;  //used for storing millis() readings.
unsigned long timer_display;  //used for storing millis() readings.
const int TIMEOUT = 1000;

//IMU CONST
MPU6050 imu; //imu object called, appropriately, imu
float x, y, z; //variables for grabbing x,y,and z values
float old_acc_mag, older_acc_mag; //maybe use for remembering older values of acceleration magnitude
float acc_mag = 0;  //used for holding the magnitude of acceleration
float avg_acc_mag = 0; //used for holding the running average of acceleration magnitude
const float ZOOM = 9.81; //for display (converts readings into m/s^2)...used for visualizing only

//for indicator
const uint8_t NO_REQ = 0;
const uint8_t REQ = 1;
uint8_t request_indicator = REQ;
uint32_t timer_sec = 0;

void setup() {
  Serial.begin(115200); //begin serial comms

  //IMU SET UP
  delay(50); //pause to make sure comms get set up
  Wire.begin();
  delay(50); //pause to make sure comms get set up
  if (imu.setupIMU(1)) {
    Serial.println("IMU Connected!");
  } else {
    Serial.println("IMU Not Connected :/");
    Serial.println("Restarting");
    ESP.restart(); // restart the ESP (proper way)
  }

  //SET UP SCREEN
  tft.init();  //init screen
  tft.setRotation(2); //adjust rotation
  tft.setTextSize(1); //default font size
  tft.fillScreen(TFT_BLACK); //fill background
  tft.setTextColor(TFT_GREEN, TFT_BLACK); //set color of font to green foreground, black background

  // NETWORK
  if (scanning) {
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
      Serial.println("no networks found");
    } else {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        Serial.printf("%d: %s, Ch:%d (%ddBm) %s ", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "");
        uint8_t* cc = WiFi.BSSID(i);
        for (int k = 0; k < 6; k++) {
          Serial.print(*cc, HEX);
          if (k != 5) Serial.print(":");
          cc++;
        }
        Serial.println("");
      }
    }
  }
  delay(100); //wait a bit (100 ms)
  //if using regular connection use line below:
  WiFi.begin(network, password);
  //if using channel/mac specification for crowded bands use the following:
  //WiFi.begin(network, password, channel, bssid);
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count < 6) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);

  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.printf("%d:%d:%d:%d (%s) (%s)\n", WiFi.localIP()[3], WiFi.localIP()[2],
                  WiFi.localIP()[1], WiFi.localIP()[0],
                  WiFi.macAddress().c_str() , WiFi.SSID().c_str());
    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }
  pinMode(BUTTON_mode, INPUT_PULLUP); //set input pin as an input!
  pinMode(BUTTON_display, INPUT_PULLUP); //has problem
  //  start a timer for a minute
  primary_timer = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
  // submit to function that performs GET.  It will return output using response_buffer char array
  sprintf(request_buffer, "GET http://iesc-s3.mit.edu/esp32test/currenttime HTTP/1.1\r\n");
  strcat(request_buffer, "Host: iesc-s3.mit.edu\r\n"); //add more to the end
  strcat(request_buffer, "\r\n"); //add blank line!
  process_query("iesc-s3.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, false);

  //IMU SETUP HERE
  imu.readAccelData(imu.accelCount);
  x = ZOOM * imu.accelCount[0] * imu.aRes;
  y = ZOOM * imu.accelCount[1] * imu.aRes;
  z = ZOOM * imu.accelCount[2] * imu.aRes;
  acc_mag = sqrt(x * x + y * y + z * z);
  avg_acc_mag = (acc_mag + old_acc_mag + older_acc_mag) / 3; //three way average
  old_acc_mag = acc_mag;
  older_acc_mag = old_acc_mag;

  switch (button_display_state) {
    case ALWAYS:
      //      Serial.println("always on");
      swap_face(digitalRead(BUTTON_mode), response_buffer);
      // if button 39 is pushed from rest, move to wait 1
      if (prev_button == 1 && digitalRead(BUTTON_display) == 0) {
        button_display_state = WAIT_ONE;
        prev_button = 0;
      }
      break;

    case WAIT_ONE:
      Serial.println("waiting state one, going to motion");
      //UPON RELEASE
      if (prev_button == 0 && digitalRead(BUTTON_display) == 1) {
        swap_face(digitalRead(BUTTON_mode), response_buffer);
        button_display_state = MOTION;
        prev_button = 1;
      }
      break;

    case MOTION:
      Serial.println("Motion mode on");
      //while motion persists, display the screen + 15 seconds.
      if (avg_acc_mag >= 14) {

        Serial.println("motion persist");
        // change it to display
        swap_face(digitalRead(BUTTON_mode), response_buffer);
        timer_display = millis();

        //if button on press again
        if (prev_button == 1 && digitalRead(BUTTON_display) == 0) {
          Serial.println("button pressed, about to return ALWAYS mode");
          button_display_state = WAIT_TWO;
          prev_button = 0;
        }

      } else if (millis() - timer_display <= 5000 && avg_acc_mag < 14) {
        Serial.println("experimenting");
        swap_face(digitalRead(BUTTON_mode), response_buffer);

      }
      //otherwise go to black screen
      if (millis() - timer_display > 5000) {
        Serial.println("waiting for 15 secs until go to black screen");

        timer_display = millis();
        button_display_state = BLACK;
        //if button on press again
        if (prev_button == 1 && digitalRead(BUTTON_display) == 0) {
          Serial.println("button pressed, about to return ALWAYS mode");

          button_display_state = WAIT_TWO;
          prev_button = 0;
        }
      }


      break;

    case BLACK:
      tft.fillScreen(TFT_BLACK); //fill background
      if (avg_acc_mag >= 14) {
        Serial.println("at BLACK mode, deteched motion, leaving to motion");
        button_display_state = MOTION;
        if (prev_button == 1 && digitalRead(BUTTON_display) == 0) {
          Serial.println("at BLACK mode, button pressed, about to return ALWAYS mode (a)");
          button_display_state = WAIT_TWO;
          prev_button = 0;
        }
      }
      if (prev_button == 1 && digitalRead(BUTTON_display) == 0) {
        Serial.println("at BLACK mode, button pressed, about to return ALWAYS mode (b)");
        button_display_state = WAIT_TWO;
        prev_button = 0;
      }
      break;


    case WAIT_TWO:
      if (prev_button == 0 && digitalRead(BUTTON_display) == 1) {
        Serial.println("waiting state two, going to ALWAYS");
        button_display_state = ALWAYS;
        prev_button = 1;
      }
      break;

  }


}


void swap_face(uint8_t input, char response_buffer[OUT_BUFFER_SIZE]) {
  switch (button_state) {
    //start displaying
    case ZERO:
      //      make_hour_min(response_buffer);
      hour_min_increment_state_machine(response_buffer);
      if (prev_button_state == 1 && input == 0) { //on press, display another mode
        //pressing
        //1 is rest, 0 is press
        button_state = ONE;
        prev_button_state = 0;
      }
      break;

    case ONE:
      //release
      if (prev_button_state == 0 && input == 1) {
        tft.fillScreen(TFT_BLACK);
//        make_hour_min_sec(response_buffer);
        hour_min_sec_increment_state_machine(response_buffer);
        button_state = TWO;
        prev_button_state = 1;
      }
      break;

    case TWO:
      // pressing
//      make_hour_min_sec(response_buffer);
      hour_min_sec_increment_state_machine(response_buffer);
      if (prev_button_state == 1 && input == 0) {
        button_state = THREE;
        prev_button_state = 0;
      }
      break;

    case THREE:
      //press then rest
      if (prev_button_state == 0 && input == 1) {
        tft.fillScreen(TFT_BLACK);
        //        make_hour_min(response_buffer);
        hour_min_increment_state_machine(response_buffer);
        button_state = ZERO;
        prev_button_state = 1;
      }
      break;
  }

}

/*----------------------------------
  for display HOUR:MIN
*/

//can make a seperate state machine for incrementing time. it should take in response_buffer
void hour_min_increment_state_machine(char response_buffer[OUT_BUFFER_SIZE]) {
  switch (request_indicator) {
    //when there's a GET request, then sync
    //sprintf should take whatever is in store[]
    case REQ:{
      Serial.println("REQ STATE");
      //        int store[7];
      char *pch;
      int ind = 0;
      pch = strtok(response_buffer, " ,.-:");
      while (pch != NULL) {
        store[ind++] = atoi(pch);
        pch = strtok(NULL, " ,.-:");
      }
      //       request_indicator = NO_REQ;
      make_hour_min(store);
      request_indicator = NO_REQ;
      break;}

    //when there's no GET, increment counter and stuff
    //store should be constantly updated per min
    case NO_REQ:{
      Serial.println("NO REQ STATE");
      //store[3] is hour, store[4] is min
      //if store[4] is at 60, minus 60, and store[3] + 1. else increment
      if (millis() - timer_sec >= 60000) {
        timer_sec = millis();
        if (store[4] < 60) {
          store[4]++;
        }
        else if (store[4] >= 60) {
          store[4] -= 60;
          store[3]++;
        }
        
      }
      make_hour_min(store);
      break;}
  }
}

void hour_min_sec_increment_state_machine(char response_buffer[OUT_BUFFER_SIZE]) {
  switch (request_indicator) {
    //when there's a GET request, then sync
    //sprintf should take whatever is in store[]
    case REQ:{
      Serial.println("REQ STATE for hour_min_sec");
      //        int store[7];
      char *pch;
      int ind = 0;
      pch = strtok(response_buffer, " ,.-:");
      while (pch != NULL) {
        store[ind++] = atoi(pch);
        pch = strtok(NULL, " ,.-:");
      }
      make_hour_min_sec(store);
      request_indicator = NO_REQ;
      break;}

    //when there's no GET, increment counter and stuff
    //store should be constantly updated per min
    case NO_REQ:{
      Serial.println("NO REQ STATE for hour_min_sec");
      //store[3] is hour, store[4] is min
      //if store[4] is at 60, minus 60, and store[3] + 1. else increment
      if (millis() - timer_sec > 1000) {
        timer_sec = millis();
        if (store[5] < 59) {
          store[5]++;
        }
        else if (store[5] >= 59) {
          store[5] = 0;
          store[4]++;
        }
        make_hour_min_sec(store);
      }
      break;}
  }
}
//within that you call make_hour_min and make_hour_min_sec, where they take in store?
//void make_hour_min(char response_buffer[OUT_BUFFER_SIZE]) {
void make_hour_min(int* store) {
  Serial.println("prove that this thread is still alive");
  char output[100];
  switch (state) {
    case ON:
      // PM
      if (store[3] > 12 && store[4] < 10) {
        sprintf(output, "%d:\n0%d\nPM", store[3] % 12, store[4]);
      } else if (store[3] > 12 && store[4] >= 10) {
        sprintf(output, "%d:\n%d\nPM", store[3] % 12, store[4]);
      } else if (store[3] == 12 && store[4] >= 10) {
        sprintf(output, "%d:\n%d\nPM", store[3], store[4]);
      } else if (store[3] == 12 && store[4] < 10) {
        sprintf(output, "%d:\n0%d\nPM", store[3], store[4]);
      }
      // AM
      else if (store[3] < 12 && store[4] < 10) {
        sprintf(output, "%d:\n0%d\nAM", store[3], store[4]);
      } else if (store[3] < 12 && store[4] >= 10) {
        sprintf(output, "%d:\n%d\nAM", store[3], store[4]);
      }
      tft.setCursor(4, 4, 4);
      tft.setTextSize(2);
      tft.println(output);

      // for colon heartbeat
      if (millis() - timer > 1000) {
        timer = millis();
        state = OFF;
      }
      break;

    case OFF:
      if (store[3] >= 12 && store[4] < 10) {
        sprintf(output, "%d \n0%d\nPM", store[3] % 12, store[4]);
      } else if (store[3] >= 12 && store[4] >= 10) {
        sprintf(output, "%d \n%d\nPM", store[3] % 12, store[4]);
      } else if (store[3] == 12 && store[4] >= 10) {
        sprintf(output, "%d \n%d\nPM", store[3], store[4]);
      } else if (store[3] == 12 && store[4] < 10) {
        sprintf(output, "%d \n0%d\nPM", store[3], store[4]);
      }
      else if (store[3] < 12 && store[4] < 10) { //AM
        sprintf(output, "%d \n0%d\nAM", store[3], store[4]);
      } else if (store[3] < 12 && store[4] >= 10) {
        sprintf(output, "%d \n%d\nAM", store[3], store[4]);
      }
      tft.setCursor(4, 4, 4);
      tft.setTextSize(2);
      tft.println(output);
      if (millis() - timer > 1000) {
        timer = millis();
        state = ON;
      }
      break;
  }

}

/*----------------------------------
  for display HOUR:MIN:SEC
*/
void make_hour_min_sec(int* store) {
//  int store[7];
  char *pch;
  int ind = 0;
  pch = strtok(response_buffer, " ,.-:");
  while (pch != NULL) {
    store[ind++] = atoi(pch);
    pch = strtok(NULL, " ,.-:");
  }
  char output[100];

  if (store[3] > 12 && store[4] < 10 && store[5] < 10) {
    sprintf(output, "%d:\n0%d:0%d\nPM", store[3] % 12, store[4], store[5]);
  } else if (store[3] > 12 && store[4] >= 10 && store[5] >= 10) {
    sprintf(output, "%d:\n%d:%d\nPM", store[3] % 12, store[4], store[5]);
  } else if (store[3] > 12 && store[4] < 10 && store[5] >= 10) { //AM
    sprintf(output, "%d:\n0%d:%d\nPM", store[3] % 12, store[4], store[5]);
  } else if (store[3] > 12 && store[4] >= 10 && store[5] < 10) {
    sprintf(output, "%d:\n%d:0%d\nPM", store[3] % 12, store[4], store[5]);
  } else if (store[3] < 12 && store[4] < 10 && store[5] < 10) { //AM
    sprintf(output, "%d:\n0%d:0%d\nAM", store[3], store[4], store[5]);
  } else if (store[3] < 12 && store[4] >= 10 && store[5] >= 10) {
    sprintf(output, "%d:\n%d:%d\nAM", store[3], store[4], store[5]);
  } else if (store[3] < 12 && store[4] < 10 && store[5] >= 10) { //AM
    sprintf(output, "%d:\n0%d:%d\nAM", store[3], store[4], store[5]);
  } else if (store[3] < 12 && store[4] >= 10 && store[5] < 10) {
    sprintf(output, "%d:\n%d:0%d\nAM", store[3], store[4], store[5]);
  } else if (store[3] == 12 && store[4] >= 10 && store[5] >= 10) {
    sprintf(output, "%d:\n%d:%d\nPM", store[3], store[4], store[5]);
  } else if (store[3] == 12 && store[4] < 10 && store[5] >= 10) {
    sprintf(output, "%d:\n0%d:%d\nPM", store[3], store[4], store[5]);
  } else if (store[3] == 12 && store[4] >= 10 && store[5] < 10) {
    sprintf(output, "%d:\n%d:0%d\nPM", store[3], store[4], store[5]);
  } else if (store[3] == 12 && store[4] < 10 && store[5] < 10) {
    sprintf(output, "%d:\n0%d:0%d\nPM", store[3], store[4], store[5]);
  }


  //  Serial.println(output);
  tft.setCursor(4, 4, 4);
  tft.setTextSize(2);
  tft.println(output);

}


/*----------------------------------
   char_append Function:
   Arguments:
      char* buff: pointer to character array which we will append a
      char c:
      uint16_t buff_size: size of buffer buff

   Return value:
      boolean: True if character appended, False if not appended (indicating buffer full)
*/
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
  int len = strlen(buff);
  if (len > buff_size) return false;
  buff[len] = c;
  buff[len + 1] = '\0';
  return true;
}

/*----------------------------------
   do_http_GET Function:
   Arguments:
      char* host: null-terminated char-array containing host to connect to
      char* request: null-terminated char-arry containing properly formatted HTTP GET request
      char* response: char-array used as output for function to contain response
      uint16_t response_size: size of response buffer (in bytes)
      uint16_t response_timeout: duration we'll wait (in ms) for a response from server
      uint8_t serial: used for printing debug information to terminal (true prints, false doesn't)
   Return value:
      void (none)
*/
void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial) {
  // first time querying will just do this. and turn into another state that you have to wait
  Serial.println("this is my request");
  WiFiClient client; //instantiate a client object
  if (client.connect(host, 80)) { //try to connect to host on port 80
    if (serial) Serial.print(request);//Can do one-line if statements in C without curly braces
    client.print(request);
    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer
    uint32_t count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      client.readBytesUntil('\n', response, response_size);
      if (serial) Serial.println(response);
      if (strcmp(response, "\r") == 0) { //found a blank line!
        break;
      }
      memset(response, 0, response_size);
      if (millis() - count > response_timeout) break;
    }
    memset(response, 0, response_size);
    count = millis();
    while (client.available()) { //read out remaining text (body of response)
      char_append(response, client.read(), OUT_BUFFER_SIZE);
    }
    if (serial) Serial.println(response);
    client.stop();
    if (serial) Serial.println("-----------");
  } else {
    if (serial) Serial.println("connection failed :/");
    if (serial) Serial.println("wait 0.5 sec...");
    client.stop();
  }

}

void process_query(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial) {
  switch (query_state) {
    case INIT:
      Serial.println("init state making request");
      do_http_GET("iesc-s3.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, false);
      query_state = PER_MIN;
      break;

    case PER_MIN:
      if (millis() - primary_timer > 10000) {
        Serial.println("per_min state making request");
        //wait for primary timer to increment
        primary_timer = millis();
        do_http_GET("iesc-s3.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, false);
        request_indicator = REQ;
      }

      break;
  }
}
