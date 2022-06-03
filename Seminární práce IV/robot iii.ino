#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
WiFiUDP protokol;
NTPClient hodiny(protokol, "europe.pool.ntp.org", 3600);

#include <Firebase_Arduino_WiFiNINA.h>
#define FIREBASE_HOST "martinznamenacekdb-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "Tug08AoxblPlTlkdjpTWMK0ZqwQfU8tm1YZxFAVt"
#define WIFI_SSID "wifina"
#define WIFI_HESLO "Heslo1234"
FirebaseData databaze;

const float odpor_kalibrovany[6] = {1.9, 2.9, 12.7, 1.5, 0.33, 2.1};
const float odpor_maximalni[6] = {2.30, 1.75, 0.70, 2.00, 8.50, 1.50};
const float odpor_minimalni[6] = {0.12, 0.45, 0.15, 0.15, 0.03, 0.60};
const int koncentrace_minimalni[6] = {100, 200, 200, 20, 100, 10};
const int koncentrace_maximalni[6] = {10000, 10000, 10000, 2000, 10000, 200};
const int MQ_analog_pin[6] = {A0, A1, A2, A3, A4, A5};
int koncentrace[6];

void setup(){
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    status = WiFi.begin(WIFI_SSID, WIFI_HESLO);
    delay(500);}
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH, WIFI_SSID, WIFI_HESLO);
  Firebase.reconnectWiFi(true);
  hodiny.begin();}

void loop(){
  for (int i = 0; i <= 5; i++) {
    MERENI_MQ(i);
    Firebase.setFloat(databaze, "/koncentrace_" + String(i+1), koncentrace[i]);}
  hodiny.update();
  String cas = hodiny.getFormattedTime();
  time_t t = hodiny.getEpochTime();
  setTime(t);
  String den = String(day()) + "-" + String(month()) + "-" + String(year());
  String json = "{\"koncentrace_1\":" + String(koncentrace[0]) + ",\"koncentrace_2\":" + String(koncentrace[1]) + ",\"koncentrace_3\":" + String(koncentrace[2]) + ",\"koncentrace_4\":" + String(koncentrace[3]) + ",\"koncentrace_5\":" + String(koncentrace[4]) + ",\"koncentrace_6\":" + String(koncentrace[5]) + "}";
  Firebase.pushJSON(databaze, "/záloha měření/ze dne " + den + ", v čase " + cas, json);
}

void MERENI_MQ(byte meric){
  float MQ_napeti = (float)(analogRead(MQ_analog_pin[meric])) / 1024 * 5.0;
  float odpor_MQ = (5.0 - MQ_napeti) / MQ_napeti;
  float odpor_MQ_upraveny = odpor_MQ / odpor_kalibrovany[meric] * 100;
  if ( odpor_MQ_upraveny <= ( odpor_maximalni[meric] * 100 ) ){
    koncentrace[meric] = map(odpor_MQ_upraveny, odpor_maximalni[meric] * 100, odpor_minimalni[meric] * 100, koncentrace_minimalni[meric], koncentrace_maximalni[meric]);}
  else{
    koncentrace[meric] = koncentrace_minimalni[meric];}}