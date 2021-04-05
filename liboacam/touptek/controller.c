/*****************************************************************************
 *
 * controller.c -- Main camera controller thread
 *
 * Copyright 2019,2020,2021
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

#include <pthread.h>
#include <sys/time.h>

#include <openastro/camera.h>

#include "touptek-conf.h"
#include "oacamprivate.h"
#include "unimplemented.h"
#include "touptekprivate.h"
#include "touptekoacam.h"
#include "touptekstate.h"


static int	_processSetControl ( oaCamera*, OA_COMMAND* );
static int	_processGetControl ( TOUPTEK_STATE*, OA_COMMAND* );
static int	_processSetResolution ( TOUPTEK_STATE*, OA_COMMAND* );
static int	_processSetROI ( oaCamera*, OA_COMMAND* );
static int	_processStreamingStart ( TOUPTEK_STATE*, OA_COMMAND* );
static int	_processStreamingStop ( TOUPTEK_STATE*, OA_COMMAND* );
static int	_processExposureStart ( TOUPTEK_STATE*, OA_COMMAND* );
static int	_processAbortExposure ( TOUPTEK_STATE* );
static int	_doStart ( TOUPTEK_STATE* );
static int	_doStop ( TOUPTEK_STATE* );
static int	_setBinning ( TOUPTEK_STATE*, int );
static int	_setFrameFormat ( TOUPTEK_STATE*, int );
static void	TT_FUNC( _, PullCallbackV1 )( unsigned int, void* );
static void	TT_FUNC( _, PullCallbackV2 )( unsigned int, void* );
static void	_completeCallback ( TOUPTEK_STATE*, const void*, int, int,
									unsigned int );
static void	_timerCallback ( void* );
/*
static int	_setColourMode ( TOUPTEK_STATE*, int );
static int	_setBitDepth ( TOUPTEK_STATE*, int );
*/

/*
 * Current possible options (as of v39.15529):
 *
 * TOUPCAM_OPTION_NOFRAME_TIMEOUT			ignored
 * TOUPCAM_OPTION_THREAD_PRIORITY			ignored
 * TOUPCAM_OPTION_PROCESSMODE					ignored
 * TOUPCAM_OPTION_RAW									implemented
 * TOUPCAM_OPTION_HISTOGRAM						ignored
 * TOUPCAM_OPTION_BITDEPTH						implemented
 * TOUPCAM_OPTION_FAN									ignored
 * TOUPCAM_OPTION_TEC									implemented
 * TOUPCAM_OPTION_LINEAR							should be set to 0 in connect()
 * TOUPCAM_OPTION_CURVE								should be set to 0 in connect()
 * TOUPCAM_OPTION_TRIGGER							implemented, partially
 * TOUPCAM_OPTION_RGB									implemented
 * TOUPCAM_OPTION_COLORMATIX					should be set to 0 in connect()
 * TOUPCAM_OPTION_WBGAIN							implemented
 * TOUPCAM_OPTION_TECTARGET						implemented
 * TOUPCAM_OPTION_AUTOEXP_POLICY
 * TOUPCAM_OPTION_FRAMERATE						ignored (perhaps required for RPi3?)
 * TOUPCAM_OPTION_DEMOSAIC						should be set to 0 in connect()
 * TOUPCAM_OPTION_DEMOSAIC_VIDEO			should be set to 0 in connect()
 * TOUPCAM_OPTION_DEMOSAIC_STILL			should be set to 0 in connect()
 * TOUPCAM_OPTION_BLACKLEVEL					implemented
 * TOUPCAM_OPTION_MULTITHREAD					ignored
 * TOUPCAM_OPTION_BINNING							implemented, partially
 * TOUPCAM_OPTION_ROTATE							ignored
 * TOUPCAM_OPTION_CG									implemented
 * TOUPCAM_OPTION_PIXEL_FORMAT
 * TOUPCAM_OPTION_FFC									ignored
 * TOUPCAM_OPTION_DDR_DEPTH						should be set to 0 in connect()
 * TOUPCAM_OPTION_DFC									ignored
 * TOUPCAM_OPTION_SHARPENING
 * TOUPCAM_OPTION_FACTORY
 * TOUPCAM_OPTION_TEC_VOLTAGE					ignored
 * TOUPCAM_OPTION_TEC_VOLTAGE_MAX			ignored
 * TOUPCAM_OPTION_DEVICE_RESET				ignored
 * TOUPCAM_OPTION_UPSIDE_DOWN	
 * TOUPCAM_OPTION_AFPOSITION					ignored
 * TOUPCAM_OPTION_AFMODE							ignored
 * TOUPCAM_OPTION_AFZONE							ignored
 * TOUPCAM_OPTION_AFFEEDBACK					ignored
 * TOUPCAM_OPTION_TESTPATTERN					ignored
 * TOUPCAM_OPTION_AUTOEXP_THRESHOLD
 * TOUPCAM_OPTION_BYTEORDER						should be set to 0 in connect()
 */


