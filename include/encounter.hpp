#pragma once
#include <emp-sh2pc/emp-sh2pc.h>
#include <boost/functional/hash.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <unordered_map>
#include <openssl/sha.h>

using namespace boost::multiprecision;
namespace encounter {

const static size_t DEVICE_ID_SIZE = 32;     // size of DeviceID, in bytes
const static size_t ENCOUNTER_ID_SIZE = 32;  // size of EncounterID, in bytes
const static size_t TILE_ID_SIZE = 4;        // size of TileID, in bytes

// some (fixed-size) representations for data types we might want to upload in an encounter
using TileID = uint32_t;
using DeviceID = uint8_t[DEVICE_ID_SIZE];
using EncounterID = uint8_t[ENCOUNTER_ID_SIZE];
using Latitude = uint32_t;
using Longitude = uint32_t;
using Timestamp = uint32_t;
using Duration = uint16_t;
using Confirmed = uint8_t;
using Nonce = uint64_t;
using FieldName = uint8_t;

const static FieldName NONE = 0;
const static FieldName ID = 1;
const static FieldName DEVICE = 2;
const static FieldName ENCOUNTERED = 3;
const static FieldName LATITUDE = 4;
const static FieldName LONGITUDE = 5;
const static FieldName TILE = 6;
const static FieldName TIME = 7;
const static FieldName PREVIOUS = 8;
const static FieldName NEXT = 9;
const static FieldName DURATION = 10;
const static FieldName CONFIRMED = 11;
const static FieldName RECIPROCATED = 12;

// A convenient struct for storing encounter information
// This contains fields representing things that /might/ be in an uploaded encounter.
// Obviously not all of them should be used. 
struct encounter {  // should be 124 bytes
    EncounterID id;
    DeviceID device;
    DeviceID encountered;
    Latitude latitude;
    Longitude longitude;
    TileID tile;
    Timestamp time;
    Timestamp previous;  // time of previous encoutner from this device
    Timestamp next;      // time of next encounter from this device
    Duration duration;
    Confirmed confirmed;
    bool reciprocated;  // For testing only: should this encounter be confirmed?
};

// fills in encounters with pseudorandom bits
// technically we could just fill the whole patch of memory with hash bits, but
// this ensure platoform-dependent spacing never creeps in.
void fillBlind(struct encounter e[], const size_t count,
               const size_t start_index = 0, const Nonce nonce = 0) {
    uint8_t hash[32];
    uint64_t hashMe[4];
    size_t i;
    hashMe[2] = (uint64_t)nonce;
    hashMe[3] =
        7;  // ensures this is different from randomly generated elements
    for (size_t j = 0; j < count; ++j) {
        hashMe[1] = start_index + j;
        hashMe[0] = ID;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(EncounterID); ++i) {
            ((uint8_t *)(&(e[j].id)))[i] = hash[i];
        }
        hashMe[0] = DEVICE;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(DeviceID); ++i) {
            ((uint8_t *)(&(e[j].device)))[i] = hash[i];
        }
        hashMe[0] = ENCOUNTERED;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(DeviceID); ++i) {
            ((uint8_t *)(&(e[j].encountered)))[i] = hash[i];
        }
        hashMe[0] = LATITUDE;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(Latitude); ++i) {
            ((uint8_t *)(&(e[j].latitude)))[i] = hash[i];
        }
        hashMe[0] = LONGITUDE;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(Longitude); ++i) {
            ((uint8_t *)(&(e[j].longitude)))[i] = hash[i];
        }
        hashMe[0] = TILE;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(TileID); ++i) {
            ((uint8_t *)(&(e[j].tile)))[i] = hash[i];
        }
        hashMe[0] = TIME;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(Timestamp); ++i) {
            ((uint8_t *)(&(e[j].time)))[i] = hash[i];
        }
        hashMe[0] = PREVIOUS;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(Timestamp); ++i) {
            ((uint8_t *)(&(e[j].previous)))[i] = hash[i];
        }
        hashMe[0] = NEXT;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(Timestamp); ++i) {
            ((uint8_t *)(&(e[j].next)))[i] = hash[i];
        }
        hashMe[0] = DURATION;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        for (i = 0; i < sizeof(Duration); ++i) {
            ((uint8_t *)(&(e[j].duration)))[i] = hash[i];
        }
        hashMe[0] = CONFIRMED;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        e[j].confirmed = (hash[0] % 2) == 1;
        hashMe[0] = RECIPROCATED;
        SHA256((const unsigned char *)hashMe, 4 * sizeof(uint64_t),
               (unsigned char *)hash);
        e[j].reciprocated = (hash[0] % 2) == 1;
    }
}

