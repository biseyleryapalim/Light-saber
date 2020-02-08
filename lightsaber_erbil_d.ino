#define NUM_LEDS 92         // led sayısı

#define DFPLAYER_RX 1
#define DFPLAYER_TX 0
#define BTN 3
#define BTN_LED 2
#define LED_PIN 6
#define VOLT_PIN A1
#define SW_L_MIN 3
#define SW_L_MAX 7
#define SW_H_MIN 8
#define SW_H_MAX 11
#define SK_L_MIN 12
#define SK_L_MAX 19
#define SK_H_MIN 20
#define SK_H_MAX 27

#define OPEN_EFFECT 1
#define HUM_EFFECT 2
#define CLOSE_EFFECT 28
#define BRIGHTNESS 250      // max led parlaklığı (0 - 255)
#define BTN_TIMEOUT 800     // buton basma süresi / milisaniye

#define HUM_TIME 35000
#define SW_L_TIME 500
#define SW_H_TIME 800
#define SK_TIME 500

#define SWING_TIMEOUT 500   // salınımlar arasındaki zaman
#define SWING_L_THR 150     // açısal dönme hız süresi
#define SWING_H_THR 400       // hızlı dönüş süresi
#define STRIKE_L_THR 150      // hafif çarpma hızlanma süresi
#define STRIKE_H_THR 320    // sert çarpma hızlanma süresi
#define FLASH_DELAY 80      // vurulduğunda parlama süresi
#define OUTPUT_READABLE_ACCELGYRO

#define PULSE_ALLOW 1       // ışık titreşimi (1 - izin ver, 0 - izin verme)
#define PULSE_AMPL 20       // darbe şiddeti
#define PULSE_DELAY 30      // darbeler arasındaki gecikme
#define BREATHE_DELAY 30
#define SP_DELAY 20

#define BREATHE_MIN 100

//-------------------Libs--------------------
#include "Wire.h"
#include "MPU6050.h"
#include "I2Cdev.h"
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include "FastLED.h"
//-------------------Libs--------------------

//-------------------Vars--------------------
CRGB leds[NUM_LEDS];
DFRobotDFPlayerMini myDFPlayer;
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
unsigned long ACC, GYR, COMPL;
int gyroX, gyroY, gyroZ, accelX, accelY, accelZ, freq, freq_f = 20;
float k = 0.2;
unsigned long humTimer = (-1)*HUM_TIME, mpuTimer;
int stopTimer;
boolean bzzz_flag, ls_chg_state, ls_state;
boolean btnState, btn_flag, hold_flag, chg_color_flag;
byte btn_counter;
boolean swing_flag, swing_allow, strike_flag, HUMmode;

unsigned long btn_timer, PULSE_timer, swing_timer, swing_timeout, battery_timer, bzzTimer, breathe_timer, sp_timer;
int hv_sw_num, hv_sw_offset, hv_sw_max, hv_sw_min;    //heavy swing (0006-0008)
int sl_sw_num, sl_sw_offset, sl_sw_max, sl_sw_min;    //slight swing (0003-0005)

byte nowNumber;

byte LEDcolor;  // 0 - red, 1 - green, 2 - blue, 3 - pink, 4 - yellow, 5 - ice blue
byte nowColor, red, green, blue, redOffset, greenOffset, blueOffset;
byte eff_state;  // 1 - pulse, 2 - breath
byte special_eff_state;
int PULSEOffset;

float voltage;
//-------------------Vars--------------------

SoftwareSerial mySoftwareSerial(DFPLAYER_RX, DFPLAYER_TX); // RX, TX