void*
TT_FUNC( oacam, controller )( void* param )
{
  oaCamera*		camera = param;
  TOUPTEK_STATE*	cameraInfo = camera->_private;
  OA_COMMAND*		command;
  int			exitThread = 0;
  int			resultCode;
  int			streaming = 0;

  do {
    pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
    exitThread = cameraInfo->stopControllerThread;
    pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
    if ( exitThread ) {
      break;
    } else {
      pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
      // stop us busy-waiting
      streaming = ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ? 1 : 0 );
      if ( !streaming && oaDLListIsEmpty ( cameraInfo->commandQueue )) {
        pthread_cond_wait ( &cameraInfo->commandQueued,
            &cameraInfo->commandQueueMutex );
      }
      pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
    }
    do {
      command = oaDLListRemoveFromHead ( cameraInfo->commandQueue );
      if ( command ) {
        switch ( command->commandType ) {
          case OA_CMD_CONTROL_SET:
            resultCode = _processSetControl ( camera, command );
            break;
          case OA_CMD_CONTROL_GET:
            resultCode = _processGetControl ( cameraInfo, command );
            break;
          case OA_CMD_RESOLUTION_SET:
            resultCode = _processSetResolution ( cameraInfo, command );
            break;
          case OA_CMD_ROI_SET:
            resultCode = _processSetROI ( camera, command );
            break;
          case OA_CMD_START_STREAMING:
            resultCode = _processStreamingStart ( cameraInfo, command );
            break;
          case OA_CMD_STOP_STREAMING:
            resultCode = _processStreamingStop ( cameraInfo, command );
            break;
					case OA_CMD_START_EXPOSURE:
						resultCode = _processExposureStart ( cameraInfo, command );
						break;
					case OA_CMD_ABORT_EXPOSURE:
						resultCode = _processAbortExposure ( cameraInfo );
						break;
          default:
            oaLogError ( OA_LOG_CAMERA, "%s: Invalid command type %d",
                command->commandType );
            resultCode = -OA_ERR_INVALID_CONTROL;
            break;
        }
        if ( command->callback ) {
        } else {
          pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
          command->completed = 1;
          command->resultCode = resultCode;
          pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
          pthread_cond_broadcast ( &cameraInfo->commandComplete );
        }
      }
    } while ( command );
  } while ( !exitThread );

  return 0;
}


void
TT_FUNC( _, FrameCallbackV1 )( const void *frame, const BITMAPINFOHEADER*
    bitmapHeader, int bSnap, void *ptr )
{
  TOUPTEK_STATE*	cameraInfo = ptr;
  int			buffersFree, bitsPerPixel;
  unsigned int		dataLength;

  pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
  buffersFree = cameraInfo->buffersFree;
  bitsPerPixel = cameraInfo->currentBitsPerPixel;
  pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );

  if ( frame && buffersFree && bitmapHeader->biSizeImage ) {
    if (( dataLength = bitmapHeader->biSizeImage ) >
        cameraInfo->imageBufferLength ) {
      dataLength = cameraInfo->imageBufferLength;
    }
		_completeCallback ( cameraInfo, frame, bitsPerPixel,
				cameraInfo->nextBuffer, dataLength );
  }
}


void
TT_FUNC( _, FrameCallbackV2 )( const void *frame,
		const TT_VAR_TYPE( FrameInfoV2* ) frameInfo, int bSnap, void *ptr )
{
  TOUPTEK_STATE*	cameraInfo = ptr;
  int			buffersFree, bitsPerPixel;

  pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
  buffersFree = cameraInfo->buffersFree;
  bitsPerPixel = cameraInfo->currentBitsPerPixel;
  pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );

  if ( frame && buffersFree ) {
		_completeCallback ( cameraInfo, frame, bitsPerPixel,
				cameraInfo->nextBuffer, cameraInfo->imageBufferLength );
  }
}


