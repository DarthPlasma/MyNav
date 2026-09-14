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

#include <stddef.h>

#include "platform.h"

#include "common/maths.h"

#include "io/adsb_threat.h"

// MAVLink ADSB_FLAGS_VALID_HEADING | ADSB_FLAGS_VALID_VELOCITY: without them we cannot tell
// whether the aircraft is coming at us, nor when it would arrive.
#define ADSB_THREAT_REQUIRED_FLAGS      (4 | 8)

int32_t adsbHeadingErrorCd(const adsbVehicle_t *vehicle)
{
    const int32_t reciprocalBearingCd = vehicle->calculatedVehicleValues.dir + 18000;
    return wrap_18000(wrap_36000((int32_t)vehicle->vehicleValues.heading - reciprocalBearingCd));
}

bool adsbVehicleWithinLimits(const adsbVehicle_t *vehicle, uint32_t maxDistanceCm, uint32_t maxAboveCm)
{
    if (vehicle->ttl == 0 || !vehicle->calculatedVehicleValues.valid) {
        return false;
    }

    if (vehicle->calculatedVehicleValues.dist > maxDistanceCm) {
        return false;
    }

    // Same rule as findVehicleClosestLimit(): only traffic above us is filtered by height
    if (maxAboveCm > 0 && vehicle->calculatedVehicleValues.verticalDistance > (int32_t)maxAboveCm) {
        return false;
    }

    return true;
}

uint8_t adsbCountVehiclesWithinLimits(const adsbVehicle_t *list, uint8_t count, uint32_t maxDistanceCm, uint32_t maxAboveCm)
{
    uint8_t total = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (adsbVehicleWithinLimits(&list[i], maxDistanceCm, maxAboveCm)) {
            total++;
        }
    }
    return total;
}

adsbVehicle_t *adsbFindThreat(adsbVehicle_t *list, uint8_t count, const adsbThreatLimits_t *limits, uint32_t *toaSecondsOut)
{
    adsbVehicle_t *threat = NULL;
    uint32_t threatToaSeconds = 0;

    for (uint8_t i = 0; i < count; i++) {
        adsbVehicle_t *vehicle = &list[i];

        if (!adsbVehicleWithinLimits(vehicle, limits->maxDistanceCm, limits->maxAboveCm)) {
            continue;
        }

        if ((vehicle->vehicleValues.flags & ADSB_THREAT_REQUIRED_FLAGS) != ADSB_THREAT_REQUIRED_FLAGS || vehicle->vehicleValues.horVelocity == 0) {
            continue;
        }

        const uint32_t toaSeconds = vehicle->calculatedVehicleValues.dist / vehicle->vehicleValues.horVelocity;
        if (toaSeconds > limits->maxToaSeconds) {
            continue;
        }

        // Pointing at us: within half of the cone width from the reciprocal of our bearing to it
        if ((uint32_t)ABS(adsbHeadingErrorCd(vehicle)) * 2 > limits->coneWidthCd) {
            continue;
        }

        if (threat == NULL || toaSeconds < threatToaSeconds) {
            threat = vehicle;
            threatToaSeconds = toaSeconds;
        }
    }

    if (toaSecondsOut != NULL) {
        *toaSecondsOut = threatToaSeconds;
    }
    return threat;
}

uint8_t adsbConeHalfWidthDeg(uint8_t coneWidthDeg)
{
    const uint8_t halfWidthDeg = (coneWidthDeg + 1) / 2;
    return halfWidthDeg > 0 ? halfWidthDeg : 1;
}

int32_t adsbConeProjectedAngleCd(const adsbVehicle_t *vehicle, float velNorthCms, float velEastCms, uint32_t toaSeconds)
{
    const float t = (float)toaSeconds;
    const float distCm = (float)vehicle->calculatedVehicleValues.dist;
    const float dirRad = CENTIDEGREES_TO_RADIANS(vehicle->calculatedVehicleValues.dir);
    const float headingRad = CENTIDEGREES_TO_RADIANS(vehicle->vehicleValues.heading);

    // From the apex (the aircraft, where it is now) to where we will be
    const float toUsNorth = velNorthCms * t - distCm * cos_approx(dirRad);
    const float toUsEast = velEastCms * t - distCm * sin_approx(dirRad);

    // The same vector along the aircraft's course and across it, towards its left
    const float sinHeading = sin_approx(headingRad);
    const float cosHeading = cos_approx(headingRad);
    const float along = toUsNorth * cosHeading + toUsEast * sinHeading;
    const float left = toUsNorth * sinHeading - toUsEast * cosHeading;

    return (int32_t)RADIANS_TO_CENTIDEGREES(atan2_approx(left, along));
}

uint8_t adsbConeColumn(int32_t angleCd, uint8_t halfWidthDeg)
{
    const int32_t halfWidthCd = (int32_t)halfWidthDeg * 100;
    const int32_t scaled = angleCd * ADSB_CONE_CENTRE_CELL;
    const int32_t offset = (scaled >= 0 ? scaled + halfWidthCd / 2 : scaled - halfWidthCd / 2) / halfWidthCd;
    return constrain(ADSB_CONE_CENTRE_CELL + offset, 0, ADSB_CONE_WIDTH_CELLS - 1);
}
