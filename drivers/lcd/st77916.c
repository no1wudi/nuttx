/****************************************************************************
 * drivers/lcd/st77916.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed
 *with this work for additional information regarding copyright
 *ownership.  The ASF licenses this file to you under the Apache
 *License, Version 2.0 (the "License"); you may not use this file
 *except in compliance with the License.  You may obtain a copy of the
 *License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *implied.  See the License for the specific language governing
 *permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/kmalloc.h>
#include <nuttx/lcd/lcd.h>
#include <nuttx/lcd/st77916.h>
#include <nuttx/spi/qspi.h>
#include <stdio.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LCD_OP_READ_CMD   0x0b
#define LCD_OP_WRITE_CMD  0x02
#define LCD_OP_WRITE_DATA 0x32

#define LCD_CMD_NOP       0x00
#define LCD_CMD_SWRESET   0x01
#define LCD_CMD_RDDID     0x04
#define LCD_CMD_RDDST     0x09
#define LCD_CMD_RDDPM     0x0A
#define LCD_CMD_RDDMADCTL 0x0B
#define LCD_CMD_RDDCOLMOD 0x0C
#define LCD_CMD_RDDIM     0x0D
#define LCD_CMD_RDDSM     0x0E
#define LCD_CMD_RDBST     0x0F
#define LCD_CMD_SLPIN     0x10
#define LCD_CMD_SLPOUT    0x11
#define LCD_CMD_NOROFF    0x12
#define LCD_CMD_NORON     0x13
#define LCD_CMD_INVOFF    0x20
#define LCD_CMD_INVON     0x21
#define LCD_CMD_DISPOFF   0x28
#define LCD_CMD_DISPON    0x29
#define LCD_CMD_CASET     0x2A
#define LCD_CMD_RASET     0x2B
#define LCD_CMD_RAMWR     0x2C
#define LCD_CMD_RAMRD     0x2E
#define LCD_CMD_VSCRDEF   0x33
#define LCD_CMD_TEOFF     0x34
#define LCD_CMD_TEON      0x35
#define LCD_CMD_MADCTL    0x36
#define LCD_CMD_VSCSAD    0x37
#define LCD_CMD_IDMOFF    0x38
#define LCD_CMD_IDMON     0x39
#define LCD_CMD_MOLMOD    0x3A

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This structure describes the state of this driver */

struct st77916_dev_s
{
  /* Publicly visible device structure */

  struct lcd_dev_s dev;

  /* Private LCD-specific information follows */

  FAR struct qspi_dev_s *qspi;  /* SPI device */
  uint8_t                power; /* Current power setting */

  /* This is working memory allocated by the LCD driver for each LCD
   * device and for each color plane. This memory will hold one raster
   * line of data. The size of the allocated run buffer must therefore
   * be at least (bpp * xres / 8).  Actual alignment of the buffer
   * must conform to the bitwidth of the underlying pixel type.
   *
   * If there are multiple planes, they may share the same working
   * buffer because different planes will not be operate on
   * concurrently.  However, if there are multiple LCD devices, they
   * must each have unique run buffers.
   */
};

struct st77916_init_cmd_s
{
  uint8_t  cmd;
  uint8_t *param;
  uint8_t  paramlen;
  uint8_t  delay;
};