static int
_processSetControl ( oaCamera* camera, OA_COMMAND* command )
{
  TOUPTEK_STATE*	cameraInfo = camera->_private;
	COMMON_INFO*		commonInfo = camera->_common;
  oaControlValue*	valp = command->commandData;
  int							control = command->controlId, val = 0;

  switch ( control ) {

    case OA_CAM_CTRL_BRIGHTNESS:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= TT_DEFINE( BRIGHTNESS_MIN ) &&
					val <= TT_DEFINE( BRIGHTNESS_MAX )) {
        if ((( TT_LIB_PTR( put_Brightness ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Brightness ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_CONTRAST:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= TT_DEFINE( CONTRAST_MIN ) &&
					val <= TT_DEFINE( CONTRAST_MAX )) {
        if ((( TT_LIB_PTR( put_Contrast ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Contrast ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_GAMMA:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= TT_DEFINE( GAMMA_MIN ) &&
					val <= TT_DEFINE( GAMMA_MAX )) {
        if ((( TT_LIB_PTR( put_Gamma ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Gamma ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_HFLIP:
      if ( OA_CTRL_TYPE_BOOLEAN != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where boolean expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->boolean ? 1 : 0;
      if ((( TT_LIB_PTR( put_HFlip ))( cameraInfo->handle, val )) < 0 ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_put_HFlip ( %d ) failed", __func__, val );
        return -OA_ERR_CAMERA_IO;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_VFLIP:
      if ( OA_CTRL_TYPE_BOOLEAN != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where boolean expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->boolean ? 1 : 0;
#ifndef NO_UPSIDE_DOWN
			if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
					TT_OPTION( UPSIDE_DOWN ), val )) < 0 ) {
				// Perhaps it isn't available for this camera, so try VFlip instead
#endif
				if ((( TT_LIB_PTR( put_VFlip ))( cameraInfo->handle, val )) < 0 ) {
					oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Option ( UPSIDE_DOWN, %d ) and ",
							TT_DRIVER "_put_HFlip ( %d ) failed", __func__, val, val );
					return -OA_ERR_CAMERA_IO;
				}
#ifndef NO_UPSIDE_DOWN
      }
#endif
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ):
      if ( OA_CTRL_TYPE_BOOLEAN != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where boolean expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      if ((( TT_LIB_PTR( put_AutoExpoEnable ))( cameraInfo->handle,
						valp->boolean )) < 0) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_put_AutoExpoEnable ( %d ) failed", __func__,
						valp->boolean );
        return -OA_ERR_CAMERA_IO;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= cameraInfo->exposureMin && val <= cameraInfo->exposureMax ) {
        if ((( TT_LIB_PTR( put_ExpoTime ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_ExpoTime ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
			cameraInfo->exposureTime = val;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_GAIN:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= cameraInfo->gainMin && val <= cameraInfo->gainMax ) {
        if ((( TT_LIB_PTR( put_ExpoAGain ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_ExpoAGain ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_SPEED:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= 0 && val <= cameraInfo->speedMax ) {
        if ((( TT_LIB_PTR( put_Speed ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Speed ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_HUE:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= TT_DEFINE( HUE_MIN ) && val <= TT_DEFINE( HUE_MAX )) {
        if ((( TT_LIB_PTR( put_Hue ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Hue ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_SATURATION:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= TT_DEFINE( SATURATION_MIN ) &&
					val <= TT_DEFINE( SATURATION_MAX )) {
        if ((( TT_LIB_PTR( put_Saturation ))( cameraInfo->handle, val )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Saturation ( %d ) failed", __func__, val );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_RED_BALANCE:
    case OA_CAM_CTRL_BLUE_BALANCE:
    case OA_CAM_CTRL_GREEN_BALANCE:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= TT_DEFINE( WBGAIN_MIN ) && val <= TT_DEFINE( WBGAIN_MAX )) {
        int gain[3];
        if ((( TT_LIB_PTR( get_WhiteBalanceGain ))( cameraInfo->handle,
            gain )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_get_WhiteBalanceGain ( gain[3] ) failed",
							__func__ );
          return -OA_ERR_CAMERA_IO;
        }
        switch ( control ) {
          case OA_CAM_CTRL_RED_BALANCE:
            gain[0] = val;
            break;
          case OA_CAM_CTRL_BLUE_BALANCE:
            gain[2] = val;
            break;
          case OA_CAM_CTRL_GREEN_BALANCE:
            gain[1] = val;
            break;
        }
        if ((( TT_LIB_PTR( put_WhiteBalanceGain ))( cameraInfo->handle,
            gain )) < 0 ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_WhiteBalanceGain ( array ( %d, %d, %d )) "
							"failed", __func__, gain[0], gain[1], gain[2] );
          return -OA_ERR_CAMERA_IO;
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

		case OA_CAM_CTRL_TEMP_SETPOINT:
      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= TT_DEFINE( TEC_TARGET_MIN ) &&
					val <= TT_DEFINE( TEC_TARGET_MIN )) {
				// Weird one this, as there appears to be more than one way to do
				// it.  If we have a TEC then set that option, otherwise set the
				// target temperature.
				if ( cameraInfo->haveTEC ) {
					if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
							TT_OPTION( TECTARGET ), val )) < 0 ) {
						oaLogError ( OA_LOG_CAMERA,
								"%s: " TT_DRIVER "_put_Option ( TECTARGET, %d ) failed",
								__func__, val );
						return -OA_ERR_CAMERA_IO;
					}
				} else {
					if ((( TT_LIB_PTR( put_Temperature ))( cameraInfo->handle, val ))
							< 0 ) {
						oaLogError ( OA_LOG_CAMERA,
								"%s: " TT_DRIVER "_put_Temperature ( %d ) failed", __func__,
								val );
						return -OA_ERR_CAMERA_IO;
					}
        }
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

		case OA_CAM_CTRL_BLACKLEVEL:
		{
			int			bitspp;

      if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
			// FIX ME -- needs more work, because the black level varies depending
			// on the bit depth
		  bitspp = oaFrameFormats[ cameraInfo->currentVideoFormat ].bitsPerPixel;
			bitspp -= 8;
			if ( bitspp > 0 ) {
				int multiplier = 1 << ( bitspp );
				val *= multiplier;
			}
      if ( val >= TT_DEFINE( BLACKLEVEL_MIN ) &&
					val <= TT_DEFINE ( BLACKLEVEL8_MAX )) {
				if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
						TT_OPTION( BLACKLEVEL ), val )) < 0 ) {
					oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Option ( BLACKLEVEL, %d ) failed",
							__func__, val );
					return -OA_ERR_CAMERA_IO;
				}
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;
		}
		case OA_CAM_CTRL_CONVERSION_GAIN:
      if ( OA_CTRL_TYPE_INT32 != valp->int32 ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int32 expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->int32;
      if ( val >= 0 && val <=
					commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_CONVERSION_GAIN )) {
				if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
						TT_OPTION( CG ), val )) < 0 ) {
					oaLogError ( OA_LOG_CAMERA,
							"%s: " TT_DRIVER "_put_Option ( CG, %d ) failed",
							__func__, val );
					return -OA_ERR_CAMERA_IO;
				}
      } else {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_BINNING:
      if ( OA_CTRL_TYPE_DISCRETE != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where discrete value expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->discrete;
      return _setBinning ( cameraInfo, val );
      break;

    case OA_CAM_CTRL_COOLER:
      if ( OA_CTRL_TYPE_BOOLEAN != valp->valueType ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where boolean expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->boolean ? 0 : 1;
      if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
          TT_OPTION( TEC ), 1 )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_put_Option ( TEC, %d ) failed", __func__, val );
        return -OA_ERR_CAMERA_IO;
      }
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_FAN:
      if ( OA_CTRL_TYPE_BOOLEAN == valp->valueType ) {
				val = valp->boolean ? 0 : 1;
			} else {
				if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
					oaLogError ( OA_LOG_CAMERA,
							"%s: invalid control type %d where boolean or int32 expected "
					   "for control %d", __func__, valp->valueType, control );
					return -OA_ERR_INVALID_CONTROL_TYPE;
				}
				val = valp->int32;
			}
			if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
					TT_OPTION( FAN ), val )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_put_Option ( FAN, %d ) failed", __func__, val );
				return -OA_ERR_CAMERA_IO;
			}
			return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_FRAME_FORMAT:
      if ( valp->valueType != OA_CTRL_TYPE_DISCRETE ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where discrete value expected "
            "for control %d", __func__, valp->valueType, control );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      val = valp->discrete;
      if ( !camera->frameFormats[ val ] ) {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return _setFrameFormat ( cameraInfo, val );
      break;

    case OA_CAM_CTRL_LED_STATE:
    case OA_CAM_CTRL_LED_PERIOD:
      if ( control == OA_CAM_CTRL_LED_STATE ) {
        if ( OA_CTRL_TYPE_DISC_MENU != valp->valueType ) {
					oaLogError ( OA_LOG_CAMERA,
							"%s: invalid control type %d where menu value expected "
							"for control %d", __func__, valp->valueType, control );
          return -OA_ERR_INVALID_CONTROL_TYPE;
        }
        cameraInfo->ledState = valp->menu;
      } else {
        if ( OA_CTRL_TYPE_INT32 != valp->valueType ) {
					oaLogError ( OA_LOG_CAMERA,
							"%s: invalid control type %d where int32 expected "
							"for control %d", __func__, valp->valueType, control );
          return -OA_ERR_INVALID_CONTROL_TYPE;
        }
        cameraInfo->ledPeriod = valp->int32;
      }
      if ((( TT_LIB_PTR( put_LEDState ))( cameraInfo->handle, 0,
          cameraInfo->ledState, cameraInfo->ledPeriod )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_put_LEDState ( 0, %d, %d ) failed", __func__,
						cameraInfo->ledState, cameraInfo->ledPeriod );
        return -OA_ERR_CAMERA_IO;
      }
      return OA_ERR_NONE;
      break;
  }

  oaLogError ( OA_LOG_CAMERA, "%s: Unrecognised control %d", __func__,
			control );
  return -OA_ERR_INVALID_CONTROL;
}


static int
_processGetControl ( TOUPTEK_STATE* cameraInfo, OA_COMMAND* command )
{
  oaControlValue	*valp = command->resultData;
  int			control = command->controlId;
  int32_t		val_s32;
  uint32_t		val_u32;
  unsigned short	val_u16;
  short			val_s16;

  switch ( control ) {

    case OA_CAM_CTRL_BRIGHTNESS:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_Brightness ))( cameraInfo->handle,
          &val_s32 )) < 0 ) {
        oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_Brightness failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_s32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_CONTRAST:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_Contrast ))( cameraInfo->handle,
					&val_s32 )) < 0 ) {
        oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_Contrast failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_s32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_GAMMA:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_Gamma ))( cameraInfo->handle, &val_s32 )) < 0 ) {
        oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_Gamma failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_s32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_HFLIP:
      valp->valueType = OA_CTRL_TYPE_BOOLEAN;
      if ((( TT_LIB_PTR( get_HFlip ))( cameraInfo->handle, &val_s32 )) < 0 ) {
        oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_HFlip failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->boolean = val_s32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_VFLIP:
      valp->valueType = OA_CTRL_TYPE_BOOLEAN;
#ifndef NO_UPSIDE_DOWN
			if ((( TT_LIB_PTR( get_Option ))( cameraInfo->handle,
					TT_OPTION( UPSIDE_DOWN ), &val_u32 )) < 0 ) {
#endif
				if ((( TT_LIB_PTR( get_VFlip ))( cameraInfo->handle, &val_s32 )) < 0 ) {
					oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_VFlip failed",
							__func__ );
					return -OA_ERR_CAMERA_IO;
				}
				val_u32 = val_s32 ? 1 : 0;
#ifndef NO_UPSIDE_DOWN
			}
#endif
      valp->boolean = val_u32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ):
      valp->valueType = OA_CTRL_TYPE_BOOLEAN;
      if ((( TT_LIB_PTR( get_AutoExpoEnable ))( cameraInfo->handle,
          &val_s32 )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_get_AutoExpoEnable failed", __func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->boolean = val_s32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_ExpoTime ))( cameraInfo->handle,
					&val_u32 )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_ExpoTime failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_u32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_GAIN:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_ExpoAGain ))( cameraInfo->handle,
          &val_u16 )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_ExpoAGain failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_u16;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_SPEED:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_Speed ))( cameraInfo->handle, &val_u16 )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_Speed failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_u16;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_HUE:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_Hue ))( cameraInfo->handle, &val_s32 )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_Hue failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_s32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_SATURATION:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_Saturation ))( cameraInfo->handle,
          &val_s32 )) < 0 ){
				oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_Saturation failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_s32;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_RED_BALANCE:
    case OA_CAM_CTRL_BLUE_BALANCE:
    case OA_CAM_CTRL_GREEN_BALANCE:
    {
      int gain[3];
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_WhiteBalanceGain ))( cameraInfo->handle,
          gain )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_get_WhiteBalanceGain (gain[3]) failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      switch ( control ) {
        case OA_CAM_CTRL_RED_BALANCE:
          val_s32 = gain[0];
          break;
        case OA_CAM_CTRL_BLUE_BALANCE:
          val_s32 = gain[2];
          break;
        case OA_CAM_CTRL_GREEN_BALANCE:
          val_s32 = gain[1];
          break;
      }
      valp->int32 = val_s32;
      return OA_ERR_NONE;
      break;
    }

    case OA_CAM_CTRL_TEMPERATURE:
      valp->valueType = OA_CTRL_TYPE_INT32;
      if ((( TT_LIB_PTR( get_Temperature ))( cameraInfo->handle,
          &val_s16 )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER "_get_Temperature failed",
						__func__ );
        return -OA_ERR_CAMERA_IO;
      }
      valp->int32 = val_s16;
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_BINNING:
      // FIX ME
			oaLogWarning ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "Need to code binning control", __func__ );
      return -OA_ERR_INVALID_CONTROL;
      break;

    case OA_CAM_CTRL_COOLER:
    case OA_CAM_CTRL_FAN:
			oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "Unimplemented control control %d", __func__,
					control );
      return -OA_ERR_INVALID_CONTROL;
      break;

		case OA_CAM_CTRL_BLACKLEVEL:
		{
			int						bitspp;
			unsigned int	val;

      valp->valueType = OA_CTRL_TYPE_INT32;
			if ((( TT_LIB_PTR( get_Option ))( cameraInfo->handle,
					TT_OPTION( BLACKLEVEL ), &val )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_get_Option ( BLACKLEVEL ) failed", __func__ );
				return -OA_ERR_CAMERA_IO;
			}
			// FIX ME -- needs more work, because the black level varies depending
			// on the bit depth
		  bitspp = oaFrameFormats[ cameraInfo->currentVideoFormat ].bitsPerPixel;
			bitspp -= 8;
			if ( bitspp > 0 ) {
				int divisor = 1 << ( bitspp );
				val /= divisor;
			}
      valp->int32 = val;
      return OA_ERR_NONE;
      break;
		}

		case OA_CAM_CTRL_CONVERSION_GAIN:
		{
			unsigned int		val;

      valp->valueType = OA_CTRL_TYPE_INT32;
			if ((( TT_LIB_PTR( get_Option ))( cameraInfo->handle,
					TT_OPTION( CG ), &val )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_get_Option ( CG ) failed", __func__ );
				return -OA_ERR_CAMERA_IO;
			}
      valp->int32 = val;
      return OA_ERR_NONE;
      break;
		}
  }

  oaLogError ( OA_LOG_CAMERA, "%s: Unrecognised control %d", __func__,
			control );

  return -OA_ERR_INVALID_CONTROL;
}