// creates a unique (pseudorandom) id for device i (or i mod number of devices)
// fills deviceID d with a hash of:
// i % number_of_devices
// where number of devices is num_reporting devices (default 1000) if reporting, otherwise num_total_devices (default intmax).
void fillDeviceID(DeviceID *d, const uint64_t i, const bool reporting,
                  const uint64_t num_reporting_devices = 1000,
                  const uint64_t num_total_devices = 0xffffffffffffffff) {
    uint64_t index =
        i % (reporting ? num_reporting_devices : num_total_devices);
    SHA256((const unsigned char *)&index, sizeof(uint64_t), (unsigned char *)d);
}

// fills a pair of device IDs (they'll never be the same)
// i will be used to create device
// j will be used to create encountered, unless j==i, then we'll hash it until it's not ==i.
void fillDeviceIDPair(DeviceID *device, DeviceID *encountered, const uint64_t i,
                      const uint64_t j, const bool reporting,
                      const uint64_t num_reporting_devices = 1000,
                      const uint64_t num_total_devices = 0xffffffffffffffff) {
    uint64_t index = i % num_reporting_devices;
    SHA256((const unsigned char *)&index, sizeof(uint64_t),
           (unsigned char *)device);
    uint64_t hashMe[4];
    hashMe[0] = j;
    hashMe[1] = j;
    hashMe[2] = j;
    hashMe[3] = j;
    while ((hashMe[0] %
            (reporting ? num_reporting_devices : num_total_devices)) == index) {
        SHA256((unsigned char *)&hashMe, 32, (unsigned char *)hashMe);
    }
    index = hashMe[0] % (reporting ? num_reporting_devices : num_total_devices);
    SHA256((const unsigned char *)&index, sizeof(uint64_t),
           (unsigned char *)encountered);
}

// mostly a helper for fillEncounters
// Fills encounter e, given some index i
// Reciprocal encounters will be sequential, and differ only in device and encountered.
// So, for example, if encounter 100 is chosen (pseudorandom) to be
//  reciprocal, then its match will be encounter 101. 
// Next and previous will be max_time and min_time
void fillEncounter(
    struct encounter *e,
    const size_t index,
    const uint8_t reciprocation_percent = 90,
    const uint64_t num_reporting_devices = 1000,
    const uint64_t num_total_devices = 0xffffffffffffffff,
    const Timestamp min_time = 0,
    const Timestamp max_time = 0xffffffff,
    const Duration min_duration = 0,
    const Duration max_duration = 0xffff,
    const Latitude min_latitude = 0,
    const Latitude max_latitude = 0xffffffff,
    const Longitude min_longitude = 0,
    const Longitude max_longitude = 0xffffffff,
    const Nonce nonce = 0
    ) {
    uint8_t hash[32];
    Nonce hashMe[3];
    size_t i;
    e->next = max_time;
    e->previous = min_time;
    e->confirmed = 0;
    hashMe[1] = index - (index % 2);
    hashMe[2] = nonce;

    hashMe[0] = RECIPROCATED;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    e->reciprocated = (((((uint64_t *)hash)[0]) % 100) < reciprocation_percent);

    if (((index % 2) == 1) &&
        (!(e->reciprocated))) {  // an odd number non-reciprocated
        hashMe[1] = index;
    }

    hashMe[0] = ID;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    for (i = 0; i < sizeof(EncounterID); ++i) {
        ((uint8_t *)(&(e->id)))[i] = hash[i];
    }
    hashMe[0] = DEVICE;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    fillDeviceIDPair(
        &((((index % 2) == 1) && (e->reciprocated)) ? e->encountered
                                                    : e->device),
        &((((index % 2) == 1) && (e->reciprocated)) ? e->device
                                                    : e->encountered),
        ((uint64_t *)hash)[0], ((uint64_t *)hash)[1], e->reciprocated,
        num_reporting_devices, num_total_devices);

    hashMe[0] = LATITUDE;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    e->latitude =
        (((Latitude *)hash)[0] % (max_latitude - min_latitude)) + min_latitude;
    hashMe[0] = LONGITUDE;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    e->longitude = (((Longitude *)hash)[0] % (max_longitude - min_longitude)) +
                   min_longitude;
    hashMe[0] = TILE;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    for (i = 0; i < sizeof(TileID); ++i) {
        ((uint8_t *)(&(e->tile)))[i] = hash[i];
    }
    hashMe[0] = TIME;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    e->time = (((Timestamp *)hash)[0] % (max_time - min_time)) + min_time;
    hashMe[0] = DURATION;
    SHA256((const unsigned char *)hashMe, 3 * sizeof(Nonce),
           (unsigned char *)hash);
    e->duration =
        (((Duration *)hash)[0] % (max_duration - min_duration)) + min_duration;
}

