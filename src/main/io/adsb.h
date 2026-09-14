/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include "common/time.h"
#include "io/gps.h"
#include "fc/runtime_config.h"

#define ADSB_CALL_SIGN_MAX_LENGTH 9
#define ADSB_MAX_SECONDS_KEEP_INACTIVE_PLANE_IN_LIST 10

typedef struct {
    bool valid;
    int32_t dir;   // centidegrees direction to plane, pivot is inav FC
    uint32_t dist;              // horisontal distance to plane, cm, pivot is inav FC
    int32_t verticalDistance;   // vertical distance to plane, cm, pivot is inav FC
} adsbVehicleCalculatedValues_t;

typedef struct {
    uint32_t icao; // ICAO address
    uint16_t horVelocity; // [cm/s]
    gpsLocation_t gps;
    int32_t alt;  // [cm] Barometric/Geometric Altitude (MSL)
    uint16_t heading; // [centidegrees] Course over ground
    uint16_t flags; // Flags to indicate various statuses including valid data fields
    uint8_t altitudeType; // Type from ADSB_ALTITUDE_TYPE enum
    char callsign[ADSB_CALL_SIGN_MAX_LENGTH]; // The callsign, 8 chars + NULL
    uint8_t emitterType; // Type from ADSB_EMITTER_TYPE enum
    uint8_t tslc; // Time since last communication in seconds
} adsbVehicleValues_t;

typedef struct {
    adsbVehicleValues_t vehicleValues;
    adsbVehicleCalculatedValues_t calculatedVehicleValues;
    uint8_t ttl;
} adsbVehicle_t;

typedef struct {
   uint32_t vehiclesMessagesTotal;
   uint32_t heartbeatMessagesTotal;
} adsbVehicleStatus_t;

void adsbNewVehicle(adsbVehicleValues_t* vehicleValuesLocal);
bool adsbHeartbeat(void);
adsbVehicle_t *findVehicleClosestLimit(int32_t maxVerticalDistance);
adsbVehicle_t * findVehicle(uint8_t index);
uint8_t getActiveVehiclesCount(void);
void adsbTtlClean(timeUs_t currentTimeUs);
adsbVehicleStatus_t* getAdsbStatus(void);
adsbVehicleValues_t* getVehicleForFill(void);
bool isEnvironmentOkForCalculatingADSBDistanceBearing(void);
void recalculateVehicle(adsbVehicle_t* vehicle);

typedef struct adsbConfig_s {
    uint8_t maxVehicles;        // aircraft tracked at the same time, at most MAX_ADSB_VEHICLES
} adsbConfig_t;

PG_DECLARE(adsbConfig_t, adsbConfig);

// Limits for picking the aircraft on a critical approach, see io/adsb_threat.h
typedef struct adsbThreatLimits_s {
    uint32_t maxDistanceCm;     // traffic farther than this is ignored
    uint32_t maxAboveCm;        // traffic higher than this above us is ignored, 0 = no limit
    uint16_t coneWidthCd;       // full width of the aircraft's approach cone
    uint16_t maxToaSeconds;     // time-to-arrival limit
} adsbThreatLimits_t;

uint8_t getAdsbMaxVehicles(void);
uint8_t getVehiclesWithinLimitsCount(uint32_t maxDistanceCm, uint32_t maxAboveCm);
adsbVehicle_t *findVehicleThreat(const adsbThreatLimits_t *limits, uint32_t *toaSecondsOut);