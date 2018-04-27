#include <SPI.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <SD.h>
#include <Adafruit_GPS.h>
#include <mcp_can.h>

#include "CANBus18.h"
#include "PacketSender.h"

#define DEBUG true

#define CAN0_INT 4
MCP_CAN CAN0(10);

const int GPS_RX = A3;
const int GPS_TX = A4;

SoftwareSerial gps(GPS_TX, GPS_RX);
Adafruit_GPS GPS(&gps);

const int rxPinXBee = A4;
const int txPinXBee = A5;

const bool sd_logging = true;
const int sd_pin = 7;
bool sd_working = false;

File canfile;
File gpsfile;

SoftwareSerial XBee_Serial(rxPinXBee,txPinXBee);
PacketSender XBee(XBee_Serial, 9600);
//HardwareSender Display = HardwareSender();

const int deltaTime = 200;
unsigned long accumulator = 0;

/*double rpm;           //key: 0x30
double load;          //key: 0x31
double throttle;      //key: 0x32
double coolantF;      //key: 0x33
double o2;            //key: 0x34
double vehicleSpeed;  //key: 0x35
byte gear;            //key: 0x36
double volts; //key: 0x37
float DispFB; // 0x40
float DispBB; // 0x41*/

//intake: 44
//ingition: 45

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

void setup() {
  if (DEBUG)
    Serial.begin(115200);
    
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    if (DEBUG)
      Serial.println(F("MCP2515 Initialized Successfully!"));
  } else {
    if (DEBUG)
      Serial.println(F("Error Initializing MCP2515..."));
  }
  CAN0.setMode(MCP_NORMAL); // send acknowledgements to recieved data
  pinMode(CAN0_INT, INPUT);
  
  //GPS.begin(9600);
  
  // RMC (recommended minimum) and GGA (fix data) including altitude
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // Set the update rate
  //GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);   // 1 Hz update rate
  // Request updates on antenna status, comment out to keep quiet
  //GPS.sendCommand(PGCMD_ANTENNA);

  if (sd_logging) {
    if (SD.begin(sd_pin)) {
      sd_working = true;
      char filename[] = "CAN00.TXT";
      for (uint8_t i = 0; i < 100; i++) {
        filename[3] = i/10 + '0';
        filename[4] = i%10 + '0';
        if (! SD.exists(filename)) {
          // only open a new file if it doesn't exist
          canfile = SD.open(filename, FILE_WRITE); 
          break;  // leave the loop!
        }
      }
      /*char gpsfilename[] = "GPS00.TXT";
      for (uint8_t i = 0; i < 100; i++) {
        gpsfilename[3] = i/10 + '0';
        gpsfilename[4] = i%10 + '0';
        if (! SD.exists(gpsfilename)) {
          // only open a new file if it doesn't exist
          gpsfile = SD.open(gpsfilename, FILE_WRITE); 
          break;  // leave the loop!
        }
      }*/
      if (DEBUG)
        Serial.println(F("SD card initialized"));
    } else {
      if (DEBUG)
        Serial.println(F("Failed to load SD card"));
    }
  }

  delay(5000);
}

