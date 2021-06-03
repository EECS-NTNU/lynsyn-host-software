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

//#define AVERAGE

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

bool calibrateSensorCurrent(int sensor, double acceptance) {
  int sensors = SENSORS_BOARD_3;
  if(hwVersion == HW_VERSION_2_2) {
    sensors = SENSORS_BOARD_2;
  }

  if((sensor < 0) || (sensor >= sensors)) {
    printf("Incorrect sensor number: %d\n", sensor+1);
    fflush(stdout);
    return false;
  }

  uint8_t hwVersion, bootVersion, swVersion;
  double r[LYNSYN_MAX_SENSORS];
  lynsyn_getInfo(&hwVersion, &bootVersion, &swVersion, r);

  printf("Calibrate current sensor %d\n", sensor + 1);
  printf("--------------------------\n");

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
             "Enter measured current: ", currents[point]);
      fflush(stdout);
      if(!fgets(calCurrent, 80, stdin)) {
        printf("I/O error\n");
        fflush(stdout);
        return false;
      }

      calCurrentVal = strtod(calCurrent, NULL);

      printf("Calibrating sensor %d with calibration current %f\n\n", sensor+1, calCurrentVal);
      fflush(stdout);

      if(!lynsyn_adcCalibrateCurrent(sensor, calCurrentVal, maxCurrentVal)) {
        printf("\nCALIBRATION ERROR, please redo\n\n");
        fflush(stdout);
        return false;
      }
    }

    ///////////////////////////////////////////////////////////////////////////////

    if(!lynsyn_testAdcCurrent(sensor, calCurrentVal, acceptance)) {
      printf("\nCALIBRATION ERROR, please redo\n\n");
      fflush(stdout);
      return false;
    }

    fflush(stdout);
    printf("\n");
  }

  return true;
}

