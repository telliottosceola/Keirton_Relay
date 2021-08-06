#include <Arduino.h>
#include <ArduinoJson.h>
#include <NCD8Relay.h>

TaskHandle_t backgroundTask;
void backgroundTasks(void* pvParameters);

unsigned long durations[8];
unsigned long durationStarts[8];
unsigned long tempDurations[8];
unsigned long tempDelays[8];

unsigned long delays[8];
unsigned long delayStarts[8];


NCD8Relay relayController;

uint8_t status;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  relayController.setAddress(0,0,0);

  xTaskCreatePinnedToCore(backgroundTasks, "BackGround Tasks", 20000, NULL, 1, &backgroundTask, 1);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(Serial.available()){
    Serial.println("Data received");
    uint8_t buffer[300];
    Serial.readBytesUntil('\n', buffer, sizeof(buffer));
    DynamicJsonBuffer jBuffer;
    JsonObject& root = jBuffer.parseObject(buffer);
    if(root.containsKey("channel")&&root.containsKey("duration")&&root.containsKey("delay")){

      int channel = root["channel"].as<int>();
      if(channel < 1 || channel > 8){
        return;
      }
      unsigned long duration = root["duration"].as<unsigned long>();
      unsigned long delay = root["delay"].as<unsigned long>();

      //Example of command format
      //{"channel":1,"duration":1000,"delay":0}

      if(status & (1<<(channel-1))){
        //Channel is already on and running
        unsigned long remainingDuration = (durationStarts[channel-1]+durations[channel-1])-millis();
        if(delay>remainingDuration){
          tempDurations[channel-1] = duration;
          tempDelays[channel-1] = delay-remainingDuration;
          return;
        }else{
          if(delay + duration > remainingDuration){
            durations[channel-1] = (remainingDuration+duration)-delay;
          }else{
            //remaining duration is greater than new command duration + delay so ignore this command
            return;
          }
        }
      }else{
          delays[channel-1] = delay;
          delayStarts[channel-1] = millis();
          durations[channel-1] = duration;
        // }
      }
    }
  }
}

void backgroundTasks(void* pvParameters){
  unsigned long interval = 5000;
  unsigned long lastReport = 0;
  uint8_t previousBankStatus;
  for(;;){
    status = relayController.readRelayBankStatus();
    if(millis() > interval+lastReport || status != previousBankStatus){
      lastReport = millis();
      previousBankStatus = status;
    }
    for(int i = 0; i < 8; i++){
      // relayController.loop();
      if(delayStarts[i] != 0){
        if(millis() >= delayStarts[i]+delays[i]){
          //delay expired so turn on the relay and clear the delay variables
          relayController.turnOnRelay(i+1);
          durationStarts[i] = millis();
          delayStarts[i] = 0;
          delays[i] = 0;
        }
      }
      if(status & (1<<(i))){
        if(millis() >= durationStarts[i]+durations[i]){
          relayController.turnOffRelay(i+1);
          if(tempDurations[i] != 0){
            durations[i] = tempDurations[i];
            delays[i] = tempDelays[i];
            tempDurations[i] = 0;
            tempDelays[i] = 0
          }else{
            //Duration time has passed so turn off relay
            durationStarts[i] = 0;
            durations[i] = 0;
          }
        }
      }
    }
  }
  vTaskDelete( NULL );
}
