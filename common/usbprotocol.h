/******************************************************************************
 *
 *  This file is part of the Lynsyn PMU firmware.
 *
 *  Copyright 2018-2019 Asbjørn Djupdal, NTNU
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

#ifndef USBPROTOCOL_H
#define USBPROTOCOL_H

#include <stdint.h>

#define SW_VERSION        SW_VERSION_2_2
#define SW_VERSION_STRING "V2.2"

#define HW_VERSION_2_0 0x20
#define HW_VERSION_2_1 0x21
#define HW_VERSION_2_2 0x22
#define HW_VERSION_3_0 0x30
#define HW_VERSION_3_1 0x31

#define BOOT_VERSION_1_0 0x10
#define BOOT_VERSION_1_1 0x11

#define SW_VERSION_1_0 1
#define SW_VERSION_1_1 0x11
#define SW_VERSION_1_2 0x12
#define SW_VERSION_1_3 0x13
#define SW_VERSION_1_4 0x14
#define SW_VERSION_1_5 0x15
#define SW_VERSION_1_6 0x16
#define SW_VERSION_2_0 0x20
#define SW_VERSION_2_1 0x21
#define SW_VERSION_2_2 0x22

///////////////////////////////////////////////////////////////////////////////

#define USB_CMD_INIT             'i'
#define USB_CMD_HW_INIT           0
#define USB_CMD_JTAG_INIT         1
#define USB_CMD_BREAKPOINT        2
#define USB_CMD_START_SAMPLING    3
#define USB_CMD_CAL               4
#define USB_CMD_CAL_SET           5
#define USB_CMD_TEST              6
#define USB_CMD_GET_SAMPLE        7

#define USB_CMD_UPGRADE_INIT      8
#define USB_CMD_UPGRADE_STORE     9
#define USB_CMD_UPGRADE_FINALISE 10

#define USB_CMD_TCK              11
#define USB_CMD_SHIFT            12
#define USB_CMD_TRST             13

#define USB_CMD_LOG              14

///////////////////////////////////////////////////////////////////////////////

#define BP_TYPE_START 0
#define BP_TYPE_STOP  1
#define BP_TYPE_MARK  2

#define TEST_USB      0
#define TEST_LEDS_ON  4
#define TEST_LEDS_OFF 5

#define SAMPLING_FLAG_BP        0x02
#define SAMPLING_FLAG_MARK      0x04
#define SAMPLING_FLAG_PERIOD    0x08
#define SAMPLING_FLAG_AVERAGE   0x10

#define SAMPLE_REPLY_FLAG_MARK       0x01
#define SAMPLE_REPLY_FLAG_HALTED     0x02
#define SAMPLE_REPLY_FLAG_INVALID    0x04

#define FLASH_BUFFER_SIZE 64
#define SHIFT_BUFFER_SIZE 1024

#define SIZE_JTAG_DEVICE_LIST 256
#define SIZE_ARM_DEVICE_LIST 64

#define MAX_SAMPLES 32
#define MAX_CORES 4
#define MAX_SENSORS 7
#define MAX_POINTS 4
#define CHANNELS 7
#define MAX_PACKET_SIZE (sizeof(struct JtagInitRequestPacket))

#define MAX_LOG_SIZE 2048

struct JtagDevice {
  uint32_t idcode;
  uint32_t irlen;
};

enum { DEVICELIST_END, ARMV7, ARMV8 };

struct ArmDevice {
  uint32_t type;
  uint32_t pidr[5];
  uint32_t pidrmask[5];
};

///////////////////////////////////////////////////////////////////////////////

#pragma pack(push, 4)

struct RequestPacket {
  uint8_t cmd;
};

struct SetTckRequestPacket {
  struct RequestPacket request;
  uint32_t period;
};

struct TrstRequestPacket {
  struct RequestPacket request;
  uint8_t val;
};

struct ShiftRequestPacket {
  struct RequestPacket request;
  uint16_t bits;
  uint8_t tdi[SHIFT_BUFFER_SIZE];
  uint8_t tms[SHIFT_BUFFER_SIZE];
};

struct JtagInitRequestPacketV21 {
  struct RequestPacket request;
  struct JtagDevice devices[SIZE_JTAG_DEVICE_LIST];
};

struct JtagInitRequestPacket {
  struct RequestPacket request;
  struct JtagDevice jtagDevices[SIZE_JTAG_DEVICE_LIST];
  struct ArmDevice armDevices[SIZE_ARM_DEVICE_LIST];
};

struct StartSamplingRequestPacket {
  struct RequestPacket request;
  int64_t samplePeriod;
  uint64_t cores;
  uint64_t flags;
};

struct GetSampleRequestPacket {
  struct RequestPacket request;
  uint64_t cores;
  uint64_t flags;
};

struct BreakpointRequestPacket {
  struct RequestPacket request;
  uint8_t bpType;
  uint64_t addr;
};

struct HwInitRequestPacket {
  struct RequestPacket request;
  uint8_t hwVersion;
  double r[MAX_SENSORS];
};

struct CalibrateRequestPacket {
  struct RequestPacket request;
  uint8_t channel;
  int32_t calVal;
  uint32_t flags;
};

struct CalSetRequestPacket {
  struct RequestPacket request;
  uint8_t channel;
  double offset;
  double gain;
  uint8_t point;
  int16_t actual;
};

struct TestRequestPacket {
  struct RequestPacket request;
  uint8_t testNum;
};

struct UpgradeStoreRequestPacket {
	struct RequestPacket request;
  uint8_t data[FLASH_BUFFER_SIZE];
};

struct UpgradeFinaliseRequestPacket {
	struct RequestPacket request;
  uint32_t crc;
};

///////////////////////////////////////////////////////////////////////////////

struct InitReplyPacket {
  uint8_t hwVersion;
  uint8_t swVersion;
  uint8_t bootVersion;
  uint8_t sensors;
  uint8_t reserved[58];
};

struct CalInfoPacket {
  uint8_t currentPoints[MAX_SENSORS];
  double offsetCurrent[MAX_SENSORS][MAX_POINTS];
  double gainCurrent[MAX_SENSORS][MAX_POINTS];
  int16_t pointCurrent[MAX_SENSORS][MAX_POINTS];

  uint8_t voltagePoints[MAX_SENSORS];
  double offsetVoltage[MAX_SENSORS][MAX_POINTS];
  double gainVoltage[MAX_SENSORS][MAX_POINTS];
  int16_t pointVoltage[MAX_SENSORS][MAX_POINTS];

  double r[MAX_SENSORS];
};

struct TckReplyPacket {
  uint32_t period;
};

struct ShiftReplyPacket {
  uint8_t tdo[SHIFT_BUFFER_SIZE];
};

struct TestReplyPacket {
  uint32_t testStatus;
};

struct UsbTestReplyPacket {
  uint8_t buf[256];
};

struct JtagInitReplyPacket {
  uint8_t success;
  uint8_t numCores;
};

struct LogReplyPacket {
  uint32_t size;
  char buf[MAX_LOG_SIZE];
};

struct SampleReplyPacket {
  int64_t time;
  uint64_t pc[MAX_CORES];
  int16_t channel[CHANNELS];
  uint32_t flags;
};

///////////////////////////////////////////////////////////////////////////////

#pragma pack(pop)

#endif
