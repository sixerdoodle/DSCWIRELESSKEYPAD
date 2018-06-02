//
// Implement DSC KeyBus protocol to emulate a virtual keypad.  
// Virtual keypard is a slave to the master security controller
// clock pulse is ~ 500us long, so if we've got 1000us (ie 1ms) then we're in the between frame gap
//
// D0 = Clock (Yellow wire)
// D1 = Data (Green wire)
//
// seems packet is command (8 bit) Zero padding (1 bit) Data (x*8Bits) then checksum (8bits)
// looks like data held maybe 2us after clock?
//
// clock high 500us, then low 500us
// frame start is 5ms high but up to 10ms high

#define Clock 1
#define Data 0
#define DataSend 2
#define Trigger 3
#define Switch 4
#define BoardLED 7
#define LED1 A4
#define LED2 A3
#define LED3 A2
#define LED4 A2
#define LED5 A0
#define LED6 A5

//
// Storage for what we figured out about the various packets
//
uint8_t AlarmStatus[10];  // [0]panel status, [1]panel string, [2]open zones 1-8, [3]Bus Error code, [4-9]YYMMDDHHMM, [10]RSSI
uint8_t OldAlarmStatus[10];
#define S_PanelStatus 0
#define S_PanelString 1
#define S_OpenZonesA 2
#define S_BusErr 3
#define S_YY 4
#define S_MM 5
#define S_DD 6
#define S_HH 7
#define S_MN 8
#define S_RSSI 9

uint32_t TickCnt;
bool LEDToggle;
bool First;
uint8_t Frame[128];         // all possible master sent bytes in the frame
uint8_t OldFrame[128];
uint8_t SFrame[128];
char OutStr[256];
uint8_t FrameCnt;
uint8_t CurKey;         // which key we're currently working on
uint8_t XmitKeys[16];   // null terminated list of keys to press (using key codes)
bool Switcher;
void ReleaseData(void);


int currentTime;
int TCPPing;
#define TCPPingLimit 400

UDP U;
byte syslog_addy[] = { 0,0,0,0 }; // configure IP address running a syslog server
uint16_t syslog_prt = 514;

byte UDPEvt_addy[] = { 0,0,0,0 }; // configure IP address to recieve UDP broadcast on alarm status change
uint16_t UDPEvt_prt = 3127;       // UDP port at that IP address....

typedef struct item_t { const uint8_t key; const uint8_t code;} item_t;
item_t KeyTable[] = {
    {'0',0x03 },
    {'1',0x0B },
    {'2',0x15 },
    {'3',0xBF },
    {'4',0x23 },
    {'5',0x2D },
    {'6',0x37 },
    {'7',0x39 },
    {'8',0x45 },
    {'9',0x4F },
    {'#',0x5B },
    {'*',0x51 },
    { NULL, 0 }
};
  
TCPServer MyTCP_Server = TCPServer(nnnn);  // configure port number
TCPClient MyTCP_Client;

SYSTEM_THREAD(ENABLED);


void setup() {
    
    WiFi.selectAntenna(ANT_EXTERNAL);
    
    pinMode(Clock,INPUT);
    pinMode(Data,INPUT);
    pinMode(DataSend,OUTPUT);
    pinSetFast(DataSend);           // high level keeps LED off, which keeps data pulldown inactive
    pinMode(BoardLED,OUTPUT);
    pinMode(Trigger,OUTPUT);
    pinMode(Switch,INPUT_PULLUP);
    pinMode(LED1,OUTPUT);
    pinMode(LED2,OUTPUT);
    pinMode(LED3,OUTPUT);
    pinMode(LED4,OUTPUT);
    pinMode(LED5,OUTPUT);
    pinMode(LED6,OUTPUT);
    LEDToggle = false;
    CurKey = 4;
    XmitKeys[4] = 0x00; // terminator
    
    // wait here for WiFi  to connect
    waitFor(WiFi.ready,60000);          // wait for a minute to make sure WiFi is ready
    
    delayMicroseconds(500);
    
    waitFor(Particle.connected,60000); // wait for another minute to make sure connected to particle cloud
    
    TCPPing = Time.now() + TCPPingLimit;                // maximum time to wait without a TCP ping from the server
    MyTCP_Server.begin();
        
    U.stop();
    U.begin(syslog_prt);
    U.sendPacket("SETUP", sizeof("SETUP")-1, syslog_addy, syslog_prt);
    
    First = false;
    delayMicroseconds(500);
    Switcher = true;                // have not triggered the switch yet
    
    currentTime = Time.now();
    TCPPing = currentTime + TCPPingLimit;
    
}

