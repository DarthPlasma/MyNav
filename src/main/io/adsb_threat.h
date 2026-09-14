/*
 * This file is part of INAV Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License Version 3, as described below:
 *
 * This file is free software: you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "io/adsb.h"

// Threat selection and cone geometry for the OSD_ADSB_CRITICAL_WARNING, OSD_ADSB_CONE and
// OSD_ADSB_STATUS elements. Pure functions of the vehicle data, without globals, so they can be
// unit tested.
//
// The cone belongs to the approaching aircraft: its apex is the aircraft where it is now and its
// axis is the aircraft's course. Angles are signed as seen by a pilot facing the aircraft:
// positive = to our right while facing it, which is the aircraft's left.

#define ADSB_CONE_WIDTH_CELLS   21
#define ADSB_CONE_CENTRE_CELL   (ADSB_CONE_WIDTH_CELLS / 2)

// Our angular position in the aircraft's cone, in [-18000, 18000] centidegrees. It is also the
// aircraft's course minus the reciprocal of our bearing to it: 0 = it points straight at us.
int32_t adsbHeadingErrorCd(const adsbVehicle_t *vehicle);

bool adsbVehicleWithinLimits(const adsbVehicle_t *vehicle, uint32_t maxDistanceCm, uint32_t maxAboveCm);
uint8_t adsbCountVehiclesWithinLimits(const adsbVehicle_t *list, uint8_t count, uint32_t maxDistanceCm, uint32_t maxAboveCm);

// Among the aircraft within the limits that point at us (inside their cone) and would arrive within
// the time limit (distance / their ground speed), the one arriving first. NULL if there is none.
adsbVehicle_t *adsbFindThreat(adsbVehicle_t *list, uint8_t count, const adsbThreatLimits_t *limits, uint32_t *toaSecondsOut);

// Half width of the cone scale in whole degrees, rounded up, at least 1.
uint8_t adsbConeHalfWidthDeg(uint8_t coneWidthDeg);

// Our angular position in the aircraft's CURRENT cone after flying for toaSeconds at our own
// velocity (x = North, y = East, cm/s); the aircraft's motion is not projected. Equals
// adsbHeadingErrorCd() when toaSeconds is 0. A projection past the apex ends up beyond +-90 deg.
int32_t adsbConeProjectedAngleCd(const adsbVehicle_t *vehicle, float velNorthCms, float velEastCms, uint32_t toaSeconds);

// Column of the cone scale for an angle, rounded to the nearest cell and clamped to the scale.
uint8_t adsbConeColumn(int32_t angleCd, uint8_t halfWidthDeg);