void setup() {

  pinMode(BTN, INPUT_PULLUP);
  pinMode(BTN_LED, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(BTN_LED, 1);

  //  LED connection
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(100);  // ~40% led ışığı
  setAll(0, 0, 0);             // ve kapat

  // MPU6050 connection
  Wire.begin();
  accelgyro.initialize();
  accelgyro.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);
  accelgyro.setFullScaleGyroRange(MPU6050_GYRO_FS_250);

  //  DFPlayer connection
  delay(500);
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    while(1){
        digitalWrite(BTN_LED, HIGH);   //LED'i açın (YÜKSEK voltaj seviyesidir)
        delay(1000);                       // bekle
        digitalWrite(BTN_LED, LOW);    // gerilimi DÜŞÜK yaparak LED'i kapatın
        delay(1000);                
    }
  }

  randomSeed(analogRead(2));    // rasgele varyasyon için başlangıç ​​noktası

  //---------------geçici bölge------------
  ls_state = 0;
  btn_flag = 0;
  HUMmode = 0;
  swing_flag = 0;
  strike_flag = 0;
  bzzz_flag = 0;
  nowColor = 0;
  btnState = 0;
  eff_state = 1;
  special_eff_state = 0;
  setColor(nowColor);

  //myDFPlayer.volume(0);
  //myDFPlayer.play(SW_L_MIN);
  myDFPlayer.volume(29);

  FastLED.setBrightness(BRIGHTNESS);   // parlaklık
}


void loop() {

  btnTick();
  on_off_sound();
  special_effect();
  effect();
  getFreq();
  strikeTick();
  swingTick();
    
}

void btnTick() {
  btnState = !digitalRead(BTN);
  if (btnState && !btn_flag) {
    btn_flag = 1;
    btn_counter++;
    btn_timer = millis();
  }
  if (!btnState && btn_flag) {
    btn_flag = 0;
    hold_flag = 0;
  }

  // turn on/off saber 
  if (btn_flag && btnState && (millis() - btn_timer > BTN_TIMEOUT) && !hold_flag) {
    ls_chg_state = 1;                     // kılıç durumunu değiştirmek için  (açık / kapalı)
    hold_flag = 1;
    btn_counter = 0;
  }

  // functionality
  if ((millis() - btn_timer > BTN_TIMEOUT) && (btn_counter != 0)) {
    if (ls_state) {
      if (btn_counter == 2) {               // 2 defa basıldığında renk değiştir
        
        nowColor++;                         
        if (nowColor >= 5) nowColor = 0;
        setColor(nowColor);
        chg_color_flag = 1;
        special_eff_state = 0;
        eff_state = 1;
      
      }else if (btn_counter == 3) {               // 3 kere basıldığında diğer moda geç
        
        eff_state++;

        if(special_eff_state == 1)
          special_eff_state = 0;

        if(eff_state == 3){ 
          eff_state = 1;
          FastLED.setBrightness(BRIGHTNESS);
        }

      }else if(btn_counter == 4){                 // 4 defa basıldığında özel moda geç

        special_eff_state++;

        if(special_eff_state == 1){

          eff_state = 0;
          FastLED.setBrightness(BRIGHTNESS);
        
        }else if(special_eff_state == 2){

          special_eff_state = 0;
          eff_state = 1;
        }
      }
    }
    btn_counter = 0;
  }
}


void on_off_sound(){

  if (ls_chg_state) {                // eğer bayrak değişirse
    if (!ls_state) {                 // kılıcı kapat
      
        myDFPlayer.play(OPEN_EFFECT);
        
        light_up();
        delay(200);
        bzzz_flag = 1;
        HUMmode = 1;        
        ls_state = true;               // hatırla kılıç açık
        eff_state = 1;
        if (HUMmode) {
          myDFPlayer.play(HUM_EFFECT); 
        } else {
          myDFPlayer.pause();
        }

    } else {                         // eğer kılıç kapanırsa
      
      bzzz_flag = 0;
      myDFPlayer.play(CLOSE_EFFECT);
      delay(100);
      light_down();
      delay(100);
     myDFPlayer.pause();
      ls_state = false;

    }
    ls_chg_state = 0;
  }

  if(chg_color_flag){

    myDFPlayer.play(OPEN_EFFECT);
    light_up();
    delay(200);
    bzzz_flag = 1;

    if (HUMmode) {
      myDFPlayer.play(HUM_EFFECT); 
    } else {
      myDFPlayer.pause();
    }
    chg_color_flag = 0;
  }
  
  if (((millis() - humTimer) > HUM_TIME) && bzzz_flag && HUMmode) {   //uğultu sesi
    myDFPlayer.play(HUM_EFFECT); 
    humTimer = millis();
    swing_flag = 1;
    strike_flag = 0; 
  }

  long delta = millis() - bzzTimer;
  if ((delta > 3) && bzzz_flag && !HUMmode) {
    if (strike_flag) {
      myDFPlayer.pause();
      strike_flag = 0;
    }
    bzzTimer = millis();
  }
}