// fills each encounter in array, filling in previous and next fields when possible.
// this will fill in next and previous times for each device as well
// - earliest encounter of each device will have previous = min_time
// - last encounter of each device will have next = max time
// pseudorandom values (in range) will be used for everything else.
void fillEncounters(
    struct encounter e[], const size_t count, const size_t start_index = 0,
    const uint8_t reciprocation_percent = 90,
    const uint64_t num_reporting_devices = 1000,
    const uint64_t num_total_devices = 0xffffffffffffffff,
    const Timestamp min_time = 0, const Timestamp max_time = 0xffffffff,
    const Duration min_duration = 0, const Duration max_duration = 0xffff,
    const Latitude min_latitude = 0, const Latitude max_latitude = 0xffffffff,
    const Longitude min_longitude = 0,
    const Longitude max_longitude = 0xffffffff, const Nonce nonce = 0) {
    uint256_t device_key[1];  // this is basically a hack to get around
                              // DeviceIDs not being hashable
    std::unordered_map<uint256_t, struct encounter *> *most_recent =
        (new std::unordered_map<uint256_t, struct encounter *>(
            num_reporting_devices));

    for (size_t i = 0; i < count; ++i) {
        fillEncounter(&(e[i]), start_index + i, reciprocation_percent,
                      num_reporting_devices, num_total_devices, min_time,
                      max_time, min_duration, max_duration, min_latitude,
                      max_latitude, min_longitude, max_longitude, nonce);
        SHA256((const unsigned char *)&(e[i].device), sizeof(DeviceID),
               (unsigned char *)device_key);
        if (most_recent->find(device_key[0]) != most_recent->end()) {
            (*most_recent)[(device_key[0])]->next = e[i].time;
            e[i].previous = (*most_recent)[(device_key[0])]->time;
        }
        (*most_recent)[(device_key[0])] = &(e[i]);
    }
    most_recent->clear();
    delete most_recent;
}

// splits encounters into xor-able aliceShare and bobShare
// uses fillBlind for pseudorandom generation
void shareEncounters(const struct encounter encounters[],
                     struct encounter aliceShare[], struct encounter bobShare[],
                     const size_t count, const size_t start_index = 0,
                     const Nonce nonce = 0) {
    fillBlind(bobShare, count, start_index, nonce);
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < sizeof(struct encounter); ++j) {
            ((uint8_t *)(&(aliceShare[i])))[j] =
                ((uint8_t *)(&(encounters[i])))[j] ^
                ((uint8_t *)(&(bobShare[i])))[j];
        }
    }
}

// fills each encounter in array, filling in previous and next fields when possible.
// then shares teh array into two xor-able shares
// this will fill in next and previous times for each device as well
// - earliest encounter of each device will have previous = min_time
// - last encounter of each device will have next = max time
// pseudorandom values (in range) will be used for everything else.
void fillShareEncounters(
    struct encounter encounters[], struct encounter aliceShare[],
    struct encounter bobShare[], const size_t count,
    const size_t start_index = 0, const uint8_t reciprocation_percent = 90,
    const uint64_t num_reporting_devices = 1000,
    const uint64_t num_total_devices = 0xffffffffffffffff,
    const Timestamp min_time = 0, const Timestamp max_time = 0xffffffff,
    const Duration min_duration = 0, const Duration max_duration = 0xffff,
    const Latitude min_latitude = 0, const Latitude max_latitude = 0xffffffff,
    const Longitude min_longitude = 0,
    const Longitude max_longitude = 0xffffffff, const Nonce nonce = 0) {
    fillEncounters(encounters, count, start_index, reciprocation_percent,
                   num_reporting_devices, num_total_devices, min_time, max_time,
                   min_duration, max_duration, min_latitude, max_latitude,
                   min_longitude, max_longitude, nonce);
    shareEncounters(encounters, aliceShare, bobShare, count, start_index,
                    nonce);
}