static const struct st77916_init_cmd_s init_cmds[] = {
    {0xF0, (uint8_t[]){0x08}, 1, 0},
    {0xF2, (uint8_t[]){0x08}, 1, 0},
    {0x9B, (uint8_t[]){0x51}, 1, 0},
    {0x86, (uint8_t[]){0x53}, 1, 0},
    {0xF2, (uint8_t[]){0x80}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},
    {0xB0, (uint8_t[]){0x54}, 1, 0},
    {0xB1, (uint8_t[]){0x3F}, 1, 0},
    {0xB2, (uint8_t[]){0x2A}, 1, 0},
    {0xB4, (uint8_t[]){0x46}, 1, 0},
    {0xB5, (uint8_t[]){0x34}, 1, 0},
    {0xB6, (uint8_t[]){0xD5}, 1, 0},
    {0xB7, (uint8_t[]){0x30}, 1, 0},
    {0xBA, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x08}, 1, 0},
    {0xBC, (uint8_t[]){0x08}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x80}, 1, 0},
    {0xC1, (uint8_t[]){0x10}, 1, 0},
    {0xC2, (uint8_t[]){0x37}, 1, 0},
    {0xC3, (uint8_t[]){0x80}, 1, 0},
    {0xC4, (uint8_t[]){0x10}, 1, 0},
    {0xC5, (uint8_t[]){0x37}, 1, 0},
    {0xC6, (uint8_t[]){0xA9}, 1, 0},
    {0xC7, (uint8_t[]){0x41}, 1, 0},
    {0xC8, (uint8_t[]){0x51}, 1, 0},
    {0xC9, (uint8_t[]){0xA9}, 1, 0},
    {0xCA, (uint8_t[]){0x41}, 1, 0},
    {0xCB, (uint8_t[]){0x51}, 1, 0},
    {0xD0, (uint8_t[]){0x91}, 1, 0},
    {0xD1, (uint8_t[]){0x68}, 1, 0},
    {0xD2, (uint8_t[]){0x69}, 1, 0},
    {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},
    {0xDD, (uint8_t[]){0x3F}, 1, 0},
    {0xDE, (uint8_t[]){0x3F}, 1, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x02}, 1, 0},
    {0xE0,
     (uint8_t[]){0x70, 0x09, 0x12, 0x0C, 0x0B, 0x27, 0x38, 0x54, 0x4E, 0x19,
                 0x15, 0x15, 0x2C, 0x2F},
     14, 0},
    {0xE1,
     (uint8_t[]){0x70, 0x08, 0x11, 0x0C, 0x0B, 0x27, 0x38, 0x43, 0x4C, 0x18,
                 0x14, 0x14, 0x2B, 0x2D},
     14, 0},
    {0xF0, (uint8_t[]){0x10}, 1, 0},
    {0xF3, (uint8_t[]){0x10}, 1, 0},
    {0xE0, (uint8_t[]){0x08}, 1, 0},
    {0xE1, (uint8_t[]){0x00}, 1, 0},
    {0xE2, (uint8_t[]){0x00}, 1, 0},
    {0xE3, (uint8_t[]){0x00}, 1, 0},
    {0xE4, (uint8_t[]){0xE0}, 1, 0},
    {0xE5, (uint8_t[]){0x06}, 1, 0},
    {0xE6, (uint8_t[]){0x21}, 1, 0},
    {0xE7, (uint8_t[]){0x00}, 1, 0},
    {0xE8, (uint8_t[]){0x05}, 1, 0},
    {0xE9, (uint8_t[]){0x82}, 1, 0},
    {0xEA, (uint8_t[]){0xDF}, 1, 0},
    {0xEB, (uint8_t[]){0x89}, 1, 0},
    {0xEC, (uint8_t[]){0x20}, 1, 0},
    {0xED, (uint8_t[]){0x14}, 1, 0},
    {0xEE, (uint8_t[]){0xFF}, 1, 0},
    {0xEF, (uint8_t[]){0x00}, 1, 0},
    {0xF8, (uint8_t[]){0xFF}, 1, 0},
    {0xF9, (uint8_t[]){0x00}, 1, 0},
    {0xFA, (uint8_t[]){0x00}, 1, 0},
    {0xFB, (uint8_t[]){0x30}, 1, 0},
    {0xFC, (uint8_t[]){0x00}, 1, 0},
    {0xFD, (uint8_t[]){0x00}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x00}, 1, 0},
    {0x60, (uint8_t[]){0x42}, 1, 0},
    {0x61, (uint8_t[]){0xE0}, 1, 0},
    {0x62, (uint8_t[]){0x40}, 1, 0},
    {0x63, (uint8_t[]){0x40}, 1, 0},
    {0x64, (uint8_t[]){0x02}, 1, 0},
    {0x65, (uint8_t[]){0x00}, 1, 0},
    {0x66, (uint8_t[]){0x40}, 1, 0},
    {0x67, (uint8_t[]){0x03}, 1, 0},
    {0x68, (uint8_t[]){0x00}, 1, 0},
    {0x69, (uint8_t[]){0x00}, 1, 0},
    {0x6A, (uint8_t[]){0x00}, 1, 0},
    {0x6B, (uint8_t[]){0x00}, 1, 0},
    {0x70, (uint8_t[]){0x42}, 1, 0},
    {0x71, (uint8_t[]){0xE0}, 1, 0},
    {0x72, (uint8_t[]){0x40}, 1, 0},
    {0x73, (uint8_t[]){0x40}, 1, 0},
    {0x74, (uint8_t[]){0x02}, 1, 0},
    {0x75, (uint8_t[]){0x00}, 1, 0},
    {0x76, (uint8_t[]){0x40}, 1, 0},
    {0x77, (uint8_t[]){0x03}, 1, 0},
    {0x78, (uint8_t[]){0x00}, 1, 0},
    {0x79, (uint8_t[]){0x00}, 1, 0},
    {0x7A, (uint8_t[]){0x00}, 1, 0},
    {0x7B, (uint8_t[]){0x00}, 1, 0},
    {0x80, (uint8_t[]){0x48}, 1, 0},
    {0x81, (uint8_t[]){0x00}, 1, 0},
    {0x82, (uint8_t[]){0x05}, 1, 0},
    {0x83, (uint8_t[]){0x02}, 1, 0},
    {0x84, (uint8_t[]){0xDD}, 1, 0},
    {0x85, (uint8_t[]){0x00}, 1, 0},
    {0x86, (uint8_t[]){0x00}, 1, 0},
    {0x87, (uint8_t[]){0x00}, 1, 0},
    {0x88, (uint8_t[]){0x48}, 1, 0},
    {0x89, (uint8_t[]){0x00}, 1, 0},
    {0x8A, (uint8_t[]){0x07}, 1, 0},
    {0x8B, (uint8_t[]){0x02}, 1, 0},
    {0x8C, (uint8_t[]){0xDF}, 1, 0},
    {0x8D, (uint8_t[]){0x00}, 1, 0},
    {0x8E, (uint8_t[]){0x00}, 1, 0},
    {0x8F, (uint8_t[]){0x00}, 1, 0},
    {0x90, (uint8_t[]){0x48}, 1, 0},
    {0x91, (uint8_t[]){0x00}, 1, 0},
    {0x92, (uint8_t[]){0x09}, 1, 0},
    {0x93, (uint8_t[]){0x02}, 1, 0},
    {0x94, (uint8_t[]){0xE1}, 1, 0},
    {0x95, (uint8_t[]){0x00}, 1, 0},
    {0x96, (uint8_t[]){0x00}, 1, 0},
    {0x97, (uint8_t[]){0x00}, 1, 0},
    {0x98, (uint8_t[]){0x48}, 1, 0},
    {0x99, (uint8_t[]){0x00}, 1, 0},
    {0x9A, (uint8_t[]){0x0B}, 1, 0},
    {0x9B, (uint8_t[]){0x02}, 1, 0},
    {0x9C, (uint8_t[]){0xE3}, 1, 0},
    {0x9D, (uint8_t[]){0x00}, 1, 0},
    {0x9E, (uint8_t[]){0x00}, 1, 0},
    {0x9F, (uint8_t[]){0x00}, 1, 0},
    {0xA0, (uint8_t[]){0x48}, 1, 0},
    {0xA1, (uint8_t[]){0x00}, 1, 0},
    {0xA2, (uint8_t[]){0x04}, 1, 0},
    {0xA3, (uint8_t[]){0x02}, 1, 0},
    {0xA4, (uint8_t[]){0xDC}, 1, 0},
    {0xA5, (uint8_t[]){0x00}, 1, 0},
    {0xA6, (uint8_t[]){0x00}, 1, 0},
    {0xA7, (uint8_t[]){0x00}, 1, 0},
    {0xA8, (uint8_t[]){0x48}, 1, 0},
    {0xA9, (uint8_t[]){0x00}, 1, 0},
    {0xAA, (uint8_t[]){0x06}, 1, 0},
    {0xAB, (uint8_t[]){0x02}, 1, 0},
    {0xAC, (uint8_t[]){0xDE}, 1, 0},
    {0xAD, (uint8_t[]){0x00}, 1, 0},
    {0xAE, (uint8_t[]){0x00}, 1, 0},
    {0xAF, (uint8_t[]){0x00}, 1, 0},
    {0xB0, (uint8_t[]){0x48}, 1, 0},
    {0xB1, (uint8_t[]){0x00}, 1, 0},
    {0xB2, (uint8_t[]){0x08}, 1, 0},
    {0xB3, (uint8_t[]){0x02}, 1, 0},
    {0xB4, (uint8_t[]){0xE0}, 1, 0},
    {0xB5, (uint8_t[]){0x00}, 1, 0},
    {0xB6, (uint8_t[]){0x00}, 1, 0},
    {0xB7, (uint8_t[]){0x00}, 1, 0},
    {0xB8, (uint8_t[]){0x48}, 1, 0},
    {0xB9, (uint8_t[]){0x00}, 1, 0},
    {0xBA, (uint8_t[]){0x0A}, 1, 0},
    {0xBB, (uint8_t[]){0x02}, 1, 0},
    {0xBC, (uint8_t[]){0xE2}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x12}, 1, 0},
    {0xC1, (uint8_t[]){0xAA}, 1, 0},
    {0xC2, (uint8_t[]){0x65}, 1, 0},
    {0xC3, (uint8_t[]){0x74}, 1, 0},
    {0xC4, (uint8_t[]){0x47}, 1, 0},
    {0xC5, (uint8_t[]){0x56}, 1, 0},
    {0xC6, (uint8_t[]){0x00}, 1, 0},
    {0xC7, (uint8_t[]){0x88}, 1, 0},
    {0xC8, (uint8_t[]){0x99}, 1, 0},
    {0xC9, (uint8_t[]){0x33}, 1, 0},
    {0xD0, (uint8_t[]){0x21}, 1, 0},
    {0xD1, (uint8_t[]){0xAA}, 1, 0},
    {0xD2, (uint8_t[]){0x65}, 1, 0},
    {0xD3, (uint8_t[]){0x74}, 1, 0},
    {0xD4, (uint8_t[]){0x47}, 1, 0},
    {0xD5, (uint8_t[]){0x56}, 1, 0},
    {0xD6, (uint8_t[]){0x00}, 1, 0},
    {0xD7, (uint8_t[]){0x88}, 1, 0},
    {0xD8, (uint8_t[]){0x99}, 1, 0},
    {0xD9, (uint8_t[]){0x33}, 1, 0},
    {0xF3, (uint8_t[]){0x01}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},
    {0xA0, (uint8_t[]){0x0B}, 1, 0},
    {0xA3, (uint8_t[]){0x2A}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x2B}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x2C}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x2D}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x2E}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x2F}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x30}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x31}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x32}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA3, (uint8_t[]){0x33}, 1, 0},
    {0xA5, (uint8_t[]){0xC3}, 1, 1},
    {0xA0, (uint8_t[]){0x09}, 1, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x67}, 4, 0},
    {0x2B, (uint8_t[]){0x01, 0x68, 0x01, 0x68}, 4, 0},
    {0x4D, (uint8_t[]){0x00}, 1, 0},
    {0x4E, (uint8_t[]){0x00}, 1, 0},
    {0x4F, (uint8_t[]){0x00}, 1, 0},
    {0x4C, (uint8_t[]){0x01}, 1, 10},
    {0x4C, (uint8_t[]){0x00}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x67}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0x67}, 4, 0},
    {0x21, (uint8_t[]){0x00}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0}, // color=16
    {0x11, (uint8_t[]){0x00}, 1, 100},
    {0x29, (uint8_t[]){0x00}, 1, 20},
    // {0x4d, (uint8_t[]){0xff}, 1, 0},
    // {0x4e, (uint8_t[]){0xff}, 1, 0},
    // {0x4f, (uint8_t[]){0xff}, 1, 0},
    // {0x4c, (uint8_t[]){0x01}, 1, 0},
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void lcd_sendcmd(FAR struct st77916_dev_s *dev, uint8_t cmd,
                        uint8_t *param, uint32_t len)
{
  struct qspi_cmdinfo_s cmdinfo;

  cmdinfo.flags   = QSPICMD_ADDRESS | (len > 0 ? QSPICMD_WRITEDATA : 0);
  cmdinfo.cmd     = LCD_OP_WRITE_CMD;
  cmdinfo.addrlen = 3;
  cmdinfo.addr    = cmd << 8;
  cmdinfo.buflen  = len;

  cmdinfo.buffer = param;

  QSPI_LOCK(dev->qspi, true);
  QSPI_SETBITS(dev->qspi, 8);
  QSPI_SETMODE(dev->qspi, QSPIDEV_MODE0);
  QSPI_SETFREQUENCY(dev->qspi, CONFIG_LCD_ST77916_FREQUENCY);
  QSPI_COMMAND(dev->qspi, &cmdinfo);
  QSPI_LOCK(dev->qspi, false);
}

