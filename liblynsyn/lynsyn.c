/******************************************************************************
 *
 *  liblynsyn
 *
 *  A library for controlling the lynsyn measurement boards
 *  Link with libusb-1.0 when using this library
 *
 *  Copyright 2019 Asbjørn Djupdal, NTNU
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include "lynsyn.h"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <libusb.h>

#define LYNSYN_MAX_SENSOR_VALUE 32768
#define LYNSYN_REF_VOLTAGE 2.5

#define VOLTAGE_DIVIDER_R1 82000
#define VOLTAGE_DIVIDER_R2 10000

#define MAX_TRIES 20

#define CURRENT_SENSOR_GAIN_V3_1 20
#define CURRENT_SENSOR_FACTOR_V3_1 0.125

#define CURRENT_SENSOR_GAIN_V3 100
#define CURRENT_SENSOR_FACTOR_V3 (0.125/5)

#define CURRENT_SENSOR_GAIN_V2 20
#define CURRENT_SENSOR_FACTOR_V2 0.125

///////////////////////////////////////////////////////////////////////////////

bool setBreakpoint(uint8_t type, uint8_t core, uint64_t addr);
bool lynsyn_setStartBreakpoint(uint64_t addr);
bool lynsyn_setStopBreakpoint(uint64_t addr);
void sendBytes(uint8_t *bytes, int numBytes);
bool getBytes(uint8_t *bytes, int numBytes, uint32_t timeout);
bool getArray(uint8_t *bytes, int maxNum, int numBytes, unsigned *elementsReceived, uint32_t timeout);
double getCurrent(int16_t current, int sensor);
double getVoltage(int16_t voltage, int sensor);
void convertSample(struct LynsynSample *dest, struct SampleReplyPacket *source);
uint8_t getCurrentChannel(uint8_t sensor);
uint8_t getVoltageChannel(uint8_t sensor);
static bool lynsyn_getNext(void);

///////////////////////////////////////////////////////////////////////////////

unsigned samplesLeft;
struct SampleReplyPacket *buf;

struct SampleReplyPacket *sampleBuf;

struct CalInfoPacket calInfo;

struct libusb_device_handle *lynsynHandle;
uint8_t outEndpoint;
uint8_t inEndpoint;
struct libusb_context *usbContext;
libusb_device **devs;

uint8_t hwVer;
uint8_t bootVer;
uint8_t swVer;
unsigned numCores;

struct LynsynJtagDevice *devices;

static bool useMarkBp;
static bool inited = false;

unsigned pointNumCurrent[LYNSYN_MAX_SENSORS];
static double lastWantedCurrent[LYNSYN_MAX_SENSORS];
static double lastActualCurrent[LYNSYN_MAX_SENSORS];

unsigned pointNumVoltage[LYNSYN_MAX_SENSORS];
static double lastWantedVoltage[LYNSYN_MAX_SENSORS];
static double lastActualVoltage[LYNSYN_MAX_SENSORS];

///////////////////////////////////////////////////////////////////////////////

bool lynsyn_preinit(unsigned maxTries) {
  libusb_device *lynsynBoard;

  int r = libusb_init(&usbContext);

  if(r < 0) {
    printf("Init Error\n");
    return false;
  }

  bool found = false;
  int numDevices = libusb_get_device_list(usbContext, &devs);
  unsigned tries = 0;
  while(!found && (tries++ < maxTries)) {
    for(int i = 0; i < numDevices; i++) {
      struct libusb_device_descriptor desc;
      libusb_device *dev = devs[i];
      libusb_get_device_descriptor(dev, &desc);
      if(desc.idVendor == 0x10c4 && desc.idProduct == 0x8c1e) {
        lynsynBoard = dev;
        found = true;
        break;
      }
    }
    if(!found) {
      printf("Waiting for Lynsyn device\n");
      fflush(stdout);
      sleep(1);
      numDevices = libusb_get_device_list(usbContext, &devs);
    }
  }

  if(!found) return false;

  int err = libusb_open(lynsynBoard, &lynsynHandle);

  if(err < 0) {
    printf("Could not open USB device\n");
    return false;
  }

  if(libusb_kernel_driver_active(lynsynHandle, 0x1) == 1) {
    err = libusb_detach_kernel_driver(lynsynHandle, 0x1);
    if (err) {
      printf("Failed to detach kernel driver for USB. Someone stole the board?\n");
      return false;
    }
  }

  if((err = libusb_claim_interface(lynsynHandle, 0x1)) < 0) {
    printf("Could not claim interface 0x1, error number %d\n", err);
    return false;
  }

  struct libusb_config_descriptor * config;
  libusb_get_active_config_descriptor(lynsynBoard, &config);
  if(config == NULL) {
    printf("Could not retrieve active configuration for device :(\n");
    return false;
  }

  struct libusb_interface_descriptor interface = config->interface[1].altsetting[0];
  for(int ep = 0; ep < interface.bNumEndpoints; ++ep) {
    if(interface.endpoint[ep].bEndpointAddress & 0x80) {
      inEndpoint = interface.endpoint[ep].bEndpointAddress;
    } else {
      outEndpoint = interface.endpoint[ep].bEndpointAddress;
    }
  }

  inited = true;

  return true;
}

bool lynsyn_postinit(void) {
  struct RequestPacket initRequest;
  initRequest.cmd = USB_CMD_INIT;
  sendBytes((uint8_t*)&initRequest, sizeof(struct RequestPacket));

  struct InitReplyPacket initReply;
  getBytes((uint8_t*)&initReply, sizeof(struct InitReplyPacket), 0);

  hwVer = initReply.hwVersion;
  bootVer = initReply.bootVersion;
  swVer = initReply.swVersion;

  useMarkBp = false;

  if(!initReply.hwVersion) {
    printf("Unsupported Lynsyn HW version: %x\n", initReply.hwVersion);
  }

  if((initReply.swVersion != SW_VERSION_2_0) &&
     (initReply.swVersion != SW_VERSION_2_1) &&
     (initReply.swVersion != SW_VERSION_2_2)){
    printf("Unsupported Lynsyn SW Version: %x\n", initReply.swVersion);
    return false;
  }

  getBytes((uint8_t*)&calInfo, sizeof(struct CalInfoPacket), 0);

  return true;
}

bool lynsyn_init(void) {
  if(!lynsyn_preinit(MAX_TRIES)) {
    fflush(stdout);
    return false;
  }
  if(!lynsyn_postinit()) {
    lynsyn_prerelease();
    fflush(stdout);
    return false;
  }

  sampleBuf = (struct SampleReplyPacket*)malloc(MAX_SAMPLES * sizeof(struct SampleReplyPacket));
  if(!sampleBuf) {
    lynsyn_prerelease();
    return false;
  }

  devices = (struct LynsynJtagDevice*)malloc(sizeof(struct LynsynJtagDevice));
  if(!devices) {
    free(sampleBuf);
    lynsyn_prerelease();
    return false;
  }

  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    pointNumCurrent[i] = 0;
    pointNumVoltage[i] = 0;
  }

  fflush(stdout);

  return true;
}

void lynsyn_prerelease(void) {
  libusb_release_interface(lynsynHandle, 0x1);
  libusb_attach_kernel_driver(lynsynHandle, 0x1);
  libusb_free_device_list(devs, 1);

  libusb_close(lynsynHandle);
  libusb_exit(usbContext);

  inited = false;
}

void lynsyn_release(void) {
  free(devices);
  free(sampleBuf);
  lynsyn_prerelease();
}

bool lynsyn_getInfo(uint8_t *hwVersion, uint8_t *bootVersion, uint8_t *swVersion, double *r) {
  *hwVersion = hwVer;
  *bootVersion = bootVer;
  *swVersion = swVer;
  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    r[i] = calInfo.r[i];
  }
  return true;
}

void sendBytes(uint8_t *bytes, int numBytes) {
  int remaining = numBytes;
  int transfered = 0;
  while(remaining > 0) {
    libusb_bulk_transfer(lynsynHandle, outEndpoint, bytes, numBytes, &transfered, 0);
    remaining -= transfered;
    bytes += transfered;
  }
}

bool getBytes(uint8_t *bytes, int numBytes, uint32_t timeout) {
  int transfered = 0;
  int ret = libusb_bulk_transfer(lynsynHandle, inEndpoint, bytes, numBytes, &transfered, timeout);

  if(ret != 0) {
    return false;
  }

  if(transfered != numBytes) {
    return false;
  }

  return true;
}

bool getArray(uint8_t *bytes, int maxNum, int numBytes, unsigned *elementsReceived, uint32_t timeout) {
  int transfered = 0;
  int ret = libusb_bulk_transfer(lynsynHandle, inEndpoint, bytes, maxNum * numBytes, &transfered, timeout);

  *elementsReceived = transfered / numBytes;

  if(ret != 0) {
    return false;
  }

  if(transfered % numBytes) {
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool lynsyn_cleanNonVolatile(uint8_t hwVersion, double *r) {
  // program flash with HW version info
  struct HwInitRequestPacket req;
  req.request.cmd = USB_CMD_HW_INIT;
  req.hwVersion = hwVersion;
  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    req.r[i] = r[i];
  }
  sendBytes((uint8_t*)&req, sizeof(struct HwInitRequestPacket));

  // read back hw info
  struct RequestPacket initRequest;
  initRequest.cmd = USB_CMD_INIT;
  sendBytes((uint8_t*)&initRequest, sizeof(struct RequestPacket));

  struct InitReplyPacket initReply;
  getBytes((uint8_t*)&initReply, sizeof(struct InitReplyPacket), 0);

  struct CalInfoPacket calInfo;
  getBytes((uint8_t*)&calInfo, sizeof(struct CalInfoPacket), 0);

  if(initReply.hwVersion != hwVersion) {
    return false;
  }

  return true;
}

bool lynsyn_testUsb(void) {
  for(int i = 0; i < 100; i++) {
    struct TestRequestPacket req;
    req.request.cmd = USB_CMD_TEST;
    req.testNum = TEST_USB;
    sendBytes((uint8_t*)&req, sizeof(struct TestRequestPacket));

    struct UsbTestReplyPacket reply;
    getBytes((uint8_t*)&reply, sizeof(struct UsbTestReplyPacket), 0);

    for(int i = 0; i < 256; i++) {
      if(reply.buf[i] != i) {
        return false;
      }
    }
  }

  return true;
}

void lynsyn_setLed(bool on) {
  struct TestRequestPacket req;
  req.request.cmd = USB_CMD_TEST;
  req.testNum = on ? TEST_LEDS_ON : TEST_LEDS_OFF;
  sendBytes((uint8_t*)&req, sizeof(struct TestRequestPacket));
  return;
}

bool lynsyn_adcCalibrateCurrent(uint8_t sensor, double current, double maxCurrent) {
  if(swVer >= SW_VERSION_2_1) {
    double wanted = (current / maxCurrent) * LYNSYN_MAX_SENSOR_VALUE;
    double actual = 0;

    {
      unsigned n = 0;
      lynsyn_startPeriodSampling(1, 0);
      while(lynsyn_getNext()) {
        struct SampleReplyPacket *source = (struct SampleReplyPacket*)buf;
        actual += source->channel[getCurrentChannel(sensor)];
        n++;
      }
      actual /= n;
    }

    double slope = (actual - lastActualCurrent[sensor]) / (wanted - lastWantedCurrent[sensor]);
    double offset = actual - slope * wanted;
    double gain = 1 / slope;

    lastWantedCurrent[sensor] = wanted;
    lastActualCurrent[sensor] = actual;
  
    if(pointNumCurrent[sensor] > 0) {
      if(isinf(offset) || isnan(offset) || isinf(gain) || isnan(gain) ||
         (gain < 0.5) || (gain > 1.5)) {
        printf("Calibration values do not make sense\n");
        return false;
      }

      printf("Calibrating ADC sensor %d (offset %f gain %f) point %d\n", sensor+1, offset, gain, pointNumCurrent[sensor]);

      struct CalSetRequestPacket req;
      req.request.cmd = USB_CMD_CAL_SET;
      req.channel = getCurrentChannel(sensor);
      req.offset = offset;
      req.gain = gain;
      req.point = pointNumCurrent[sensor]-1;
      req.actual = (int16_t)actual;
      sendBytes((uint8_t*)&req, sizeof(struct CalSetRequestPacket));

      if(!lynsyn_postinit()) return false;
    }

    pointNumCurrent[sensor]++;

  } else {
    struct CalibrateRequestPacket req;
    req.request.cmd = USB_CMD_CAL;
    req.channel = getCurrentChannel(sensor);
    req.calVal = (current / maxCurrent) * 0x10000;
    sendBytes((uint8_t*)&req, sizeof(struct CalibrateRequestPacket));
  }

  return true;
}

bool lynsyn_adcCalibrateVoltage(uint8_t sensor, double voltage, double maxVoltage) {
  if(swVer >= SW_VERSION_2_1) {
    double wanted = (voltage / maxVoltage) * LYNSYN_MAX_SENSOR_VALUE;
    double actual = 0;

    {
      unsigned n = 0;
      lynsyn_startPeriodSampling(1, 0);
      while(lynsyn_getNext()) {
        struct SampleReplyPacket *source = (struct SampleReplyPacket*)buf;
        actual += source->channel[getVoltageChannel(sensor)];
        n++;
      }
      actual /= n;
    }

    double slope = (actual - lastActualVoltage[sensor]) / (wanted - lastWantedVoltage[sensor]);
    double offset = actual - slope * wanted;
    double gain = 1 / slope;

    lastWantedVoltage[sensor] = wanted;
    lastActualVoltage[sensor] = actual;
  
    if(pointNumVoltage[sensor] > 0) {
      if(isinf(offset) || isnan(offset) || isinf(gain) || isnan(gain) ||
         (gain < 0.5) || (gain > 1.5)) {
        printf("Calibration values do not make sense\n");
        return false;
      }

      printf("Calibrating ADC sensor %d (offset %f gain %f) point %d\n", sensor+1, offset, gain, pointNumVoltage[sensor]);

      struct CalSetRequestPacket req;
      req.request.cmd = USB_CMD_CAL_SET;
      req.channel = getVoltageChannel(sensor);
      req.offset = offset;
      req.gain = gain;
      req.point = pointNumVoltage[sensor]-1;
      req.actual = (int16_t)actual;
      sendBytes((uint8_t*)&req, sizeof(struct CalSetRequestPacket));

      if(!lynsyn_postinit()) return false;
    }

    pointNumVoltage[sensor]++;

  } else {
    struct CalibrateRequestPacket req;
    req.request.cmd = USB_CMD_CAL;
    req.channel = getVoltageChannel(sensor);
    req.calVal = (voltage / maxVoltage) * 0x10000;
    sendBytes((uint8_t*)&req, sizeof(struct CalibrateRequestPacket));
  }

  return true;
}

bool lynsyn_testAdcCurrent(unsigned sensor, double val, double acceptance) {
  if(!lynsyn_postinit()) {
    return false;
  }

  /* printf("Current points: %d\n", calInfo.currentPoints[sensor]); */
  /* for(int i = 0; i < calInfo.currentPoints[sensor]; i++) { */
  /*   printf("Offset %f Gain %f Point %d\n", calInfo.offsetCurrent[0][i], calInfo.gainCurrent[0][i], calInfo.pointCurrent[0][i]); */
  /* } */

  struct LynsynSample sample;
  if(!lynsyn_getAvgSample(&sample, 1, 0)) {
    return false;
  }

  if((sample.current[sensor] < (val * (1 - acceptance))) || (sample.current[sensor] > (val * (1 + acceptance)))) {
    return false;
  }

  return true;
}

