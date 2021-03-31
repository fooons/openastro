/*****************************************************************************
 *
 * ZWASI2control.c -- control functions for ZW ASI cameras, APIv2
 *
 * Copyright 2015,2017,2018,2021
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

#include <ASICamera2.h>
#include <pthread.h>

#include "oacamprivate.h"
#include "ZWASI.h"
#include "ZWASIoacam.h"
#include "ZWASI2oacam.h"
#include "ZWASIstate.h"


int
oaZWASI2CameraTestControl ( oaCamera* camera, int control,
    oaControlValue* val )
{
  int32_t	val_s32;
  ZWASI_STATE*	cameraInfo = camera->_private;
  COMMON_INFO*	commonInfo = camera->_common;

  oacamDebugMsg ( DEBUG_CAM_CTRL, "%s: ( %d, ? )\n", __func__, control );

  if ( !camera->OA_CAM_CTRL_TYPE( control )) {
    return -OA_ERR_INVALID_CONTROL;
  }

  if ( camera->OA_CAM_CTRL_TYPE( control ) != val->valueType ) {
    return -OA_ERR_INVALID_CONTROL_TYPE;
  }

  switch ( control ) {
    case OA_CAM_CTRL_BRIGHTNESS:
    case OA_CAM_CTRL_BLUE_BALANCE:
    case OA_CAM_CTRL_RED_BALANCE:
    case OA_CAM_CTRL_GAMMA:
    case OA_CAM_CTRL_GAIN:
    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
    case OA_CAM_CTRL_USBTRAFFIC:
    case OA_CAM_CTRL_OVERCLOCK:
      val_s32 = val->int32;
      if ( val_s32 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_s32 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_s32 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;

    case OA_CAM_CTRL_BINNING:
    {
      int i = 0;

      val_s32 = val->discrete;
      while ( cameraInfo->binModes[i] && i < 16 ) {
        if ( val_s32 == cameraInfo->binModes[i] ) {
          return OA_ERR_NONE;
        }
        i++;
      }
      break;
    }

    // This lot are all boolean, so we'll take any value
    case OA_CAM_CTRL_HIGHSPEED:
    case OA_CAM_CTRL_HFLIP:
    case OA_CAM_CTRL_VFLIP:
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_GAIN ):
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_GAMMA ):
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_BRIGHTNESS ):
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ):
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_RED_BALANCE ):
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_BLUE_BALANCE ):
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_USBTRAFFIC ):
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_OVERCLOCK ):
      return OA_ERR_NONE;
      break;

    default:
      // If we reach here it's because we don't recognise the control
      fprintf ( stderr, "Unrecognised control %d in oaZWASICameraTestControl\n",
          control );
      return -OA_ERR_INVALID_CONTROL;
      break;
  }
  // And if we reach here it's because the value wasn't valid
  return -OA_ERR_OUT_OF_RANGE;
}
