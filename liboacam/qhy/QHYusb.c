/*****************************************************************************
 *
 * QHYusb.c -- USB interface for QHY cameras
 *
 * Copyright 2013,2014,2015,2017 James Fidell (james@openastroproject.org)
 *
 * License:
 *
 * This file is part of the Open Astro Project.
 *
 * The Open Astro Project is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Open Astro Project is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Open Astro Project.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <oa_common.h>
#include <openastro/camera.h>
#include <openastro/util.h>
#include <openastro/errno.h>

#include "oacamprivate.h"
#include "QHY.h"
#include "QHYoacam.h"
#include "QHYstate.h"
#include "QHYusb.h"


int
_usbControlMsg ( QHY_STATE* cameraInfo, uint8_t reqType, uint8_t req,
    uint16_t value, uint16_t index, unsigned char* data, uint16_t length,
    unsigned int timeout )
{
  int		ret;

  pthread_mutex_lock ( &cameraInfo->usbMutex );

  oacamDebugMsg ( DEBUG_CAM_USB,
      "QHY USB: %s ( %d, 0x%x, %d, %d, %0lx, %d, %d )\n", __FUNCTION__,
      reqType, req, value, index, ( unsigned long ) data, length, timeout );

  ret = libusb_control_transfer ( cameraInfo->usbHandle, reqType, req, value,
      index, data, length, timeout );
  pthread_mutex_unlock ( &cameraInfo->usbMutex );
  if ( ret != length ) {
    fprintf ( stderr, "libusb control error: %d\n", ret );
    fprintf ( stderr, "%s\n", libusb_error_name ( ret ));
  }
  return ret;
}


int
_usbBulkTransfer ( QHY_STATE* cameraInfo, unsigned char endpoint,
    unsigned char* data, int length, unsigned int* transferred,
    unsigned int timeout )
{
  int		ret;

  pthread_mutex_lock ( &cameraInfo->usbMutex );

  oacamDebugMsg ( DEBUG_CAM_USB,
      "QHY USB: %s ( 0x%x, %0lx, %d, xferred, %d )\n", __FUNCTION__,
      endpoint, ( unsigned long ) data, length, timeout );

  ret = libusb_bulk_transfer ( cameraInfo->usbHandle, endpoint, data, length,
      ( int* ) transferred, timeout );
  pthread_mutex_unlock ( &cameraInfo->usbMutex );
  // if ( ret ) {
  //   fprintf ( stderr, "libusb bulk error: %d\n", ret );
  //   fprintf ( stderr, "%s\n", libusb_error_name ( ret ));
  // }
  return ret;
}


unsigned short
_i2cRead16 ( QHY_STATE* cameraInfo, unsigned short address )
{
  unsigned char data[2];

  oacamDebugMsg ( DEBUG_CAM_USB, "QHY: USB: %s ( %d )\n", __FUNCTION__,
      address );

  _usbControlMsg ( cameraInfo, QHY_CMD_DEFAULT_IN, 0xb7, 0, address, data,
      2, 0 );
  return data[0] * 256 + data[1];
}


int
_i2cWrite16 ( QHY_STATE* cameraInfo, unsigned short address,
    unsigned short value )
{
  unsigned char data[2];

  oacamDebugMsg ( DEBUG_CAM_USB, "QHY USB: %s ( %d, %d )\n",
      __FUNCTION__, address, value );

  data[0] = value >> 8;
  data[1] = value & 0xff;

  return ( _usbControlMsg ( cameraInfo, QHY_CMD_DEFAULT_OUT, 0xbb, 0, address,
      data, 2, 0 ) == 2 ? 0 : -1 );
}


int
_i2cWriteIMX035 ( QHY_STATE* cameraInfo, unsigned char address,
    unsigned char value )
{
  unsigned char data[32];

  oacamDebugMsg ( DEBUG_CAM_USB, "QHY USB IMX035: %s ( %d, %d )\n",
      __FUNCTION__, address, value );

  memset ( data, 0, 32 );
  data[1] = address;
  data[2] = value;

  return ( _usbControlMsg ( cameraInfo, QHY_CMD_DEFAULT_OUT, 0xb8, 0, 0,
      data, 0x13, 3000 ) == 0x13 ? 0 : -1 );
}


void
qhyStatusCallback ( struct libusb_transfer* transfer )
{
  int           resubmit = 1;

  switch ( transfer->status ) {
    case LIBUSB_TRANSFER_ERROR:
    case LIBUSB_TRANSFER_NO_DEVICE:
      // fprintf ( stderr, "not processing/resubmitting status xfer: err = %d\n",
      //     transfer->status );
      return;
      break;

    case LIBUSB_TRANSFER_CANCELLED:
      // FIX ME -- I can't get this to work without causing a crash, but
      // things seem to work ok for the moment without it.  Needs more
      // investigation
/*
      pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
      if ( cameraInfo->statusTransfer ) {
        free ( cameraInfo->statusBuffer );
        libusb_free_transfer ( transfer );
        cameraInfo->statusTransfer = 0;
      }
      pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
      resubmit = 0;
*/
      break;

    case LIBUSB_TRANSFER_COMPLETED:
      // This is the good one, but for the moment we'll do nothing here
      // fprintf ( stderr, "unhandled completed status xfer\n" );
      break;

    case LIBUSB_TRANSFER_TIMED_OUT:
    case LIBUSB_TRANSFER_STALL:
    case LIBUSB_TRANSFER_OVERFLOW:
      // fprintf ( stderr, "retrying xfer, status = %d\n", transfer->status );
      break;
    default:
      fprintf ( stderr, "unexpected interrupt transfer status = %d\n",
          transfer->status );
      break;
  }

  if ( resubmit ) {
    libusb_submit_transfer ( transfer );
  }
  return;
}