void effect() {

  if(eff_state == 1 && ls_state){                                   //titreme efekti
    
    if (PULSE_ALLOW && ls_state && (millis() - PULSE_timer > PULSE_DELAY)) {
      
      PULSE_timer = millis();
      PULSEOffset = PULSEOffset * k + random(-PULSE_AMPL, PULSE_AMPL) * (1 - k);
      if (nowColor == 0) PULSEOffset = constrain(PULSEOffset, -15, 5);
      redOffset = constrain(red + PULSEOffset, 0, 255);
      greenOffset = constrain(green + PULSEOffset, 0, 255);
      blueOffset = constrain(blue + PULSEOffset, 0, 255);
      setAll(redOffset, greenOffset, blueOffset);
    }
  }

  if(eff_state == 2 && ls_state){                                   //nefes efekti
    
    if(millis()-breathe_timer > BREATHE_DELAY){
      
      breathe_timer = millis();

      float breath = (exp(sin(millis()/2000.0*PI)) - 0.36787944) * 108.0;
      FastLED.setBrightness(breath);
      FastLED.show();
    }
  }
}

//her örnekleme zamanında ax, ay, az değeriyle rengi değiştirin
void special_effect(){

      if(special_eff_state == 1 && ls_state){
  
        if(millis()-sp_timer > SP_DELAY){
  
          sp_timer = millis();
  //      FastLED.setBrightness(BRIGHTNESS);
  
          for(char i = NUM_LEDS-1; i > 0; i--){
            setPixel(i, leds[i-1].r, leds[i-1].g, leds[i-1].b);
          }
        
          redOffset = map(abs(ax), 0, 15500, 0, 255);
          greenOffset = map(abs(ay), 0, 16000, 0, 255);
          blueOffset = map(abs(az), 0, 17500, 0, 255);
  
          setPixel(0, redOffset, greenOffset, blueOffset);
  //      setAll(redOffset, greenOffset, blueOffset);

      FastLED.show();
    }
  }
  
  
  }


void light_up() {

  FastLED.setBrightness(BRIGHTNESS);
  for (char i = 0; i <= (NUM_LEDS / 2 - 1); i++) {    //eğer lediniz tek uzun ledi paralel yapmak isterseniz bu seçenek kalmalı formül şu 
                                                      // (toplam led sayısı yarıya böl bir eksiğinden başlat        
    setPixel(i, red, green, blue);
    setPixel((NUM_LEDS - 1 - i), red, green, blue);
    FastLED.show();
    delay(5);
  }
}

void light_down() {
  for (char i = (NUM_LEDS / 2 - 1); i >= 0; i--) {    //eğer lediniz tek uzun ledi paralel yapmak isterseniz bu seçenek kalmalı formül şu 
                                                      // (toplam led sayısı yarıya böl bir eksiğinden başlat   
    setPixel(i, 0, 0, 0);
    setPixel((NUM_LEDS - 1 - i), 0, 0, 0);
    FastLED.show();
    delay(5);
  }
}

void setPixel(int Pixel, byte red, byte green, byte blue) {
  leds[Pixel].r = red;
  leds[Pixel].g = green;
  leds[Pixel].b = blue;
}

