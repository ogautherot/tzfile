/*
 * Copyright (C) Olivier Gautherot
 * */

#ifndef __TZFILE_H__
#define __TZFILE_H__

#include <array>
#include <ctime>
#include <string>
#include <vector>

#include "int_endian.h"

using namespace std;

/** Descriptor del archivos con enteros de 32 bits, formato tradicional
 */
typedef struct TzFileHeader {
    int8_be magic[4];       // "TZif"
    int8_be version;        // Either '\0' or '2'
    int8_be reserved[15];   // For future use

    int32_be tzh_ttisgmtcnt;   // Number of UTC/local indicators in the file
    int32_be tzh_ttisstdcnt;   // Number of standard/wall indicators in the file
    int32_be tzh_leapcnt;      // Number of leap seconds
    int32_be tzh_timecnt;      // Number of transition times
    int32_be tzh_typecnt;      // Number of "local time types" (must not be 0)
    int32_be tzh_charcnt;      // Number of characters of "timezone abbreviation
                               // strings"
} TzFileHeader;

/** Estructura ttinfo del archivo
 */
typedef struct TTInfo {
    int32_be tt_gmtoff;
    int8_be tt_isdst;
    uint8_be tt_abbrind;
} TTInfo;

/** Estructura de saltos de segundos del archivo
 */
typedef struct LeapSeconds {
    int32_be ts;
    int32_be leapstep;
} LeapSeconds;

typedef struct LeapSeconds64 {
    int64_be ts;
    int32_be leapstep;
} LeapSeconds64;

/** Registro de la abreviacion de la zona horaria con su ubicacion en el archivo
 */
typedef struct AbbrevDesc {
    string name;
    int offset;
} AbbrevDesc;

/** Informacion consolidada del descriptor de zona horaria
 */
typedef struct TzInfo {
    int64_t gmtoff;
    int abbrevindex;
    bool isdst;
    bool isgmt;
    bool isstandard;
    string abbrev;
} TzInfo;

typedef struct TzLeap {
    int64_t ts;
    int32_t step;
    bool is64bits;
} TzLeap;

/** Informacion consolidada de cambios de hora
 */
typedef struct TzTransition {
    time_t ts;
    int8_t tz_idx;
    bool is64bits;
    bool overflow;
} TzTransition;

/** Clase de representacion del archivo de zona horaria
 */
class TzFile {
 private:
    string _filename;
    TzTransition *transition_ts;
    TzInfo *ttinfo;
    vector<TzLeap> leapdefs;
    int nTransitions;
    int nInfo;

    bool parse();
    bool parse32(TzFileHeader &hdr, fstream &f);
    bool parse64(TzFileHeader &hdr, fstream &f);

 public:
    TzFile(string &name);
    ~TzFile();

    void dump();
    void dump_ttinfo(TzInfo &info);
    bool transition(int32_t pos, TzTransition &tr, TzInfo &info);
    struct tm gettime(int32_t pos);
    int getnumtransitions() { return nInfo; }
};

bool parseTzFile(string &name);

#endif   //__TZFILE_H__
