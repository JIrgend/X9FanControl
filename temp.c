/*
 * chairman/temp.c
 *
 * Temperature control for X9 based Supermicro boards.
 * Using Bang–bang control with hysteresis and f=1/s
 * Δx = 2*1K
 * t=1s
 * PT1 first order lag element.
 *
 * Relational approach g(x) = e^((x - 17.33793493) / 15) + 7.65
 *
 * Copyright (c) Fabian Druschke, Johannes Goßmann 2020 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the project's author nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_VALUES 24 //no. of cores
#define MAX_FANSPEED 12000 // Fan specific data, assuming the default Supermicro fan
#define MAX_PWM_VAL 255 
#define FAN_DIFF MAX_FANSPEED / MAX_PWM_VAL
#define INT_MIN 1 // Minimum hysteresis time in seconds.

int tachoControl;  // Hex 0 - 255 PWM frequency in kHz.
int a[MAX_VALUES]; // Max amount of sensors we want to read in
int interval;      // Our interval to repeat execution every x seconds
int debug;

//debug mode
void printfVals() {
  for (float i = 0.0; i <= 100.0; i++) { // From 0 to 100 degrees
    float pwm_frequency = (exp((i - 17.33793493) / 15.0) + 7.65);
    printf("Hunk for %0.f °C: %f = %f 1/60s\n", i, pwm_frequency,(pwm_frequency * FAN_DIFF));
  }
}

float hysteresisControl(int temp) {
  float pwm_frequency;
  return pwm_frequency = exp((temp - 17.33793493) / 15) + 7.65;
}

int interpolateFanSpeed(float pwm_frequency) {
  return tachoControl = (int)(pwm_frequency < 0 ? (pwm_frequency - 0.5) : (pwm_frequency + 0.5));
}

//linear search, max will be at highest addr in array
void max(int *keys, int n_keys) { 
int max,i;
	for(i;i<n_keys;i++) 
	if (keys[i] > max) 
		max = keys[i];
	keys[n_keys]=max;
}


void setFanSpeed() {
  FILE *fp; //file handler
  char path[1035];

  // Open the command for reading.
  fp = popen("sysctl -a | grep temperature", "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    exit(1);
  }

  // Read the output a line at a time and fill into array.
  int i = 0;
  while (fgets(path, sizeof(path), fp) != NULL) {
    char *discardChar;
    discardChar = (char *)calloc(sizeof(path), sizeof(char));
    char discard[sizeof(path)];
    sscanf(path, "%s %i", &discard[i], &a[i]);
    free(discardChar);
    i++;
  }

  max(a, sizeof(a) / sizeof(int));
  
  for (int k = sizeof(a); k > 0; k--) {
    if (a[k] > 0) {
      int max = a[k];
      int targetFanSpeed = interpolateFanSpeed(hysteresisControl(max));
      unsigned char buffer[32];
      sprintf(buffer, "ipmitool raw 0x30 0x91 0x5A 0x3 0x10 0x%x", targetFanSpeed); // Issued command
      system(buffer);
      sprintf(buffer, "ipmitool raw 0x30 0x91 0x5A 0x3 0x11 0x%x", targetFanSpeed); // Issued command
      system(buffer);
      if (debug == 1) {
        printf("Highest temp: %d\n", a[k]);
        float realFanSpeed = (targetFanSpeed * FAN_DIFF);
        printf("Target fan speed: 0x%x = %f 1/60s\n\n", targetFanSpeed, realFanSpeed);
      }
      break;
    }
  } 
  pclose(fp); // Close file handler
}

int main(int argc, char *argv[]) {
  debug = 0;

  if (argc == 1) {
    printf("Fanspeed set.\n");
    setFanSpeed();
    return 0;
  }

  if (argc == 2) {
    if (strcmp(argv[1], "--help") == 0) {

      printf("Execute: %s ... [INTERVAL] ... [OPTION] \n\n", argv[0]);
      printf(" --table, prints scaled fan speed information\n Temperature control for X9 based Supermicro boards.\n");
      printf("Using Bang-bang control with hysteresis and f=1/s\n Δtx = 2*1K\n t=1s\n PT1 first order lag element.\n\n");
      printf("Relational approach g(x) = e^((x - 17.33793493) / 15) + 7.65\n\n");
      printf("Copyright (c) Fabian Druschke, Johannes Goßmann 2020\nAll rights reserved.\n");
      return 0;
    }
    if (strcmp(argv[1], "--table") == 0) {
      printfVals();
      return 0;
    }
    if (strcmp(argv[1], "--debug") == 0) {
      printf("Fanspeed set.\n");
      debug = 1;
      setFanSpeed();
      return 0;
    }
  }
  
  if ((argc == 2) || (argc == 3 && strcmp(argv[2], "--debug") == 0)) {
    // Check if a valid interval has been entered
    if (strcmp(argv[2], "--debug") == 0) {
      debug = 1;
    }
    char *p;

    errno = 0;
    long conv = strtol(argv[1], &p, 10);

    // Check for errors: e.g., the string does not represent an integer
    // or the integer is less than int
    if (errno != 0 || *p != '\0' || conv < INT_MIN) {
      // Put here the handling of the error, like exiting the program with
      // an error message
      printf("Invalid input specified.");
    } else {
      // No error
      interval = conv;
      printf("Hysteresis: %d seconds. This program will continue until being interrupted.\n", interval);
      // Block
    LOOP:
      setFanSpeed();
      sleep(interval);
      goto LOOP;
    }
  }
  return 0;
}
