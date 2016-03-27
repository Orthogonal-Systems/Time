/*
 * Time_NTP.pde
 * Example showing time sync to NTP time source
 *
 * This sketch uses the Ethernet library
 */
 
#include <TimeLib.h>
#include <UIPEthernet.h>
//#include <UIPEthernetUdp.h>

IPAddress ip    (169,254,5,11);

// NTP Servers:
IPAddress timeServer(169, 254,   5, 183); // LAN NTP Server


const int timeZone = 0;     // unix time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

void digitalClockDisplay(time_t now);
void printDigits(int digits, char delim);
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

void setup() 
{
  // minimal SPI bus config (cant have two devices being addressed at once)
  PORTG |= (1<<0);  // set AMC7812 CS pin high if connected
  digitalWrite(SS, HIGH); // set ENCJ CS pin high 

  Serial.begin(115200);
  Serial.println("TimeNTP Example");

  // set up ethernet chip
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
#if DHCP && UIP_UDP // cant use DHCP without using UDP
  Serial.println(F("DHCP..."));
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed DHCP"));
    for(;;)
      ;
  }
  Serial.print("IP number assigned by DHCP is ");
#else
  Serial.println(F("STATIC IP Addr"));
  Ethernet.begin(mac,ip);
#endif

  Serial.println(Ethernet.localIP());
  Serial.println(Ethernet.subnetMask());
  Serial.println(Ethernet.gatewayIP());
  Serial.println(Ethernet.dnsServerIP());

  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{  
  if (timeStatus() != timeNotSet) {
    if ((now()>>32) != (prevDisplay>>32)) { //update the display only if seconds has changed
      prevDisplay = now();
      //digitalClockDisplay(prevDisplay);  
      time_t temp = prevDisplay;
      for(uint8_t i=0; i<16; i++){
        Serial.print((uint8_t)(temp>>60),HEX);
        temp = temp << 4;
      }
      Serial.println();
    }
  }
}

void digitalClockDisplay(time_t now){
  // digital clock display of the time
  Serial.print(hour(now));
  printDigits(minute(now),':');
  printDigits(second(now),':');
  //printDigits(msec()/10,'.');
  Serial.print(" ");
  Serial.print(day(now));
  Serial.print(" ");
  Serial.print(month(now));
  Serial.print(" ");
  Serial.print(year(now)); 
  Serial.println(); 
}

void printDigits(int digits, char delim){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(delim);
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      unsigned long fracSecs;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      fracSecs =  (unsigned long)packetBuffer[44] << 24;
      fracSecs |= (unsigned long)packetBuffer[45] << 16;
      fracSecs |= (unsigned long)packetBuffer[46] << 8;
      fracSecs |= (unsigned long)packetBuffer[47];
      return ((time_t)(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR)<<32) + fracSecs;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
