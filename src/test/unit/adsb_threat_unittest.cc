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

#include <stdint.h>
#include <stdbool.h>
#include <cstdlib>
#include <cstring>

extern "C" {
    #include "io/adsb_threat.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

// Frame: x = North, y = East. Bearings and courses in centidegrees, clockwise from North.
static adsbVehicle_t makeVehicle(uint32_t distCm, int32_t bearingCd, uint16_t courseCd, uint16_t speedCms)
{
    adsbVehicle_t vehicle;
    memset(&vehicle, 0, sizeof(vehicle));
    vehicle.ttl = 10;
    vehicle.calculatedVehicleValues.valid = true;
    vehicle.calculatedVehicleValues.dist = distCm;
    vehicle.calculatedVehicleValues.dir = bearingCd;
    vehicle.vehicleValues.heading = courseCd;
    vehicle.vehicleValues.horVelocity = speedCms;
    vehicle.vehicleValues.flags = 1 | 2 | 4 | 8;  // coords, altitude, heading, velocity
    return vehicle;
}

// 20 km, no height limit, 20 deg cone, 60 s
static const adsbThreatLimits_t limits = { 2000000, 0, 2000, 60 };

TEST(AdsbThreatTest, HeadingError)
{
    // North of us, flying South: straight at us
    adsbVehicle_t vehicle = makeVehicle(300000, 0, 18000, 5000);
    EXPECT_EQ(0, adsbHeadingErrorCd(&vehicle));

    // Flying 170 deg it passes East of us: we are on its right, left of us while facing it
    vehicle.vehicleValues.heading = 17000;
    EXPECT_EQ(-1000, adsbHeadingErrorCd(&vehicle));
    vehicle.vehicleValues.heading = 19000;
    EXPECT_EQ(1000, adsbHeadingErrorCd(&vehicle));

    // Wrap around North
    vehicle = makeVehicle(300000, 35900, 17900, 5000);
    EXPECT_EQ(0, adsbHeadingErrorCd(&vehicle));
    vehicle = makeVehicle(300000, 100, 100, 5000);
    EXPECT_EQ(18000, abs(adsbHeadingErrorCd(&vehicle)));
}

TEST(AdsbThreatTest, ThreatNeedsConeTimeAndLimits)
{
    uint32_t toa = 99;
    adsbVehicle_t vehicle = makeVehicle(300000, 0, 18000, 5000);  // 3 km, 50 m/s: 60 s

    EXPECT_EQ(&vehicle, adsbFindThreat(&vehicle, 1, &limits, &toa));
    EXPECT_EQ(60U, toa);

    // Cone edges are inclusive: half of 20 deg
    vehicle.vehicleValues.heading = 19000;
    EXPECT_EQ(&vehicle, adsbFindThreat(&vehicle, 1, &limits, nullptr));
    vehicle.vehicleValues.heading = 19001;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, nullptr));
    vehicle.vehicleValues.heading = 18000;

    // Too late
    vehicle.vehicleValues.horVelocity = 4900;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, &toa));
    EXPECT_EQ(0U, toa);
    vehicle.vehicleValues.horVelocity = 0;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, nullptr));
    vehicle.vehicleValues.horVelocity = 5000;

    // Too far
    vehicle.calculatedVehicleValues.dist = 2000001;
    vehicle.vehicleValues.horVelocity = 40000;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, nullptr));
    vehicle.calculatedVehicleValues.dist = 300000;
    vehicle.vehicleValues.horVelocity = 5000;

    // Heading and velocity must be flagged valid
    vehicle.vehicleValues.flags = 1 | 2 | 8;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, nullptr));
    vehicle.vehicleValues.flags = 1 | 2 | 4;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, nullptr));
    vehicle.vehicleValues.flags = 1 | 2 | 4 | 8;

    // Expired or not computed
    vehicle.ttl = 0;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, nullptr));
    vehicle.ttl = 10;
    vehicle.calculatedVehicleValues.valid = false;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &limits, nullptr));
}

TEST(AdsbThreatTest, HeightLimitOnlyFiltersTrafficAbove)
{
    adsbThreatLimits_t heightLimits = limits;
    heightLimits.maxAboveCm = 50000;
    adsbVehicle_t vehicle = makeVehicle(300000, 0, 18000, 5000);

    vehicle.calculatedVehicleValues.verticalDistance = 50000;
    EXPECT_EQ(&vehicle, adsbFindThreat(&vehicle, 1, &heightLimits, nullptr));
    vehicle.calculatedVehicleValues.verticalDistance = 50001;
    EXPECT_EQ(nullptr, adsbFindThreat(&vehicle, 1, &heightLimits, nullptr));
    vehicle.calculatedVehicleValues.verticalDistance = -500000;
    EXPECT_EQ(&vehicle, adsbFindThreat(&vehicle, 1, &heightLimits, nullptr));

    // 0 = no limit
    vehicle.calculatedVehicleValues.verticalDistance = 500000;
    EXPECT_EQ(&vehicle, adsbFindThreat(&vehicle, 1, &limits, nullptr));
}

