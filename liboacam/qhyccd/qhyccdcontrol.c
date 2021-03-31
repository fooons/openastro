/*****************************************************************************
 *
 * qhyccdcontrol.c -- control functions for libqhyccd cameras
 *
 * Copyright 2019,2021 James Fidell (james@openastroproject.org)
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
#include <openastro/camera.h>

#include "oacamprivate.h"
#include "qhyccdprivate.h"
#include "qhyccdoacam.h"
#include "qhyccdstate.h"


int
oaQHYCCDCameraTestControl ( oaCamera* camera, int control,
    oaControlValue* valp )
{
	unsigned int	i, found;
	QHYCCD_STATE*	cameraInfo = camera->_private;
	COMMON_INFO*	commonInfo = camera->_common;
	int32_t				val_s32;
	int64_t				val_s64;

  if ( !camera->OA_CAM_CTRL_TYPE( control )) {
    return -OA_ERR_INVALID_CONTROL;
  }

  if ( camera->OA_CAM_CTRL_TYPE( control ) != valp->valueType ) {
    return -OA_ERR_INVALID_CONTROL_TYPE;
  }

	if ( OA_CAM_CTRL_BINNING == control ) {
		val_s32 = valp->discrete;
		if ( val_s32 < 0 || val_s32 > OA_MAX_BINNING ||
				cameraInfo->frameSizes[ val_s32 ].numSizes < 1 ) {
			return -OA_ERR_OUT_OF_RANGE;
		}
		return OA_ERR_NONE;
	}

	found = 0;
	for ( i = 0; i < numQHYControls && !found; i++ ) {
		if ( QHYControlData[i].oaControl == control ) {
			found = 1;
			switch ( QHYControlData[i].oaControlType ) {
				case OA_CTRL_TYPE_INT32:
					val_s32 = valp->int32;
					if ( val_s32 < commonInfo->OA_CAM_CTRL_MIN( control ) ||
							val_s32 > commonInfo->OA_CAM_CTRL_MAX( control )) {
						return -OA_ERR_OUT_OF_RANGE;
					}
					break;
				case OA_CTRL_TYPE_INT64:
					val_s64 = valp->int64;
					if ( val_s64 < commonInfo->OA_CAM_CTRL_MIN( control ) ||
							val_s64 > commonInfo->OA_CAM_CTRL_MAX( control )) {
						return -OA_ERR_OUT_OF_RANGE;
					}
					break;
				case OA_CTRL_TYPE_BOOLEAN:
					// anything is good here
					break;
				default:
					return -OA_ERR_OUT_OF_RANGE;
					break;
			}
		}
	}

	if ( found ) {
		return OA_ERR_NONE;
	}

  fprintf ( stderr, "Unrecognised control %d in %s\n", control, __func__ );
  return -OA_ERR_INVALID_CONTROL;
}
