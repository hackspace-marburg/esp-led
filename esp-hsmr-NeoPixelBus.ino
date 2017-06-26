// NeoPixelCylon
// This example will move a Cylon Red Eye back and forth across the 
// the full collection of pixels on the strip.
// 
// This will demonstrate the use of the NeoEase animation ease methods; that provide
// simulated acceleration to the animations.
//
//

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>


const char* wlan_ssid = "";
const char* wlan_password = "";

const uint16_t PixelCount = 3*49; // make sure to set this to the number of pixels in your strip
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
RgbColor CylonEyeColor = HtmlColor(0x7f0000);

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
// for esp8266 omit the pin
//NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount);

NeoPixelAnimator animations(2); // only ever need 2 animations

uint16_t lastPixel = 0; // track the eye position
int8_t moveDir = 1; // track the direction of movement
int mil;  // millisecond timer for scheduling the api query

// uncomment one of the lines below to see the effects of
// changing the ease function on the movement animation
AnimEaseFunction moveEase =
//      NeoEase::Linear;
//      NeoEase::QuadraticInOut;
//      NeoEase::CubicInOut;
        NeoEase::QuarticInOut;
//      NeoEase::QuinticInOut;
//      NeoEase::SinusoidalInOut;
//      NeoEase::ExponentialInOut;
//      NeoEase::CircularInOut;

void FadeAll(uint8_t darkenBy)
{
    RgbColor color;
    for (uint16_t indexPixel = 0; indexPixel < strip.PixelCount(); indexPixel++)
    {
        color = strip.GetPixelColor(indexPixel);
        color.Darken(darkenBy);
        strip.SetPixelColor(indexPixel, color);
    }
}

void FadeAnimUpdate(const AnimationParam& param)
{
    if (param.state == AnimationState_Completed)
    {
        FadeAll(10);
        animations.RestartAnimation(param.index);
    }
}

void MoveAnimUpdate(const AnimationParam& param)
{
    // apply the movement animation curve
    float progress = moveEase(param.progress);

    // use the curved progress to calculate the pixel to effect
    uint16_t nextPixel;
    if (moveDir > 0)
    {
        nextPixel = progress * PixelCount;
    }
    else
    {
        nextPixel = (1.0f - progress) * PixelCount;
    }

    // if progress moves fast enough, we may move more than
    // one pixel, so we update all between the calculated and
    // the last
    if (lastPixel != nextPixel)
    {
        for (uint16_t i = lastPixel + moveDir; i != nextPixel; i += moveDir)
        {
            strip.SetPixelColor(i, CylonEyeColor);
        }
    }
    strip.SetPixelColor(nextPixel, CylonEyeColor);

    lastPixel = nextPixel;

    if (param.state == AnimationState_Completed){
        // reverse direction of movement
        moveDir *= -1;

        // done, time to restart this position tracking animation/timer
        animations.RestartAnimation(param.index);
    }
}

void SetupAnimations() {
    // fade all pixels providing a tail that is longer the faster
    // the pixel moves.
    animations.StartAnimation(0, 5, FadeAnimUpdate);

    // take several seconds to move eye fron one side to the other
    animations.StartAnimation(1, 2000, MoveAnimUpdate);
}

void setup() {
    // fire up wifi and query space api
    Serial.begin(115200);
    
    strip.Begin();
    strip.Show();

    Serial.println();
    Serial.println("Running...");
    
    WiFi.begin(wlan_ssid, wlan_password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    SetupAnimations();

    if(getSpaceApi()){
      CylonEyeColor = HtmlColor(0x669900);
    }else{
      CylonEyeColor = HtmlColor(0xCC0033);
    }

    //WiFi.forceSleepBegin();
}

void loop(){
    // this is all that is needed to keep it running
    // and avoiding using delay() is always a good thing for
    // any timing related routines

    // query the server every minute
    if(millis() - mil >= 1000 * 60){
      //WiFi.forceSleepWake();
      if(getSpaceApi()){
        CylonEyeColor = HtmlColor(0x669900);
      }else{
        CylonEyeColor = HtmlColor(0xCC0033);
      }
      //WiFi.forceSleepBegin();
      
      // query every minute
      mil = millis();
    }
    if (millis() - mil < 0) {
      // should overflow after 50 d
      mil = millis();
    }
    
    animations.UpdateAnimations();    
    strip.Show();
}

bool getSpaceApi(){
  StaticJsonBuffer<2000> jsonBuffer;
  
  // set up client
  WiFiClient client;
  const int httpPort = 80;
  const char* url = "hsmr.cc";
  const char* file = "/spaceapi.json";

  //connect to api
  if (!client.connect(url, httpPort)) {
    Serial.println("connection failed");
    return false;
  }

  //download status
  client.print(String("GET ") + file + " HTTP/1.1\r\n" +
           "Host: " + url + "\r\n" + 
           "Connection: close\r\n\r\n");

  // try until it exceeds 1 second
  int started = millis();
  while(!client.available()){
    delay(1);
    if(millis() - started > 1000){
      return false;
    }
  }
  
  // read the returned strings
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.println(line);
    
    JsonObject& root = jsonBuffer.parseObject(line);
    if (!root.success()){
      Serial.println("parseObject() failed");
    }else{
      Serial.println(root["state"]["open"].asString());
      return root["state"]["open"] == true;
    }
  }
}

