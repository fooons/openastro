/*****************************************************************************
 *
 * nodes.h -- header for Basler Pylon node-handlers
 *
 * Copyright 2020 James Fidell (james@openastroproject.org)
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

#ifndef OA_PYLON_NODES_H
#define OA_PYLON_NODES_H

extern int	_pylonGetEnumerationNode ( NODEMAP_HANDLE, const char*, int,
								NODE_HANDLE* );
extern int	_pylonGetBooleanNode ( NODEMAP_HANDLE, const char*, int,
								NODE_HANDLE* );
extern int	_pylonGetIntNode ( NODEMAP_HANDLE, const char*, int, NODE_HANDLE* );
extern int	_pylonGetFloatNode ( NODEMAP_HANDLE, const char*, int,
								NODE_HANDLE* );

extern int	_pylonGetIntSettings ( NODE_HANDLE, int64_t*, int64_t*, int64_t*,
								int64_t* );
extern int	_pylonGetFloatSettings ( NODE_HANDLE, double*, double*, double* );

extern void	_pylonShowEnumValues ( NODE_HANDLE, const char* );

#endif	/* OA_PYLON_NODES_H */