void loop() {
  payload outgoing;
  int rawRPM, rawLoad, rawThrottle, coolantC, rawIntake, rawo2, rawSpeed, rawVolts, rawIgn, rawRB, rawFB;
  float rpm, load, throttle, coolantF, intake, o2, vehicleSpeed, volts, ign, rearBrake, frontBrake;
  byte gear;
  /*if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) {
      outgoing.intData = 0;
      XBee.sendPayloadTimestamp(outgoing, 0x42);
      outgoing.intData = 0;
      XBee.sendPayloadTimestamp(outgoing, 0x43);
    } else {
      log_gps_data();
      outgoing.intData = GPS.latitude_fixed;
      XBee.sendPayloadTimestamp(outgoing, 0x42);
      outgoing.intData = GPS.longitude_fixed;
      XBee.sendPayloadTimestamp(outgoing, 0x43);
    }
  }*/

  if (!digitalRead(CAN0_INT)) {
    //Serial.println("got data");
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
    Serial.println(rxId, HEX);
    log_can_data();
      switch (rxId & 0x1FFFFFFF) {

        case ID_AEMEngine0: {

            rawRPM = GET_AEMEngine0_EngineSpeed(rxBuf);
            rpm = CALC_AEMEngine0_EngineSpeed(rawRPM, 1.0);

            outgoing.floatData = rpm;
            XBee.sendPayloadTimestamp(outgoing, 0x30);

            rawLoad = GET_AEMEngine0_EngineLoad(rxBuf);
            load = CALC_AEMEngine0_EngineLoad(rawLoad, 1.0);

            outgoing.floatData = load;
            XBee.sendPayloadTimestamp(outgoing, 0x31);
            //Display.sendPayload(outgoing, 0x31);

            //log throttle position
            rawThrottle = GET_AEMEngine0_ThrottlePos(rxBuf);
            throttle = CALC_AEMEngine0_ThrottlePos(rawThrottle, 1.0);

            outgoing.floatData = throttle;
            XBee.sendPayloadTimestamp(outgoing, 0x32);

            //log coolant temp
            coolantC = GET_AEMEngine0_CoolantTemp(rxBuf);
            coolantF = CALC_AEMEngine0_CoolantTemp(coolantC, 1.0);

            outgoing.floatData = coolantF;
            XBee.sendPayloadTimestamp(outgoing, 0x33);

            rawIntake = GET_AEMEngine0_IntakeManifoldAirTemp(rxBuf);
            intake = CALC_AEMEngine0_IntakeManifoldAirTemp(rawIntake, 1.0);

            outgoing.floatData = intake;
            XBee.sendPayloadTimestamp(outgoing, 0x44);

            break;
          }

        case ID_AEMEngine3: {
            //dataLine = "O2_SPEED_GEAR_VOLTAGE, ";
            //log O2
            /*uint8_t rawo2 = (uint8_t)rxBuf[0];
            o2 = rawo2 * O2_SCALE + 0.5;*/
            //dataLine = dataLine + o2 +  ", ";

            rawo2 = GET_AEMEngine3_AFR1(rxBuf);
            o2 = CALC_AEMEngine3_AFR1(rawo2, 1.0);

            outgoing.floatData = o2;
            XBee.sendPayloadTimestamp(outgoing, 0x34);

            /*uint16_t rawSpeed = (uint16_t)rxBuf[2] << 8;
            rawSpeed |= rxBuf[3];
            vehicleSpeed = rawSpeed * SPEED_SCALE;*/
            //dataLine = dataLine + vehicleSpeed + ", ";

            rawSpeed = GET_AEMEngine3_VehicleSpeed(rxBuf);
            vehicleSpeed = CALC_AEMEngine3_VehicleSpeed(rawSpeed, 1.0);

            outgoing.floatData = vehicleSpeed;
            XBee.sendPayloadTimestamp(outgoing, 0x35);
            //Display.sendPayload(outgoing, 0x35);

            gear = rxBuf[4];
            XBee.sendByteTimestamp(gear, 0x36);
            //Display.sendByte(gear, 0x36);

            rawVolts = GET_AEMEngine3_ECUBatteryVoltage(rxBuf);
            volts = CALC_AEMEngine3_ECUBatteryVoltage(rawVolts, 1.0);

            outgoing.floatData = volts;
            XBee.sendPayloadTimestamp(outgoing, 0x37);

            rawIgn = GET_AEMEngine3_IgnitionTiming(rxBuf);
            ign = CALC_AEMEngine3_IgnitionTiming(rawIgn, 1.0);

            outgoing.floatData = ign;
            XBee.sendPayloadTimestamp(outgoing, 0x45);
            
            // log time (since arduino started)
            //dataLine = dataLine + (millis()/1000.0);
            //Serial.println(dataLine);
            //logFile.println(dataLine);
            break;
          }

        case ID_RearController0: {
            rawRB = GET_RearController0_RearBrakePressure(rxBuf);
            rearBrake = CALC_RearController0_RearBrakePressure(rawRB, 1.0);
            outgoing.floatData = rearBrake;
            XBee.sendPayloadTimestamp(outgoing, 0x41);

            rawFB = GET_RearController0_FrontBrakePressure(rxBuf);
            frontBrake = CALC_RearController0_FrontBrakePressure(rawFB, 1.0);
            outgoing.floatData = frontBrake;
            XBee.sendPayloadTimestamp(outgoing, 0x40);

            /*Serial.print("FRONT: ");
            Serial.print(frontBrake);
            Serial.print("\t");
            Serial.print("REAR:");
            Serial.println(rearBrake);*/
            
        }
        
        default: {
            break;
        }
          
      }
  }
}

char timestr[20];
void update_time() {
  sprintf(timestr, "%04d-%02d-%02d-%02d-%02d-%02d", GPS.year, GPS.month, GPS.day, GPS.hour, GPS.minute, GPS.seconds);
}

void log_can_data()
{
  if (!sd_working)
    return;
  int i;
  if (canfile) {
    canfile.println();
    canfile.println("FSAE");
    //update_time();
    canfile.println(millis());
    canfile.println(rxId & 0x1FFFFFFF);
    canfile.println(len);
    for (i = 0; i < len; i++) {
      canfile.write(rxBuf[i]);
    }
    canfile.flush();
  }
}

void log_gps_data()
{
  if (!sd_working)
    return;
  int i;
  if (gpsfile) {
    update_time();
    gpsfile.print(timestr);
    gpsfile.print(",");
    gpsfile.print(GPS.latitude);
    gpsfile.print(",");
    gpsfile.print(GPS.longitude);
    gpsfile.println();
    gpsfile.flush();
  }
}