//
// Interrupt routine for rising clock as that's where
// we want to always release any data we're sending
//
void ReleaseData()
{
  pinSetFast(DataSend);      // set to 1 to release and float
}

//
// lookup keycodes by keys
//
int lookup(const uint8_t name)
{
  for (item_t *p = KeyTable; p->key != NULL; ++p) {
      if (p->key == name) {
          return p->code;
      }
  }
  return 0xff;
}

void loop() {
    
    uint8_t MasterBit, MasterByte;
    uint8_t SlaveBit, SlaveByte;
    uint8_t BitCnt;
    bool ValidData;
    int ii,jj;
    char TempStr[32];
    uint8_t XmitByte;
    uint8_t XmitBit;
    bool DoXmitKey;
    uint8_t mbit;
    uint8_t sbit;
    uint8_t BusErr;
    
    currentTime = Time.now();
    
    //Particle.process();                 // do some particle processing so we don't hang
    if( (pinReadFast(Switch) == LOW) && Switcher){
        Switcher = false;
        CurKey = 0;                     // first time we ground the switch, transmit they keys
    }
    
    if (MyTCP_Client.connected()) {
        waitFor(MyTCP_Client.available,10000);
        if (MyTCP_Client.available()) {
            char PacketBuffer[64];
            int cnt;
            cnt = MyTCP_Client.read((uint8_t*)PacketBuffer,63);
            PacketBuffer[cnt] = 0; // make sure null terminated
            if(strcmp(PacketBuffer,"Ping")==0){                 // allows us to do a heartbeat from outside, use to reset the Photon if network is fubar
                    TCPPing = Time.now() + TCPPingLimit;
            }
            if(PacketBuffer[0] == 'K'){                         // then it's a KeyPress command
                strcpy((char*)XmitKeys,(char*)&PacketBuffer[1]);              // start after the K
                CurKey = 0;
                MyTCP_Client.write("OK");
            }
            if(PacketBuffer[0] == 'S'){                         // then it's a Status command
                MyTCP_Client.write(AlarmStatus,sizeof(AlarmStatus));
            }
            if(strcmp(PacketBuffer,"Ping")==0){TCPPing = currentTime + TCPPingLimit;} 
        }
        MyTCP_Client.stop(); 
        MyTCP_Client = NULL;
    } else {
        MyTCP_Client.stop(); 
        MyTCP_Client = NULL;
        MyTCP_Client = MyTCP_Server.available();  // if no client is yet connected, check for a new connection
    }
    
    if(currentTime > TCPPing){ System.reset();}
    
    //
    // roughly go through this section of code every 50ms, so 200x through
    // is about 1 second, so 24,000 times is 2 minutes
    //
    SINGLE_THREADED_BLOCK() {           // I want to make sure nothing interrupts this as timing is critical
        //
        // find beginning of frame
        //
        
        //
        // wait until I have 1ms of clock high, that's a good start of frame
        // 
        ValidData = false;
        BusErr = 1;
        TickCnt = 0;
        while(pinReadFast(Clock) == HIGH){                       // make sure we have at least 1ms of high
            delayMicroseconds(10);                               // pauses for 10us
            if(TickCnt++ > 100 ){ValidData = true; BusErr = 0; break;}  // if we've had at least 1ms high, then we're good so far
        }
        //
        // Now wait until the clock low as the official end of start frame
        //
        if(ValidData){
            TickCnt = 0;
            while(pinReadFast(Clock) == HIGH){                  // make sure we have no more than 15ms of high
                delayMicroseconds(10);
                if(TickCnt++ > 10000 ){ValidData = false; BusErr = 2; break;}  // if we still high after 100ms, then can't have found start, seems runs high really long after a transition
            }
            delayMicroseconds(100);                             // this is where we'd read if we wanted this bit
        }
        //
        // if we had the long high, then a low, we're at the start of the data
        //
        if(ValidData){
            
            //
            // Read one data byte
            //
            MasterByte = SlaveByte = 0;
            BitCnt = 0;
            FrameCnt = 0;
            do {
                MasterByte = MasterByte << 1;
                TickCnt = 0;
                while(pinReadFast(Clock) == LOW){     // Wait for clock to go high to get master bit
                    //delayMicroseconds(12);
                    if(TickCnt++ > 2147483647 ){ValidData = false; BusErr = 3; break;}  // if we still high after 750us, then can't have valid data (already waited 100us to read keypad data bit, so 13 rather than 15)
                }
                if(!ValidData)break;                // stop the loop if we got invalid data
                delayMicroseconds(12);
                //pinSetFast(Trigger);
                MasterByte += pinReadFast(Data);
                //pinResetFast(Trigger);
                SlaveByte = SlaveByte << 1;
                TickCnt = 0;
                while(pinReadFast(Clock) == HIGH){     // Wait for clock to go low to get master bit
                    //delayMicroseconds(12);
                    if(TickCnt++ > 2147483647 ){ValidData = false; BusErr = 4; break;}  // if we still low after 750us, then can't have valid data
                }
                if(!ValidData)break;                // stop the loop if we got invalid data
                delayMicroseconds(150);             // wait at least 100us before we read keypad data to give time to set up.  LTE interface is alike 2us, but keypad is like 56us
                //pinSetFast(Trigger);
                SlaveByte += pinReadFast(Data);
                //pinResetFast(Trigger);
            } while(BitCnt++ < 7);

            //
            // Read the wacky 9th bit, pull 9'th bit low if I'm going to transmit
            //
            if(ValidData){
                TickCnt = 0;
                // see if we're supposed to be transmitting a keypress
                DoXmitKey = (XmitKeys[CurKey] != 0) && (MasterByte == 0x05); // simplicy test as we're going to use this a couple of times and I want FAST execution
                while(pinReadFast(Clock) == LOW){     // Wait for clock to go high to get master bit
                    //delayMicroseconds(12);
                    if(TickCnt++ > 2147483647 ){ValidData = false; BusErr = 5; break;}  // if we still high after 750us, then can't have valid data  (already waited 100us to read keypad data bit, so 13 rather than 15)
                }
                delayMicroseconds(12);
                //pinSetFast(Trigger);
                mbit += pinReadFast(Data);
                if(DoXmitKey){  // we only transmit if we have keys and if we're inside a 0x05 command
                    XmitByte = lookup(XmitKeys[CurKey]);
                    attachInterrupt(Clock, ReleaseData, RISING, 6 );
                }
                //pinResetFast(Trigger);
                TickCnt = 0;
                while(pinReadFast(Clock) == HIGH){     // Wait for clock to go low to get master bit
                   // delayMicroseconds(6);
                    if(TickCnt++ > 2147483647 ){ValidData = false; BusErr = 6; break;}  // if we still low after 750us, then can't have valid data
                }
                if(DoXmitKey){  // we only transmit if we have keys and if we're inside a 0x05 command
                    //XmitByte = XmitKeys[CurKey];
                    pinResetFast(DataSend);        // set to 0 to pull data low
                    delayMicroseconds(20);      // wait for 20us  and sampl 
                    //pinSetFast(Trigger);
                    sbit = pinReadFast(Data); 
                    //pinResetFast(Trigger);
                    TickCnt = 0;
                    while(pinReadFast(Clock) == LOW){     // spin here while the clock is low, release as soo as it goes high
                        if(TickCnt++ > 2147483647 ){ValidData = false; BusErr = 7; break;}  // don't wait forever
                    }
                    //pinSetFast(DataSend);      // set to 1 to release and float  (no longer need to release on Low to High transition, handled via interrupt)
                } else {
                    delayMicroseconds(150);
                    //pinSetFast(Trigger);
                    sbit = pinReadFast(Data);
                    //pinResetFast(Trigger);
                }
            }
            
            //
            // Continue to read bytes until they stop
            //
            if(ValidData){
                //
                // Loop reading bytes until the clock stops
                //
                do{
                    //
                    // Read one data byte
                    //
                    Frame[FrameCnt] = MasterByte;
                    SFrame[FrameCnt++] = SlaveByte;
                    BitCnt = MasterByte = SlaveByte = 0;
                    do {
                        MasterByte = MasterByte << 1;
                        TickCnt = 0;
                        while(pinReadFast(Clock) == LOW){     // Wait for clock to go high to get master bit
                            delayMicroseconds(1);
                            if(TickCnt++ > 500 ){ break;}  // if we still high after 500, then can't have valid data  (already waited 100us to read keypad data bit, so 13 rather than 15)
                        }
                        if(TickCnt >= 500)break;                // stop the read one byte loop
                        //pinSetFast(Trigger);
                        MasterByte += pinReadFast(Data);
                        //pinResetFast(Trigger);
                        if(DoXmitKey){                              // figure this here, in the wait between clock changes
                            XmitBit = XmitByte >> 7;                // bit to transmit comes off the top
                            XmitByte = XmitByte << 1;
                        } 
                        SlaveByte = SlaveByte << 1;
                        TickCnt = 0;
                        while(pinReadFast(Clock) == HIGH){     // Wait for clock to go low to get slave bit
                            delayMicroseconds(1);
                            if(TickCnt++ > 500 ){ break;}  // if we still low after 500us, then can't have valid data
                        }
                        if(TickCnt >= 500)break;                // stop the read one byte loop
                        if(DoXmitKey){
                            if(XmitBit == 0){                   // if we have to output a zero, then pull down the line                
                                pinResetFast(DataSend);           // set to 1 to pull data low
                            }  //                               // else if we have to output a one, do nothing, the line pulls up to one automaticallly
                        }
                        delayMicroseconds(150);
                        //pinSetFast(Trigger);
                        SlaveByte += pinReadFast(Data);
                        //pinResetFast(Trigger);
                    } while(BitCnt++ < 7);
                    // end one byte
                    if(DoXmitKey){
                        DoXmitKey = false;    // we only do one key/byte with each 05 we find
                        CurKey++;             // done this one move next.
                       detachInterrupt(Clock);  // really don't need now until we start transmitting again
                    }
                } while(BitCnt == 8);
            }
        }
    }
    
#ifdef DebugOutput
    //
    // use the Trigger bit if we're trying to capture output on a scope or
    // other data capture device.  The trigger happens AFTER the bits we want to see
    // so important that the data capture device has storage
    //
    if((Frame[0] == 0x05) && (SFrame[1] != 0xff)){
        pinSetFast(Trigger);
        delayMicroseconds(50);  // flag someone tried to transmit in this frame
        pinResetFast(Trigger);
    }
#endif
    
    //
    // if we recieved a valid data frame, parse the output
    //
    AlarmStatus[S_BusErr] = BusErr;     // if we got invalid data, where did it go bad
    AlarmStatus[S_RSSI] = WiFi.RSSI();
    if(ValidData){
        switch(Frame[0]){
            case 0x05:                      // status update, no checksum, Frame[0]-Frame[3] is the status.  same in 027, 1st 4 bytes are same status
                AlarmStatus[S_PanelStatus] = Frame[1];
                AlarmStatus[S_PanelString] = Frame[2];
                break;
                
            case 0xA5:                  // time stamp and partition (this has a checksum)
                if(((Frame[0]+Frame[1]+Frame[2]+Frame[3]+Frame[4]+Frame[5]+Frame[6]) & 0xff)==Frame[7]){   // ensure valid checksum
                    AlarmStatus[S_YY] = (Frame[1] >> 4) * 10 + (Frame[1] & 0x0f);                   // the last two digits of the year xxYY
                    AlarmStatus[S_MM] = (Frame[2] >> 2) & 0x0f;     // month MM
                    AlarmStatus[S_DD] = ((Frame[2] << 3) + (Frame[3] >> 5)) & 0x1f; // day DD
                    AlarmStatus[S_HH] = (Frame[3] ) & 0x1f; // hr HH
                    AlarmStatus[S_MN] = (Frame[4] >> 2 ) & 0x3f; // minutes MN
                    //ii = Frame[2] >> 6;             // partition number (not sure what to do with that yet)
                }
                break;
                
            case 0x11:                  // query keyboards
                break;
                
            case 0x16:                  // 160E23F53C 
                break;
                
            case 0x5D:                  // alarm memory group 1
                break;
                
            case 0x0A:                  // Seems to mirror 1st 2 bytes (status) of 05 cmd, plus other stuff
                break;
                
            case 0xB1:                  // "SENSOR CFG"
                break;
                
            case 0x27:                  // status update zone 1-8
                if(((Frame[0]+Frame[1]+Frame[2]+Frame[3]+Frame[4]+Frame[5]) & 0xff)==Frame[6]){   // ensure valid checksum
                    AlarmStatus[S_OpenZonesA] = Frame[5];
                    // S_OpenZonesB
                }
                break;
        }
        pinResetFast(LED4); // no error in the bus
    } else {
        pinSetFast(LED4);   // show an error in the bus
    }
        // Ready LED
    if((AlarmStatus[S_PanelStatus] & (1<<0)) != 0 ){
        pinSetFast(LED1);
    } else {
        pinResetFast(LED1);
    }
    // Armed LED (flash every other second)
    if(( (AlarmStatus[S_PanelStatus] & (1<<1)) != 0) && ((Time.second() & 1) == 0) ){
        pinSetFast(LED2);
    } else {
        pinResetFast(LED2);
    }
    if(AlarmStatus[S_OpenZonesA] != 0 ){
        pinSetFast(LED6);
    } else {
        pinResetFast(LED6);
    }
    // System/Memory LED
    if((AlarmStatus[S_PanelStatus] & (1<<2)) != 0 ){
        pinSetFast(LED3);
    } else {
        pinResetFast(LED3);
    }
    if(AlarmStatus[S_PanelStatus] != OldAlarmStatus[S_PanelStatus])
    {
        for (ii = 0; ii < sizeof(AlarmStatus); ii++){OldAlarmStatus[ii] = AlarmStatus[ii];}
        if(U.sendPacket(AlarmStatus, sizeof(AlarmStatus), UDPEvt_addy, UDPEvt_prt) < 0 ){
            U.stop();
            waitFor(WiFi.ready,10000);
            U.begin(syslog_prt);
            U.sendPacket(AlarmStatus, sizeof(AlarmStatus), UDPEvt_addy, UDPEvt_prt);
        }
    }
    
    //
    // debug output
    //
//    if(1==0){
    if(ValidData){

        for (ii = 0; ii < FrameCnt; ii++){
            if(Frame[ii] != OldFrame[ii])break;     // see if this one is different
            if(SFrame[ii] != 0xff)break;     // see if this one is different
        }
        if( (ii != FrameCnt)){                         // then the new frame is different, so send it
            for (ii = 0; ii < FrameCnt; ii++){
                OldFrame[ii] = Frame[ii];     // remember this one
            }
            char *ptr = OutStr;
            if(mbit){
                sprintf(ptr, "%02XH-", Frame[0]);  // command byte
            } else {
                sprintf(ptr, "%02XL-", Frame[0]);  // command byte
            }
            ptr += 4;
            for (ii = 1; ii < FrameCnt; ii++){
                sprintf(ptr, "%02X", Frame[ii]); // rest of the frame
                ptr += 2;
            }
            if(sbit){
                sprintf(ptr, "%02XH ", SFrame[ii]);
            } else {
                sprintf(ptr, "%02XL ", SFrame[ii]);
            }
            ptr += 4;
            for (ii = 1; ii < FrameCnt; ii++){
                sprintf(ptr, "%02X", SFrame[ii]);
                ptr += 2;
            }
            //strcat(OutStr,"=");
            bool SendIt = false;
            
            switch(Frame[0]){
                case 0x05:                      // status update, no checksum, Frame[0]-Frame[3] is the status.  same in 027, 1st 4 bytes are same status
                    //if(SFrame[1] != 0xff){pinSetFast(Trigger);}
                    //GetStatus(OutStr);
                    //pinResetFast(Trigger);
                    if((Frame[3]!= 0x91) || (Frame[4] !=0xc7))SendIt = true;
                    break;
                    
                case 0xA5:                  // time stamp and partition (this has a checksum)
                    if(((Frame[0]+Frame[1]+Frame[2]+Frame[3]+Frame[4]+Frame[5]+Frame[6]) & 0xff)==Frame[7]){   // ensure valid checksum
                      /*  sprintf(TempStr,"20%1u",Frame[1] >> 4);     // year first dig
                        strcat(OutStr,TempStr);
                        sprintf(TempStr,"%1u-",Frame[1] & 0x0f);    // year 2nd dig
                        strcat(OutStr,TempStr);
                        sprintf(TempStr,"%02u-",(Frame[2] >> 2) & 0x0f);  // month mm
                        strcat(OutStr,TempStr);
                        sprintf(TempStr,"%02u ",((Frame[2] << 3) + (Frame[3] >> 5)) & 0x1f ); // day
                        strcat(OutStr,TempStr);
                        sprintf(TempStr,"%02u:",(Frame[3] ) & 0x1f ); // hr
                        strcat(OutStr,TempStr);
                        sprintf(TempStr,"%02u ",(Frame[4] >> 2 ) & 0x3f ); // min
                        strcat(OutStr,TempStr);*/
                        ii = Frame[2] >> 6;             // partition number (not sure what to do with that yet)
                        if (ii == 0) {
                            if(((Frame[4] & 0x0f) != 0x0) || (Frame[5] !=0x0) || (Frame[6] != 0) )SendIt = true;     // only want to see partition #0 as that is battery, ac, etc
                        }
                    }
                    break;
                    
                case 0x11:                  // query keyboards
                    break;
                    
                case 0x16:                  // 160E23F53C 
                    break;
                    
                case 0x5D:                  // alarm memory group 1
                    if((Frame[1] != 0) || (Frame[2] != 0) || (Frame[3] != 0) || (Frame[4] != 0) || (Frame[5] != 0)) SendIt = true;          // wwant to see if this ever occurs
                    break;
                    
                case 0x0A:                  // Seems to mirror 1st 2 bytes (status) of 05 cmd, plus other stuff
                    SendIt = true;          // wwant to see if this ever occurs
                    break;
                    
                case 0xB1:                  // "SENSOR CFG"
                    break;
                    
                case 0x27:                  // status update zone 1-8
                    if(((Frame[0]+Frame[1]+Frame[2]+Frame[3]+Frame[4]+Frame[5]) & 0xff)==Frame[6]){   // ensure valid checksum
                     /*   GetStatus(OutStr);
                        strcat(OutStr," OpenZones:");
                        if((Frame[5] & (1<<7)) != 0 ){ strcat(OutStr,"8,");}
                        if((Frame[5] & (1<<6)) != 0 ){ strcat(OutStr,"7,");}
                        if((Frame[5] & (1<<5)) != 0 ){ strcat(OutStr,"6,");}
                        if((Frame[5] & (1<<4)) != 0 ){ strcat(OutStr,"5,");}
                        if((Frame[5] & (1<<3)) != 0 ){ strcat(OutStr,"4,");}
                        if((Frame[5] & (1<<2)) != 0 ){ strcat(OutStr,"3,");}
                        if((Frame[5] & (1<<1)) != 0 ){ strcat(OutStr,"2,");}
                        if((Frame[5] & (1<<0)) != 0 ){ strcat(OutStr,"1,");}
                    */
                    if((Frame[3]!= 0x91) || (Frame[4] !=0xc7) || (Frame[5] != 0) )SendIt = true;
                    }
                    break;
            }
            if(SendIt)U.sendPacket(OutStr, strlen(OutStr), syslog_addy, syslog_prt);
            //LOGI(OutStr);
        }
    }

}