bool lynsyn_testAdcVoltage(unsigned sensor, double val, double acceptance) {
  if(!lynsyn_postinit()) {
    return false;
  }

  /* printf("Voltage points: %d\n", calInfo.voltagePoints[sensor]); */
  /* for(int i = 0; i < calInfo.voltagePoints[sensor]; i++) { */
  /*   printf("Offset %f Gain %f Point %d\n", calInfo.offsetVoltage[0][i], calInfo.gainVoltage[0][i], calInfo.pointVoltage[0][i]); */
  /* } */

  struct LynsynSample sample;
  if(!lynsyn_getAvgSample(&sample, 1, 0)) {
    return false;
  }

  if((sample.voltage[sensor] < (val * (1 - acceptance))) || (sample.voltage[sensor] > (val * (1 + acceptance)))) {
    printf("Error: measured value %f != expected value %f\n", sample.voltage[sensor], val);
    return false;
  }

  return true;
}

uint32_t lynsyn_crc32(uint32_t crc, uint32_t *data, int length) {
  for(int i = 0; i < length; i++) {
    crc = crc + data[i] ;
  }
  return crc ;
}

bool lynsyn_firmwareUpgrade(int size, uint8_t *buf) {
  assert(!inited);

  if(!lynsyn_preinit(MAX_TRIES)) {
    fflush(stdout);
    return false;
  }

  { // post init
    struct RequestPacket initRequest;
    initRequest.cmd = USB_CMD_INIT;
    sendBytes((uint8_t*)&initRequest, sizeof(struct RequestPacket));

    struct InitReplyPacket initReply;
    getBytes((uint8_t*)&initReply, sizeof(struct InitReplyPacket), 0);

    if(initReply.swVersion < SW_VERSION_1_4) {
      printf("Unsupported Lynsyn SW Version: %x\n", initReply.swVersion);
      fflush(stdout);
      return false;
    }
  }

  assert(!(size & 0x3));

  if(size > 0x70000) {
    fflush(stdout);
    return false;
  }

  uint32_t crc = 0;

	struct RequestPacket initRequest;
  initRequest.cmd = USB_CMD_UPGRADE_INIT;
  sendBytes((uint8_t*)&initRequest, sizeof(struct RequestPacket));

  while(size > 0) {
    struct UpgradeStoreRequestPacket upgradeStoreRequest;
    upgradeStoreRequest.request.cmd = USB_CMD_UPGRADE_STORE;

    int bytesToSend = size > FLASH_BUFFER_SIZE ? FLASH_BUFFER_SIZE : size;

    memcpy(upgradeStoreRequest.data, buf, bytesToSend);

    crc = lynsyn_crc32(crc, (uint32_t *)upgradeStoreRequest.data, FLASH_BUFFER_SIZE / 4);
    sendBytes((uint8_t*)&upgradeStoreRequest, sizeof(struct UpgradeStoreRequestPacket));

    size -= bytesToSend;
    buf += bytesToSend;
  }

  struct UpgradeFinaliseRequestPacket finalizePacket;

  finalizePacket.request.cmd = USB_CMD_UPGRADE_FINALISE;
  finalizePacket.crc=crc;
  sendBytes((uint8_t*)&finalizePacket, sizeof(struct UpgradeFinaliseRequestPacket));

  lynsyn_prerelease();

  sleep(1);

  bool success = lynsyn_init();
  if(success) lynsyn_release();

  fflush(stdout);
  return success;
}

