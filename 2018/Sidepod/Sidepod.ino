#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_GPS.h>
#include <mcp_can.h>

#include "CANBus18.h"
#include "PacketSender.h"

#define CAN0_INT 4
MCP_CAN CAN0(10);

const int GPS_RX = A1;
const int GPS_TX = A0;

SoftwareSerial gps(GPS_TX, GPS_RX);
Adafruit_GPS GPS(&gps);

const int rxPinXBee = A4;
const int txPinXBee = A5;


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
  //Serial.begin(115200);
  // put your setup code here, to run once:
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
      //Serial.println("MCP2515 Initialized Successfully!");
  }
  else {
      //Serial.println("Error Initializing MCP2515...");
  }
  CAN0.setMode(MCP_NORMAL); // send acknowledgements to recieved data
  pinMode(CAN0_INT, INPUT);
  
  GPS.begin(9600);
  
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);   // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  /*OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);*/

  delay(500);
}

void loop() {
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
    payload outgoing;
    if (!GPS.parse(GPS.lastNMEA())) {
      outgoing.intData = 0;
      XBee.sendPayloadTimestamp(outgoing, 0x42);
      outgoing.intData = 0;
      XBee.sendPayloadTimestamp(outgoing, 0x43);
    } else {
      outgoing.intData = GPS.latitude_fixed;
      XBee.sendPayloadTimestamp(outgoing, 0x42);
      outgoing.intData = GPS.longitude_fixed;
      XBee.sendPayloadTimestamp(outgoing, 0x43);
    }
  }

  if (!digitalRead(CAN0_INT)) {
    //Serial.println("got data");
    CAN0.readMsgBuf(&rxId, &len, rxBuf);

      
      payload outgoing;

      switch (rxId & 0x1FFFFFFF) {

        case ID_AEMEngine0: {

            int rawRPM = GET_AEMEngine0_EngineSpeed(rxBuf);
            float rpm = CALC_AEMEngine0_EngineSpeed(rawRPM, 1.0);

            outgoing.floatData = rpm;
            XBee.sendPayloadTimestamp(outgoing, 0x30);

            int rawLoad = GET_AEMEngine0_EngineLoad(rxBuf);
            float load = CALC_AEMEngine0_EngineLoad(rawLoad, 1.0);

            outgoing.floatData = load;
            XBee.sendPayloadTimestamp(outgoing, 0x31);
            //Display.sendPayload(outgoing, 0x31);

            //log throttle position
            int rawThrottle = GET_AEMEngine0_ThrottlePos(rxBuf);
            float throttle = CALC_AEMEngine0_ThrottlePos(rawThrottle, 1.0);

            outgoing.floatData = throttle;
            XBee.sendPayloadTimestamp(outgoing, 0x32);

            //log coolant temp
            int coolantC = GET_AEMEngine0_CoolantTemp(rxBuf);
            float coolantF = CALC_AEMEngine0_CoolantTemp(coolantC, 1.0);

            outgoing.floatData = coolantF;
            XBee.sendPayloadTimestamp(outgoing, 0x33);

            int rawIntake = GET_AEMEngine0_IntakeManifoldAirTemp(rxBuf);
            float intake = CALC_AEMEngine0_IntakeManifoldAirTemp(rawIntake, 1.0);

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

            int rawo2 = GET_AEMEngine3_AFR1(rxBuf);
            float o2 = CALC_AEMEngine3_AFR1(rawo2, 1.0);

            outgoing.floatData = o2;
            XBee.sendPayloadTimestamp(outgoing, 0x34);

            /*uint16_t rawSpeed = (uint16_t)rxBuf[2] << 8;
            rawSpeed |= rxBuf[3];
            vehicleSpeed = rawSpeed * SPEED_SCALE;*/
            //dataLine = dataLine + vehicleSpeed + ", ";

            int rawSpeed = GET_AEMEngine3_VehicleSpeed(rxBuf);
            float vehicleSpeed = CALC_AEMEngine3_VehicleSpeed(rawSpeed, 1.0);

            outgoing.floatData = vehicleSpeed;
            XBee.sendPayloadTimestamp(outgoing, 0x35);
            //Display.sendPayload(outgoing, 0x35);

            byte gear = rxBuf[4];
            XBee.sendByteTimestamp(gear, 0x36);
            //Display.sendByte(gear, 0x36);

            int rawVolts = GET_AEMEngine3_ECUBatteryVoltage(rxBuf);
            float volts = CALC_AEMEngine3_ECUBatteryVoltage(rawVolts, 1.0);

            outgoing.floatData = volts;
            XBee.sendPayloadTimestamp(outgoing, 0x37);

            int rawIgn = GET_AEMEngine3_IgnitionTiming(rxBuf);
            float ign = CALC_AEMEngine3_IgnitionTiming(rawIgn, 1.0);

            outgoing.floatData = ign;
            XBee.sendPayloadTimestamp(outgoing, 0x45);
            
            // log time (since arduino started)
            //dataLine = dataLine + (millis()/1000.0);
            //Serial.println(dataLine);
            //logFile.println(dataLine);
            break;
          }

        case ID_RearController0: {
            int rawRB = GET_RearController0_RearBrakePressure(rxBuf);
            float rearBrake = CALC_RearController0_RearBrakePressure(rawRB, 1.0);
            outgoing.floatData = rearBrake;
            XBee.sendPayloadTimestamp(outgoing, 0x41);

            int rawFB = GET_RearController0_FrontBrakePressure(rxBuf);
            float frontBrake = CALC_RearController0_FrontBrakePressure(rawFB, 1.0);
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