// xors aliceShare and bobShare into encounters.
void unShareEncounters(struct encounter encounters[],
                       const struct encounter aliceShare[],
                       const struct encounter bobShare[], const size_t count,
                       const size_t start_index = 0, const Nonce nonce = 0) {
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < sizeof(struct encounter); ++j) {
            ((uint8_t *)(&(encounters[i])))[j] =
                ((uint8_t *)(&(aliceShare[i])))[j] ^
                ((uint8_t *)(&(bobShare[i])))[j];
        }
    }
}

// populate the bits of these integers using the encounters given.
// specify which fields (and in what order) are added to the integer using the
// "field_x" fields.
// if field_1 is not specified, we'll use all the fields in their traditional
// order.
// recall that these integers are little-endian, so put "most significant"
// fields last.
// "dummy" indicates that this is a dummy integer, and the bits used don't
// matter.
// this is written in a really repetitive way, but the use semantics are
// convenient:
//
// ```
// struct encounter[20];
// emp::Integer integer[20];
// fillIntegers(encounter, integer, emp::ALICE, 20, false, TIME, DEVICE);
// ```
void fillIntegers(const struct encounter encounter[], emp::Integer integer[],
                  const int party = emp::PUBLIC, const size_t count = 1,
                  const bool dummy = false, FieldName field_1 = NONE,
                  FieldName field_2 = NONE, FieldName field_3 = NONE,
                  FieldName field_4 = NONE, FieldName field_5 = NONE,
                  FieldName field_6 = NONE, FieldName field_7 = NONE,
                  FieldName field_8 = NONE, FieldName field_9 = NONE,
                  FieldName field_10 = NONE, FieldName field_11 = NONE,
                  FieldName field_12 = NONE) {
    bool *bools = new bool[8 * sizeof(struct encounter)];
    size_t i;
    if (field_1 == NONE) {
        field_1 = ID;
        field_2 = DEVICE;
        field_3 = ENCOUNTERED;
        field_4 = LATITUDE;
        field_5 = LONGITUDE;
        field_6 = TILE;
        field_7 = TIME;
        field_8 = PREVIOUS;
        field_9 = NEXT;
        field_10 = DURATION;
        field_11 = CONFIRMED;
        field_12 = RECIPROCATED;
    }
    if (dummy) {
        i = 0;
        for (FieldName field :
             {field_1, field_2, field_3, field_4, field_5, field_6, field_7,
              field_8, field_9, field_10, field_11, field_12}) {
            switch (field) {
                case ID: {
                    i += 8 * sizeof(EncounterID);
                } break;
                case DEVICE: {
                    i += 8 * sizeof(DeviceID);
                } break;
                case ENCOUNTERED: {
                    i += 8 * sizeof(DeviceID);
                } break;
                case LATITUDE: {
                    i += 8 * sizeof(Latitude);
                } break;
                case LONGITUDE: {
                    i += 8 * sizeof(Longitude);
                } break;
                case TILE: {
                    i += 8 * sizeof(TileID);
                } break;
                case TIME: {
                    i += 8 * sizeof(Timestamp);
                } break;
                case PREVIOUS: {
                    i += 8 * sizeof(Timestamp);
                } break;
                case NEXT: {
                    i += 8 * sizeof(Timestamp);
                } break;
                case DURATION: {
                    i += 8 * sizeof(Duration);
                } break;
                case CONFIRMED: {
                    i += 8 * sizeof(Confirmed);
                } break;
                case RECIPROCATED: {
                    ++i;
                } break;
            }
        }
        for (size_t j = 0; j < count; ++j) {
            integer[j].init(bools, i, party);
        }
    } else {
        for (size_t j = 0; j < count; ++j) {
            i = 0;
            for (FieldName field :
                 {field_1, field_2, field_3, field_4, field_5, field_6, field_7,
                  field_8, field_9, field_10, field_11, field_12}) {
                switch (field) {
                    case ID: {
                        emp::to_bool(&(bools[i]), &(encounter[j].id),
                                     8 * sizeof(EncounterID));
                        i += 8 * sizeof(EncounterID);
                    } break;
                    case DEVICE: {
                        emp::to_bool(&(bools[i]), &(encounter[j].device),
                                     8 * sizeof(DeviceID));
                        i += 8 * sizeof(DeviceID);
                    } break;
                    case ENCOUNTERED: {
                        emp::to_bool(&(bools[i]), &(encounter[j].encountered),
                                     8 * sizeof(DeviceID));
                        i += 8 * sizeof(DeviceID);
                    } break;
                    case LATITUDE: {
                        emp::to_bool(&(bools[i]), &(encounter[j].latitude),
                                     8 * sizeof(Latitude));
                        i += 8 * sizeof(Latitude);
                    } break;
                    case LONGITUDE: {
                        emp::to_bool(&(bools[i]), &(encounter[j].longitude),
                                     8 * sizeof(Longitude));
                        i += 8 * sizeof(Longitude);
                    } break;
                    case TILE: {
                        emp::to_bool(&(bools[i]), &(encounter[j].tile),
                                     8 * sizeof(TileID));
                        i += 8 * sizeof(TileID);
                    } break;
                    case TIME: {
                        emp::to_bool(&(bools[i]), &(encounter[j].time),
                                     8 * sizeof(Timestamp));
                        i += 8 * sizeof(Timestamp);
                    } break;
                    case PREVIOUS: {
                        emp::to_bool(&(bools[i]), &(encounter[j].previous),
                                     8 * sizeof(Timestamp));
                        i += 8 * sizeof(Timestamp);
                    } break;
                    case NEXT: {
                        emp::to_bool(&(bools[i]), &(encounter[j].next),
                                     8 * sizeof(Timestamp));
                        i += 8 * sizeof(Timestamp);
                    } break;
                    case DURATION: {
                        emp::to_bool(&(bools[i]), &(encounter[j].duration),
                                     8 * sizeof(Duration));
                        i += 8 * sizeof(Duration);
                    } break;
                    case CONFIRMED: {
                        emp::to_bool(&(bools[i]), &(encounter[j].confirmed),
                                     8 * sizeof(Confirmed));
                        i += 8 * sizeof(Confirmed);
                    } break;
                    case RECIPROCATED: {
                        bools[i] = encounter[j].reciprocated;
                        ++i;
                    } break;
                }
            }
            integer[j].init(bools, i, party);
        }
    }
    delete[] bools;
}