static void lcd_senddata(FAR struct st77916_dev_s *dev, uint8_t cmd,
                         uint8_t *param, uint32_t len)
{
  struct qspi_meminfo_s meminfo;

  meminfo.flags   = QSPIMEM_WRITE;
  meminfo.cmd     = LCD_OP_WRITE_CMD;
  meminfo.addrlen = 3;
  meminfo.addr    = cmd << 8;
  meminfo.buflen  = len;
  meminfo.buffer  = param;
  meminfo.dummies = 0;

  QSPI_LOCK(dev->qspi, true);
  QSPI_SETBITS(dev->qspi, 8);
  QSPI_SETMODE(dev->qspi, QSPIDEV_MODE0);
  QSPI_SETFREQUENCY(dev->qspi, CONFIG_LCD_ST77916_FREQUENCY);
  QSPI_MEMORY(dev->qspi, &meminfo);
  QSPI_LOCK(dev->qspi, false);
}

static void st77916_setarea(FAR struct st77916_dev_s *dev, uint16_t x0,
                            uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint8_t param[4];

  param[0] = x0 >> 8;
  param[1] = x0 & 0xff;
  param[2] = x1 >> 8;
  param[3] = x1 & 0xff;

  lcd_sendcmd(dev, LCD_CMD_CASET, param, 4);

  param[0] = y0 >> 8;
  param[1] = y0 & 0xff;
  param[2] = y1 >> 8;
  param[3] = y1 & 0xff;

  lcd_sendcmd(dev, LCD_CMD_RASET, param, 4);
}