static bool isWhiteSpace(char *s) {
  while(*s != '\0') {
    if(!isspace((unsigned char)*s)) {
      return false;
    }
    s++;
  }
  return true;
}

struct LynsynJtagDevice *lynsyn_getJtagDevices(char *filename) {
  FILE *fp = fopen(filename, "rb");
  if(!fp) return NULL;
  
  char line[80];

  int numDevices = 0;

  while(fgets(line, 80, fp)) {

    if(line[0] != ';') {
      if(!isWhiteSpace(line)) {
        char *delim = " \t\n\r";

        char *typetoken = strtok(line, delim);

        if(!strncmp(typetoken, "jtag", 4)) {
          char *idtoken = strtok(NULL, delim);
          char *irlentoken = strtok(NULL, delim);
        
          if(!idtoken || !irlentoken) {
            fclose(fp);
            return NULL;
          }

          struct LynsynJtagDevice device;
          device.type = LYNSYN_JTAG;
          device.idcode = strtol(idtoken, NULL, 0);
          device.irlen = strtol(irlentoken, NULL, 0);

          numDevices++;
          devices = realloc(devices, numDevices * sizeof(struct LynsynJtagDevice));
          devices[numDevices-1] = device;

        } else if(!strncmp(typetoken, "armv7", 5) || !strncmp(typetoken, "armv8", 5)) {
          char *pidrtoken[5];
          char *pidrmasktoken[5];

          for(int i = 0; i < 5; i++) {
            pidrtoken[i] = strtok(NULL, delim);
            pidrmasktoken[i] = strtok(NULL, delim);
            if(!pidrtoken[i] || !pidrmasktoken[i]) {
              fclose(fp);
              return NULL;
            }
          }

          struct LynsynJtagDevice device;
          device.type = !strncmp(typetoken, "armv7", 5) ? LYNSYN_ARMV7 : LYNSYN_ARMV8;
          for(int i = 0; i < 5; i++) {
            device.pidr[i] = strtol(pidrtoken[i], NULL, 0);
            device.pidrmask[i] = strtol(pidrmasktoken[i], NULL, 0);
          }

          numDevices++;
          devices = realloc(devices, numDevices * sizeof(struct LynsynJtagDevice));
          devices[numDevices-1] = device;
        }
      }
    }
  }

