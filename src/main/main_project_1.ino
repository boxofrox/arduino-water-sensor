#include <SD.h>
#include <OneWire.h>
#include <SoftwareSerial.h>                                                    //add the soft serial libray
#define rxpin 2                                                                //set the RX pin to pin 2
#define txpin 3  

//Temperature chip i/o
int DS18S20_Pin = 2; //DS18S20 Signal pin on digital 2
OneWire ds(DS18S20_Pin); 
const int ERROR_NO_SENSORS = 0; 
const int ERROR_CRC_NOT_VALID = 1;
const int ERROR_DEVICE_NOT_RECOGNIZED = 2;
const int ERROR_NO_SD = 3;
boolean errors[4] = {false}; //Error warning log


//Ph Sensor
String inputstring = "";                                                       //a string to hold incoming data from the PC
String sensorstring = "";                                                      //a string to hold the data from the Atlas Scientific product
String save_ph_String = "";
boolean input_stringcomplete = false;                                          //have we received all the data from the PC
boolean sensor_stringcomplete = false; 
SoftwareSerial myserial(rxpin, txpin);  

//SD card
const int chipSelect = 10;
int led = 13;

//Main
long interval = 1000;


void setup()
{
    Serial.begin(38400);
    myserial.begin(38400);    //set baud rate for software serial port to 38400
    inputstring.reserve(5);    //set aside some bytes for receiving data from the PC
    sensorstring.reserve(30);  //set aside some bytes for receiving data from Atlas Scientific product
    
  //SD Card Setup
  //Debugging Purposes -- Serial.println, LED
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);
  if (!SD.begin(chipSelect))
  {
      Serial.println("Card failed, or not present");
      errors[ERROR_NO_SD] = true;
      digitalWrite(led, HIGH); //Set the light high so we know
  }
  else //Debugging Purposes
  {
    Serial.println("card initialized.");
  }
  

void loop()
{
  //This initializes the variable to start counting
    unsigned long currentMillis = millis();
    String saved_data;
    //If 1 second, (1000 ms) has passed, start the data gathering
    if(currentMillis - previousMillis > interval) 
    {
         // Make the current time the previous time
         previousMillis = currentMillis;
         //This calls the temperature function and save it
         float temp = getTemp();
         saved_data = String(temp);
         //Seperate the text file for putting to the SD card
         saved_data += ",";
         //Call the Ph function
         saved_data += callph();
         //Save it all to the SD card
         saveSD(saved_data);
         saved_data = "";
    }  
}

String callph()
{                                                    
  if (input_stringcomplete)
  {                                                   //if a string from the PC has been recived by the arduino in its entirety 
      myserial.print(inputstring);                    //send that string to the Atlas Scientific product
      inputstring = "";                               //clear the string:
      input_stringcomplete = false;                   //reset the flage used to tell if we have recived a completed string from the PC
  }
 

  while (myserial.available())                   //if there is more to be read
  {                                               //while a char is holding in the serial buffer
      char inchar = (char)myserial.read();         //get the new char, read
      sensorstring += inchar;                     //add it to the sensorString, add to string
      if (inchar == '\r') 
      {
           sensor_stringcomplete = true;          
      } //if the incoming character is a <CR>, set the flag
  }


  if (sensor_stringcomplete)
  {    //Debugging                                  //if a string from the Atlas Scientific product has been received in its entirety
       Serial.print(sensorstring);              //use the hardware serial port to send that data to the PC
       save_ph_String = sensorstring;
       sensorstring = "";                          //clear the string:
       sensor_stringcomplete = false;              //reset the flag used to tell if we have received a completed string from the Atlas Scientific product
   
  }
  return save_ph_String;
}

void saveSD(String data)
{
}

//Temperature Function of getting 
float getTemp()
{
  //returns the temperature from one DS18S20 in DEG Celsius

  byte data[12];
  byte addr[8];

  
  if ( !ds.search(addr)) {
      //no more sensors on chain, reset search
      ds.reset_search();
      //Debugging-- Serial.println
      Serial.println ("No more sensors found.");
      error[ERROR_NO_SENSORS]=true;
      return -1;
      
  }

  if ( OneWire::crc8( addr, 7) != addr[7])
 //This is a redundant check to make sure our address isn't garbage
 //Using OneWire::crc8 because we are checking the CRC of the specific address
 {
      //Debugging-- Serial.println
      Serial.println("CRC is not valid!");
      error[ERROR_CRC_NOT_VALID]=true;
      return -2;
  }

  if ( addr[0] != 0x10 && addr[0] != 0x28) {
    //possible classification of the sensor
      //Debugging -- Serial.println
      Serial.print("Device is not recognized");
      error[ERROR_DEVICE_NOT_RECOGNIZED]=true;
      return -3;
  }

  ds.reset();
  //Must do reset first before selecting the address
  ds.select(addr); //This selects the particular Wire sensor we are looking for
  ds.write(0x44,1); // start conversion, with parasite power on at the end
  //convert command 44h, 

  byte present = ds.reset(); //if present is high, then the sensor is present....We don't even use this?! wtf?... doesn't force a reset, just waits for communication to end
  //Mostly it just tells us when we can start talking... it might mean sensor is there or something is broken... 
 // just using it as a delay until the pin goes high and we can continue...
  ds.select(addr);   //select the specific sensor
  ds.write(0xBE); // Read Scratchpad, read command is BEh

  
  for (int i = 0; i < 9; i++) 
  { // The Memory Scratch pad has 9 bytes on it, byte 0 is LSB, byte 1 is MSB
    data[i] = ds.read(); //onewire only reads in one byte chunks (or can read one bit chunks)
  }
  
  ds.reset_search(); //zeros out the address, really resets it
  
  byte MSB = data[1]; //Memory scratch pad's first and second bytes are MSB and LSB of temperature
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); //using two's compliment (Using bit-wise shifts to make a 2 byte number, or-ing 0's and LSB)
  float TemperatureSum = tempRead / 16; //Gives the answer in multiple of 16, need to divide by sixteen
  return TemperatureSum;
  
}


//DEBUGGING Purposes for Ph sensor
//This function is only for communication between arduino and computer, not Ph  
void serialEvent() 
{                                                         //if the hardware serial port receives a char
     char inchar = (char)Serial.read();                               //get the char we just received, typecasting to character
     inputstring += inchar;                                           //add it to the inputString (Meaning concatenate the character to the end of the string)
     if(inchar == '\r') // \r is the carriage return, <CR> (not newline)
     {
         input_stringcomplete = true;  //set the flag
     }   
     
}
 
 
