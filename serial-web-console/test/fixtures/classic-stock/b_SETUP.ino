// =======================================================================================
//                          Initialize - Setup Function
// =======================================================================================
void setup()
{
    //Debug Serial for use with USB Debugging
    Serial.begin(115200);
    while (!Serial);
    
    if (Usb.Init() == -1)
    {
        Serial.print(F("\r\nOSC did not start"));
        while (1); //halt
    }
    
    Serial.print(F("\r\nBluetooth Library Started"));
    
    output.reserve(200); // Reserve 200 bytes for the output string

    //Setup for the controllers (abstraction layer, see ControllerConfig.h)
    PS3NavFoot->begin();
    PS3NavDome->begin();
    #ifdef USE_JOYCON
      joyFootImpl.setStickRange(JOYCON_STICK_RANGE);
      joyDomeImpl.setStickRange(JOYCON_STICK_RANGE);
      joyFootImpl.setGrip(JOYCON_GRIP);
      joyDomeImpl.setGrip(JOYCON_GRIP);
      // What the droid says during pairing: an R2 sound and a line on the console. A lambda
      // rather than a function, so the Arduino IDE does not hoist a prototype above the
      // JoyConPairEvent type.
      joyPairing.begin([](JoyConPairEvent e)
      {
        const char* sound = JOYCON_SOUND_STARTED;
        switch (e)
        {
          case JCP_STARTED:         sound = JOYCON_SOUND_STARTED; Serial.println(F("JOYPAIR started: hold sync on a Joy-Con")); break;
          case JCP_PAIRED_ONE:      sound = JOYCON_SOUND_PAIRED;  Serial.println(F("JOYPAIR paired one, now the other")); break;
          case JCP_DONE:            sound = JOYCON_SOUND_DONE;    Serial.println(F("JOYPAIR done")); break;
          case JCP_TIMEOUT:         sound = JOYCON_SOUND_TIMEOUT; Serial.println(F("JOYPAIR timed out")); break;
          case JCP_HOLD_SYNC_AGAIN: sound = JOYCON_SOUND_AGAIN;   Serial.println(F("JOYPAIR hold sync again on that Joy-Con")); break;
        }
        Serial1.print(sound);
        Serial1.write(13);   // MarcDuino commands end with a carriage return
      }, JOYCON_PAIRING_MODE);
    #endif

    //Setup for PS3
    PS3NavFoot->attachOnInit(onInitPS3NavFoot); // onInitPS3NavFoot is called upon a new connection
    Serial.print(F("\r\nInitFootNav"));
    PS3NavDome->attachOnInit(onInitPS3NavDome); 

    //Setup for Serial2:: Motor Controllers - Sabertooth (Feet) 
    Serial2.begin(motorControllerBaudRate);
    ST->autobaud();          // Send the autobaud command to the Sabertooth controller(s).
    ST->setTimeout(10);      //DMB:  How low can we go for safety reasons?  multiples of 100ms
    ST->setDeadband(driveDeadBandRange);
    ST->stop(); 
    SyR->autobaud();
    SyR->setTimeout(20);      //DMB:  How low can we go for safety reasons?  multiples of 100ms
    SyR->stop(); 

    //Setup for Serial1:: MarcDuino Dome Control Board
    Serial1.begin(marcDuinoBaudRate); 
    
    //Setup for Serial3:: Optional MarcDuino Control Board for Body Panels
    Serial3.begin(marcDuinoBaudRate);
    
    randomSeed(analogRead(0));  // random number seed for dome automation 

     
     //PLay MP3 Trigger sound
    //MP3Trigger trigger;//make trigger object
    // Start serial communication with the trigger (over Serial)
//    trigger.setup(&Serial3);
//    trigger.setVolume(10);//Amount of attenuation  higher=LOWER volume..ten is pretty loud 
}
