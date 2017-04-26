// Combination lock cracker

// template function to determine size of arrays of any type
template <typename T,unsigned S>
inline unsigned arraysize(const T (&v)[S]) { return S; }

// indlude stepper.h file
#include <CheapStepper.h>

// define stepper pins that allow straight pluggining in to the WeMOS board
const int stepper_pins[] = { D1, D2, D3, D4 };

// initial zones declaration, will be modified dependant on user input
int zones[] = {2, 8, 14, 20, 26, 32, 38, 44, 50, 56};

// state machine enumeration
typedef enum {
    reset,
    first,
    second,
    last,
    idle
}PICK_STATES;

// current state variable
PICK_STATES pick_state = reset;

//define stepper
CheapStepper stepper( stepper_pins[0], stepper_pins[1], stepper_pins[2], stepper_pins[3]);

// cracking housekeeping variables
int first_num, second_num, last_num, second_zone;
boolean new_round = true;
int loops=10;

// motor off function 
void motor_off() {
    for (int i = 0; i < arraysize(stepper_pins) ; i++)
      digitalWrite(stepper_pins[i], LOW);
}

// turn right CW 2 times plus offset 
void lock_reset(int offset) {
  new_round = true;
  stepper.newMoveDegrees(false, 360*2 + offset);   
}

// test shackle, auto shackle test would be implemented here
void check_lock() {
  while(Serial.available()){Serial.read();} // flush serial 
  Serial.println("Pull shackle, press any key to continue"); // wait for input
  while(!Serial.available())
    yield();
}

void setup() {
  stepper.setRpm(15); // compromise between accuracy (no skipping) and speed
  stepper.set4076StepMode(); // motor is 4076.33 steps per rotation, this is closes INT
  Serial.begin(115200); 
  Serial.println("");
  Serial.println("Ready to start moving!");
  char buf[256];
  boolean initialized = false;
  int current_pos;

  while(Serial.available()){Serial.read();} // flush serial buffer
  Serial.println("Please enter the current dial location"); // wait for input
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
  while(Serial.available()){Serial.read();} // flush serial buffer
  Serial.println("Please enter the first zone (lowest start number > 0)"); // wait for input
  while(!initialized) 
  {
    yield();
    if (Serial.available()>1)
    {
      first_zone = Serial.parseInt();
      initialized = true;
    }
  }

  //calculate zones
  for (int i = 0; i < arraysize(zones); i++)
  {
    zones[i] = zones[i] + first_zone;
  }
    
  sprintf(buf, "Simplified zones are: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d" , 
    zones[0], zones[1], zones[2], zones[3], zones[4], zones[5], zones[6], zones[7], zones[8], zones[9]);
  Serial.println(buf);
  
  sprintf(buf, "Current location is: %d. Moving to zero", current_pos);
  Serial.println(buf);
  
  // move stepper to zero and wait until is complete, yeild() used to prevent watchdog reset
  stepper.newMoveDegrees(false, 6*(60-current_pos));
  while(stepper.getStepsLeft())
  {
    yield();
    stepper.run();
  }
  motor_off();

  initialized = false;
  while(Serial.available()){Serial.read();} // flush serial buffer 
  Serial.println("Please enter first zone to test (0-9)"); // wait until input
  while(!initialized) 
  {
    yield();
    if (Serial.available()>1) // calculate offsets
    {
      first_num = Serial.parseInt();
      second_num = (first_num + arraysize(zones) - 1) % arraysize(zones);
      initialized = true;
    }
  }

  while(Serial.available()){Serial.read();} // flush serial buffer
  Serial.println("Press Any Key to continue");
  while(!Serial.available()) // wait for input
    yield();
  
  Serial.println("Cracking Starting:");
}

void loop() {

  stepper.run();  // everytime we loop, we step the motor

  switch ( pick_state)
  {
    case reset:
      if (stepper.getStepsLeft()==0)  // previous move completed
      {
        last_num = 0;
        char buf[256];
        if (new_round) // if this is the first time, we want to reset the lock with three turns
        {
          lock_reset(360);
          pick_state = first;
        } else if (second_num == (first_num + 1) % arraysize(zones)) { // we reached the end of the testable second zones
          lock_reset(6*(66-zones[second_num])); // resets the lock and preserves location
          first_num = (first_num + 1) % arraysize(zones); // increment new first zone with wraparound
          second_num = (first_num + arraysize(zones) - 1) % arraysize(zones); // second zne should always start as one zone behind first
          pick_state = first; // move to first zone
        } else { // we have still second numbers to test, we should test them before reseting the lock, dont need to dial in first number
          second_num = (second_num + arraysize(zones) - 1) % arraysize(zones); // decrement second zone with wraparound. 
          pick_state = second; // move to second zone
        }
        
        sprintf(buf, "New round, current code %d - %d - XX ", zones[first_num], zones[second_num]);
        Serial.println(buf);
      }
      break;

    case first:
      if (stepper.getStepsLeft()==0)  // previous move completed
      {
        stepper.newMoveDegrees(false, 6*zones[first_num]); // move to first zone from zero 
        pick_state = second; // move to second zone
      }
      break;

    case second:
      if (stepper.getStepsLeft() == 0)  // previous move completed
      {
        if(new_round) // if it is the first time we are dailing in the second zone, we have to first pass the first zone
        {
          new_round = false;
          stepper.newMoveDegrees(true, (360+6*6)); // we always start with the zone immediately behind the first zone
        } else {
          stepper.newMoveDegrees(true, 360); // we stopped the previous last zone where the new second zone should be
                                            // a full turn is all that is required to nudge the internal mech to the new second zone
        } 
        pick_state = last;  // move to last zone
      }
      break;

    case last:
      if (stepper.getStepsLeft() == 0)  // previous move completed
      {
        stepper.newMoveDegrees(false, 6 * 6); // increment each zone and test
        if(last_num != 0)
          check_lock(); // test lock as long as it is not the first time we entered this state

        last_num++;
        if (last_num == 10) // reached the last zone to test
        {
          stepper.stop();
          pick_state = reset; // move to reset 
        }
      }
      break;

    case idle: // reserved for future. 
      motor_off();
      break;
      
    default: // reserved for debug
      Serial.println("Something went wrong...");
      pick_state = idle;
      break;
  }
}
