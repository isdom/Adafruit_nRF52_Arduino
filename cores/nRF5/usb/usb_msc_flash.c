/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach for Adafruit
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#if NRF52840_XXAA

#include "tusb.h"

#if CFG_TUD_MSC

#include "nrf_gpio.h"
#include "flash/flash_qspi.h"

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, const uint8_t scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  const void* response = NULL;
  uint16_t resplen = 0;

  switch ( scsi_cmd[0] )
  {
    case SCSI_CMD_TEST_UNIT_READY:
      // Command that host uses to check our readiness before sending other commands
      resplen = 0;
    break;

    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      // Host is about to read/write etc ... better not to disconnect disk
      resplen = 0;
    break;

    case SCSI_CMD_START_STOP_UNIT:
      // Host try to eject/safe remove/poweroff us. We could safely disconnect with disk storage, or go into lower power
      /* scsi_start_stop_unit_t const * start_stop = (scsi_start_stop_unit_t const *) scsi_cmd;
       // Start bit = 0 : low power mode, if load_eject = 1 : unmount disk storage as well
       // Start bit = 1 : Ready mode, if load_eject = 1 : mount disk storage
       start_stop->start;
       start_stop->load_eject;
       */
      resplen = 0;
    break;

    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return len must not larger than bufsize
  if ( resplen > bufsize )
  {
    resplen = bufsize;
  }

  // copy response to stack's buffer if any
  if ( response && resplen )
  {
    memcpy(buffer, response, resplen);
  }

  return resplen;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  return flash_qspi_read(buffer, lba * CFG_TUD_MSC_BLOCK_SZ + offset, bufsize);
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  (void) lun;

  uint32_t wrcount = flash_qspi_write(lba * CFG_TUD_MSC_BLOCK_SZ + offset, buffer, bufsize);

  // update fatfs's cache if address matches
  extern ExternalFS_usbmsc_write (uint32_t lba, void const* buffer, uint32_t bufsize) ATTR_WEAK;
  if ( ExternalFS_usbmsc_write ) ExternalFS_usbmsc_write(lba, buffer, bufsize);

  return wrcount;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void tud_msc_write10_complete_cb (uint8_t lun)
{
  (void) lun;

  // flush pending cache when write10 is complete
  flash_qspi_flush();
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
  (void) lun;

  *block_count = flash_qspi_size() / CFG_TUD_MSC_BLOCK_SZ;
  *block_size  = CFG_TUD_MSC_BUFSIZE;
}

#endif // CFG_TUD_MSC
#endif // nrf52840