  devices = realloc(devices, (numDevices+1) * sizeof(struct LynsynJtagDevice));
  struct LynsynJtagDevice device;
  device.type = LYNSYN_DEVICELIST_END;
  devices[numDevices] = device;

  fclose(fp);

  return devices;
}

struct LynsynJtagDevice *lynsyn_getDefaultJtagDevices(void) {
  struct LynsynJtagDevice *devices = NULL;

  if(!devices) devices = lynsyn_getJtagDevices("jtagdevices");
  if(!devices) devices = lynsyn_getJtagDevices(".jtagdevices");
  if(!devices) devices = lynsyn_getJtagDevices("~/.jtagdevices");
  if(!devices) devices = lynsyn_getJtagDevices("/etc/jtagdevices");

  return devices;
}

bool lynsyn_jtagInit(struct LynsynJtagDevice *devices) {
  if(!devices) return false;

  if(swVer <= SW_VERSION_2_1) {
    struct JtagInitRequestPacketV21 req;
    req.request.cmd = USB_CMD_JTAG_INIT;

    memset(req.devices, 0, sizeof(req.devices));

    unsigned curDevice = 0;
    while(devices[curDevice].type != LYNSYN_DEVICELIST_END) {
      req.devices[curDevice].idcode = devices[curDevice].idcode;
      req.devices[curDevice].irlen = devices[curDevice].irlen;
      curDevice++;

      if(curDevice > SIZE_JTAG_DEVICE_LIST) {
        printf("Too many JTAG devices, can handle maximum %d devices\n", SIZE_JTAG_DEVICE_LIST);
        return false;
      }
    }

    sendBytes((uint8_t*)&req, sizeof(struct JtagInitRequestPacketV21));

  } else {
    struct JtagInitRequestPacket req;
    req.request.cmd = USB_CMD_JTAG_INIT;

    memset(req.jtagDevices, 0, sizeof(req.jtagDevices));
    unsigned curDevice = 0;
    while(devices[curDevice].type != LYNSYN_DEVICELIST_END) {
      req.jtagDevices[curDevice].idcode = devices[curDevice].idcode;
      req.jtagDevices[curDevice].irlen = devices[curDevice].irlen;
      curDevice++;

      if(curDevice > SIZE_JTAG_DEVICE_LIST) {
        printf("Too many JTAG devices, can handle maximum %d devices\n", SIZE_JTAG_DEVICE_LIST);
        return false;
      }
    }

    memset(req.armDevices, 0, sizeof(req.armDevices));
    curDevice = 0;
    while(devices[curDevice].type != DEVICELIST_END) {
      req.armDevices[curDevice].type = devices[curDevice].type;

      for(int i = 0; i < 5; i++) {
        req.armDevices[curDevice].pidr[i] = devices[curDevice].pidr[i];
        req.armDevices[curDevice].pidrmask[i] = devices[curDevice].pidrmask[i];
      }
      curDevice++;

      if(curDevice > SIZE_ARM_DEVICE_LIST) {
        printf("Too many ARM devices, can handle maximum %d devices\n", SIZE_ARM_DEVICE_LIST);
        return false;
      }
    }

    sendBytes((uint8_t*)&req, sizeof(struct JtagInitRequestPacket));
  }
  
  struct JtagInitReplyPacket reply;
  getBytes((uint8_t*)&reply, sizeof(struct JtagInitReplyPacket), 0);
  numCores = reply.numCores;

  return reply.success;
}