// Read the integers given into the encoutner array.
// You can specify which order the fields are represented within each Integer. 
// This is the reverse operation from fillIntegers
// ```
// struct encounter[20];
// emp::Integer integer[20];
// fromIntegers(encounter, integer, emp::ALICE, 20, false, TIME, DEVICE);
// ```
void fromIntegers(struct encounter encounter[], const emp::Integer integer[],
                  const int party = emp::PUBLIC, const size_t count = 1,
                  const bool dummy = false, FieldName field_1 = NONE,
                  FieldName field_2 = NONE, FieldName field_3 = NONE,
                  FieldName field_4 = NONE, FieldName field_5 = NONE,
                  FieldName field_6 = NONE, FieldName field_7 = NONE,
                  FieldName field_8 = NONE, FieldName field_9 = NONE,
                  FieldName field_10 = NONE, FieldName field_11 = NONE,
                  FieldName field_12 = NONE) {
    bool *bools = new bool[8 * sizeof(struct encounter)];
    size_t i;
    if (field_1 == NONE) {
        field_1 = ID;
        field_2 = DEVICE;
        field_3 = ENCOUNTERED;
        field_4 = LATITUDE;
        field_5 = LONGITUDE;
        field_6 = TILE;
        field_7 = TIME;
        field_8 = PREVIOUS;
        field_9 = NEXT;
        field_10 = DURATION;
        field_11 = CONFIRMED;
        field_12 = RECIPROCATED;
    }
    if (dummy) {
        for (size_t j = 0; j < count; ++j) {
            integer[j].revealBools(bools, party);
        }
    } else {
        for (size_t j = 0; j < count; ++j) {
            integer[j].revealBools(bools, party);
            i = 0;
            for (FieldName field :
                 {field_1, field_2, field_3, field_4, field_5, field_6, field_7,
                  field_8, field_9, field_10, field_11, field_12}) {
                switch (field) {
                    case ID: {
                        emp::from_bool(&(bools[i]), &(encounter[j].id),
                                       8 * sizeof(EncounterID));
                        i += 8 * sizeof(EncounterID);
                    } break;
                    case DEVICE: {
                        emp::from_bool(&(bools[i]), &(encounter[j].device),
                                       8 * sizeof(DeviceID));
                        i += 8 * sizeof(DeviceID);
                    } break;
                    case ENCOUNTERED: {
                        emp::from_bool(&(bools[i]), &(encounter[j].encountered),
                                       8 * sizeof(DeviceID));
                        i += 8 * sizeof(DeviceID);
                    } break;
                    case LATITUDE: {
                        emp::from_bool(&(bools[i]), &(encounter[j].latitude),
                                       8 * sizeof(Latitude));
                        i += 8 * sizeof(Latitude);
                    } break;
                    case LONGITUDE: {
                        emp::from_bool(&(bools[i]), &(encounter[j].longitude),
                                       8 * sizeof(Longitude));
                        i += 8 * sizeof(Longitude);
                    } break;
                    case TILE: {
                        emp::from_bool(&(bools[i]), &(encounter[j].tile),
                                       8 * sizeof(TileID));
                        i += 8 * sizeof(TileID);
                    } break;
                    case TIME: {
                        emp::from_bool(&(bools[i]), &(encounter[j].time),
                                       8 * sizeof(Timestamp));
                        i += 8 * sizeof(Timestamp);
                    } break;
                    case PREVIOUS: {
                        emp::from_bool(&(bools[i]), &(encounter[j].previous),
                                       8 * sizeof(Timestamp));
                        i += 8 * sizeof(Timestamp);
                    } break;
                    case NEXT: {
                        emp::from_bool(&(bools[i]), &(encounter[j].next),
                                       8 * sizeof(Timestamp));
                        i += 8 * sizeof(Timestamp);
                    } break;
                    case DURATION: {
                        emp::from_bool(&(bools[i]), &(encounter[j].duration),
                                       8 * sizeof(Duration));
                        i += 8 * sizeof(Duration);
                    } break;
                    case CONFIRMED: {
                        emp::from_bool(&(bools[i]), &(encounter[j].confirmed),
                                       8 * sizeof(Confirmed));
                        i += 8 * sizeof(Confirmed);
                    } break;
                    case RECIPROCATED: {
                        encounter[j].reciprocated = bools[i];
                        ++i;
                    } break;
                }
            }
        }
    }
    delete[] bools;
}

