// ----------------------------------------------------------------------------
// Maaspoort Kids klok
// Created by Jan de Laet (October 2020)
// ----------------------------------------------------------------------------
//
// Een eenvoudige klok met alarm
// Gebruikte componenten: 
// - Arduino
// - LCD (20x4 of 16x2)  
// - RTC
// - een aantal buttons/knopjes
//
// NOTE bij LCD:
//  If the sketch fails to produce the expected results, or blinks the LED,
//  run the included I2CexpDiag sketch to test the i2c signals and the LCD.
//
// Voor het overige verwijs ik naar de documentatie en voorbeelden bij de hd44780 library

#include <Wire.h>

// libraries voor de LCD
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h> // uitbreiding voor i2c expander op LCD

hd44780_I2Cexp lcd; // declareer lcd object: auto locate & auto config expander chip

//  libraries voor de RTC
#include <DS3231.h>

DS3231 Clock;

// LCD opzet, in ons geval 4 rijen met 20 kolommen
const int LCD_ROWS = 2; //4;
const int LCD_COLS = 16; //20;

const bool toon_seconden = false;

// deze variabele scherm bepaalt wat er wordt getoond op LCD
// 0 = hoofdscherm met tijd en alarm aan/uit
// 1 = zet het uur van de klok
// 2 = zet de minuten van de klok
// 3 = zet het uur van het alarm
// 4 = zet de minuten van het alarm
int scherm = 0; 
int vorig_scherm = -1;       // om wijzigingen te detecteren, voorkomt onnodig display
long schermteller = 0;
long schermactief = 5000;   // hoe lang is een scherm actief voordat we terug gaan naar scherm 0

int knipper = 0;            // om uur/minuut te laten knipperen bij het wijzigen ervan
long knipperteller = 0;     
long knipperactief = 500;   // hoe vaak wordt er geknipperd

const int const_uur = 0;
const int const_minuut = 1;

// de variabelen van de klok
int klok_uur = 0;
int klok_minuut = 0;
int klok_seconde = 0;

long klokteller = 0;

// de variabelen van het alarm
int alarm_uur = 0;
int alarm_minuut = 0;

int alarm = 0; // alarm uit (0) of aan (1)
int vorig_alarm = -1;

// de variabelen voor de RTC
bool h12;  // true is 12-h, false is 24-h
bool PM;

// de pinnen waar de knoppen zitten
const int schermknopPin = 4; //2;     // op welke pin zit de schermknop 
const int zetknopPin = 5;//3;     // op welke pin zit de schermknop 
const int zoemerPin = 6;//4;
const int ledPin = 13;

// =====================================================================================
void setup() {

  init_lcd();

  init_rtc();

  // zet de modus voor de knoppen
  pinMode(schermknopPin, INPUT);
  pinMode(zetknopPin, INPUT);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(zoemerPin, OUTPUT);

  // Toon een welkomst boodschap
  lcd.print(" Maaspoort Kids");
  delay(2000);

  Serial.begin(9600);

}

// ======================================================================================
void loop() {

  // haal elke seconde de tijd opnieuw op
  if ((millis() - klokteller) > 1000) { bepaal_tijd(); }

  // ------------------------------------------------------------------------------------
  // wordt de scherm knop ingedrukt?
  if (digitalRead(schermknopPin) == HIGH) {
    // toon volgend scherm en herstart de timeout
    scherm++;
    schermteller = millis();

    // herstart het knipperen bij scherm wissel
    knipper = 0;
    knipperteller = schermteller;
    
    // wacht tot knop wordt losgelaten
    while (digitalRead(schermknopPin) == HIGH) { delay(50); }   

  }

  // ------------------------------------------------------------------------------------
  // toon het gevraagde scherm; bewaar het huidige scherm
  switch (scherm) {
    case 0:
      toon_tijd();
      break;
    case 1:
      zet_klok(const_uur);
      break;
    case 2:
      zet_klok(const_minuut);
      break;
    case 3:
      zet_alarm(const_uur);
      break;
    case 4:
      zet_alarm(const_minuut);
      break;
    default:
      vorig_scherm = scherm;
      scherm = 0;
   }

  delay(100);
  
}

// ======================================================================================
// initialiseer de LCD met het aantal kolommen en rijen
void init_lcd() {
 
  // lcd.begin geeft status terug waaraan te zien is of het wel/niet gelukt is
  // (zie hd44780.h voor de betekenis van de foutcode [RV_XXXX])
  int status;
  status = lcd.begin(LCD_COLS, LCD_ROWS);
  if( status )  { // status gevuld, dan ging het fout
    // hd44780 heeft een fatalError() routine die led laat knipperen
    // toon de status ook in de Serial monitor
    Serial.print(status);
    hd44780::fatalError(status); // does not return
  }

  // het initialiseren is gedaan, backlight is nu aan.
}

// ======================================================================================
// initialiseer de rtc
void init_rtc() {

  bepaal_tijd();

}