static int
_processSetResolution ( TOUPTEK_STATE* cameraInfo, OA_COMMAND* command )
{
  FRAMESIZE*			size = command->commandData;
  unsigned int			s, restart = 0;
  int				found;

  found = -1;
  for ( s = 0; s < cameraInfo->frameSizes[ cameraInfo->binMode ].numSizes;
      s++ ) {
    if ( cameraInfo->frameSizes[ cameraInfo->binMode ].sizes[ s ].x == size->x
        && cameraInfo->frameSizes[ cameraInfo->binMode ].sizes[ s ].y ==
        size->y ) {
      found = s;
      break;
    }
  }

  if ( found < 0 ) {
    return -OA_ERR_OUT_OF_RANGE;
  }

  if ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) {
    restart = 1;
    _doStop ( cameraInfo );
  }

  // Reset the ROI

  if ((( TT_LIB_PTR( put_Roi ))( cameraInfo->handle, 0, 0, 0, 0 )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't clear " TT_DRIVER " ROI", __func__ );
    return -OA_ERR_CAMERA_IO;
  }

  if ((( TT_LIB_PTR( put_Size ))( cameraInfo->handle, size->x,
			size->y )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: Can't set " TT_DRIVER " frame size %dx%d", __func__, size->x,
				size->y );
    return -OA_ERR_CAMERA_IO;
  }

  cameraInfo->xSize = cameraInfo->currentXResolution = size->x;
  cameraInfo->ySize = cameraInfo->currentYResolution = size->y;
  cameraInfo->imageBufferLength = cameraInfo->xSize *
      cameraInfo->ySize * cameraInfo->currentBytesPerPixel;

  if ( restart ) {
    _doStart ( cameraInfo );
  }

  return OA_ERR_NONE;
}