// WARNING: this has never worked properly. 
// I couldn't figure out the bit order to put stuff into the circuit.
// supposed to compute sha256 of a bunch of integers (concatenated)
//  using the bristol-format circuit.
void sha256Integers(emp::Integer hash, const emp::Integer integers[],
                    const size_t count) {
    bool bools[512];
    emp::block blocks[1];
    for (size_t i = 0; i < 512; ++i) {
        bools[i] = false;
    }
    BristolFormat cf("circuits/bristol_format/sha-256.txt");
    emp::Integer hashMe;
    emp::Integer nextHash;
    hashMe.init(bools, 512, emp::PUBLIC);
    nextHash.init(bools, 256, emp::PUBLIC);
    hash.init(bools, 256, emp::PUBLIC);

    size_t index = 0;
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < integers[i].size(); ++j) {
            hashMe[index] = integers[i][j];
            ++index;
            if (index == 512) {
                cf.compute(&(nextHash[0].bit), &(hashMe[0].bit), blocks);
                hash ^= nextHash;
                index = 0;
            }
        }
    }
    if (index != 0) {
        cf.compute(&(nextHash[0].bit), &(hashMe[0].bit), blocks);
        hash ^= nextHash;
    }
}

// Concatenates bits from integers,
// computes thair SHA1 using the circuit file,
// and puts it in hash.
void sha1Integers(emp::Integer hash, const emp::Integer integers[],
                  const size_t count) {
    bool bools[512];
    emp::block blocks[1];
    for (size_t i = 0; i < 512; ++i) {
        bools[i] = false;
    }
    BristolFormat cf("circuits/bristol_format/sha-1.txt");
    emp::Integer hashMe;
    emp::Integer nextHash;
    hashMe.init(bools, 512, emp::PUBLIC);
    nextHash.init(bools, 160, emp::PUBLIC);
    hash.init(bools, 160, emp::PUBLIC);
    size_t index = 0;
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < integers[i].size(); ++j) {
            hashMe[index] = integers[i][j];
            ++index;
            if (index == 512) {
                cf.compute(&(nextHash[0].bit), &(hashMe[0].bit), blocks);
                hash ^= nextHash;
                index = 0;
            }
        }
    }
    if (index != 0) {
        cf.compute(&(nextHash[0].bit), &(hashMe[0].bit), blocks);
        hash ^= nextHash;
    }
}

