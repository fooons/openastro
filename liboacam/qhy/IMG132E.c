/*****************************************************************************
 *
 * IMG132E.c -- IMG132E camera interface
 *
 * Copyright 2017,2018,2019,2020,2021,2023
 *   James Fidell (james@openastroproject.org)
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

#include <libusb-1.0/libusb.h>

#include "oacamprivate.h"
#include "QHY.h"
#include "QHYoacam.h"
#include "QHYstate.h"
#include "IMG132E.h"
#include "QHYusb.h"


static void		_IMG132EInitFunctionPointers ( oaCamera* );
void*			_img132eEventHandler ( void* );

static int		oaIMG132ECameraGetFramePixelFormat ( oaCamera* );
static const FRAMESIZES* oaIMG132ECameraGetFrameSizes ( oaCamera* );
static int		oaIMG132ECloseCamera ( oaCamera* );


/**
 * Initialise a given camera device
 */

int
_IMG132EInitCamera ( oaCamera* camera )
{
  int		i, j;
  QHY_STATE*	cameraInfo = camera->_private;
  COMMON_INFO*	commonInfo = camera->_common;
  void*		dummy;

  oaLogInfo ( OA_LOG_CAMERA, "%s ( %p ): entered", __func__, camera );

  _IMG132EInitFunctionPointers ( camera );

  cameraInfo->currentFrameFormat = OA_PIX_FMT_BGGR8;
  camera->frameFormats [ OA_PIX_FMT_BGGR8 ] = 1;
  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_FRAME_FORMAT ) = OA_CTRL_TYPE_DISCRETE;

  cameraInfo->currentBitDepth = 8;
  cameraInfo->longExposureMode = 0;

  cameraInfo->maxResolutionX = IMG132E_IMAGE_WIDTH;
  cameraInfo->maxResolutionY = IMG132E_IMAGE_HEIGHT;

  if (!( cameraInfo->frameSizes[1].sizes =
      ( FRAMESIZE* ) malloc ( 5 * sizeof ( FRAMESIZE )))) {
    oaLogError ( OA_LOG_CAMERA, "%s: malloc ( FRAMESIZE ) failed", __func__ );
    return -OA_ERR_MEM_ALLOC;
  }

  cameraInfo->frameSizes[1].sizes[0].x = cameraInfo->maxResolutionX;
  cameraInfo->frameSizes[1].sizes[0].y = cameraInfo->maxResolutionY;
  cameraInfo->frameSizes[1].numSizes = 1;

  cameraInfo->xSize = cameraInfo->maxResolutionX;
  cameraInfo->ySize = cameraInfo->maxResolutionY;

  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_RED_BALANCE ) = OA_CTRL_TYPE_INT32;
  commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_RED_BALANCE ) = 65;
  commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_RED_BALANCE ) = 255;
  commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_RED_BALANCE ) = 1;
  commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_RED_BALANCE ) = 58;
  cameraInfo->currentRedBalance = 58;

  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_BLUE_BALANCE ) = OA_CTRL_TYPE_INT32;
  commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_BLUE_BALANCE ) = 65;
  commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_BLUE_BALANCE ) = 255;
  commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_BLUE_BALANCE ) = 1;
  commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_BLUE_BALANCE ) = 58;
  cameraInfo->currentBlueBalance = 58;

  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_GREEN_BALANCE ) = OA_CTRL_TYPE_INT32;
  commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_GREEN_BALANCE ) = 65;
  commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_GREEN_BALANCE ) = 255;
  commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_GREEN_BALANCE ) = 1;
  commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_GREEN_BALANCE ) = 58;
  cameraInfo->currentGreenBalance = 58;

  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_GAIN ) = OA_CTRL_TYPE_INT32;
  commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_GAIN ) = 1;
  commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_GAIN ) = 0xe00;
  commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_GAIN ) = 1;
  commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_GAIN ) =
      IMG132E_DEFAULT_GAIN;
  cameraInfo->currentGain = IMG132E_DEFAULT_GAIN;

  // FIX ME -- offset command ?

  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) =
      OA_CTRL_TYPE_INT64;
  commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) = 1;
  commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) = 79000;
  commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) = 1;
  commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) =
      IMG132E_DEFAULT_EXPOSURE;
  cameraInfo->currentExposure = IMG132E_DEFAULT_EXPOSURE;

  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_DIGITAL_GAIN ) =
      OA_CTRL_TYPE_INT32;
  commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_DIGITAL_GAIN ) = 0;
  commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_DIGITAL_GAIN ) = 3;
  commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_DIGITAL_GAIN ) = 1;
  commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_DIGITAL_GAIN ) =
      IMG132E_DEFAULT_DIGITAL_GAIN;
  cameraInfo->currentDigitalGain = IMG132E_DEFAULT_DIGITAL_GAIN;

  cameraInfo->buffers = 0;
  cameraInfo->configuredBuffers = 0;

  cameraInfo->frameSize = cameraInfo->maxResolutionX *
      cameraInfo->maxResolutionY;
  cameraInfo->captureLength = cameraInfo->frameSize;
  cameraInfo->imageBufferLength = 2 * ( cameraInfo->maxResolutionX *
      cameraInfo->maxResolutionY );

  if ( oaIMG132EInitialiseRegisters ( cameraInfo ) != OA_ERR_NONE ) {
    oaLogError ( OA_LOG_CAMERA, "%s: IMG132E register initialisation failed",
				__func__ );
    free (( void* ) cameraInfo->frameSizes[1].sizes );
    return -OA_ERR_SYSTEM_ERROR;
  }

  pthread_create ( &cameraInfo->eventHandler, 0, _img132eEventHandler,
      ( void* ) cameraInfo );

  if (!( cameraInfo->buffers = calloc ( OA_CAM_BUFFERS,
      sizeof ( frameBuffer )))) {
    oaLogError ( OA_LOG_CAMERA, "%s: malloc of buffer array failed",
				__func__ );
    cameraInfo->stopCallbackThread = 1;
    pthread_join ( cameraInfo->eventHandler, &dummy );
    free (( void* ) cameraInfo->frameSizes[1].sizes );
    return -OA_ERR_MEM_ALLOC;
  }

  for ( i = 0; i < OA_CAM_BUFFERS; i++ ) {
    void* m = malloc ( cameraInfo->imageBufferLength );
    if ( m ) {
      cameraInfo->buffers[i].start = m;
      cameraInfo->configuredBuffers++;
    } else {
      oaLogError ( OA_LOG_CAMERA, "%s: malloc of buffer failed",  __func__ );
      if ( i ) {
        for ( j = 0; j < i; j++ ) {
          free (( void* ) cameraInfo->buffers[j].start );
        }
      }
      cameraInfo->stopCallbackThread = 1;
      pthread_join ( cameraInfo->eventHandler, &dummy );
      free (( void* ) cameraInfo->buffers );
      free (( void* ) cameraInfo->frameSizes[1].sizes );
      return -OA_ERR_MEM_ALLOC;
    }
  }

  cameraInfo->buffersFree = cameraInfo->configuredBuffers;
  cameraInfo->nextBuffer = 0;
  cameraInfo->stopControllerThread = cameraInfo->stopCallbackThread = 0;
  cameraInfo->commandQueue = oaDLListCreate();
  cameraInfo->callbackQueue = oaDLListCreate();
  if ( pthread_create ( &( cameraInfo->controllerThread ), 0,
      oacamIMG132Econtroller, ( void* ) camera )) {
    for ( j = 0; j < OA_CAM_BUFFERS; j++ ) {
      free (( void* ) cameraInfo->buffers[j].start );
    }
    cameraInfo->stopCallbackThread = 1;
    pthread_join ( cameraInfo->eventHandler, &dummy );
    free (( void* ) cameraInfo->frameSizes[1].sizes );
    free (( void* ) cameraInfo->buffers );
    free (( void* ) camera->_common );
    free (( void* ) camera->_private );
    free (( void* ) camera );
    oaDLListDelete ( cameraInfo->commandQueue, 0 );
    oaDLListDelete ( cameraInfo->callbackQueue, 0 );
    return -OA_ERR_SYSTEM_ERROR;
  }
  if ( pthread_create ( &( cameraInfo->callbackThread ), 0,
      oacamQHYcallbackHandler, ( void* ) camera )) {

    void* dummy;
    cameraInfo->stopControllerThread = 1;
    pthread_cond_broadcast ( &cameraInfo->commandQueued );
    pthread_join ( cameraInfo->controllerThread, &dummy );
    for ( j = 0; j < OA_CAM_BUFFERS; j++ ) {
      free (( void* ) cameraInfo->buffers[j].start );
    }
    cameraInfo->stopCallbackThread = 1;
    pthread_join ( cameraInfo->eventHandler, &dummy );
    free (( void* ) cameraInfo->frameSizes[1].sizes );
    free (( void* ) cameraInfo->buffers );
    free (( void* ) camera->_common );
    free (( void* ) camera->_private );
    free (( void* ) camera );
    oaDLListDelete ( cameraInfo->commandQueue, 0 );
    oaDLListDelete ( cameraInfo->callbackQueue, 0 );
    return -OA_ERR_SYSTEM_ERROR;
  }

  camera->features.flags |= OA_CAM_FEATURE_STREAMING;
  camera->features.pixelSizeX = 3360;
  camera->features.pixelSizeY = 3360;

  oaLogInfo ( OA_LOG_CAMERA, "%s: exiting", __func__ );

  return OA_ERR_NONE;
}


