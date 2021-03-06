// Copyright (c) 2021 SQ9MDD Rysiek Labus
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <Arduino.h>
#include <EEPROM.h>

// inputs and outputs
#define bo1               3                                     // water pump 1
#define bi1               A1                                    // water tank low lvl
#define bi3               4                                     // push button 
#define ai1               A0                                    // soil moisture

#define CHILD_ID_BO1      1                                     // child ID for BO1 sensor pump                            
#define CHILD_ID_BI1      2                                     // child ID for BI1 sensor tank low
#define CHILD_ID_AI1      4                                     // child ID for AI1 sensor moisture mesure percentage
#define CHILD_ID_AO1      5                                     // child ID for AO1 sensor moisture setpoint percentage 0% - 100%

// mysensors config, moved into platformio.ini

boolean bo_state = false;
boolean low_lvl = false;
boolean save_to_flash_flag = false;
int moisture_low_limit = 10;
int moisture_hi_limit = 80;
int moisture = 0;
int moisture_set = 65;                                          // default moisture setpoint
unsigned long bo1_start_time = 0;
unsigned long impulse_ml = 250;                                 // one impulse amount 
unsigned long impulse_time = 38;                                // impulse time
unsigned long last_moisture_send = 0;                           // moisture sending timer
unsigned long last_moisture_read = 0;                           // pomiar co 6 sekund * 10 pomiarów w filtrze daje nam średnia z minuty
unsigned long save_to_flash_after = 0;                          // save settings to flash after x time

// mysensors library
#include <MySensors.h>

MyMessage msgBO1(CHILD_ID_BO1, V_STATUS);
MyMessage msgBI1(CHILD_ID_BI1, V_STATUS);
MyMessage msgAI1(CHILD_ID_AI1, V_LEVEL);
MyMessage msgAO1(CHILD_ID_AO1, V_PERCENTAGE);

void BO_SET() {
  if(low_lvl == false && moisture > moisture_low_limit && moisture < moisture_set){
    bo_state = true; 
    bo1_start_time = millis(); 
    send(msgBO1.set(true,false));
    delay(100);
    digitalWrite(bo1, LOW);
  }else{
    delay(200);
    send(msgBO1.set(false,false));
  }
}

void BO_RESET() {
  bo_state = false;
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
  sprintf(etykieta,"R%02u.AI1",addr);   present(CHILD_ID_AI1, S_MOISTURE, etykieta);   
  sprintf(etykieta,"R%02u.AO1",addr);   present(CHILD_ID_AO1, S_DIMMER, etykieta);
}

void receive(const MyMessage &message){
  int percent = 0;
  switch (message.sensor) {
      case 1:
        if (message.getBool() == true){ 
          BO_SET();
        }else{
          BO_RESET();
        }
      break;
      case 5:
        percent = message.getInt(); 
        moisture_set = constrain(percent,moisture_low_limit,moisture_hi_limit);
        delay(300);
        send(msgAO1.set(moisture_set));
        save_to_flash_after = millis() + 10000;
        save_to_flash_flag = true;
      break;   
  }
}

void setup(){  
  //analogReference(INTERNAL);
  pinMode(bo1,OUTPUT); digitalWrite(bo1, HIGH);
  pinMode(bi1,INPUT_PULLUP);
  pinMode(bi3,INPUT_PULLUP);
  pinMode(ai1,INPUT);

  if(digitalRead(bi1) == LOW){
    low_lvl = true;
  }

  int moisture_percent = map(analogRead(ai1),670,277,0,100);
  moisture = constrain(moisture_percent,0,100);

  EEPROM.get(710, moisture_set);   
  if(isnan(moisture_set) || moisture_set < 0){
    moisture_set = 60;
  }

  send(msgBO1.set(false,1));
  send(msgBI1.set(low_lvl,1));
  send(msgAI1.set(moisture, 0));
  send(msgAO1.set(moisture_set, 0));
}

void loop(){
  // zapis nastawy wilgotności do eeprom
  if(save_to_flash_flag == true && millis() > save_to_flash_after){
    EEPROM.put(710,moisture_set);
    save_to_flash_flag = false;
    send(msgAO1.set(moisture_set));
  }

  // wylaczenie pompki po przepompowaniu dawki
  if(bo_state == true && (millis() - bo1_start_time) >= (impulse_time * impulse_ml)){
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

  // przycisk
  if(digitalRead(bi3) == LOW){
    delay(10);
    if(digitalRead(bi3) == LOW){
      BO_SET();
    }
  }

  //test czujnika wilgotnosci gleby
  if(bo_state == false && (millis() - last_moisture_read) > 1000 ){
    int moisture_percent = map(analogRead(ai1),670,277,0,100);
    moisture_percent = constrain(moisture_percent,0,100);
    moisture = ((moisture * 10) + moisture_percent) / 11; 
    //Serial.println(moisture);
    last_moisture_read = millis();   
  }
  
  // raz na 5 minut wysyłka do domoticza i  wrazie potrzeby podlewanie automatyczne
  if(bo_state == false && (millis() - last_moisture_send) > 300000 ){
    send(msgAI1.set(moisture, 0));
    last_moisture_send = millis(); 
    // w tym miejscu podlewamy jeśli pomiar jest mniejszy niż nastawa %
    if(moisture < moisture_set){
      BO_SET();
    }
  }
}