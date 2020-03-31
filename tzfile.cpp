/*
 * Copyright (C) Olivier Gautherot
 * */

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "tzfile.h"

typedef union {
    char raw[4];
    int v;
} intByteMap;

static const intByteMap refMagic = {'T', 'Z', 'i', 'f'};

TzFile::TzFile(string &name)
    : _filename(name), transition_ts(nullptr), ttinfo(nullptr),
      nTransitions(-1), nInfo(-1) {
    leapdefs.clear();
    parse();
}

TzFile::~TzFile() {
    if (transition_ts != NULL) {
        delete[] transition_ts;
        transition_ts = NULL;
    }
    if (ttinfo != NULL) {
        delete[] ttinfo;
        ttinfo = NULL;
    }
}

static bool checkMagic(int8_be *magic) {
    intByteMap *arg = (intByteMap *)magic;
    return arg->v == refMagic.v;
}

bool TzFile::parse32(TzFileHeader &hdr, fstream &f) {
    bool ret = false;

    // Timestamps of recorded transitions
    nTransitions = hdr.tzh_timecnt;
    transition_ts = new TzTransition[nTransitions];
    for (int i = 0; i < nTransitions; i++) {
        int32_be v;

        f.read((char *)&v, sizeof(v));
        transition_ts[i].ts = v;
        transition_ts[i].is64bits = false;
        transition_ts[i].overflow = false;
    }

    // Offset of the time descriptor string
    for (int i = 0; i < nTransitions; i++) {
        int8_be idx;

        idx = f.get();
        transition_ts[i].tz_idx = idx;
    }

    // ttinfo array
    nInfo = hdr.tzh_typecnt;
    ttinfo = new TzInfo[nInfo];
    for (int i = 0; i < nInfo; i++) {
        TTInfo tti;
        f.read((char *)&tti, sizeof(tti));
        ttinfo[i].gmtoff = tti.tt_gmtoff;
        ttinfo[i].isdst = (0 != tti.tt_isdst);
        ttinfo[i].abbrevindex = tti.tt_abbrind;
        ttinfo[i].abbrev = "";          // Default
        ttinfo[i].isgmt = false;        // Default
        ttinfo[i].isstandard = false;   // Default
    }

    // Abbreviations
    if (0 < hdr.tzh_charcnt) {
        AbbrevDesc field;

        field.name.clear();
        field.offset = 0;
        for (int i = 0; i < hdr.tzh_charcnt; i++) {
            char c = f.get();
            if (0 == c) {
                for (int j = 0; j < nInfo; j++) {
                    if (ttinfo[j].abbrevindex == field.offset) {
                        ttinfo[j].abbrev = field.name;
                    }
                }
                field.name.clear();
                field.offset = i + 1;
            } else {
                field.name += c;
            }
        }
    }
    // Leap seconds descriptors
    for (int i = 0; i < hdr.tzh_leapcnt; i++) {
        LeapSeconds var;
        f.read((char *)&var, sizeof(var));
        TzLeap leap = {var.ts, var.leapstep, false};
        leapdefs.push_back(leap);
    }

    // Standard/wall clock time indicators
    for (int i = 0; i < hdr.tzh_ttisstdcnt; i++) {
        int8_be indicator = f.get();
        ttinfo[i].isstandard = (0 == indicator);
    }

    // UTC/local indicators
    for (int i = 0; i < hdr.tzh_ttisgmtcnt; i++) {
        int8_be flag = f.get();
        ttinfo[i].isgmt = (0 == flag);
    }
    ret = true;
    return ret;
}