const static FRAMESIZES*
oaIMG132ECameraGetFrameSizes ( oaCamera* camera )
{
  QHY_STATE*    cameraInfo = camera->_private;

  return &cameraInfo->frameSizes[1];
}


static int
oaIMG132ECloseCamera ( oaCamera* camera )
{
  int		j, res;
  QHY_STATE*	cameraInfo;
  void*		dummy;

  oaLogInfo ( OA_LOG_CAMERA, "%s ( %p ): entered", __func__, camera );

  if ( camera ) {

    cameraInfo = camera->_private;

    cameraInfo->stopControllerThread = 1;

    pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
    if ( cameraInfo->statusTransfer ) {
      res = libusb_cancel_transfer ( cameraInfo->statusTransfer );
      if ( res < 0 && res != LIBUSB_ERROR_NOT_FOUND ) {
        libusb_free_transfer ( cameraInfo->statusTransfer );
        cameraInfo->statusTransfer = 0;
      }
    }
    pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );

    pthread_cond_broadcast ( &cameraInfo->commandQueued );
    pthread_join ( cameraInfo->controllerThread, &dummy );

    cameraInfo->stopCallbackThread = 1;
    pthread_cond_broadcast ( &cameraInfo->callbackQueued );
    pthread_join ( cameraInfo->callbackThread, &dummy );

    libusb_release_interface ( cameraInfo->usbHandle, 0 );
    libusb_close ( cameraInfo->usbHandle );
    libusb_exit ( cameraInfo->usbContext );

    if ( cameraInfo->buffers ) {
      for ( j = 0; j < OA_CAM_BUFFERS; j++ ) {
        if ( cameraInfo->buffers[j].start ) {
          free (( void* ) cameraInfo->buffers[j].start );
        }
      }
    }

    free (( void* ) cameraInfo->frameSizes[1].sizes );

    oaDLListDelete ( cameraInfo->commandQueue, 1 );
    oaDLListDelete ( cameraInfo->callbackQueue, 0 );

    free (( void* ) cameraInfo->buffers );
    free (( void* ) cameraInfo );
    free (( void* ) camera->_common );
    free (( void* ) camera );
  } else {
   return -OA_ERR_INVALID_CAMERA;
  }

  oaLogInfo ( OA_LOG_CAMERA, "%s: exiting", __func__ );

  return 0;
}