// ======================================================================================
// toon de tijd
void toon_tijd() {
  
  // wordt de zet knop ingedrukt?
  if (digitalRead(zetknopPin) == HIGH) {

    alarm = 1 - alarm;   // wissel alarm van 0 -> 1 en van 1 -> 0
    vorig_scherm = -1;   // forceer een redisplay 1e scherm
    
    // wacht tot knop wordt losgelaten
    while (digitalRead(zetknopPin) == HIGH) { delay(50); }   
  }
  
  // test of alarm aanstaat en of het inmiddels zo laat is 
  digitalWrite(ledPin, LOW);
  noTone(zoemerPin);
  if (alarm == 1) {
    if (alarm_uur == klok_uur and alarm_minuut == klok_minuut) {
      digitalWrite(ledPin, HIGH);
      tone(zoemerPin, 370);
    }
  }
  
  if (scherm != vorig_scherm) {   // display alleen als er ander scherm voor zat, voorkomt flikkeren
    vorig_scherm = scherm;
    lcd.clear();
    lcd.setCursor(0,0);  // kolom, rij   -> MQ: start met 0, niet 1
    
    if (toon_seconden) 
    {
      lcd.print(TweeCijfers(klok_uur) + ":" + TweeCijfers(klok_minuut) + ":" + TweeCijfers(klok_seconde));
    }
    else
    {
      lcd.print(TweeCijfers(klok_uur) + ":" + TweeCijfers(klok_minuut));
    }
    lcd.setCursor(0,1);
    if ( alarm == 0 ) {
      lcd.print("Alarm: uit");
    } else {
      lcd.print("Alarm: aan");
    }

  }   

  delay(100);

}

// ======================================================================================
// zet het uur van de klok
void zet_klok(int soort) {

  // wordt de zet knop ingedrukt?
  if (digitalRead(zetknopPin) == HIGH) {

    // herstart de schermteller voor de timeout van dit scherm
    schermteller = millis();
    
    if (soort == const_uur) {
      klok_uur++;
      if (klok_uur > 23) { klok_uur = 0; }
    } else {
      klok_minuut++;
      if (klok_minuut > 59) { klok_minuut = 0; }
    }

    // zet klok terug in RTC
    Clock.setClockMode(false);  // set to 24h
    Clock.setHour(klok_uur);
    Clock.setMinute(klok_minuut);
    Clock.setSecond(klok_seconde);
    
    // wacht tot knop wordt losgelaten
    while (digitalRead(zetknopPin) == HIGH) { delay(50); }   
  }
  
  // moet het knipperen omgewisseld worden
  if ((millis() - knipperteller) > knipperactief) {  // knipper wissel?
    knipper = 1 - knipper;                           // van 0->1 en 1->0
    knipperteller = millis();                        // start de knipperteller opnieuw
  }
    
  if ((millis() - schermteller) < schermactief) {
    lcd.clear();
    lcd.setCursor(2,0);  // kolom, rij   (4,0) voor 20x4 display
    lcd.print("Zet de klok");    
    lcd.setCursor(3,2); // kolom, rij   (7,2) voor 20x4 display
    if (knipper == 0) {
      lcd.print(TweeCijfers(klok_uur) + ":" + TweeCijfers(klok_minuut));
    } else {
      if (soort == const_uur) {
        lcd.print("  :" + TweeCijfers(klok_minuut));
      } else {
        lcd.print(TweeCijfers(klok_uur) + ":  ");
        
      }
    }  
  } else {
    vorig_scherm = scherm;
    scherm = 0;  // val na een tijdje terug naar scherm 0
  }   

}

// ======================================================================================
// zet het uur van het alarm
void zet_alarm(int soort) {

  // wordt de zet knop ingedrukt?
  if (digitalRead(zetknopPin) == HIGH) {

    // herstart de schermteller voor de timeout van dit scherm
    schermteller = millis();
    
    if (soort == const_uur) {
      alarm_uur++;
      if (alarm_uur > 23) { alarm_uur = 0; }
    } else {
      alarm_minuut++;
      if (alarm_minuut > 59) { alarm_minuut = 0; }
    }
    
    // wacht tot knop wordt losgelaten
    while (digitalRead(zetknopPin) == HIGH) { delay(50); }   
  }
  
  // moet het knipperen van uur/minuut gewisseld worden
  if ((millis() - knipperteller) > knipperactief) {
    knipper = 1 - knipper;
    knipperteller = millis();
  }
  
  if ((millis() - schermteller) < schermactief) {
    lcd.clear();
    lcd.setCursor(2,0);  // kolom, rij   (4,0) voor 20x4 display
    lcd.print("Zet het alarm");
    lcd.setCursor(3,2); // kolom, rij   (7,2) voor 20x4 display
    if (knipper == 0) {
      lcd.print(TweeCijfers(alarm_uur) + ":" + TweeCijfers(alarm_minuut));
    } else {
      if (soort == const_uur) {
        lcd.print("  :" + TweeCijfers(alarm_minuut));
      } else {
        lcd.print(TweeCijfers(alarm_uur) + ":  ");
      }     
    }
  } else {
    vorig_scherm = scherm;
    scherm = 0;  // val na een tijdje terug naar scherm 0
  }

}

// ======================================================================================
String TweeCijfers(int var) {
  String toon = String(var);
  if (var < 10) { toon = "0" + String(var); }
  return toon;
}

// ======================================================================================
void bepaal_tijd() {

  int vorig_minuut = klok_minuut;
  int vorig_seconde = klok_seconde;
  
  // haal de tijd op uit de RTC
  klok_uur = (int) Clock.getHour(h12, PM);
  klok_minuut = (int) Clock.getMinute();
  klok_seconde = (int) Clock.getSecond();

  klokteller = millis();

  // forceer display als seconde of minuut is gewijzigd
  if (toon_seconden)   {
    if (vorig_seconde != klok_seconde) { vorig_scherm = -1; }  
  }
  else {
    if (vorig_minuut != klok_minuut) { vorig_scherm = -1; }  
  }
  
  

}