static void lcd_read(struct st77916_dev_s *dev, uint8_t cmd)
{
  struct qspi_cmdinfo_s cmdinfo;
  uint8_t               data[4];
  int                   ret;

  cmdinfo.flags   = QSPICMD_READDATA | QSPICMD_ADDRESS;
  cmdinfo.cmd     = LCD_OP_READ_CMD;
  cmdinfo.buflen  = sizeof(data);
  cmdinfo.buffer  = data;
  cmdinfo.addrlen = 3;
  cmdinfo.addr    = cmd;

  QSPI_LOCK(dev->qspi, true);
  QSPI_SETBITS(dev->qspi, 8);
  QSPI_SETFREQUENCY(dev->qspi, CONFIG_LCD_ST77916_FREQUENCY);
  ret = QSPI_COMMAND(dev->qspi, &cmdinfo);
  QSPI_LOCK(dev->qspi, false);

  printf("ret = %d\n", ret);
  printf("DATA: %02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);
}

uint8_t frame[0xffff] = {0xff};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct lcd_dev_s *st77916_lcdinitialize(FAR struct qspi_dev_s *spi)
{
  struct st77916_dev_s *dev;

  dev = (struct st77916_dev_s *)kmm_malloc(sizeof(struct st77916_dev_s));
  if (!dev)
    {
      return NULL;
    }

  dev->qspi = spi;

  for (int i = 0; i < sizeof(init_cmds) / sizeof(struct st77916_init_cmd_s);
       i++)
    {
      lcd_sendcmd(dev, init_cmds[i].cmd, init_cmds[i].param,
                  init_cmds[i].paramlen);

      up_mdelay(init_cmds[i].delay);
    }

  // st77916_setarea(dev, 0, 0, 359, 359);

  lcd_sendcmd(dev, 0x2c, frame, 0xffff);

  return &dev->dev;
}