bool lynsyn_setBreakpoint(uint8_t type, uint64_t addr) {
  struct BreakpointRequestPacket req;
  req.request.cmd = USB_CMD_BREAKPOINT;
  req.bpType = type;
  req.addr = addr;

  sendBytes((uint8_t*)&req, sizeof(struct BreakpointRequestPacket));

  return true;
}

bool lynsyn_setStartBreakpoint(uint64_t addr) {
  return lynsyn_setBreakpoint(BP_TYPE_START, addr);
}

bool lynsyn_setStopBreakpoint(uint64_t addr) {
  return lynsyn_setBreakpoint(BP_TYPE_STOP, addr);
}

bool lynsyn_setMarkBreakpoint(uint64_t addr) {
  if(lynsyn_setBreakpoint(BP_TYPE_MARK, addr)) {
    useMarkBp = true;
    return true;
  }
  return false;
}

void lynsyn_startPeriodSampling(double duration, uint64_t cores) {
  struct StartSamplingRequestPacket req;
  req.request.cmd = USB_CMD_START_SAMPLING;
  req.samplePeriod = lynsyn_secondsToCycles(duration);
  req.cores = cores;
  req.flags = SAMPLING_FLAG_PERIOD;
  sendBytes((uint8_t*)&req, sizeof(struct StartSamplingRequestPacket));

  samplesLeft = 0;
  buf = sampleBuf;
}