//
// convert the first 4 bytes of the frame to status string
// several commands 0x05 and 0x27 use the same info
//
void GetStatus(char* s){
    
   // if((Frame[1] & (1<<7)) != 0 ){ strcat(s,"Backlight,");}
    if((Frame[1] & (1<<6)) != 0 ){ strcat(s,"Fire,");}
    if((Frame[1] & (1<<5)) != 0 ){ strcat(s,"Program,");}
    if((Frame[1] & (1<<4)) != 0 ){ strcat(s,"Error,");}  // aka trouble
    
    if((Frame[1] & (1<<3)) != 0 ){ strcat(s,"Bypass,");}
    if((Frame[1] & (1<<2)) != 0 ){ strcat(s,"Memory,");}  // aka system aka the alarm sounded!! see alarm memory group
    if((Frame[1] & (1<<1)) != 0 ){ strcat(s,"Armed,");}
    if((Frame[1] & (1<<0)) != 0 ){ strcat(s,"Ready,");}

    //sprintf(OutStr, "%02X", Frame[1]);
    // byte 2 is the LED display
    switch(Frame[2]){
        case 0x01:
            strcat(s,"Enter Code");
            break;
        case 0x03:
            strcat(s,"Secure B4 Arming");
            break;
        case 0x11:
            strcat(s,"System Armed");
            break;
        case 0x08:
            strcat(s,"Exit Delay in Progress");
            break;
        case 0x09:
            strcat(s,"Armed with no Delay");
            break;
        case 0x06:  // armed in away mode
            strcat(s,"Armed in Away Mode");
            break;
        case 0x3E:  // system disarmed
            strcat(s,"System Disarmed");
            break;
        case 0x9E: // enter zone to bypass
            strcat(s,"Enter Zones to bypass");
            break;
        case 0x9f:
            strcat(s,"Enter your access code");
            break;
        case 0x8f:
            strcat(s,"Invalid Access Code");
            break;
        case 0xBA:
            strcat(s,"There are no zone low Bats");
            break;
        //case 0xa1:
         //   break;
        default:
            strcat(s,"????");
    }
}