static int
_processSetROI ( oaCamera* camera, OA_COMMAND* command )
{
  TOUPTEK_STATE*		cameraInfo = camera->_private;
  FRAMESIZE*			size = command->commandData;
  unsigned int			offsetX, offsetY, x, y;

  if (!( camera->features.flags & OA_CAM_FEATURE_ROI )) {
    return -OA_ERR_INVALID_CONTROL;
  }

  x = size->x;
  y = size->y;

  if ( x < 16 || x > cameraInfo->currentXResolution || ( x % 2 ) || y < 16 ||
     y > cameraInfo->currentYResolution || ( y % 2 )) {
    return -OA_ERR_OUT_OF_RANGE;
  }

  // ROI position must be even co-ordinate values

  offsetX = (( cameraInfo->currentXResolution - x ) / 2 ) & ~1;
  offsetY = (( cameraInfo->currentYResolution - y ) / 2 ) & ~1;

  if ((( TT_LIB_PTR( put_Roi ))( cameraInfo->handle, offsetX, offsetY, x,
      y )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: Can't set " TT_DRIVER " ROI ( %d, %d, %d, %d )", __func__,
        offsetX, offsetY, x, y );
    return -OA_ERR_CAMERA_IO;
  }

  cameraInfo->xSize = x;
  cameraInfo->ySize = y;
  cameraInfo->imageBufferLength = cameraInfo->xSize *
      cameraInfo->ySize * cameraInfo->currentBytesPerPixel;

  return OA_ERR_NONE;
}


