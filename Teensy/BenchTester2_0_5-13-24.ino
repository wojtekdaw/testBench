#include <Adafruit_MCP4725.h>
#include <Adafruit_INA219.h>
#include <FlexCAN_T4.h>
#include "TeensyTimerTool.h"
using namespace TeensyTimerTool;
FlexCAN_T4FD<CAN3, RX_SIZE_256, TX_SIZE_16> Can3;     //Defines CANFD (T4FD) and CAN3 (only FD bus)

// Input Defines
#define LF    34
#define RF    35
#define LR    36
#define RR    37
#define RES1  38
#define RES2  39
#define BST   40
#define DIV   41

// Output Defines
#define strobe_100ms    33
#define strobe_1        23
#define strobe_2        22
#define strobe_3        16
#define strobe_4        15
#define LED             13
#define ECU_Relay_Pin   6
#define IGN_HW_Pin      17

//PS Defines
#define VoltageFB_ID   12
#define CurrentFB_ID   14
#define SetVFB_ID      11
#define SetIFB_ID      13

// PWM Defines
#define position_LF_PWM_Pin 0
#define position_RF_PWM_Pin 2
#define position_LR_PWM_Pin 4
#define position_RR_PWM_Pin 28
unsigned char LF_DC = 0;                // [%]
unsigned char RF_DC = 0;                // [%]
unsigned char LR_DC = 0;                // [%]
unsigned char RR_DC = 0;                // [%]

//RS 232 Defines
#define AVAIL	1
#define UNAVAIL	0
#define TRUE	1
#define FALSE	0

// Digital to Analog Defines
//This is the I2C Address of the MCP4725, by default (A0 pulled to GND).
//Please note that this breakout is for the MCP4725A0.
//For devices with A0 pulled HIGH, use 0x61
#define P1_ADDR 0x60
#define P2_T_ADDR 0x61
enum SensorState { high_state, low_state, temperature_state, pressure1_state, pressure2_state, pressure3_state, pressure4_state};
enum SensorState sensor_state = high_state;
Adafruit_MCP4725 dac_P1;
Adafruit_MCP4725 dac_P2;

//Current Sensor
//Wire2 SDA2, SCL2
Adafruit_INA219 ina219_Comp;
Adafruit_INA219 ina219_Exh(0x41);

//Wire SDA, SCL
Adafruit_INA219 ina219_Damper1;
Adafruit_INA219 ina219_Damper2(0x41);
Adafruit_INA219 ina219_Damper3(0x44);
Adafruit_INA219 ina219_Damper4(0x45);

// CAN Network
#define NUM_TX_MAILBOXES 2
#define NUM_RX_MAILBOXES 2
CANFD_message_t Tester_CMD;                       //Message Structure
CANFD_message_t Tester_CFG;                       //Message Structure
CANFD_message_t Tester_RET;                       //Message Structure
CANFD_message_t Tester_RET2;
CANFD_timings_t config;

//Globals
volatile unsigned char Process_37msTask = 0;
volatile unsigned char Process_100msTask = 0;
unsigned short Mux_State_Val = 0;
unsigned char HallFeedback = 0;
unsigned char Tester_CMD_Receive_Flag = 0;
char PS_SetV_Buf[14] = {"<01000000000>"};
char PS_ReadV_CMD_Buf[14] = {"<02000000000>"};
char PS_SetI_Buf[14] = {"<03000000000>"};
char PS_ReadI_CMD_Buf[14] = {"<04000000000>"};
char PS_Read_RSP_Buf[14] = {"<00000000000>"};
char Convert_Buf[5];
unsigned int PS_Response_Code = 0;
unsigned int Timer100msCntr = 0;

/*float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float power_mW = 0;
float shuntvoltage_b = 0;
float busvoltage_b = 0;
float current_mA_b = 0;
float loadvoltage_b = 0;
float power_mW_b = 0;
*/

float Compressor_I = 0;
float Exhaust_I = 0;
float Damper1_I = 0;
float Damper2_I = 0;
float Damper3_I = 0;
float Damper4_I = 0;