bool calibrateSensorVoltage(int sensor, double acceptance) {
  if((sensor < 0) || (sensor >= SENSORS_BOARD_3)) {
    printf("Incorrect sensor number: %d\n", sensor+1);
    fflush(stdout);
    return false;
  }

  uint8_t hwVersion, bootVersion, swVersion;
  double r[LYNSYN_MAX_SENSORS];
  lynsyn_getInfo(&hwVersion, &bootVersion, &swVersion, r);

  { // voltage
    char calVoltage[80];

    double maxVoltageVal = lynsyn_getMaxVoltage();

    printf("Calibrate voltage sensor %d\n", sensor + 1);
    printf("--------------------------\n");
    fflush(stdout);

    ///////////////////////////////////////////////////////////////////////////////

    double voltages[3] = {1, 11, 22};
    double calVoltageVal;

    for(int point = 0; point < sizeof(voltages)/sizeof(double); point++) {

      printf("*** Connect a calibration voltage source.\n"
             "The voltage should be around %fV.\n"
             "Do not trust the voltage source display, use a multimeter to confirm.\n"
             "Enter measured voltage: ", voltages[point]);
      fflush(stdout);
      if(!fgets(calVoltage, 80, stdin)) {
        printf("I/O error\n");
        fflush(stdout);
        return false;
      }

      calVoltageVal = strtod(calVoltage, NULL);

      printf("Calibrating sensor %d with calibration voltage %f\n\n", sensor+1, calVoltageVal);
      fflush(stdout);

      if(!lynsyn_adcCalibrateVoltage(sensor, calVoltageVal, maxVoltageVal)) {
        printf("\nCALIBRATION ERROR, please redo\n\n");
        fflush(stdout);
        return false;
      }
    }

    ///////////////////////////////////////////////////////////////////////////////

    if(!lynsyn_testAdcVoltage(sensor, calVoltageVal, acceptance)) {
      printf("\nCALIBRATION ERROR, please redo\n\n");
      fflush(stdout);
      return false;
    }

    printf("\n");
    fflush(stdout);
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void preTest(void) {
  if(hwVersion >= HW_VERSION_3_0) {
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

  printf("Pre-tests done\n");
  fflush(stdout);
}

void program(void) {
  if(hwVersion >= HW_VERSION_3_0) {
    printf("*** Connect the EFM32 starter kit to Lynsyn J5 (Cortex Debug).\n");
    fflush(stdout);

  } else {
    printf("*** Connect the Xilinx USB cable to Lynsyn J5.\n");
    fflush(stdout);
    getchar();

    printf("*** Enter fpga mcs filename [fwbin/original/lynsyn_2.0.mcs]:\n");
    fflush(stdout);
    char filename[80];
    if(!fgets(filename, 80, stdin)) {
      printf("I/O error\n");
      exit(-1);
    }
    if(filename[0] == '\n') strncpy(filename, "fwbin/original/lynsyn_2.0.mcs", 80);
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

  char *bootbin;
  char *mainbin;

  if(hwVersion >= HW_VERSION_3_0) {
    bootbin = "fwbin/lite/lynsyn_boot_1.1.bin";
    mainbin = "fwbin/lite/lynsyn_main_2.2.bin";
  } else {
    bootbin = "fwbin/original/lynsyn_boot_1.1.bin";
    mainbin = "fwbin/original/lynsyn_main_2.2.bin";
  }

  printf("*** Enter boot bin filename [%s]:\n", bootbin);
  fflush(stdout);
  char bootFilename[80];
  if(!fgets(bootFilename, 80, stdin)) {
    printf("I/O error\n");
    fflush(stdout);
    exit(-1);
  }
  if(bootFilename[0] == '\n') strncpy(bootFilename, bootbin, 80);
  bootFilename[strcspn(bootFilename, "\n")] = 0;

  printf("*** Enter main bin filename [%s]:\n", mainbin);
  fflush(stdout);
  char mainFilename[80];
  if(!fgets(mainFilename, 80, stdin)) {
    printf("I/O error\n");
    fflush(stdout);
    exit(-1);
  }
  if(mainFilename[0] == '\n') strncpy(mainFilename, mainbin, 80);
  mainFilename[strcspn(mainFilename, "\n")] = 0;

  if(!programMcu(bootFilename, mainFilename)) {
    printf("Can't program lynsyn\n");
    fflush(stdout);
    exit(-1);
  }

  if(!lynsyn_preinit(20)) {
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

  lynsyn_prerelease();
  if(!lynsyn_init()) {
    printf("Can't init lynsyn.\n");
    fflush(stdout);
    exit(-1);
  }

  lynsyn_release();

  printf("\nProgrammed and initialized OK.\n");
  fflush(stdout);
}

void testJtag(void) {
  printf("\n*** Connect Lynsyn JTAG to a supported ARM development board (must be powered on)\n");
  fflush(stdout);
  getchar();
  
  if(!lynsyn_init()) {
    printf("Can't init lynsyn.\n");
    fflush(stdout);
    exit(-1);
  }

  bool jtagOk = lynsyn_jtagInit(lynsyn_getDefaultJtagDevices());
  
  if(jtagOk) printf("JTAG works\n");
  else printf("JTAG FAILURE\n");
}

void calibrateCurrents(double acceptance) {
  if(!lynsyn_init()) {
    printf("Can't init lynsyn.\n");
    fflush(stdout);
    exit(-1);
  }

  int sensors = SENSORS_BOARD_3;
  if(hwVersion == HW_VERSION_2_2) {
    sensors = SENSORS_BOARD_2;
  }
      
  for(int i = 0; i < sensors; i++) {
    do {
      pointNumCurrent[i] = 0;
    } while(!calibrateSensorCurrent(i, acceptance));
  }

  printf("All current sensors calibrated\n");
  fflush(stdout);

  lynsyn_release();
}

void calibrateVoltages(double acceptance) {
  if(!lynsyn_init()) {
    printf("Can't init lynsyn.\n");
    fflush(stdout);
    exit(-1);
  }

  printf("This lynsyn has a maximum voltage of %fV\n\n", lynsyn_getMaxVoltage());
  fflush(stdout);

  for(int i = 0; i < SENSORS_BOARD_3; i++) {
    do {
      pointNumVoltage[i] = 0;
    } while(!calibrateSensorVoltage(i, acceptance));
  }

  printf("All voltage sensors calibrated\n");
  fflush(stdout);

  lynsyn_release();
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
      pointNumCurrent[s] = 0;
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
      pointNumVoltage[s] = 0;
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

#ifdef AVERAGE
    sleep(1);
    lynsyn_getSample(&sample, true, 0);
#else
    lynsyn_getAvgSample(&sample, 1, 0);
#endif

    for(int i = 0; i < LYNSYN_MAX_CORES; i++) {
      printf("%" LONGLONGHEX, sample.pc[i]);
    }
    printf(" : ");
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      printf("%f/%f ", sample.current[i], sample.voltage[i]);
    }
    printf("\n");
    fflush(stdout);
  }

  return true;
}

void printBoardInfo(void) {
  if(!lynsyn_preinit(1)) {
    printf("Lynsyn either unconnected, or unprogrammed\n\n");
    fflush(stdout);
    return;
  }
  lynsyn_prerelease();

  if(!lynsyn_init()) {
    printf("Can't initialize lynsyn\n");
    fflush(stdout);
    return;
  }
  
  printf("Lynsyn information:\n");

  uint8_t hwVersion;
  uint8_t bootVersion;
  uint8_t swVersion;
  double r[LYNSYN_MAX_SENSORS];

  lynsyn_getInfo(&hwVersion, &bootVersion, &swVersion, r);

  printf("  HW version:   %s\n", lynsyn_getVersionString(hwVersion));
  printf("  Boot version: %s\n", lynsyn_getVersionString(bootVersion));
  printf("  SW version:   %s\n", lynsyn_getVersionString(swVersion));

  printf("\n  Sensors:\n");
  for(unsigned sensor = 0; sensor < lynsyn_numSensors(); sensor++) {
    printf("    %d: Rs=%f ohm\n", sensor + 1, r[sensor]);

    if(calInfo.pointCurrent[sensor][0] == 0) {
      printf("      Current UNCALIBRATED\n");
    } else {
      printf("      Current calibrated with %d points\n", calInfo.currentPoints[sensor] + 1);
    }

    if(hwVersion >= HW_VERSION_3_0) {
      if(calInfo.pointVoltage[sensor][0] == 0) {
        printf("      Voltage UNCALIBRATED\n");
      } else {
        printf("      Voltage calibrated with %d points\n", calInfo.voltagePoints[sensor] + 1);
      }
    }
    printf("\n");
  }

  printf("\n");

  fflush(stdout);

  lynsyn_release();
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

  printf("\n*** Connect Lynsyn to the PC USB port.\n");
  fflush(stdout);
  getchar();

  printBoardInfo();

  if(arguments.procedure < 1) { 
    char choiceBuf[80];

    printf("Which procedure do you want to perform?\n");
    printf("Enter '1' for pre-init tests, programming and initialization\n");
    printf("Enter '2' for JTAG testing\n");
    printf("Enter '3' for calibrating single current sensor\n");
    printf("Enter '4' for calibrating all current sensors\n");
    if(arguments.board >= 3) {
      printf("Enter '5' for calibrating single voltage sensor\n");
      printf("Enter '6' for calibrating all voltage sensors\n");
    }
    printf("Enter '7' for live measurements\n");

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
      printf("This procedure tests, programs and initializes the Lynsyn board.\n\n");
      fflush(stdout);

      preTest();
      printf("\n");
      program();
      break;

    case 2:
      printf("This procedure tests the Lynsyn board.\n\n");
      fflush(stdout);

      testJtag();
      break;

    case 3:
      calCurrentSensor(arguments.acceptance);
      break;

    case 4:
      calibrateCurrents(arguments.acceptance);
      break;

    case 5:
      if(arguments.board >= 3) {
        calVoltageSensor(arguments.acceptance);
      }
      break;

    case 6:
      if(arguments.board >= 3) {
        calibrateVoltages(arguments.acceptance);
      }
      break;

    case 7:
      live();
      break;

    default:
      return 0;
  }

  return 0;
}

