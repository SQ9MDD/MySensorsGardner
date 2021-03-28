// Mysensors Gardner Project
// sq9mdd@2021
//
// library
#include <Arduino.h>

// inputs and outputs
#define bo1               3                                     // water pump 1
#define bi1               A1                                    // water tank low lvl
#define bi2               A2                                    // water tank high lvl
#define bi3               4                                     // push button 
#define ai1               A0                                    // soil moisture

#define CHILD_ID_BO1      1                                     // child ID for BO1 sensor                                     
#define CHILD_ID_BI1      2                                     // child ID for BI1 sensor
#define CHILD_ID_BI2      3                                     // child ID for BI2 sensor
#define CHILD_ID_AI1      4                                     // child ID for AI1 sensor

// mysensors config
//#define MY_GATEWAY_SERIAL                                     // Enable serial gateway
//#define MY_REPEATER_FEATURE                                   // Enabled repeater feature for this node
#define MY_DEBUG                                                // Enable debug prints to serial monitor
#define MY_RADIO_NRF24                                          // Enable and select radio type attached
#define MY_RF24_CHANNEL 80                                      // mysensors network channel
#define MY_NODE_ID 21                                            // mysensors node id

boolean bo_state[2] = {false, false};
boolean low_lvl = false;
boolean high_lvl = false;
int moisture = 0;
unsigned long bo1_start_time = 0;
unsigned long impulse_ml = 500;                                 // one impulse amount
unsigned long impulse_time = 38;                                // impulse time
unsigned long last_moisture_send = 0;                           // moisture sending timer
unsigned long last_moisture_read = 0;                           // pomiar co 6 sekund * 10 pomiarów w filtrze daje nam średnia z minuty

// library
#include <MySensors.h>
MyMessage msgBO1(CHILD_ID_BO1, V_STATUS);
MyMessage msgBI1(CHILD_ID_BI1, V_STATUS);
MyMessage msgBI2(CHILD_ID_BI2, V_STATUS);
MyMessage msgAI1(CHILD_ID_AI1, V_LEVEL);

void BO_SET() {
  bo_state[0] = true; 
  bo1_start_time = millis(); 
  send(msgBO1.set(true,false));
  delay(100);
  digitalWrite(bo1, LOW);
}

void BO_RESET() {
  bo_state[0] = false;
  digitalWrite(bo1, HIGH);
  delay(100);
  send(msgBO1.set(false,false));
}

void presentation(){   
  sendSketchInfo("AS-Gardner", "1.2");
  char etykieta[] = "       ";
  int addr = MY_NODE_ID;  
  sprintf(etykieta,"R%02u.BO1",addr);   present(CHILD_ID_BO1, S_BINARY, etykieta); 
  sprintf(etykieta,"R%02u.BI1L",addr);  present(CHILD_ID_BI1, S_BINARY, etykieta);
  sprintf(etykieta,"R%02u.BI2H",addr);  present(CHILD_ID_BI2, S_BINARY, etykieta); 
  sprintf(etykieta,"R%02u.AI1",addr);   present(CHILD_ID_AI1, S_MOISTURE, etykieta);   
}

void receive(const MyMessage &message){
  switch (message.sensor) {
      case 1:
        if (message.getBool() == true && low_lvl == false){ 
          BO_SET();
        }else{
          BO_RESET();
        }
      break;   
  }
}

void setup(){  
  //analogReference(INTERNAL);
  pinMode(bo1,OUTPUT); digitalWrite(bo1, HIGH);
  pinMode(bi1,INPUT_PULLUP);
  pinMode(bi2,INPUT_PULLUP);
  pinMode(bi3,INPUT_PULLUP);
  pinMode(ai1,INPUT);

  if(digitalRead(bi1) == HIGH){low_lvl == false;}else{low_lvl = true;}
  if(digitalRead(bi2) == HIGH){high_lvl == false;}else{high_lvl = true;}

  int moisture_percent = map(analogRead(ai1),70,1023,100,0);
  moisture = constrain(moisture_percent,0,100);

  send(msgBO1.set(false,1));
  send(msgBI1.set(low_lvl,1));
  send(msgBI2.set(high_lvl,1));
  send(msgAI1.set(moisture, 0));
}

void loop(){
  if(bo_state[0] == true && (millis() - bo1_start_time) >= (impulse_time * impulse_ml)){
    BO_RESET();
  }
  
  // test plywaka dolnego
  if(digitalRead(bi1) != low_lvl){
    delay(10);
    if(digitalRead(bi1) != low_lvl){
      low_lvl = digitalRead(bi1); 
      boolean low_lvl_i = !low_lvl;
      send(msgBI1.set(low_lvl_i,1));
    }
  }

  // test plywaka gornego
  if(digitalRead(bi2) != high_lvl){
    delay(10);
    if(digitalRead(bi2) != high_lvl){
      high_lvl = digitalRead(bi2); 
      boolean high_lvl_i = !high_lvl;
      send(msgBI2.set(high_lvl_i,1));
    }
  }

    // przycisk
  if(digitalRead(bi3) == LOW && low_lvl == false){
    delay(10);
    if(digitalRead(bi3) == LOW && low_lvl == false){
      BO_SET();
    }
  }

  //test czujnika wilgotnosci gleby
  if(bo_state[0] == false && (millis() - last_moisture_read) > 6000 ){
    int moisture_percent = map(analogRead(ai1),670,277,0,100);
    moisture_percent = constrain(moisture_percent,0,100);
    moisture = ((moisture * 10) + moisture_percent) / 11; 
    //Serial.println(moisture);
    last_moisture_read = millis();   
  }
  
  // raz na 5 minut wysyłka do domoticza
  if(bo_state[0] == false && (millis() - last_moisture_send) > 300000 ){
    send(msgAI1.set(moisture, 0));
    last_moisture_send = millis(); 
  }
}