short Compressor_I_16 = 0;
short Exhaust_I_16 = 0;
short Damper1_I_16 = 0;
short Damper2_I_16 = 0;
short Damper3_I_16 = 0;
short Damper4_I_16 = 0;

unsigned char debug = 0;

//CAN CMD Received Variables (declare as volatile)
volatile unsigned short PWM_LF_DC_Cmd = 0x0;
volatile unsigned short PWM_RF_DC_Cmd = 0x0;
volatile unsigned short PWM_LR_DC_Cmd = 0x0;
volatile unsigned short PWM_RR_DC_Cmd = 0x0;
volatile unsigned short PWM_LF_Freq_Cmd = 0x0;
volatile unsigned short PWM_RF_Freq_Cmd = 0x0;
volatile unsigned short PWM_LR_Freq_Cmd = 0x0;
volatile unsigned short PWM_RR_Freq_Cmd = 0x0;
volatile unsigned short P1_Cmd = 0x0;
volatile unsigned short P2_Cmd = 0x0;
volatile unsigned short Temperature_Cmd = 0x0;
volatile unsigned short Set_Voltage_PS = FALSE;
volatile unsigned short Set_Current_PS = FALSE;
volatile unsigned short Send_232 = AVAIL;
volatile unsigned short PS_ReadV = 0;
volatile unsigned short PS_ReadI = 0;

//CAN CFG Received Variables (declare as volatile)
volatile unsigned int PWM_LF_Freq_Cfg = 0x0;
volatile unsigned int PWM_RF_Freq_Cfg = 0x0;
volatile unsigned int PWM_LR_Freq_Cfg = 0x0;
volatile unsigned int PWM_RR_Freq_Cfg = 0x0;
volatile unsigned short ECU_Relay_Cfg = 0x0;
volatile unsigned short IGN_HW_Cfg = 0x0;
volatile unsigned short Disable_P1_Outp_Cfg = 0x0;
volatile unsigned short Disable_P2T_Outp_Cfg = 0x0;
volatile unsigned short Disable_DMP1_Fdbk_Cfg = 0x0;
volatile unsigned short Disable_DMP2_Fdbk_Cfg = 0x0;
volatile unsigned short Disable_DMP3_Fdbk_Cfg = 0x0;
volatile unsigned short Disable_DMP4_Fdbk_Cfg = 0x0;
volatile unsigned short Disable_CMP_Fdbk_Cfg = 0x0;
volatile unsigned short Disable_EX_Fdbk_Cfg = 0x0;
volatile unsigned int PS_Voltage_CMD;
volatile unsigned int PS_Voltage_FB;
volatile unsigned int PS_Current_CMD;
volatile unsigned int PS_Current_FB;
float fVoltage_CMD;
float fCurrent_CMD;

//Timer Setup
PeriodicTimer Tmr_37_25ms; // generate a timer from the pool (Pool: 2xGPT, 16xTMR(QUAD), 20xTCK)
PeriodicTimer Tmr_100ms; // generate a timer from the pool (Pool: 2xGPT, 16xTMR(QUAD), 20xTCK)

void setup() {
Serial.begin(115200);
Serial5.begin(9600);
pinMode(1, OUTPUT);

pinMode(LF, INPUT_PULLDOWN);
pinMode(RF, INPUT_PULLDOWN);
pinMode(LR, INPUT_PULLDOWN);
pinMode(RR, INPUT_PULLDOWN);
pinMode(RES1, INPUT_PULLDOWN);
pinMode(RES2, INPUT_PULLDOWN);
pinMode(BST, INPUT_PULLDOWN);
pinMode(DIV, INPUT_PULLDOWN);

pinMode(strobe_100ms, OUTPUT);
pinMode(strobe_1, OUTPUT);
pinMode(strobe_2, OUTPUT);
pinMode(strobe_3, OUTPUT);
pinMode(strobe_4, OUTPUT);
pinMode(LED, OUTPUT);
pinMode(ECU_Relay_Pin, OUTPUT);
pinMode(IGN_HW_Pin, OUTPUT);

//PWM Setup
analogWriteResolution(15);                        //PWM Value 0-32757
analogWriteFrequency(position_LF_PWM_Pin, 2000);  //Set Frequency to 2kHz
analogWriteFrequency(position_RF_PWM_Pin, 2000);  //Set Frequency to 2kHz
analogWriteFrequency(position_LR_PWM_Pin, 2000);  //Set Frequency to 2kHz
analogWriteFrequency(position_RR_PWM_Pin, 2000);  //Set Frequency to 2kHz
//End PWM Setup

//Digital to Analog Setup
dac_P1.begin(P1_ADDR,&Wire2);                     //&Wire2 selects SCL2 and SDA2
dac_P2.begin(P2_T_ADDR,&Wire2);                   //&Wire2 selects SCL2 and SDA2
//End D2A Setup

//Current Sensor Setup
  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).