bool TzFile::parse64(TzFileHeader &hdr, fstream &f) {
    bool ret = false;

    // Timestamps of recorded transitions
    if (hdr.tzh_timecnt != nTransitions) {
        if (transition_ts != NULL) {
            delete[] transition_ts;
            transition_ts = NULL;
        }
        nTransitions = hdr.tzh_timecnt;
        transition_ts = new TzTransition[nTransitions];
    }
    for (int i = 0; i < nTransitions; i++) {
        int64_be v;
        int msb;

        f.read((char *)&v, sizeof(v));
        transition_ts[i].ts = v;
        transition_ts[i].is64bits = true;
        msb = (uint64_t)v >> 56;
        transition_ts[i].overflow = ((msb != 0) && (msb != 0xff));
    }

    // Offset of the time descriptor string
    for (int i = 0; i < nTransitions; i++) {
        int8_be idx;

        idx = f.get();
        transition_ts[i].tz_idx = idx;
    }

    // ttinfo array
    if (nInfo != hdr.tzh_typecnt) {
        if (ttinfo != NULL) {
            delete[] ttinfo;
            ttinfo = 0;
        }
        nInfo = hdr.tzh_typecnt;
        ttinfo = new TzInfo[nInfo];
    }
    for (int i = 0; i < nInfo; i++) {
        TTInfo tti;
        f.read((char *)&tti, sizeof(tti));
        ttinfo[i].gmtoff = tti.tt_gmtoff;
        ttinfo[i].isdst = (0 != tti.tt_isdst);
        ttinfo[i].abbrevindex = tti.tt_abbrind;
        ttinfo[i].abbrev = "";          // Default
        ttinfo[i].isgmt = false;        // Default
        ttinfo[i].isstandard = false;   // Default
    }

    // Abbreviations
    if (0 < hdr.tzh_charcnt) {
        AbbrevDesc field;

        field.name.clear();
        field.offset = 0;
        for (int i = 0; i < hdr.tzh_charcnt; i++) {
            char c = f.get();
            if (0 == c) {
                for (int j = 0; j < nInfo; j++) {
                    if (ttinfo[j].abbrevindex == field.offset) {
                        ttinfo[j].abbrev = field.name;
                    }
                }
                field.name.clear();
                field.offset = i + 1;
            } else {
                field.name += c;
            }
        }
    }
    // Leap seconds descriptors
    for (int i = 0; i < hdr.tzh_leapcnt; i++) {
        LeapSeconds64 var;
        f.read((char *)&var, sizeof(var));
        TzLeap leap = {var.ts, var.leapstep, true};
        leapdefs.push_back(leap);
    }

    // Standard/wall clock time indicators
    for (int i = 0; i < hdr.tzh_ttisstdcnt; i++) {
        int8_be indicator = f.get();
        ttinfo[i].isstandard = (0 == indicator);
    }

    // UTC/local indicators
    for (int i = 0; i < hdr.tzh_ttisgmtcnt; i++) {
        int8_be flag = f.get();
        ttinfo[i].isgmt = (0 == flag);
    }
    ret = true;
    return ret;
}

bool TzFile::parse() {
    bool ret = false;
    TzFileHeader hdr1 = {0};
    TzFileHeader hdr2 = {0};
    fstream f(_filename, fstream::in | fstream::binary);

    if (f.good()) {
        f.read((char *)&hdr1, sizeof(hdr1));
        if (checkMagic(hdr1.magic)) {
            ret = parse32(hdr1, f);

            if ((ret) && (('2' == hdr1.version) || ('3' == hdr1.version))) {
                f.read((char *)&hdr2, sizeof(hdr2));
                ret = parse64(hdr2, f);
            }
        }
    }

    return ret;
}

bool TzFile::transition(int32_t pos, TzTransition &tr, TzInfo &info) {
    bool ret = false;

    if (pos < nTransitions) {
        tr = transition_ts[pos];
        info = ttinfo[transition_ts[pos].tz_idx];
        ret = true;
    } else {
        tr.ts = -1;
        tr.tz_idx = -1;
        info.abbrev = "";
        info.abbrevindex = -1;
        info.gmtoff = -1;
        info.isdst = false;
        info.isgmt = false;
        info.isstandard = false;
    }
    return ret;
}

struct tm TzFile::gettime(int32_t pos) {
    struct tm ret = {
        0,
    };

    if (pos < nTransitions) {
        if (!transition_ts[pos].overflow) {
            time_t ts = transition_ts[pos].ts;
            ret = *localtime(&ts);
        }
    }
    return ret;
}

void TzFile::dump_ttinfo(TzInfo &info) {
    cout << "Offset: " << info.gmtoff << ", is DST: " << info.isdst
         << ", abbreviation (" << info.abbrevindex << "): " << info.abbrev
         << ", " << ((info.isstandard) ? "Standard" : "Wall clock") << ", "
         << ((info.isgmt) ? "GMT" : "local") << endl;
}

void TzFile::dump() {
    cout << "Counters:\nTransitions indexes: " << nTransitions
         << "\nTtinfo: " << nInfo << "\nLeap seconds defs: " << leapdefs.size()
         << endl;

    cout << "Transitions: (" << nTransitions << ")\n";
    for (int i = 0; i < nTransitions; i++) {
        time_t ts = transition_ts[i].ts;
        string ascts;

        if (transition_ts[i].overflow) {
            ascts = "<overflow>\n";
        } else {
            // if (transition_ts[i].is64bits)  {
            ascts = asctime(localtime(&ts));
            // } else {
            // ascts = asctime(localtime(&ts));
        }
        ascts.erase(ascts.length() - 1);
        cout << ascts << " - ";
        dump_ttinfo(ttinfo[transition_ts[i].tz_idx]);
    }

    cout << "Leap seconds definitions:\n";
    for (unsigned int i = 0; i < leapdefs.size(); i++) {
        time_t ts = leapdefs[i].ts;
        string ascts(asctime(localtime(&ts)));

        ascts.erase(ascts.length() - 1);
        cout << "- " << ascts << ", step " << leapdefs[i].step << endl;
    }
    cout.flush();
}
