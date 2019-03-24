#include <SoftwareSerial.h>

/*
 * RS845 Hyunyang - for use with cheap chinese VFD controller. This is an intermediary between GRBL & your VFD.
 * 
 * Use this version of GRBL on your controller, thanks mmcristil981:
 * https://github.com/mmcristi1981/grbl-mega-spi/tree/master/grbl
 * 
 * Have your GRBL board connected via SPI to a second board with this running.
 * 
 * Use a MAX485 chip on pins 4,5,6,7 to connect to the "+" and "-" of the VFD's serial connection
 * https://www.maximintegrated.com/en/products/interface/transceivers/MAX485.html
 * 
 * Credit for VFD code:
 * http://fightpc.blogspot.com/2014/10/vfd-control-with-arduino-using-rs485.html
 * 
 * RS485 connections 
 * D4 = DI (TX) &/& D5 = DE &/& D6 = RE &/& D7 = RO (RX)
 * 
  */

// HYUNYANG
char* change_speed=":010203030BB8";

char* sp_start =":01030101"; // CW
char* sp_start_rev=":01030111"; // CCW
char* stop=":01030108";

SoftwareSerial myserial(7,4); // RX, TX

// GRBL SNIFFER
volatile uint8_t byte_index;
volatile uint8_t rec;
volatile uint8_t bytes[2];

uint8_t copy[10];
uint8_t prev[10];
volatile uint8_t state;

// Reception Complete ISR
ISR(SPI_STC_vect) {
  if (byte_index == 2) {
    return;
  }
  
  bytes[byte_index] = SPDR;
  byte_index++;
}

void setup() {
   Serial.begin(9600);
   
   pinMode(5,OUTPUT);
   pinMode(6,OUTPUT);
   
   myserial.begin(9600);
   
   digitalWrite(5,LOW); 
   digitalWrite(5,LOW);

   // GRBL SNIFFER
   
   // 1111 0111
   // 4->MISO OUTPUT &/& 2->SCK INPUT &/& 1->MOSI INPUT &/& 0->SS INPUT  
   DDRB |= 0x10;     //MISO as OUTPUT, rest as input
   DDRB &= 0xF8;
   
   SPCR = (1<<SPE) | (1<<SPIE);   //Enable SPI
  
   byte_index = 0;
}

void setSpeed(unsigned int v) {
    double hzDouble = (double) v / 60;
    unsigned int rpmToHz = hzDouble * 100;
    Serial.print("Set RPM: "); Serial.print(v,DEC); Serial.print(" Hz: ");  Serial.println(hzDouble, 2);
    char* n=change_speed;
    
    n[9]=toHexa((rpmToHz/256)>>4);
    n[10]=toHexa((rpmToHz/256)&0xf);
    n[11]=toHexa((rpmToHz&0xff)>>4);
    n[12]=toHexa((rpmToHz&0xff)&0xf);
    
    Serial.println(n);
    query(n);
}

uint16_t last_speed = 0;

int repeatCount = 0;

void loop() {
  prev[0] = copy[0];
  prev[1] = copy[1];

  if (byte_index == 2 && ((PINB & 0x04) == 0x04)) {
    copy[0] = bytes[0];
    copy[1] = bytes[1];
    SPDR = 0x01;
    byte_index = 0;

    if (copy[0]==prev[0] && copy[1]==prev[1]) {

      repeatCount ++;

      if(repeatCount != 10) {
        return;
      }
      
      uint8_t dir = (copy[1] & 0x80) >> 7;
      uint16_t received = ((uint16_t)(copy[1] & 0x7F) << 8) | bytes[0];  
    
      if(received == 0) {
          if(last_speed == 0)
            return;
            
          setSpeed(0);
          last_speed = 0;
          query(stop);
      } else if(last_speed != 0) {
          if(last_speed == received)
            return;
            
          setSpeed(received);
          last_speed = received;
      } else {
          if(dir != 0) {
            setSpeed(received);
            query(sp_start);
            Serial.println("Start: CW");
          } else {
            setSpeed(received);
            query(sp_start_rev);
            Serial.println("Start: CCW");
          }
            
          last_speed = received;
      }
    } else
      repeatCount = 0;
  }
}

void transmit(String msg) {
    digitalWrite(5,HIGH); 
    digitalWrite(5,HIGH);
    delay(50);
    
    myserial.print(msg);
    
    delay(1);
    digitalWrite(5,LOW); 
    digitalWrite(5,LOW);
}

char hexa(char byte) { // return hexa value of that ASCII char
    if(byte<='9') return byte & 0x0f;
    if(byte<='F') return (byte & 0x0f) + 9; // A=0x41 --> 0x0a
}
  
char toHexa(int i) {
    if(i<10) return 0x30 | i;
    return 0x41 + i - 10;
}

char crc(char* buffer) {
    int l=strlen(buffer);
    int i=1;
    int chk=0;
    
    while(i<l) { Serial.print(hexa(buffer[i])<<4|hexa(buffer[i+1]),HEX); chk+= ( hexa(buffer[i])<<4|hexa(buffer[i+1]) ); i+=2; Serial.print(" ");}
    
    Serial.print(":"); 
    Serial.println(chk,HEX);
    Serial.println(0x100-chk,HEX);
    
    return (0x100-chk) & 0xff; 
}
  
void query(char* cmd) {
    char lrc = crc(cmd);
    String msg = cmd;
    msg+=toHexa((lrc&0xf0)>>4);
    msg+=toHexa(lrc&0x0f);
    msg+="\r\n";
    
    Serial.print(msg);
    
    transmit(msg);
}