if (! ina219_Comp.begin(&Wire2)) {                //&Wire2 selects SCL2 and SDA2
  Serial.println("Failed to find INA219 W2 0x40 chip (Compressor)");
  while (1) { delay(10); }
}
if (! ina219_Exh.begin(&Wire2)) {                 //&Wire2 selects SCL2 and SDA2
  Serial.println("Failed to find INA219 W2 0x41 chip (Exhaust)");
  while (1) { delay(10); }
}

if (! ina219_Damper1.begin(&Wire)) {                //&Wire selects SCL and SDA
  Serial.println("Failed to find INA219 W 0x40 chip (Damper 1)");
  while (1) { delay(10); }
}

if (! ina219_Damper2.begin(&Wire)) {                //&Wire selects SCL and SDA
  Serial.println("Failed to find INA219 W 0x41 chip (Damper 2)");
  while (1) { delay(10); }
}
if (! ina219_Damper3.begin(&Wire)) {                //&Wire selects SCL and SDA
  Serial.println("Failed to find INA219 W 0x44 chip (Damper 3)");
  while (1) { delay(10); }
}
if (! ina219_Damper4.begin(&Wire)) {                //&Wire selects SCL and SDA
  Serial.println("Failed to find INA219 W 0x45 chip (Damper 4)");
  while (1) { delay(10); }
}

//CAN Setup
Can3.begin();
config.clock = CLK_24MHz;
config.baudrate = 500000;
config.baudrateFD = 2000000; // 2000kbps data rate
config.propdelay = 190;
config.bus_length = 1;
config.sample = 80;

Can3.setBaudRate(config);
//FLEXCAN_RXTX mode;
//mode=LISTEN_ONLY;
//Can3.setBaudRateAdvanced(config,1,1,mode);

Can3.setRegions(64); // returns a value of 14 (mailboxes), each one supporting 64 bytes payload
//Can3.setMaxMB(NUM_TX_MAILBOXES + NUM_RX_MAILBOXES);
for (int i = 0; i<NUM_RX_MAILBOXES; i++){
  Can3.setMB((FLEXCAN_MAILBOX)i,RX,STD);
}
for (int i = NUM_RX_MAILBOXES; i<(NUM_TX_MAILBOXES + NUM_RX_MAILBOXES); i++){
  Can3.setMB((FLEXCAN_MAILBOX)i,TX,STD);
}
//Setup Tester_RET CAN message
Tester_RET.len = 20;
Tester_RET.id = 0x18E;

Tester_RET2.len = 12;
Tester_RET2.id = 0x28E;

Can3.setMBFilter(REJECT_ALL);
//Setup Tester_CFG CAN message
//Tester_CFG.id = 0x68D;
Can3.enableMBInterrupt(MB0);              // Assign Tester_CFG to MB0
Can3.onReceive(MB0, Tester_CFG_Receive);  // allows mailbox 0 messages to be received in the supplied callback.
Can3.setMBFilter(MB0,0x068D);

//Setup Tester_CMD CAN message
//Tester_CMD.id = 0x18D;
Can3.enableMBInterrupt(MB1); // Assign Tester_CMD to MB1
Can3.onReceive(MB1, Tester_CMD_Receive);  // allows mailbox 0 messages to be received in the supplied callback.
Can3.setMBFilter(MB1,0x018D);
//Can3.mailboxStatus();                     // prints to the serial monitor

