/*****************************************************************************
 *
 * V4L2control.c -- control functions for V4L2 cameras
 *
 * Copyright 2013,2014,2015,2017,2019,2021
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

#if HAVE_LIBV4L2

#include <openastro/camera.h>
#include <openastro/util.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <sys/mman.h>
#include <libv4l2.h>
#include <pthread.h>

#include "oacamprivate.h"
#include "V4L2.h"
#include "V4L2oacam.h"
#include "V4L2state.h"
#include "V4L2ioctl.h"
#include "V4L2cameras.h"

#define	cameraState		camera->_v4l2


int
oaV4L2CameraTestControl ( oaCamera* camera, int control, oaControlValue* valp )
{
  int32_t	val_s32;
  int64_t	val_s64;
  COMMON_INFO*	commonInfo = camera->_common;

  if ( !camera->OA_CAM_CTRL_TYPE( control )) {
    return -OA_ERR_INVALID_CONTROL;
  }

  if ( camera->OA_CAM_CTRL_TYPE( control ) != valp->valueType ) {
    return -OA_ERR_INVALID_CONTROL_TYPE;
  }

  // FIX ME -- this is horribly wrong.  Should be done by control type
  // rather than by the actual control itself

  switch ( valp->valueType ) {

    case OA_CTRL_TYPE_INT32:
      val_s32 = valp->int32;
      if ( val_s32 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_s32 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_s32 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;

    case OA_CTRL_TYPE_DISCRETE:
      val_s32 = valp->discrete;
      if ( val_s32 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_s32 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_s32 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;

    case OA_CTRL_TYPE_MENU:
      val_s32 = valp->menu;
      if ( val_s32 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_s32 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_s32 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;

    case OA_CTRL_TYPE_BOOLEAN:
      return OA_ERR_NONE;
      break;

    case OA_CTRL_TYPE_INT64:
      val_s64 = valp->int64;
      if ( val_s64 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_s64 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_s64 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;

    case OA_CTRL_TYPE_BUTTON:
      return OA_ERR_NONE;
      break;

    case OA_CTRL_TYPE_STRING:
      return OA_ERR_NONE;
      break;

    default:
      fprintf ( stderr, "%s: unhandled value type %d\n", __func__,
          valp->valueType );
      return -OA_ERR_INVALID_CONTROL_TYPE;
      break;
  }

  // And if we reach here it's because the value wasn't valid
  return -OA_ERR_OUT_OF_RANGE;
}


/*
int
oaV4L2CameraReset ( oaCamera* camera )
{
  // should check here that buffers have been released etc.?

  v4l2_close ( cameraState.fd );
  cameraState.lastUsedBuffer = -1;
  if (( cameraState.fd = v4l2_open ( cameraState.devicePath,
      O_RDWR | O_NONBLOCK, 0 )) < 0 ) {
    fprintf ( stderr, "cannot open video device %s\n",
        cameraState.devicePath );
    // errno should be set?
    free (( void * ) camera );
    return -OA_ERR_CAMERA_IO;
  }
  return OA_ERR_NONE;
}

*/

const char*
oaV4L2CameraGetMenuString ( oaCamera* camera, int control, int index )
{
  OA_COMMAND    command;
  int           retval;
  V4L2_STATE*   cameraInfo = camera->_private;

  oacamDebugMsg ( DEBUG_CAM_CTRL, "V4L2: control: %s()\n", __func__ );

  OA_CLEAR ( command );
  command.commandType = OA_CMD_MENU_ITEM_GET;
  command.controlId = control;
  command.commandData = &index;

  oaDLListAddToTail ( cameraInfo->commandQueue, &command );
  pthread_cond_broadcast ( &cameraInfo->commandQueued );
  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  while ( !command.completed ) {
    pthread_cond_wait ( &cameraInfo->commandComplete,
        &cameraInfo->commandQueueMutex );
  }
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
  retval = command.resultCode;

  if ( retval ) {
    return "";
  }

  return ( const char* ) command.resultData;
}


// Used when the WB setting is a menu so we don't have a programmatically
// defined way to be sure of turning the white balance off

int
oaV4L2CameraGetAutoWBManualSetting ( oaCamera* camera )
{
  V4L2_STATE*	cameraInfo = camera->_private;

  if ( !cameraInfo->haveWhiteBalanceManual ) {
    fprintf ( stderr, "%s: have no manual value for white balance menu\n",
        __func__ );
  }
  return cameraInfo->autoWhiteBalanceOff;
}

#endif /* HAVE_LIBV4L2 */