// Concatenates the bits in integers, and computes their AES256 using key.
// Note that this is not proper encryption with an IV and so on.
// use the emp-tool encryption libraries for that. 
void aes256Integers(emp::Integer *output, const emp::Integer *key,
                    const emp::Integer integers[], const size_t count) {
    size_t total_size = 0;
    for (size_t i = 0; i < count; ++i) {
        total_size += integers[i].size();
    }
    total_size = 128 * ((total_size + 127) / 128);
    bool bools[total_size];
    for (size_t i = 0; i < total_size; ++i) {
        bools[i] = false;
    }
    output->init(bools, total_size, emp::PUBLIC);
    emp::Integer nextEncrypt;
    nextEncrypt.init(bools, 128, emp::PUBLIC);
    key->revealBools(bools);
    emp::Integer nextChunk;
    nextChunk.init(bools, 256 + 128, emp::PUBLIC);

    BristolFashion cf("circuits/bristol_fashion/aes_256.txt");

    size_t index = 0;
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < integers[i].size(); ++j) {
            nextChunk[256 + (index % 128)] = integers[i][j];
            ++index;
            if ((index % 128) == 0) {
                cf.compute(&(nextEncrypt[0].bit), &(nextChunk[0].bit));
                for (size_t k = 0; k < 128; ++k) {
                    (*output)[index - 128 + k] = nextEncrypt[k];
                }
            }
        }
    }
    if ((index % 128) != 0) {
        cf.compute(&(nextEncrypt[0].bit), &(nextChunk[0].bit));
        for (size_t k = 0; k < 128; ++k) {
            (*output)[index - 128 + k] = nextEncrypt[k];
        }
    }
}

// Concatenates the bits in integers, and computes their AES128 using key.
// Note that this is not proper encryption with an IV and so on.
// use the emp-tool encryption libraries for that. 
void aes128Integers(emp::Integer *output, const emp::Integer *key,
                    const emp::Integer integers[], const size_t count) {
    size_t total_size = 0;
    for (size_t i = 0; i < count; ++i) {
        total_size += integers[i].size();
    }
    total_size = 128 * ((total_size + 127) / 128);
    bool bools[total_size];
    for (size_t i = 0; i < total_size; ++i) {
        bools[i] = false;
    }
    output->init(bools, total_size, emp::PUBLIC);
    emp::Integer nextEncrypt;
    nextEncrypt.init(bools, 128, emp::PUBLIC);
    emp::Integer nextChunk;
    nextChunk.init(bools, 128, emp::PUBLIC);

    BristolFormat cf("circuits/bristol_format/AES-non-expanded.txt");

    size_t index = 0;
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < integers[i].size(); ++j) {
            nextChunk[index % 128] = integers[i][j];
            ++index;
            if ((index % 128) == 0) {
                cf.compute(&(nextEncrypt[0].bit), &((*key)[0].bit),
                           &(nextChunk[0].bit));
                for (size_t k = 0; k < 128; ++k) {
                    (*output)[index - 128 + k] = nextEncrypt[k];
                }
            }
        }
    }
    if ((index % 128) != 0) {
        cf.compute(&(nextEncrypt[0].bit), &((*key)[0].bit),
                   &(nextChunk[0].bit));
        for (size_t k = 0; k < 128; ++k) {
            (*output)[index - 128 + k] = nextEncrypt[k];
        }
    }
}
}  // namespace encounter