digitalWrite(LED, HIGH);

// Start Timer for Interrupts
Tmr_37_25ms.begin(Task_37_25ms, 37.25ms); // 37.250ms
Tmr_100ms.begin(Task_100ms, 100ms);       // 100ms

}

void loop() {
  // put your main code here, to run repeatedly:
Can3.events();
if (Process_100msTask == 1) 
{
  digitalWrite(strobe_100ms, HIGH);
  Process_100msTask = 0;
  analogWrite(position_LF_PWM_Pin, PWM_LF_DC_Cmd);
  analogWrite(position_RF_PWM_Pin, PWM_RF_DC_Cmd);
  analogWrite(position_LR_PWM_Pin, PWM_LR_DC_Cmd);
  analogWrite(position_RR_PWM_Pin, PWM_RR_DC_Cmd);
  
  if (Disable_CMP_Fdbk_Cfg == 0)
  {   Compressor_I = ina219_Comp.getCurrent_mA();  }
  if (Disable_EX_Fdbk_Cfg == 0)
  {   Exhaust_I = ina219_Exh.getCurrent_mA();  }
  if (Disable_DMP1_Fdbk_Cfg == 0)
  {   Damper1_I = ina219_Damper1.getCurrent_mA();  }
  if (Disable_DMP2_Fdbk_Cfg == 0)
  {   Damper2_I = ina219_Damper2.getCurrent_mA();  }
  if (Disable_DMP3_Fdbk_Cfg == 0)
  {   Damper3_I = ina219_Damper3.getCurrent_mA();  }
  if (Disable_DMP4_Fdbk_Cfg == 0)
  {   Damper4_I = ina219_Damper4.getCurrent_mA();  }
 
  //Collect Inputs from Hall Effect Sensors
  HallFeedback = ((digitalRead(LF)<<7) + (digitalRead(RF)<<6) + (digitalRead(LR)<<5) + (digitalRead(RR)<<4) + (digitalRead(RES1)<<3) + (digitalRead(RES2)<<2) + (digitalRead(BST)<<1) + (digitalRead(DIV)));

  //Send Tester_RET
  Tester_RET.buf[0] = HallFeedback;
  Compressor_I_16 = (short)(Compressor_I);
  Tester_RET.buf[1] = Compressor_I_16/0x100;
  Tester_RET.buf[2] = Compressor_I_16&0xFF;
  Exhaust_I_16 = (short)(Exhaust_I);
  Tester_RET.buf[3] = Exhaust_I_16/0x100;
  Tester_RET.buf[4] = Exhaust_I_16&0xFF;
  Damper1_I_16 = (short)(Damper1_I);
  Tester_RET.buf[5] = Damper1_I_16/0x100;
  Tester_RET.buf[6] = Damper1_I_16&0xFF;
  Damper2_I_16 = (short)(Damper2_I);
  Tester_RET.buf[7] = Damper2_I_16/0x100;
  Tester_RET.buf[8] = Damper2_I_16&0xFF;
  Damper3_I_16 = (short)(Damper3_I);
  Tester_RET.buf[9] = Damper3_I_16/0x100;
  Tester_RET.buf[10] = Damper3_I_16&0xFF;
  Damper4_I_16 = (short)(Damper4_I);
  Tester_RET.buf[11] = Damper4_I_16/0x100;
  Tester_RET.buf[12] = Damper4_I_16&0xFF;
  Tester_RET.buf[13] = PS_Voltage_FB/0x100;
  Tester_RET.buf[14] = PS_Voltage_FB&0xFF;
  Tester_RET.buf[15] = PS_Current_FB/0x100;
  Tester_RET.buf[16] = PS_Current_FB&0xFF;
  if (Tester_CMD_Receive_Flag == 1)
  {
    Can3.write(MB2, Tester_RET);     // Write to mailbox 2 (provided it's a transmit mailbox)
    Tester_CMD_Receive_Flag = 0;
    //NOTE: This CAN write will overflow the 100ms loop if the tester is not connected to a CAN node
  }
//  Can3.write(MB3, Tester_RET2);     // Write to mailbox 2 (provided it's a transmit mailbox)
//  Can3.mailboxStatus();
  digitalWrite(strobe_100ms, LOW);
}// Process_100msTask

if (Process_37msTask == 1) 
{
  digitalWrite(strobe_1, HIGH);
  Process_37msTask = 0;
  switch (sensor_state) 
  {
	case high_state: Mux_State_Val = 0xFFF; sensor_state = low_state; break;
	case low_state: Mux_State_Val = 0; sensor_state = temperature_state; break;
	case temperature_state: Mux_State_Val = Temperature_Cmd; sensor_state = pressure1_state; break;
	case pressure1_state: Mux_State_Val = P2_Cmd; sensor_state = pressure2_state; break;
	case pressure2_state: Mux_State_Val = P2_Cmd; sensor_state = pressure3_state; break;
	case pressure3_state: Mux_State_Val = P2_Cmd; sensor_state = pressure4_state; break;
	case pressure4_state: Mux_State_Val = P2_Cmd; sensor_state = high_state; break;
	default: sensor_state = high_state; break;
  }

  if (Disable_P1_Outp_Cfg == 0)
  {   dac_P1.setVoltage(P1_Cmd, false);  }
  if (Disable_P2T_Outp_Cfg == 0)
  {   dac_P2.setVoltage(Mux_State_Val, false);  }
  digitalWrite(strobe_1, LOW);
} //Process_37msTask

/////////////  Power Supply Set Voltage
if (Set_Voltage_PS && Serial5.availableForWrite() && (Send_232==AVAIL))
{
  Send_232 = UNAVAIL;

  fVoltage_CMD = (float)PS_Voltage_CMD/100;
  if (fVoltage_CMD > 30.0)
  { fVoltage_CMD = 30.0;}
    
  if (fVoltage_CMD < 0.0)
  { fVoltage_CMD = 0;}

  if (fVoltage_CMD < 10.0)
  {
    dtostrf(fVoltage_CMD,3,2,Convert_Buf);
    PS_SetV_Buf[4] = 0x30;
    PS_SetV_Buf[5] = Convert_Buf[0];
    PS_SetV_Buf[6] = Convert_Buf[2];
    PS_SetV_Buf[7] = Convert_Buf[3];
  }
  else
  {
    dtostrf(fVoltage_CMD,4,2,Convert_Buf);
    PS_SetV_Buf[4] = Convert_Buf[0];
    PS_SetV_Buf[5] = Convert_Buf[1];
    PS_SetV_Buf[6] = Convert_Buf[3];
    PS_SetV_Buf[7] = Convert_Buf[4];
  }

  for (unsigned int cnt = 0; cnt < strlen(PS_SetV_Buf); cnt++)
  {
    Serial5.print(PS_SetV_Buf[cnt]);
  }
  Set_Voltage_PS = FALSE;
}

/////////////  Power Supply Set Current
if (Set_Current_PS && Serial5.availableForWrite() && (Send_232==AVAIL))
{
  Send_232 = UNAVAIL;

  fCurrent_CMD = (float)PS_Current_CMD/100;
  if (fCurrent_CMD > 30.0)
  { fCurrent_CMD = 30.0;}

  if (fCurrent_CMD < 0.0)
  { fCurrent_CMD = 0;}

  if (fCurrent_CMD < 10.0)
  {
    dtostrf(fCurrent_CMD,3,2,Convert_Buf);
    PS_SetI_Buf[4] = 0x30;
    PS_SetI_Buf[5] = Convert_Buf[0];
    PS_SetI_Buf[6] = Convert_Buf[2];
    PS_SetI_Buf[7] = Convert_Buf[3];
  }
  else
  {
    dtostrf(fCurrent_CMD,4,2,Convert_Buf);
    PS_SetI_Buf[4] = Convert_Buf[0];
    PS_SetI_Buf[5] = Convert_Buf[1];
    PS_SetI_Buf[6] = Convert_Buf[3];
    PS_SetI_Buf[7] = Convert_Buf[4];
  }

  for (unsigned int cnt = 0; cnt < strlen(PS_SetI_Buf); cnt++)
  {
    Serial5.print(PS_SetI_Buf[cnt]);
  }
  Set_Current_PS = FALSE;
}
/////////////  Power Supply Read Voltage/Current (every 1 second)

if (Timer100msCntr >= 5)
{
  Timer100msCntr=0;
  PS_ReadV = 1;   //Schedule to acquire V and I from PS
  PS_ReadI = 1;
}

if (PS_ReadV && Serial5.availableForWrite() && (Send_232==AVAIL))
{  
  Send_232 = UNAVAIL;
  
  PS_ReadV = 0;
  for (unsigned int cnt = 0; cnt < strlen(PS_ReadV_CMD_Buf); cnt++)
  {
    Serial5.print(PS_ReadV_CMD_Buf[cnt]);
  }
}

if (PS_ReadI && Serial5.availableForWrite() && (Send_232==AVAIL))
{  
  Send_232 = UNAVAIL;

  PS_ReadI = 0;
  for (unsigned int cnt = 0; cnt < strlen(PS_ReadI_CMD_Buf); cnt++)
  {
    Serial5.print(PS_ReadI_CMD_Buf[cnt]);
  }
}

if (Serial5.available() > 0) 
{
  Serial5.readBytes(PS_Read_RSP_Buf,13);
  PS_Response_Code = (PS_Read_RSP_Buf[1]-0x30) * 10 + (PS_Read_RSP_Buf[2]-0x30);
  switch (PS_Response_Code) 
  {
	case SetVFB_ID: 
    //do nothing
    break;
	case VoltageFB_ID:
    PS_Voltage_FB = (PS_Read_RSP_Buf[4]-0x30)*1000 + (PS_Read_RSP_Buf[5]-0x30)*100 + (PS_Read_RSP_Buf[6]-0x30)*10 + (PS_Read_RSP_Buf[7]-0x30);
    break;
	case SetIFB_ID: 
    //do nothing
    break;
	case CurrentFB_ID: 
    PS_Current_FB = (PS_Read_RSP_Buf[4]-0x30)*1000 + (PS_Read_RSP_Buf[5]-0x30)*100 + (PS_Read_RSP_Buf[6]-0x30)*10 + (PS_Read_RSP_Buf[7]-0x30);
    break;
	default: break;
  }
  Send_232 = AVAIL;
}

if (debug)
{
  Serial.print("C1 Current: "); Serial.print(Compressor_I);Serial.print(" E1 Current: "); Serial.print(Exhaust_I); Serial.println(" mA");
  Serial.print("D1 Current: "); Serial.print(Damper1_I);Serial.print(" D2 Current: "); Serial.print(Damper2_I); Serial.println(" mA");
  Serial.print("D3 Current: "); Serial.print(Damper3_I);Serial.print(" D4 Current: "); Serial.print(Damper4_I); Serial.println(" mA");
}

}// End Program