void lynsyn_startBpPeriodSampling(uint64_t startAddr, double duration, uint64_t cores) {
  lynsyn_setStartBreakpoint(startAddr);

  struct StartSamplingRequestPacket req;
  req.request.cmd = USB_CMD_START_SAMPLING;
  req.samplePeriod = lynsyn_secondsToCycles(duration);
  req.cores = cores;
  req.flags = SAMPLING_FLAG_PERIOD | SAMPLING_FLAG_BP;
  sendBytes((uint8_t*)&req, sizeof(struct StartSamplingRequestPacket));

  samplesLeft = 0;
  buf = sampleBuf;
}

void lynsyn_startBpSampling(uint64_t startAddr, uint64_t endAddr, uint64_t cores) {
  lynsyn_setStartBreakpoint(startAddr);
  lynsyn_setStopBreakpoint(endAddr);

  struct StartSamplingRequestPacket req;
  req.request.cmd = USB_CMD_START_SAMPLING;
  req.samplePeriod = 0;
  req.cores = cores;
  req.flags = SAMPLING_FLAG_BP;
  if(useMarkBp) req.flags |= SAMPLING_FLAG_MARK;
  sendBytes((uint8_t*)&req, sizeof(struct StartSamplingRequestPacket));

  samplesLeft = 0;
  buf = sampleBuf;
}