void*
_img132eEventHandler ( void* param )
{
  struct timeval        tv;
  QHY_STATE*            cameraInfo = param;
  int                   exitThread;

  tv.tv_sec = 1;
  tv.tv_usec = 0;
  do { 
    libusb_handle_events_timeout_completed ( cameraInfo->usbContext, &tv, 0 );
    pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
    exitThread = cameraInfo->stopCallbackThread;
    pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
  } while ( !exitThread );
  return 0;
}


static int
oaIMG132ECameraGetFramePixelFormat ( oaCamera* camera )
{
  return OA_PIX_FMT_BGGR8;
}


static int
oaIMG132ECameraTestControl ( oaCamera* camera, int control,
    oaControlValue* valp )
{
  int32_t	val_s32;
  int64_t	val_s64;
  COMMON_INFO*	commonInfo = camera->_common;

  oaLogInfo ( OA_LOG_CAMERA, "%s ( %p, %d, %p ): entered", __func__, camera,
			control, valp );

  if ( !camera->OA_CAM_CTRL_TYPE( control )) {
    return -OA_ERR_INVALID_CONTROL;
  }

  if ( camera->OA_CAM_CTRL_TYPE( control ) != valp->valueType ) {
    return -OA_ERR_INVALID_CONTROL_TYPE;
  }

  switch ( control ) {

    case OA_CAM_CTRL_GAIN:
    case OA_CAM_CTRL_RED_BALANCE:
    case OA_CAM_CTRL_BLUE_BALANCE:
    case OA_CAM_CTRL_GREEN_BALANCE:
    case OA_CAM_CTRL_BINNING:
      val_s32 = valp->int32;
      if ( val_s32 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_s32 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_s32 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
      val_s64 = valp->int64;
      if ( val_s64 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_s64 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_s64 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;
  }

  oaLogInfo ( OA_LOG_CAMERA, "%s: exiting", __func__ );

  return -OA_ERR_OUT_OF_RANGE;
}


static void
_IMG132EInitFunctionPointers ( oaCamera* camera )
{
  // Set by QHYinit()
  // camera->funcs.initCamera = oaQHYInitCamera;
  camera->funcs.closeCamera = oaIMG132ECloseCamera;

  camera->funcs.testControl = oaIMG132ECameraTestControl;
  camera->funcs.getControlRange = oaQHYCameraGetControlRange;

  camera->funcs.hasAuto = oacamHasAuto;

  camera->funcs.enumerateFrameSizes = oaIMG132ECameraGetFrameSizes;
  camera->funcs.getFramePixelFormat = oaIMG132ECameraGetFramePixelFormat;
}