static int
_processStreamingStart ( TOUPTEK_STATE* cameraInfo, OA_COMMAND* command )
{
  CALLBACK*		cb = command->commandData;

  if ( cameraInfo->runMode != CAM_RUN_MODE_STOPPED ) {
    return -OA_ERR_INVALID_COMMAND;
  }

  cameraInfo->streamingCallback.callback = cb->callback;
  cameraInfo->streamingCallback.callbackArg = cb->callbackArg;

  cameraInfo->imageBufferLength = cameraInfo->xSize *
      cameraInfo->ySize * cameraInfo->currentBytesPerPixel;

  return _doStart ( cameraInfo );
}


static int
_doStart ( TOUPTEK_STATE* cameraInfo )
{
  int			ret;

	if ( cameraInfo->libMajorVersion > 28 ) {
		if (( ret = ( TT_LIB_PTR( StartPushModeV2 ))( cameraInfo->handle,
				TT_FUNC( _, FrameCallbackV2 ), cameraInfo )) < 0 ) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "_StartPushModeV2 failed returning 0x%x",
					__func__, ret );
			return -OA_ERR_CAMERA_IO;
		}
  } else {
		if (( ret = ( TT_LIB_PTR( StartPushMode ))( cameraInfo->handle,
				TT_FUNC( _, FrameCallbackV1 ), cameraInfo )) < 0 ) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "_StartPushMode failed returning 0x%x",
					__func__, ret );
			return -OA_ERR_CAMERA_IO;
		}
	}

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->runMode = CAM_RUN_MODE_STREAMING;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
  return OA_ERR_NONE;
}


static int
_processStreamingStop ( TOUPTEK_STATE* cameraInfo, OA_COMMAND* command )
{
  if ( cameraInfo->runMode != CAM_RUN_MODE_STREAMING ) {
    return -OA_ERR_INVALID_COMMAND;
  }

  return _doStop ( cameraInfo );
}


static int
_doStop ( TOUPTEK_STATE* cameraInfo )
{
  int		ret;

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->runMode = CAM_RUN_MODE_STOPPED;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

  if (( ret = ( TT_LIB_PTR( Stop ))( cameraInfo->handle )) < 0 ) {
		oaLogError ( OA_LOG_CAMERA,
				"%s: " TT_DRIVER "_Stop failed, returning %d", __func__, ret );
    return -OA_ERR_CAMERA_IO;
  }
  return OA_ERR_NONE;
}


static int
_setBinning ( TOUPTEK_STATE* cameraInfo, int binMode )
{
  int		restart = 0, x, y;

  if ( binMode < 0 || binMode > OA_MAX_BINNING ||
      cameraInfo->frameSizes[ binMode ].numSizes < 1 ) {
    return -OA_ERR_OUT_OF_RANGE;
  }

  // Reset the ROI

  if ((( TT_LIB_PTR( put_Roi ))( cameraInfo->handle, 0, 0, 0, 0 )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't clear " TT_DRIVER " ROI", __func__ );
    return -OA_ERR_CAMERA_IO;
  }

  if ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) {
    restart = 1;
    _doStop ( cameraInfo );
  }

  x = cameraInfo->frameSizes[ binMode ].sizes[0].x;
  y = cameraInfo->frameSizes[ binMode ].sizes[0].y;
  if ((( TT_LIB_PTR( put_Size ))( cameraInfo->handle, x, y )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't set " TT_DRIVER " frame size",
				__func__ );
    return -OA_ERR_CAMERA_IO;
  }

  cameraInfo->binMode = binMode;
  cameraInfo->xSize = cameraInfo->currentXResolution = x;
  cameraInfo->ySize = cameraInfo->currentYResolution = y;

  if ( restart ) {
    _doStart ( cameraInfo );
  }

  return OA_ERR_NONE;
}