static bool lynsyn_getNext(void) {
  if(samplesLeft == 0) {
    bool transferOk = getArray((uint8_t*)sampleBuf, MAX_SAMPLES, sizeof(struct SampleReplyPacket), &samplesLeft, 0);
    if(!transferOk) return false;
    buf = sampleBuf;

  } else {
    buf++;
  }

  struct SampleReplyPacket *reply = (struct SampleReplyPacket*)buf;
  if(reply->time == -1) return false;
  samplesLeft--;

  return true;
}

bool lynsyn_getNextSample(struct LynsynSample *sample) {
  if(!lynsyn_getNext()) return false;
  convertSample(sample, buf);
  return true;
}

bool lynsyn_getSample(struct LynsynSample *sample, bool average, uint64_t cores) {
  struct GetSampleRequestPacket req;
  req.request.cmd = USB_CMD_GET_SAMPLE;
  req.cores = cores;
  req.flags = (average ? SAMPLING_FLAG_AVERAGE : 0);
  sendBytes((uint8_t*)&req, sizeof(struct GetSampleRequestPacket));

  struct SampleReplyPacket reply;
  getBytes((uint8_t*)&reply, sizeof(struct SampleReplyPacket), 0);

  convertSample(sample, &reply);

  return true;
}

bool lynsyn_getAvgSample(struct LynsynSample *sample, double duration, uint64_t cores) {
#if 0
  struct LynsynSample avgSample;
  unsigned n = 0;

  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    avgSample.current[i] = 0;
    avgSample.voltage[i] = 0;
  }

  for(int i = 0; i < 1000; i++) {
    lynsyn_getSample(sample, false, cores);

    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      avgSample.current[i] += sample->current[i];
      avgSample.voltage[i] += sample->voltage[i];
    }
    n++;
  }

  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    sample->current[i] = avgSample.current[i] / n;
    sample->voltage[i] = avgSample.voltage[i] / n;
  }

  return true;

#else
  struct LynsynSample avgSample;
  unsigned n = 0;

  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    avgSample.current[i] = 0;
    avgSample.voltage[i] = 0;
  }

  lynsyn_startPeriodSampling(duration, cores);

  while(lynsyn_getNextSample(sample)) {
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      avgSample.current[i] += sample->current[i];
      avgSample.voltage[i] += sample->voltage[i];
    }
    n++;
  }

  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    sample->current[i] = avgSample.current[i] / n;
    sample->voltage[i] = avgSample.voltage[i] / n;
  }

  return true;
#endif
}

///////////////////////////////////////////////////////////////////////////////

uint32_t lynsyn_setTck(uint32_t period) {
  struct SetTckRequestPacket req;
  req.request.cmd = USB_CMD_TCK;
  req.period = period;

  sendBytes((uint8_t*)&req, sizeof(struct SetTckRequestPacket));

  struct TckReplyPacket tckReply;
  getBytes((uint8_t*)&tckReply, sizeof(struct TckReplyPacket), 0);

  return tckReply.period;
}

bool lynsyn_shift(int numBits, uint8_t *tmsVector, uint8_t *tdiVector, uint8_t *tdoVector) {
  int numBytes = (numBits + 7) / 8;

  assert(numBytes <= SHIFT_BUFFER_SIZE);

  struct ShiftRequestPacket req;
  req.request.cmd = USB_CMD_SHIFT;
  req.bits = numBits;
  memcpy(req.tms, tmsVector, numBytes);
  memcpy(req.tdi, tdiVector, numBytes);
  sendBytes((uint8_t*)&req, sizeof(struct ShiftRequestPacket));

  struct ShiftReplyPacket shiftReply;
  getBytes((uint8_t*)&shiftReply, sizeof(struct ShiftReplyPacket), 0);
  if(tdoVector) memcpy(tdoVector, shiftReply.tdo, numBytes);

  /* printf("Shifting %d bits\n", numBits); */
  /* for(int i = 0; i < numBytes; i++) { */
  /*   printf("  tms %x tdi %x tdo %x\n", tmsVector[i], tdiVector[i], shiftReply.tdo[i]); */
  /* } */

  return true;
}

bool lynsyn_trst(uint8_t val) {
  struct TrstRequestPacket req;
  req.request.cmd = USB_CMD_TRST;
  req.val = val;
  sendBytes((uint8_t*)&req, sizeof(struct TrstRequestPacket));

  return true;
}

