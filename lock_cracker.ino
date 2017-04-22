/*
 * cheapStepper_simple.ino
 * ///////////////////////////////////////////
 * using CheapStepper Arduino library v.0.2.0
 * created by Tyler Henry, 7/2016
 * ///////////////////////////////////////////
 * 
 * this sketch illustrates basic step() functionality of the library:
 * the stepper performs a full rotation, pauses 1 second,
 * then does a full rotation in the other direction, and so on
 * 
 * //////////////////////////////////////////////////////
 */


template <typename T,unsigned S>
inline unsigned arraysize(const T (&v)[S]) { return S; }

// first, include the library :)

#include <CheapStepper.h>

const int stepper_pins[] = { D1, D2, D3, D4 };
int zones[] = {2, 8, 14, 20, 26, 32, 38, 44, 50, 56};

int first_num, second_num, last_num, second_zone;

typedef enum {
    reset,
    first,
    second,
    last,
    idle
}PICK_STATES;

PICK_STATES pick_state = reset;

CheapStepper stepper( stepper_pins[0], stepper_pins[1], stepper_pins[2], stepper_pins[3]);
// here we declare our stepper using default pins:
// arduino pin <--> pins on ULN2003 board:
// 8 <--> IN1
// 9 <--> IN2
// 10 <--> IN3
// 11 <--> IN4

 // let's create a boolean variable to save the direction of our rotation

boolean new_round = true;
int loops=10;

void motor_off() {
    for (int i = 0; i < arraysize(stepper_pins) ; i++)
      digitalWrite(stepper_pins[i], LOW);
}

void lock_reset(int offset) {
  // turn right three times 
  new_round = true;
  stepper.newMoveDegrees(false, 360*2 + offset);
   
}

void check_lock() {
  while(Serial.available()){Serial.read();}
  Serial.println("Pull shackle, press any key to continue");
  while(!Serial.available())
    yield();
}

void setup() {
  // let's just set up a serial connection and test print to the console
  stepper.setRpm(15);
  stepper.set4076StepMode();
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Ready to start moving!");
  char buf[256];
  boolean initialized = false;
  int current_pos;

  while(Serial.available()){Serial.read();}
  Serial.println("Please enter the current dial location");
  while(!initialized) 
  {
    yield();
    if (Serial.available()>2)
    {
      current_pos = Serial.parseInt();
      initialized = true;
    }
  }
  
  int first_zone = 0;
  initialized = false;
  while(Serial.available()){Serial.read();}
  Serial.println("Please enter the first zone (lowest start number > 0)");
  while(!initialized) 
  {
    yield();
    if (Serial.available()>1)
    {
      first_zone = Serial.parseInt();
      initialized = true;
    }
  }

  for (int i = 0; i < arraysize(zones); i++)
  {
    zones[i] = zones[i] + first_zone;
  }

  sprintf(buf, "Simplified zones are: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d" , 
    zones[0], zones[1], zones[2], zones[3], zones[4], zones[5], zones[6], zones[7], zones[8], zones[9]);
  Serial.println(buf);
  

  sprintf(buf, "Current location is: %d. Moving to zero", current_pos);
  Serial.println(buf);
  
  stepper.newMoveDegrees(false, 6*(60-current_pos));
  while(stepper.getStepsLeft())
  {
    yield();
    stepper.run();
  }
  motor_off();

  initialized = false;
  while(Serial.available()){Serial.read();}
  Serial.println("Please enter first zone to test (0-9)");
  while(!initialized) 
  {
    yield();
    if (Serial.available()>1)
    {
      first_num = Serial.parseInt();
      second_num = (first_num + arraysize(zones) - 1) % arraysize(zones);
      initialized = true;
    }
  }

  while(Serial.available()){Serial.read();}
  Serial.println("Press Any Key to continue");
  while(!Serial.available())
    yield();
  
  Serial.println("Cracking Starting:");
}

void loop() {

  stepper.run();

  switch ( pick_state)
  {
    case reset:
      if (stepper.getStepsLeft()==0)
      {
        last_num = 0;
        char buf[256];
        if (new_round)
        {
          lock_reset(0);
          pick_state = first;
        } else if (second_num == (first_num + 1) % arraysize(zones)) {
          lock_reset(6*(66-zones[second_num]));
          first_num = (first_num + 1) % arraysize(zones);
          second_num = (first_num + arraysize(zones) - 1) % arraysize(zones);
          pick_state = first;
        } else {
          second_num = (second_num + arraysize(zones) - 1) % arraysize(zones);
          pick_state = second;
        }
        
        sprintf(buf, "New round, current code %d - %d - XX ", zones[first_num], zones[second_num]);
        Serial.println(buf);
      }
      break;

    case first:
      if (stepper.getStepsLeft()==0)
      {
        stepper.newMoveDegrees(false, 6*zones[first_num]);
        pick_state = second;
      }
      break;

    case second:
      if (stepper.getStepsLeft() == 0)
      {
        if(new_round)
        {
          new_round = false;
          stepper.newMoveDegrees(true, (360+6*6));
        } else {
          stepper.newMoveDegrees(true, 360);
        }
        pick_state = last;
      }
      break;

    case last:
      if (stepper.getStepsLeft() == 0)
      {
        stepper.newMoveDegrees(false, 6 * 6);
        if(last_num != 0)
          check_lock();

        last_num++;
        if (last_num == 10)
        {
          stepper.stop();
          pick_state = reset;
        }
      }
      break;

    case idle:
      motor_off();
      break;
      
    default:
      Serial.println("Something went wrong...");
      pick_state = idle;
      break;
  }
}