static int
_setFrameFormat ( TOUPTEK_STATE* cameraInfo, int format )
{
  int           restart = 0;
  int           raw = 0, bitspp;

  // Only need to do this if we're dealing with a colour camera

  if ( !oaFrameFormats[ format ].monochrome ) {

    // FIX ME -- could make this more effcient by doing nothing here unless
    // we need to change it

    if ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) {
      restart = 1;
      _doStop ( cameraInfo );
    }

    raw = oaFrameFormats[ format ].rawColour ? 1 : 0;
    if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle, TT_OPTION( RAW ),
        raw  )) < 0 ) {
      oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "_put_Option ( RAW, %d ) failed", __func__, raw );
      return -OA_ERR_CAMERA_IO;
    }

    if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle, TT_OPTION( RGB ),
        format == OA_PIX_FMT_RGB48LE ? 1 : 0 )) < 0 ) {
      oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "_put_Option ( RAW, %d ) failed", __func__, raw );
      return -OA_ERR_CAMERA_IO;
    }
    if ( restart ) {
      _doStart ( cameraInfo );
    }
  }

  // FIX ME -- don't do this if we don't need to
  // And now change the bit depth

  bitspp = oaFrameFormats[ format ].bitsPerPixel;
  if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
			TT_OPTION( BITDEPTH ), ( bitspp > 8 ) ? 1 : 0  )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: " TT_DRIVER "_put_Option ( BITDEPTH, %d ) failed", __func__,
        bitspp > 8 ? 1 : 0 );
    return -OA_ERR_CAMERA_IO;
  }

  cameraInfo->currentVideoFormat = format;
  cameraInfo->currentBitsPerPixel = bitspp;
  // This converts from float, but should be ok for these cameras
  cameraInfo->currentBytesPerPixel = oaFrameFormats[ format ].bytesPerPixel;
  cameraInfo->imageBufferLength = cameraInfo->xSize *
      cameraInfo->ySize * cameraInfo->currentBytesPerPixel;

  return OA_ERR_NONE;
}


static int
_processExposureStart ( TOUPTEK_STATE* cameraInfo, OA_COMMAND* command )
{
  CALLBACK*		cb = command->commandData;
	int					ret;

  if ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) {
    return -OA_ERR_INVALID_COMMAND;
  }

	if ( cameraInfo->exposureInProgress ) {
		return OA_ERR_NONE;
	}

	if ( cameraInfo->runMode == CAM_RUN_MODE_STOPPED ) {

		if ((( TT_LIB_PTR( put_Option ))( cameraInfo->handle,
				TT_OPTION( TRIGGER ), 1 )) < 0 ) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "_put_Option ( TRIGGER, 1 ) failed", __func__ );
			return -OA_ERR_CAMERA_IO;
		}

		cameraInfo->streamingCallback.callback = cb->callback;
		cameraInfo->streamingCallback.callbackArg = cb->callbackArg;

		cameraInfo->imageBufferLength = cameraInfo->xSize *
				cameraInfo->ySize * cameraInfo->currentBytesPerPixel;

		if ( cameraInfo->libMajorVersion > 28 ) {
			if (( ret = ( TT_LIB_PTR( StartPullModeWithCallback ))(
					cameraInfo->handle, TT_FUNC( _, PullCallbackV2 ),
					cameraInfo )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA,
						"%s: " TT_DRIVER "_StartPullModeWithCallback failed, returns 0x%x",
						__func__, ret );
				return -OA_ERR_CAMERA_IO;
			}
		} else {
			if (( ret = ( TT_LIB_PTR( StartPullModeWithCallback ))(
					cameraInfo->handle, TT_FUNC( _, PullCallbackV1 ),
					cameraInfo )) < 0 ) {
				oaLogError ( OA_LOG_CAMERA, "%s: " TT_DRIVER
						"_StartPullModeWithCallback failed, returning 0x%x\n", __func__,
						ret );
				return -OA_ERR_CAMERA_IO;
			}
		}

		cameraInfo->runMode = CAM_RUN_MODE_SINGLE_SHOT;
	}

	cameraInfo->timerCallback = _timerCallback;
	oacamStartTimer ( cameraInfo->exposureTime, cameraInfo );

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->exposureInProgress = 1;
  cameraInfo->abortExposure = 0;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
  return OA_ERR_NONE;
}