///////////////////////////////////////////////////////////////////////////////

double getCurrent(int16_t current, int sensor) {
  int point;
  for(point = 0; point < (calInfo.currentPoints[sensor]-1); point++) {
    if(current < calInfo.pointCurrent[sensor][point]) break;
  }

  double v;
  double vs;
  double i;

  double offset = calInfo.offsetCurrent[sensor][point];
  double gain = calInfo.gainCurrent[sensor][point];

  v = (((double)current-offset) * (double)LYNSYN_REF_VOLTAGE / (double)LYNSYN_MAX_SENSOR_VALUE) * gain;

  if(hwVer >= HW_VERSION_3_1) {
    vs = v / CURRENT_SENSOR_GAIN_V3_1;
  } else if(hwVer == HW_VERSION_3_0) {
    vs = v / CURRENT_SENSOR_GAIN_V3;
  } else {
    vs = v / CURRENT_SENSOR_GAIN_V2;
  }

  i = vs / calInfo.r[sensor];

  return i;
}

double getVoltage(int16_t voltage, int sensor) {
  int point;
  for(point = 0; point < (calInfo.voltagePoints[sensor]-1); point++) {
    if(voltage < calInfo.pointVoltage[sensor][point]) break;
  }

  double offset = calInfo.offsetVoltage[sensor][point];
  double gain = calInfo.gainVoltage[sensor][point];

  double v = (((double)voltage-offset) * (double)LYNSYN_REF_VOLTAGE / (double)LYNSYN_MAX_SENSOR_VALUE) * gain;
  return v * (VOLTAGE_DIVIDER_R1 + VOLTAGE_DIVIDER_R2) / VOLTAGE_DIVIDER_R2;
}

void convertSample(struct LynsynSample *dest, struct SampleReplyPacket *source) {
  dest->time = source->time;
  for(int i = 0; i < MAX_CORES; i++) {
    dest->pc[i] = source->pc[i];
  }
  for(unsigned i = 0; i < MAX_SENSORS; i++) {
    if(i < lynsyn_numSensors()) {
      dest->current[i] = getCurrent(source->channel[getCurrentChannel(i)], i);
      if(hwVer >= HW_VERSION_3_0) {
        dest->voltage[i] = getVoltage(source->channel[getVoltageChannel(i)], i);
      } else {
        dest->voltage[i] = 0;
      }
    } else {
      dest->current[i] = 0;
      dest->voltage[i] = 0;
    }
  }
  dest->flags = source->flags;
}

uint8_t getCurrentChannel(uint8_t sensor) {
  if(hwVer >= HW_VERSION_3_0) {
    return (2 - sensor) * 2;
  } else {
    return sensor;
  }
}

uint8_t getVoltageChannel(uint8_t sensor) {
  assert(hwVer >= HW_VERSION_3_0);
  return ((2 - sensor) * 2) + 1;
}

double lynsyn_getMaxCurrent(double rl) {
  if(hwVer >= HW_VERSION_3_1) {
    return CURRENT_SENSOR_FACTOR_V3_1/(double)rl;
  } else if(hwVer == HW_VERSION_3_0) {
    return CURRENT_SENSOR_FACTOR_V3/(double)rl;
  } else {
    return CURRENT_SENSOR_FACTOR_V2/(double)rl;
  }
}

double lynsyn_getMaxVoltage(void) {
  return LYNSYN_REF_VOLTAGE * (VOLTAGE_DIVIDER_R1 + VOLTAGE_DIVIDER_R2) / VOLTAGE_DIVIDER_R2;
}

unsigned lynsyn_numCores(void) {
  return numCores;
}

unsigned lynsyn_numSensors(void) {
  if(hwVer >= HW_VERSION_3_0) {
    return 3;
  } else {
    return 7;
  }
}

char *lynsyn_getVersionString(uint8_t version) {
  static char versionString[4];

  if(version == 1) return "1.0";
  versionString[0] = (version >> 4) + '0';
  versionString[1] = '.';
  versionString[2] = (version & 0xf) + '0';
  versionString[3] = 0;

  return versionString;
}

bool lynsyn_getLog(char *buf, unsigned size) {
  if(swVer >= SW_VERSION_2_2) {
    struct RequestPacket req;
    req.cmd = USB_CMD_LOG;
    sendBytes((uint8_t*)&req, sizeof(struct RequestPacket));

    struct LogReplyPacket reply;
    getBytes((uint8_t*)&reply, sizeof(struct LogReplyPacket), 0);

    if(reply.size < size) size = reply.size;
    memcpy(buf, reply.buf, size);

    return true;
  }

  return false;
}
