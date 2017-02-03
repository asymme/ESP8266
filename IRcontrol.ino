#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "DHT.h"
#define DHTPIN 14
#define DHTTYPE DHT22
#define IR_SEND_PIN 5


// TOUT無効(電源電圧測定の為)
ADC_MODE(ADC_VCC);


// SSID
const char* _ssid = "********";
// PASSWORD
const char* _password = "********";
// ホスト名
String _host = "http://localhost/hoge";
// ディープスリープ時間(us)
unsigned long _deepSleepTime = 5 * 60e6;



// エアコンオン
unsigned int _powerON[] = {/* リモコンより受信した内容 */};
// エアコンオフ
unsigned int _powerOFF[] = {};
// 冷房
unsigned int _cool[] = {};
// ドライ
unsigned int _dry[] = {};
// 暖房
unsigned int _heat[] = {};
// ハイパワー
unsigned int _highPower[] = {};
// 省パワー
unsigned int _savingPower[] = {};



// dataからリモコン信号を送信
void sendSignal(unsigned int data[], int dataSize) {
    for (int cnt = 0; cnt < dataSize; cnt++) {
        unsigned long len = data[cnt]*10;  // dataは10us単位でON/OFF時間を記録している
        unsigned long us = micros();
        do {
            digitalWrite(IR_SEND_PIN, 1 - (cnt&1)); // cntが偶数なら赤外線ON、奇数ならOFFのまま
            delayMicroseconds(8);  // キャリア周波数38kHzでON/OFFするよう時間調整
            digitalWrite(IR_SEND_PIN, 0);
            delayMicroseconds(7);
        } while (long(us + len - micros()) > 0); // 送信時間に達するまでループ
    }
}



// HTTP POST
void postRequest(String url, String param) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  int httpCode = http.POST(param);
  if(httpCode > 0) {
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}



// エアコンリクエストの判定
void judgeRequest(int val) {
  if(val & 100000 == 100000) {
    // 冷房
    sendSignal(_powerON, sizeof(_powerON) / sizeof(_powerON[0]));
    delay(5000);
    sendSignal(_cool, sizeof(_cool) / sizeof(_cool[0]));
    addOption(val);
  } else if(val & 10000 == 10000) {
    // ドライ
    sendSignal(_powerON, sizeof(_powerON) / sizeof(_powerON[0]));
    delay(5000);
    sendSignal(_dry, sizeof(_dry) / sizeof(_dry[0]));
    addOption(val);
  } else if(val & 1000 == 1000) {
    // 暖房
    sendSignal(_powerON, sizeof(_powerON) / sizeof(_powerON[0]));
    delay(5000);
    sendSignal(_heat, sizeof(_heat) / sizeof(_heat[0]));
    addOption(val);
  } else if(val & 100 == 100) {
    // OFF
    sendSignal(_powerOFF, sizeof(_powerOFF) / sizeof(_powerOFF[0]));
  } else {
    // 0またはイレギュラー値(何もしない)
    Serial.println("DO NOTHING...");
  }
}


void addOption(int val) {
  if(val & 10 == 10) {
    // ハイパワー
    delay(5000);
    sendSignal(_highPower, sizeof(_highPower) / sizeof(_highPower[0]));
  } else if(val & 1 == 1) {
    // 省パワー
    delay(5000);
    sendSignal(_savingPower, sizeof(_savingPower) / sizeof(_savingPower[0]));
  }
}


void setup() {
  Serial.begin(74880);
  pinMode(IR_SEND_PIN, OUTPUT);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    // Connection failed
    WiFi.disconnect();
    ESP.deepSleep(_deepSleepTime, WAKE_RF_DEFAULT);
  }
  
  
  // Reading Vcc, temperature and humidity
  DHT dht(DHTPIN, DHTTYPE);
  dht.begin();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float vcc = (float)ESP.getVcc() / 1000.0;
  
  String q_temperature = (!isnan(temperature)) ? (String)temperature : "";
  String q_humidity = (!isnan(humidity)) ? (String)humidity : "";
  String q_vcc = (!isnan(vcc)) ? (String)vcc : "";
  
  String param = "vcc=" + q_vcc + "&temperature=" + q_temperature + "&humidity=" + q_humidity;
  postRequest(_host + "/log/", param);


  /**
   * Check A/C Request
   * @payload
   *  100000: 冷房
   *  010000: ドライ
   *  001000: 暖房
   *  000100: 電源OFF
   *  000010: ハイパワー(冷暖房時などと組み合わせる)
   *  000001: 省パワー
   *  000000: 何もしない
   */
  HTTPClient http;
  http.begin(_host + "/request/?q=get");
  int httpCode = http.GET();
  if(httpCode > 0) {
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      judgeRequest(payload.toInt());

      // 「何もしない」で予約しなおす
      param = "user=foo&pass=bar&cmd=000000";
      postRequest(_host + "/request/", param);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  
  ESP.deepSleep(_deepSleepTime, WAKE_RF_DEFAULT);
}


void loop() {
  delay(500);
}