static void
_completeCallback ( TOUPTEK_STATE* cameraInfo, const void* frame,
		int bitsPerPixel, int nextBuffer, unsigned int dataLength )
{
	int				shiftBits;

	// Now here's the fun...
	//
	// In 12-bit (and presumably 10- and 14-bit) mode, Touptek cameras
	// appear to return little-endian data, but right-aligned rather than
	// left-aligned as many other cameras do.  So if we have such an image we
	// try to fix it here.
	//
	// FIX ME -- I'm not sure this is the right place to be doing this.
	// Perhaps there should be a flag to tell the user whether the data is
	// left-or right-aligned and they can sort it out.

	if ( bitsPerPixel > 8 && bitsPerPixel < 16 ) {
		shiftBits = 16 - bitsPerPixel;
		if ( shiftBits ) {
			const uint16_t	*s;
			uint16_t				*t = cameraInfo->buffers[ nextBuffer ].start;
			uint16_t				v;
			unsigned int		i;

			s = frame ? frame : t;
			for ( i = 0; i < dataLength; i += 2 ) {
				v = *s++;
				v <<= shiftBits;
				*t++ = v;
			}
		}
	} else {
		if ( frame ) {
			( void ) memcpy ( cameraInfo->buffers[ nextBuffer ].start, frame,
					dataLength );
		}
	}

	cameraInfo->frameCallbacks[ nextBuffer ].callbackType =
			OA_CALLBACK_NEW_FRAME;
	cameraInfo->frameCallbacks[ nextBuffer ].callback =
			cameraInfo->streamingCallback.callback;
	cameraInfo->frameCallbacks[ nextBuffer ].callbackArg =
			cameraInfo->streamingCallback.callbackArg;
	cameraInfo->frameCallbacks[ nextBuffer ].buffer =
			cameraInfo->buffers[ nextBuffer ].start;
	cameraInfo->frameCallbacks[ nextBuffer ].bufferLen = dataLength;
	pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
	oaDLListAddToTail ( cameraInfo->callbackQueue,
			&cameraInfo->frameCallbacks[ nextBuffer ]);
	cameraInfo->buffersFree--;
	cameraInfo->nextBuffer = ( nextBuffer + 1 ) %
			cameraInfo->configuredBuffers;
	pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
	pthread_cond_broadcast ( &cameraInfo->callbackQueued );
}


static void
TT_FUNC( _, PullCallbackV1 )( unsigned int event, void* ptr )
{
	TOUPTEK_STATE*			cameraInfo = ptr;
  int										buffersFree, nextBuffer, bitsPerPixel;
	int										bytesPerPixel, ret, abort;
  unsigned int					dataLength, height, width;

  pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
  buffersFree = cameraInfo->buffersFree;
  bitsPerPixel = cameraInfo->currentBitsPerPixel;
  bytesPerPixel = cameraInfo->currentBytesPerPixel;
	abort = cameraInfo->abortExposure;
  pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );

  if ( !abort && buffersFree && event == TT_DEFINE( EVENT_IMAGE )) {
    dataLength = cameraInfo->imageBufferLength;
    nextBuffer = cameraInfo->nextBuffer;

		if (( ret = ( TT_LIB_PTR( PullImage ))( cameraInfo->handle,
				cameraInfo->buffers[ nextBuffer ].start, bytesPerPixel * 8, &height,
				&width )) < 0 ) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "_PullImage failed, returning 0x%x\n", __func__,
					ret );
			return;
		}

		cameraInfo->exposureInProgress = 0;
		_completeCallback ( cameraInfo, 0, bitsPerPixel, nextBuffer,
				dataLength );
	}
}


static void
TT_FUNC( _, PullCallbackV2 )( unsigned int event, void* ptr )
{
	TOUPTEK_STATE*			cameraInfo = ptr;
	TT_VAR_TYPE( FrameInfoV2 )	frameInfo;
  int										buffersFree, nextBuffer, bitsPerPixel;
	int										bytesPerPixel, ret, abort;
  unsigned int					dataLength;

  pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
  buffersFree = cameraInfo->buffersFree;
  bitsPerPixel = cameraInfo->currentBitsPerPixel;
  bytesPerPixel = cameraInfo->currentBytesPerPixel;
	abort = cameraInfo->abortExposure;
  pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );

  if ( !abort && buffersFree && event == TT_DEFINE( EVENT_IMAGE )) {
    dataLength = cameraInfo->imageBufferLength;
    nextBuffer = cameraInfo->nextBuffer;

		if (( ret = ( TT_LIB_PTR( PullImageV2 ))( cameraInfo->handle,
				cameraInfo->buffers[ nextBuffer ].start, bytesPerPixel * 8,
				&frameInfo )) < 0 ) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: " TT_DRIVER "_PullImageV2 failed, returning 0x%x\n",
					__func__, ret );
			return;
		}

		cameraInfo->exposureInProgress = 0;
		_completeCallback ( cameraInfo, 0, bitsPerPixel, nextBuffer,
				dataLength );
	}
}


static int
_processAbortExposure ( TOUPTEK_STATE* cameraInfo )
{
	int			ret;

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->abortExposure = 1;
  cameraInfo->exposureInProgress = 0;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

	oacamAbortTimer ( cameraInfo );

  if (( ret = ( TT_LIB_PTR( Stop ))( cameraInfo->handle )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: " TT_DRIVER "_Stop failed, returning %d\n", __func__, ret );
    return -OA_ERR_CAMERA_IO;
  }

	cameraInfo->runMode = CAM_RUN_MODE_STOPPED;
  return OA_ERR_NONE;
}


static void
_timerCallback ( void* param )
{
	TOUPTEK_STATE*		cameraInfo = param;
	int								ret;

	if (( ret = TT_LIB_PTR( Trigger )( cameraInfo->handle, 1 )) < 0 ) {
		cameraInfo->exposureInProgress = 0;
		oaLogError ( OA_LOG_CAMERA, "%s: trigger image frame failed, err %04x\n",
				__func__, ret );
	}
}
