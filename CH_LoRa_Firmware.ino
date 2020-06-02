#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#include <SPI.h>

#define SERVER_ADDRESS 90
#define CLIENT_ADDRESS 91

//hardware defs
#define LED_BUILTIN 13

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 915.0

//messages
//errors
#define ERR_MANAGER_INIT_FAIL "EMIF"
#define ERR_TX_FAIL "ETXF"
#define ERR_SET_FREQ_FAIL "ESFF"
//successes
#define MANAGER_INIT_OK "OMIN"
#define TX_SEND_OK "OTXO"

//communication management mesgs
#define ENQ "ENQ"
#define ACK "ACK"
#define NAK "NAK"

//create the radio driver
//and a manager for it
RH_RF95 driver(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(driver, SERVER_ADDRESS);

//global state variables
int radio_tx_power_level = 0;
bool has_unread_message = false;
String message;
uint8_t from;

//alloate space for messages received
//the RH lib needs these stupid uint8_t types
uint8_t msgbuf[RH_RF95_MAX_MESSAGE_LEN];
char hostbuf[4];
uint8_t hostmsg[RH_RF95_MAX_MESSAGE_LEN];
char cmdbuf[1024];

void setup() {
  //pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  while (!Serial) blink_err();
  //Serial.println("Connected Home LoRa Communication Interface");
  //Serial.println("-------------------------------------------");
  //Serial.println(F("Initializing manager..."));
  delay(250);
  manager.init();
  manager.setTimeout(500); //milliseconds
  /*
    if(!manager.init()) {
    Serial.println(F(ERR_MANAGER_INIT_FAIL));
    } else {
    Serial.println(F(MANAGER_INIT_OK));
    }
  */

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!driver.setFrequency(RF95_FREQ)) {
    //Serial.println(ERR_SET_FREQ_FAIL);
    while (1);
  }
  //Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  //set the driver TX power
  driver.setTxPower(23, false);
  radio_tx_power_level = 23;

  while (Serial.available() < 3);
  char hello[4];
  Serial.readBytes(hello, 3);
  if (strcmp(hello, ENQ) == 0) {
    Serial.print(ACK);
  } else {
    Serial.print(NAK);
    digitalWrite(LED_BUILTIN, HIGH);
    while (1) delay(1000);
  }
}


void loop() {
  //check for received message
  //if(manager.available()) {
    uint8_t len = sizeof(msgbuf);
    //Serial.println("checking for msg");
    if (manager.recvfromAckTimeout(msgbuf, &len, 2000, &from)) {
      has_unread_message = true;
      message = String((char*)msgbuf);
      memset((void*)msgbuf, 0, RH_RF95_MAX_MESSAGE_LEN); //clear the recv buf
    }
  //}
  //check for command from driver and execute
  if (Serial.available() >= 3) {
    Serial.readBytes(cmdbuf, 1023);

    String cmd = String(cmdbuf);
    String type = cmd.substring(0, 3);
    if (type.equals("TXO")) {
      String msg = cmd.substring(4);
      bool sent = sendDataOverRadio(msg);
      //Serial.print("returned");
      if (sent) {
        Serial.print(TX_SEND_OK);
      } else {
        Serial.print(ERR_TX_FAIL);
      }
    } else if (type.equals("AVL")) {
      if (has_unread_message) {
        Serial.print(ACK);
      } else {
        Serial.print(NAK);
      }
    } else if (type.equals("MLN")) {
      Serial.printf("%4d", message.length());
    } else if (type.equals("RRQ")) {
      Serial.print(message.c_str());
      message = "";
      has_unread_message = false;
    } else {
      Serial.print(NAK);
    }

    //clear out the cmd buffer
    memset(cmdbuf, 0, 1024);
  }
}

bool sendDataOverRadio(String msg) {
  char data[msg.length() + 1];
  strcpy(data, msg.c_str());
  //Serial.println("transmitting");
  bool sent = manager.sendtoWait((uint8_t*)data, sizeof(data), CLIENT_ADDRESS);
  //Serial.println("done transmitting");
  return sent == true;
}

void blink_err() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
}