void setAll(byte red, byte green, byte blue) {
  for (int i = 0; i < NUM_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  FastLED.show();
  
}

void hit_flash() { // çarptığında beyaz bir parlama verecek
  
  setAll(255, 255, 255);            
  FastLED.setBrightness(100); // çarpma parlaklık değeri yüksek yaparsanız güç çekiminden sesler gidiyor
  delay(FLASH_DELAY);                

  if(special_eff_state == 1)
    setAll(0, 0, 0);
  else
    setAll(red, blue, green);
}

void setColor(byte color) {
  switch (color) {
    // 0 - kırmızı, 1 - yeşil, 2 - mavi, 3 - pembe, 4 - sarı, 5 - turkuaz
    case 0:
      red = 255;  green = 0;  blue = 0;
      redOffset = 255;  greenOffset = 0;  blueOffset = 0;
      break;      
    case 1:
      red = 0;  green = 255;  blue = 0;
      redOffset = 0;  greenOffset = 255;  blueOffset = 0;
      break;
    case 2:
      red = 0;  green = 0;  blue = 255;
      redOffset = 0;  greenOffset = 0;  blueOffset = 255;
      break;
    case 3:
      red = 255;  green = 0;  blue = 255;
      redOffset = 255;  greenOffset = 0;  blueOffset = 255;
      break;

     case 4:
      red = 0;  green = 255;  blue = 255;
      redOffset = 0;  greenOffset = 255;  blueOffset = 255;
      break;
  }

}

//dövüş efekti, ses ve darbe 
//fonksiyonu sürdürmek için hit_flash ()
void strikeTick() {
  if ((ACC > STRIKE_L_THR) && (ACC < STRIKE_H_THR)) {
    nowNumber = random(SK_L_MIN, SK_L_MAX+1);
    myDFPlayer.play(nowNumber); 
    hit_flash();
    if (!HUMmode)
      bzzTimer = millis() + SK_TIME - FLASH_DELAY;
    else
      humTimer = millis() - HUM_TIME + SK_TIME - FLASH_DELAY;
    strike_flag = 1;
  }
  if (ACC >= STRIKE_H_THR) {
    nowNumber = random(SK_H_MIN, SK_H_MAX+1);
    myDFPlayer.play(nowNumber); 
    hit_flash();
    if (!HUMmode)
      bzzTimer = millis() + SK_TIME - FLASH_DELAY;
    else
      humTimer = millis() - HUM_TIME + SK_TIME - FLASH_DELAY;
    strike_flag = 1;
  }
}

//sallama sesi
void swingTick() {
  if (GYR > 80 && (millis() - swing_timeout > 100) && HUMmode) {
    
    swing_timeout = millis();
    if (((millis() - swing_timer) > SWING_TIMEOUT) && swing_flag && !strike_flag) {
      if (GYR >= SWING_H_THR) {      
        nowNumber = random(SW_H_MIN, SW_H_MAX+1);          
        myDFPlayer.play(nowNumber);      
        humTimer = millis() - HUM_TIME + SW_H_TIME;
        swing_flag = 0;
        swing_timer = millis();
        swing_allow = 0;
      }
      if ((GYR > SWING_L_THR) && (GYR < SWING_H_THR)) {
        nowNumber = random(SW_L_MIN, SW_L_MAX+1);            
        myDFPlayer.play(nowNumber);       
        humTimer = millis() - HUM_TIME + SW_L_TIME;
        swing_flag = 0;
        swing_timer = millis();
        swing_allow = 0;
      }
    }
  }
}

void getFreq() {
  if (ls_state) {                                               // eğer kılıç kapalı ise
    if (millis() - mpuTimer > 500) {                            
      accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);       

      // mutlak bulun ve 100'e bölün
      gyroX = abs(gx / 100);
      gyroY = abs(gy / 100);
      gyroZ = abs(gz / 100);
      accelX = abs(ax / 100);
      accelY = abs(ay / 100);
      accelZ = abs(az / 100);

      // vektör toplamı
      ACC = sq((long)accelX) + sq((long)accelY) + sq((long)accelZ);
      ACC = sqrt(ACC);
      GYR = sq((long)gyroX) + sq((long)gyroY) + sq((long)gyroZ);
      GYR = sqrt((long)GYR);
      COMPL = ACC + GYR;
      
      freq = (long)COMPL * COMPL / 1500;                        // parabolik ton değişimi
      freq = constrain(freq, 18, 300);                          
      freq_f = freq * k + freq_f * (1 - k);                     // pürüzsüz filtre
      mpuTimer = micros();                                     
    }
  }
}