////////////// INTERRUPTS ////////////////
void Task_37_25ms()
{
  Process_37msTask = 1;
} //Task_37_25ms

void Task_100ms()
{
  Timer100msCntr++;
  Process_100msTask = 1;
} //Task_100ms

void Tester_CMD_Receive(const CANFD_message_t &Tester_CMD_msg)
{
  P1_Cmd = Tester_CMD_msg.buf[0]*0x100 + Tester_CMD_msg.buf[1];
  P2_Cmd = Tester_CMD_msg.buf[2]*0x100 + Tester_CMD_msg.buf[3];
  Temperature_Cmd = Tester_CMD_msg.buf[4]*0x100 + Tester_CMD_msg.buf[5];
  PWM_LF_DC_Cmd = Tester_CMD_msg.buf[6]*0x100 + Tester_CMD_msg.buf[7];
  PWM_RF_DC_Cmd = Tester_CMD_msg.buf[8]*0x100 + Tester_CMD_msg.buf[9];
  PWM_LR_DC_Cmd = Tester_CMD_msg.buf[10]*0x100 + Tester_CMD_msg.buf[11];
  PWM_RR_DC_Cmd = Tester_CMD_msg.buf[12]*0x100 + Tester_CMD_msg.buf[13];
  Tester_CMD_Receive_Flag = 1;
} //Tester_CMD_Receive ~5us