TEST(AdsbThreatTest, EarliestArrivalWins)
{
    adsbVehicle_t list[4] = {
        makeVehicle(250000, 0, 18000, 5000),      // 50 s
        makeVehicle(100000, 9000, 27000, 5000),   // 20 s, closest
        makeVehicle(120000, 18000, 0, 10000),     // 12 s, earliest
        makeVehicle(50000, 27000, 27000, 10000),  // 5 s but flying away
    };
    uint32_t toa = 0;

    EXPECT_EQ(&list[2], adsbFindThreat(list, 4, &limits, &toa));
    EXPECT_EQ(12U, toa);

    // Only the first count entries are looked at
    EXPECT_EQ(&list[1], adsbFindThreat(list, 2, &limits, &toa));
    EXPECT_EQ(20U, toa);

    EXPECT_EQ(3, adsbCountVehiclesWithinLimits(list, 3, 2000000, 0));
    EXPECT_EQ(2, adsbCountVehiclesWithinLimits(list, 4, 110000, 0));
}

TEST(AdsbThreatTest, ConeScale)
{
    EXPECT_EQ(10, adsbConeHalfWidthDeg(20));
    EXPECT_EQ(11, adsbConeHalfWidthDeg(21));
    EXPECT_EQ(1, adsbConeHalfWidthDeg(2));
    EXPECT_EQ(1, adsbConeHalfWidthDeg(1));
    EXPECT_EQ(90, adsbConeHalfWidthDeg(180));

    // 1 deg per cell at +-10 deg, rounded to the nearest cell
    EXPECT_EQ(10, adsbConeColumn(0, 10));
    EXPECT_EQ(11, adsbConeColumn(100, 10));
    EXPECT_EQ(9, adsbConeColumn(-100, 10));
    EXPECT_EQ(10, adsbConeColumn(49, 10));
    EXPECT_EQ(11, adsbConeColumn(50, 10));
    EXPECT_EQ(9, adsbConeColumn(-50, 10));
    EXPECT_EQ(20, adsbConeColumn(1000, 10));
    EXPECT_EQ(0, adsbConeColumn(-1000, 10));
    EXPECT_EQ(20, adsbConeColumn(18000, 10));
    EXPECT_EQ(0, adsbConeColumn(-18000, 10));
    EXPECT_EQ(11, adsbConeColumn(900, 90));
}

TEST(AdsbThreatTest, ProjectionStartsAtCurrentPosition)
{
    const adsbVehicle_t vehicles[] = {
        makeVehicle(300000, 0, 18000, 5000),
        makeVehicle(300000, 0, 19000, 5000),
        makeVehicle(300000, 4500, 22000, 5000),
        makeVehicle(800000, 31000, 12000, 7000),
    };

    for (const adsbVehicle_t &vehicle : vehicles) {
        const int32_t now = adsbHeadingErrorCd(&vehicle);
        // No time, or not moving: the projection is where we are
        EXPECT_NEAR(now, adsbConeProjectedAngleCd(&vehicle, 1500, -700, 0), 2);
        EXPECT_NEAR(now, adsbConeProjectedAngleCd(&vehicle, 0, 0, 60), 2);
    }
}

TEST(AdsbThreatTest, ProjectionFollowsOnlyOurMotion)
{
    // Head-on: 3 km North of us, flying South at 50 m/s, ToA 60 s
    const adsbVehicle_t headOn = makeVehicle(300000, 0, 18000, 5000);

    // Dodging to our right (East) at 5 m/s: 300 m aside at 3 km from the apex, to the right
    EXPECT_NEAR(571, adsbConeProjectedAngleCd(&headOn, 0, 500, 60), 3);
    EXPECT_NEAR(-571, adsbConeProjectedAngleCd(&headOn, 0, -500, 60), 3);

    // Flying straight at it at 15 m/s: still on its course
    EXPECT_NEAR(0, adsbConeProjectedAngleCd(&headOn, 1500, 0, 60), 2);

    // Crossing: it comes from the West flying East, we fly North, so to our right while facing it
    const adsbVehicle_t crossing = makeVehicle(300000, 27000, 9000, 5000);
    EXPECT_NEAR(1670, adsbConeProjectedAngleCd(&crossing, 1500, 0, 60), 3);

    // Slower than us head-on: the projection goes past the apex, beyond +-90 deg
    const adsbVehicle_t slow = makeVehicle(90000, 0, 18000, 1500);
    EXPECT_GT(abs(adsbConeProjectedAngleCd(&slow, 2000, 10, 60)), 9000);
}
