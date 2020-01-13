/******************************************************************************
 *
 *  This file is part of the Lynsyn host tools
 *
 *  Copyright 2019 Asbjørn Djupdal, NTNU
 *
 *  Lynsyn is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Lynsyn is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Lynsyn.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

///////////////////////////////////////////////////////////////////////////////
//
// Initialization, test and calibration program for Lynsyn V3.0 (Lynsyn Light)
//
///////////////////////////////////////////////////////////////////////////////

#include <argp.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "lynsyn.h"

#define SENSORS_BOARD_2 7
#define SENSORS_BOARD_3 3

#ifdef _WIN32
#define LONGLONGHEX "I64x"
#else
#define LONGLONGHEX PRIx64
#endif

///////////////////////////////////////////////////////////////////////////////

double rsDefault3[SENSORS_BOARD_3] = {0.025, 0.05, 0.1};
double rsDefault2[SENSORS_BOARD_2] = {0.025, 0.05, 0.05, 0.1, 0.1, 1, 10};
unsigned hwVersion = HW_VERSION_3_1;

///////////////////////////////////////////////////////////////////////////////

bool programFpga(char *filename) {
  printf("Flashing %s\n", filename);
  fflush(stdout);

  char command[256];
  snprintf(command, 256, "program_flash -f %s -flash_type s25fl128sxxxxxx0-spi-x1_x2_x4 -blank_check -verify -cable type xilinx_tcf", filename);

  int ret = system(command);
  if(ret) {
    printf("Can't program the FPGA flash.  Possible problems:\n");
    printf("- Can't find mcs file\n");
    printf("- Xilinx Vivado software problem\n");
    printf("- Lynsyn USB port is not connected to the PC\n");
    printf("- Xilinx USB cable is not connected to both PC and Lynsyn\n");
    printf("- Faulty soldering or components:\n");
    printf("  - U1 Flash\n");
    printf("  - R1, R2, R3\n");
    printf("  - U2 FPGA\n");
    printf("  - J5 FPGA_debug\n");
    fflush(stdout);
    return false;
  }

  printf("FPGA flashed OK\n");
  fflush(stdout);

  return true;
}

bool programMcu(char *bootFilename, char *mainFilename) {
  printf("Flashing %s and %s\n", bootFilename, mainFilename);
  fflush(stdout);

  char command[256];
  snprintf(command, 256, "commander flash %s --halt --device EFM32GG332F1024", bootFilename);

  int ret = system(command);
  if(!ret) {
    snprintf(command, 256, "commander flash %s --address 0x10000 --device EFM32GG332F1024", mainFilename);
    ret = system(command);
  }

  if(ret) {
    printf("Can't program the MCU.  Possible problems:\n");
    printf("- Can't find bin files\n");
    printf("- SiLabs Simplicity Commander problem\n");
    printf("- Lynsyn USB port is not connected to the PC\n");
    printf("- EFM32 programmer is not connected to both PC and Lynsyn\n");
    printf("- Faulty soldering or components:\n");
    printf("  - U1 MCU\n");
    printf("  - J5 Cortex debug\n");
    fflush(stdout);
    return false;
  }

  printf("MCU flashed OK\n");
  fflush(stdout);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool cleanNonVolatile(void) {
  double rs[LYNSYN_MAX_SENSORS];
  char text[80];

  int sensors = SENSORS_BOARD_3;
  double *rsDefault =rsDefault3;

  if(hwVersion == HW_VERSION_2_2) {
    sensors = SENSORS_BOARD_2;
    rsDefault = rsDefault2;
  }
      
  for(int i = 0; i < sensors; i++) {
    printf("*** Enter Rs%d [%f ohm]: ", i+1, rsDefault[i]);
    fflush(stdout);
    if(!fgets(text, 80, stdin)) {
      printf("I/O error\n");
      fflush(stdout);
      exit(-1);
    }
    if(text[0] == '\n') rs[i] = rsDefault[i];
    else rs[i] = strtod(text, NULL);
  }

  printf("Initializing HW %x with Rs", hwVersion);
  for(int i = 0; i < sensors; i++) {
    printf( " %f", rs[i]);
  }
  printf("\n");
  fflush(stdout);

  return lynsyn_cleanNonVolatile(hwVersion, rs);
}

///////////////////////////////////////////////////////////////////////////////

bool testUsb(void) {
  if(!lynsyn_testUsb()) {
    printf("Can't communicate over USB.  Possible problems:\n");
    printf("- Problems with libusb\n");
    printf("- Lynsyn USB port is not connected to the PC\n");
    printf("- Faulty soldering or components:\n");
    printf("  - U1 MCU\n");
    printf("  - J1 USB\n");
    printf("  - Y1 48MHz crystal\n");
    fflush(stdout);
    return false;
  }

  printf("USB communication OK\n");
  fflush(stdout);

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void calibrateSensorCurrent(int sensor, double acceptance) {
  int sensors = SENSORS_BOARD_3;
  if(hwVersion == HW_VERSION_2_2) {
    sensors = SENSORS_BOARD_2;
  }

  if((sensor < 0) || (sensor >= sensors)) {
    printf("Incorrect sensor number: %d\n", sensor+1);
    fflush(stdout);
    exit(-1);
  }

  uint8_t hwVersion, bootVersion, swVersion;
  double r[LYNSYN_MAX_SENSORS];
  lynsyn_getInfo(&hwVersion, &bootVersion, &swVersion, r);

  printf("Calibrate current sensor %d\n", sensor + 1);
  fflush(stdout);

  { // current
    char calCurrent[80];

    double shuntSizeVal = r[sensor];
    double maxCurrentVal = lynsyn_getMaxCurrent(shuntSizeVal);

    printf("%f ohm shunt resistor gives a maximum current of %fA\n\n", shuntSizeVal, maxCurrentVal);
    fflush(stdout);

    double currents[2];
    currents[0] = maxCurrentVal*0.04;
    currents[1] = maxCurrentVal*0.96;
    
    double calCurrentVal;

    for(int point = 0; point < sizeof(currents)/sizeof(double); point++) {

      printf("*** Connect a calibration current source.\n"
             "The current should be around %fA.\n"
             "Do not trust the current source display, use a multimeter to confirm.\n"
             "Enter measured current:\n", currents[point]);
      fflush(stdout);
      if(!fgets(calCurrent, 80, stdin)) {
        printf("I/O error\n");
	fflush(stdout);
        exit(-1);
      }

      calCurrentVal = strtod(calCurrent, NULL);

      printf("Calibrating sensor %d with calibration current %f\n\n", sensor+1, calCurrentVal);
      fflush(stdout);

      lynsyn_adcCalibrateCurrent(sensor, calCurrentVal, maxCurrentVal);
    }

    ///////////////////////////////////////////////////////////////////////////////

    if(!lynsyn_testAdcCurrent(sensor, calCurrentVal, acceptance)) {
      printf("Calibration error\n");
      fflush(stdout);
      exit(-1);
    }

    fflush(stdout);
    printf("\n");
  }
}

void calibrateSensorVoltage(int sensor, double acceptance) {
  if((sensor < 0) || (sensor >= SENSORS_BOARD_3)) {
    printf("Incorrect sensor number: %d\n", sensor+1);
    fflush(stdout);
    exit(-1);
  }

  uint8_t hwVersion, bootVersion, swVersion;
  double r[LYNSYN_MAX_SENSORS];
  lynsyn_getInfo(&hwVersion, &bootVersion, &swVersion, r);

  { // voltage
    char calVoltage[80];

    double maxVoltageVal = lynsyn_getMaxVoltage();

    printf("Calibrate voltage sensor %d\n", sensor + 1);
    fflush(stdout);

    ///////////////////////////////////////////////////////////////////////////////

    double voltages[3] = {1, 11, 22};
    double calVoltageVal;

    for(int point = 0; point < sizeof(voltages)/sizeof(double); point++) {

      printf("*** Connect a calibration voltage source.\n"
             "The voltage should be around %fV.\n"
             "Do not trust the voltage source display, use a multimeter to confirm.\n"
             "Enter measured voltage:\n", voltages[point]);
      fflush(stdout);
      if(!fgets(calVoltage, 80, stdin)) {
        printf("I/O error\n");
	fflush(stdout);
        exit(-1);
      }

      calVoltageVal = strtod(calVoltage, NULL);

      printf("Calibrating sensor %d with calibration voltage %f\n\n", sensor+1, calVoltageVal);
      fflush(stdout);

      lynsyn_adcCalibrateVoltage(sensor, calVoltageVal, maxVoltageVal);
    }

    ///////////////////////////////////////////////////////////////////////////////

    if(!lynsyn_testAdcVoltage(sensor, calVoltageVal, acceptance)) {
      printf("Calibration error\n");
      fflush(stdout);
      exit(-1);
    }

    printf("\n");
    fflush(stdout);
  }
}

///////////////////////////////////////////////////////////////////////////////

void programTest(void) {
  printf("First step: Manual tests.\n\n");
  fflush(stdout);

  if(hwVersion >= HW_VERSION_3_0) {
    printf("*** Connect Lynsyn to the PC USB port.\n");
    fflush(stdout);
    getchar();

    printf("*** Measure the voltage across C18.  Should be 3.3V.\n");
    fflush(stdout);
    getchar();

    printf("*** Verify that LED D1 is lit.\n");
    fflush(stdout);
    getchar();

  } else {
    printf("*** Measure the voltage across C56.  Should be 1.0V.\n");
    fflush(stdout);
    getchar();

    printf("*** Measure the voltage across C66.  Should be 1.8V.\n");
    fflush(stdout);
    getchar();

    printf("*** Measure the voltage across C68.  Should be 3.3V.\n");
    fflush(stdout);
    getchar();

    printf("*** Verify that LED D3 is lit.\n");
    fflush(stdout);
    getchar();
  }

  {
    printf("*** Connect a multimeter to TP1.  Adjust RV1 until the voltage gets as close to 2.5V as possible.\n");
    fflush(stdout);
    getchar();
  }

  printf("*** Secure RV1 by applying a drop of nail polish on top.\n");
  fflush(stdout);
  getchar();

  printf("Second step: Flashing.\n\n");
  fflush(stdout);

  if(hwVersion >= HW_VERSION_3_0) {
    printf("*** Connect the EFM32 starter kit to Lynsyn J5 (Cortex Debug).\n");
    fflush(stdout);

  } else {
    printf("*** Connect the Xilinx USB cable to Lynsyn J5.\n");
    fflush(stdout);
    getchar();

    printf("*** Enter fpga mcs filename [fwbin/lynsyn.mcs]:\n");
    fflush(stdout);
    char filename[80];
    if(!fgets(filename, 80, stdin)) {
      printf("I/O error\n");
      exit(-1);
    }
    if(filename[0] == '\n') strncpy(filename, "fwbin/lynsyn.mcs", 80);
    filename[strcspn(filename, "\n")] = 0;

    if(!programFpga(filename)) exit(-1);

    printf("*** Remove Xilinx USB cable from lynsyn.\n");
    fflush(stdout);
    getchar();

    printf("*** Reboot Lynsyn by removing and replugging the USB cable.\n");
    fflush(stdout);
    getchar();

    printf("*** Connect the EFM32 starter kit to Lynsyn J6 (MCU_debug).\n");
    fflush(stdout);
  }

  getchar();

  printf("*** Enter boot bin filename [fwbin/lynsyn_boot.bin]:\n");
  fflush(stdout);
  char bootFilename[80];
  if(!fgets(bootFilename, 80, stdin)) {
    printf("I/O error\n");
    fflush(stdout);
    exit(-1);
  }
  if(bootFilename[0] == '\n') strncpy(bootFilename, "fwbin/lynsyn_boot.bin", 80);
  bootFilename[strcspn(bootFilename, "\n")] = 0;

  printf("*** Enter main bin filename [fwbin/lynsyn_boot.bin]:\n");
  fflush(stdout);
  char mainFilename[80];
  if(!fgets(mainFilename, 80, stdin)) {
    printf("I/O error\n");
    fflush(stdout);
    exit(-1);
  }
  if(mainFilename[0] == '\n') strncpy(mainFilename, "fwbin/lynsyn_main.bin", 80);
  mainFilename[strcspn(mainFilename, "\n")] = 0;

  if(!programMcu(bootFilename, mainFilename)) {
    printf("Can't program lynsyn\n");
    fflush(stdout);
    exit(-1);
  }

  printf("\nThird step: Automatic tests.\n\n");
  fflush(stdout);

  if(!lynsyn_preinit()) {
    printf("Can't initialize lynsyn\n");
    fflush(stdout);
    exit(-1);
  }

  if(!lynsyn_testUsb()) {
    printf("USB failure\n");
    fflush(stdout);
    exit(1);
  }

  printf("\nFourth step: Initializing non-volatile memory.\n\n");
  fflush(stdout);

  if(!cleanNonVolatile()) exit(-1);

  printf("\nFift step: More manual tests.\n\n");
  fflush(stdout);

  lynsyn_setLed(true);

  printf("*** Verify that LED D2 is lit.\n");
  fflush(stdout);
  getchar();

  lynsyn_setLed(false);

  printf("*** Verify that LED D2 is unlit.\n");
  fflush(stdout);
  getchar();

  lynsyn_prerelease();
  if(!lynsyn_init()) {
    printf("Can't init lynsyn.\n");
    fflush(stdout);
    exit(-1);
  }
}

void programTestAndCalibrate(double acceptance) {
  programTest();

  printf("Sixt step: Current sensor calibration.\n\n");
  fflush(stdout);

  int sensors = SENSORS_BOARD_3;
  if(hwVersion == HW_VERSION_2_2) {
    sensors = SENSORS_BOARD_2;
  }
      
  for(int i = 0; i < sensors; i++) {
    calibrateSensorCurrent(i, acceptance);
  }

  if(hwVersion >= HW_VERSION_3_0) {
    printf("Seventh step: Voltage sensor calibration.\n\n");
    printf("This lynsyn has a maximum voltage of %fV\n\n", lynsyn_getMaxVoltage());
    fflush(stdout);

    for(int i = 0; i < SENSORS_BOARD_3; i++) {
      calibrateSensorVoltage(i, acceptance);
    }
  }

  lynsyn_release();

  printf("\nAll tests OK and all calibrations done.\n");
  fflush(stdout);
}

void calCurrentSensor(double acceptance) {
  if(!lynsyn_init()) {
    printf("Can't initialize lynsyn\n");
    fflush(stdout);
    exit(-1);
  }

  char sensor[80] = "";

  while(sensor[0] != 'x') {
    printf("Which sensor do you want to calibrate ('x' for exit)?\n");
    fflush(stdout);
    if(!fgets(sensor, 80, stdin)) {
      printf("I/O error\n");
      fflush(stdout);
      exit(-1);
    }
    if((sensor[0] != 'x') && (sensor[0] != 'X'))  {
      int s = strtol(sensor, NULL, 10)-1;
      calibrateSensorCurrent(s, acceptance);
    }
  }

  lynsyn_release();
}

void calVoltageSensor(double acceptance) {
  if(!lynsyn_init()) {
    printf("Can't initialize lynsyn\n");
    fflush(stdout);
    exit(-1);
  }

  char sensor[80] = "";

  while(sensor[0] != 'x') {
    printf("Which sensor do you want to calibrate ('x' for exit)?\n");
    fflush(stdout);
    if(!fgets(sensor, 80, stdin)) {
      printf("I/O error\n");
      fflush(stdout);
      exit(-1);
    }
    if((sensor[0] != 'x') && (sensor[0] != 'X'))  {
      int s = strtol(sensor, NULL, 10)-1;
      calibrateSensorVoltage(s, acceptance);
    }
  }

  lynsyn_release();
}

bool live(void) {
  if(!lynsyn_init()) {
    printf("Can't initialize lynsyn\n");
    fflush(stdout);
    exit(-1);
  }

  while(true) {
    struct LynsynSample sample;

    lynsyn_getSample(&sample, true, 0);

    for(int i = 0; i < LYNSYN_MAX_CORES; i++) {
      printf("%" LONGLONGHEX, sample.pc[i]);
    }
    printf(" : ");
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      printf("%f/%f ", sample.current[i], sample.voltage[i]);
    }
    printf("\n");
    fflush(stdout);

    sleep(1);
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

static char doc[] = "A test and calibration tool for Lynsyn boards";
static char args_doc[] = "";

static struct argp_option options[] = {
  {"board-version", 'b', "version",   0, "Board version (2 for original, 3 for lite)" },
  {"procedure",     'p', "procedure", 0, "Which procedure to run" },
  {"acceptance",    'a', "value",     0, "Maximum allowed error in percentage (0.01 default" },
  { 0 }
};

struct arguments {
  int board;
  int procedure;
  double acceptance;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key) {
    case 'b':
      arguments->board = strtol(arg, NULL, 10);
      break;
    case 'p':
      arguments->procedure = strtol(arg, NULL, 10);
      break;
    case 'a':
      arguments->acceptance = strtod(arg, NULL);
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 0)
        argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char *argv[]) {
  struct arguments arguments;
  arguments.procedure = -1;
  arguments.acceptance = 0.01;
  arguments.board = 31;

  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  switch(arguments.board) {
    case 2: printf("Lynsyn Original\n"); hwVersion = HW_VERSION_2_2; break;
    case 3: printf("Lynsyn Lite (first prototype)\n"); hwVersion = HW_VERSION_3_0; break;
    case 31: printf("Lynsyn Lite\n"); hwVersion = HW_VERSION_3_1; break;
    default: printf("ERROR: Unknown board\n"); fflush(stdout); exit(1);
  }

  if(arguments.procedure < 1) { 
    char choiceBuf[80];

    printf("Which procedure do you want to perform?\n");
    printf("Enter '1' for complete programming, test and calibration.\n");
    printf("Enter '2' for only current sensor calibration\n");
    if(arguments.board >= 3) {
      printf("Enter '3' for only voltage sensor calibration\n");
    }
    printf("Enter '4' for live measurements\n");

    fflush(stdout);

    if(!fgets(choiceBuf, 80, stdin)) {
      printf("I/O error\n");
      exit(-1);
    }

    arguments.procedure = strtol(choiceBuf, NULL, 10);
  }
    
  printf("\n");

  switch(arguments.procedure) {
    case 1:
      printf("This procedure programs, tests and calibrates the Lynsyn board.\n"
             "All lines starting with '***' requires you to do a certain action, and then press enter to continue.\n\n");
      fflush(stdout);

      programTestAndCalibrate(arguments.acceptance);
      break;

    case 2:
      printf("*** Connect Lynsyn to the PC USB port.\n");
      fflush(stdout);
      getchar();

      calCurrentSensor(arguments.acceptance);
      break;

    case 3:
      if(arguments.board >= 3) {
        printf("*** Connect Lynsyn to the PC USB port.\n");
	fflush(stdout);
        getchar();

        calVoltageSensor(arguments.acceptance);
      }
      break;

    case 4:
      printf("*** Connect Lynsyn to the PC USB port.\n");
      fflush(stdout);
      getchar();

      live();
      break;

    default:
      return 0;
  }

  return 0;
}