void Tester_CFG_Receive(const CANFD_message_t &Tester_CFG_msg)
{
  unsigned int PWM_LF_Freq_Rx = 0x0;
  unsigned int PWM_RF_Freq_Rx = 0x0;
  unsigned int PWM_LR_Freq_Rx = 0x0;
  unsigned int PWM_RR_Freq_Rx = 0x0;
  unsigned int PS_Voltage_CMD_Rx;
  unsigned int PS_Current_CMD_Rx;
  unsigned short ECU_Relay_Rx = 0x0;
  unsigned short IGN_HW_Rx = 0x0;
  unsigned short Disable_P1_Outp_Rx = 0x0;
  unsigned short Disable_P2T_Outp_Rx = 0x0;
  unsigned short Disable_DMP1_Fdbk_Rx = 0x0;
  unsigned short Disable_DMP2_Fdbk_Rx = 0x0;
  unsigned short Disable_DMP3_Fdbk_Rx = 0x0;
  unsigned short Disable_DMP4_Fdbk_Rx = 0x0;
  unsigned short Disable_CMP_Fdbk_Rx = 0x0;
  unsigned short Disable_EX_Fdbk_Rx = 0x0;

//  Serial.println("Tester_CFG msg received");
  PWM_LF_Freq_Rx = Tester_CFG_msg.buf[0]*0x100 + Tester_CFG_msg.buf[1];
  if (PWM_LF_Freq_Rx != PWM_LF_Freq_Cfg)
  {
    PWM_LF_Freq_Cfg = PWM_LF_Freq_Rx;
    analogWriteFrequency(position_LF_PWM_Pin, PWM_LF_Freq_Cfg);  //Set Frequency to CMD
  }
  PWM_RF_Freq_Rx = Tester_CFG_msg.buf[2]*0x100 + Tester_CFG_msg.buf[3];
  if (PWM_RF_Freq_Rx != PWM_RF_Freq_Cfg)
  {
    PWM_RF_Freq_Cfg = PWM_RF_Freq_Rx;
    analogWriteFrequency(position_RF_PWM_Pin, PWM_RF_Freq_Cfg);  //Set Frequency to CMD
  }
  PWM_LR_Freq_Rx = Tester_CFG_msg.buf[4]*0x100 + Tester_CFG_msg.buf[5];
  if (PWM_LR_Freq_Rx != PWM_LR_Freq_Cfg)
  {
    PWM_LR_Freq_Cfg = PWM_LR_Freq_Rx;
    analogWriteFrequency(position_LR_PWM_Pin, PWM_LR_Freq_Cfg);  //Set Frequency to CMD
  }
  PWM_RR_Freq_Rx = Tester_CFG_msg.buf[6]*0x100 + Tester_CFG_msg.buf[7];
  if (PWM_RR_Freq_Rx != PWM_RR_Freq_Cfg)
  {
    PWM_RR_Freq_Cfg = PWM_RR_Freq_Rx;
    analogWriteFrequency(position_RR_PWM_Pin, PWM_RR_Freq_Cfg);  //Set Frequency to CMD
  }

  PS_Voltage_CMD_Rx = (Tester_CFG_msg.buf[9] + ((Tester_CFG_msg.buf[8] & 0x0F)*0x100));
  if (PS_Voltage_CMD_Rx != PS_Voltage_CMD)
  {
    PS_Voltage_CMD = PS_Voltage_CMD_Rx;
    Set_Voltage_PS = TRUE;
  }

  PS_Current_CMD_Rx = (Tester_CFG_msg.buf[11] + ((Tester_CFG_msg.buf[10] & 0x0F)*0x100));
  if (PS_Current_CMD_Rx != PS_Current_CMD)
  {
    PS_Current_CMD = PS_Current_CMD_Rx;
    Set_Current_PS = TRUE;
  }

  ECU_Relay_Rx = (Tester_CFG_msg.buf[12] & 0x01);
  if (ECU_Relay_Rx != ECU_Relay_Cfg)
  {
    ECU_Relay_Cfg = ECU_Relay_Rx;
    digitalWrite(ECU_Relay_Pin, ECU_Relay_Cfg);
  }

  IGN_HW_Rx = ((Tester_CFG_msg.buf[12] & 0x10)>>4);
  if (IGN_HW_Rx != IGN_HW_Cfg)
  {
    IGN_HW_Cfg = IGN_HW_Rx;
    digitalWrite(IGN_HW_Pin, IGN_HW_Cfg);
  }

  Disable_P1_Outp_Rx = ((Tester_CFG_msg.buf[12] & 0x04)>>2);
  if (Disable_P1_Outp_Rx != Disable_P1_Outp_Cfg)
  {
    Disable_P1_Outp_Cfg = Disable_P1_Outp_Rx;
  }

  Disable_P2T_Outp_Rx = ((Tester_CFG_msg.buf[12] & 0x02)>>1);
  if (Disable_P2T_Outp_Rx != Disable_P2T_Outp_Cfg)
  {
    Disable_P2T_Outp_Cfg = Disable_P2T_Outp_Rx;
  }

  Disable_DMP1_Fdbk_Rx = (Tester_CFG_msg.buf[13] & 0x01);
  if (Disable_DMP1_Fdbk_Rx != Disable_DMP1_Fdbk_Cfg)
  {
    Disable_DMP1_Fdbk_Cfg = Disable_DMP1_Fdbk_Rx;
    if (Disable_DMP1_Fdbk_Cfg)
    { Damper1_I = 0;}
  }

  Disable_DMP2_Fdbk_Rx = ((Tester_CFG_msg.buf[13] & 0x02)>>1);
  if (Disable_DMP2_Fdbk_Rx != Disable_DMP2_Fdbk_Cfg)
  {
    Disable_DMP2_Fdbk_Cfg = Disable_DMP2_Fdbk_Rx;
    if (Disable_DMP2_Fdbk_Cfg)
    { Damper2_I = 0;}
  }

  Disable_DMP3_Fdbk_Rx = ((Tester_CFG_msg.buf[13] & 0x04)>>2);
  if (Disable_DMP3_Fdbk_Rx != Disable_DMP3_Fdbk_Cfg)
  {
    Disable_DMP3_Fdbk_Cfg = Disable_DMP3_Fdbk_Rx;
    if (Disable_DMP3_Fdbk_Cfg)
    { Damper3_I = 0;}
  }

  Disable_DMP4_Fdbk_Rx = ((Tester_CFG_msg.buf[13] & 0x08)>>3);
  if (Disable_DMP4_Fdbk_Rx != Disable_DMP4_Fdbk_Cfg)
  {
    Disable_DMP4_Fdbk_Cfg = Disable_DMP4_Fdbk_Rx;
    if (Disable_DMP4_Fdbk_Cfg)
    { Damper4_I = 0;}
  }

  Disable_CMP_Fdbk_Rx = ((Tester_CFG_msg.buf[12] & 0x80)>>7);
  if (Disable_CMP_Fdbk_Rx != Disable_CMP_Fdbk_Cfg)
  {
    Disable_CMP_Fdbk_Cfg = Disable_CMP_Fdbk_Rx;
    if (Disable_CMP_Fdbk_Cfg)
    { Compressor_I = 0;}
  }

  Disable_EX_Fdbk_Rx = ((Tester_CFG_msg.buf[12] & 0x40)>>6);
  if (Disable_EX_Fdbk_Rx != Disable_EX_Fdbk_Cfg)
  {
    Disable_EX_Fdbk_Cfg = Disable_EX_Fdbk_Rx;
    if (Disable_EX_Fdbk_Cfg)
    { Exhaust_I = 0;}
  }
}
