#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <dbScan.h>
#include <dbCommon.h>
#include <epicsExport.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <stringinRecord.h>

struct EipFieldLink {
    std::string port;
    std::string type;
    std::size_t offset = 0;
    std::size_t length = 0;
    int bit = -1;
};

struct EipStatusLink { std::string port; };

extern "C" bool genericEipReadConnectionState(const char*, char*, std::size_t);

static std::vector<std::string> split(const std::string& text)
{
    std::vector<std::string> result;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) result.push_back(item);
    return result;
}

static bool parseLink(const char* text, EipFieldLink& link)
{
    if (!text) return false;
    const std::string input(text);
    // EPICS strips the leading '@' from INST_IO links before device support
    // sees them, so accept both the DB spelling and the parsed spelling.
    const std::string prefix = input.compare(0, 9, "@etherip(") == 0
        ? "@etherip(" : "etherip(";
    if (input.compare(0, prefix.size(), prefix) != 0) return false;
    const auto close = input.find(')', prefix.size());
    if (close == std::string::npos) return false;
    const auto args = split(input.substr(prefix.size(), close - prefix.size()));
    if (args.size() < 2 || args.size() > 3) return false;
    try {
        link.port = args[0];
        link.offset = std::stoul(args[1]);
        if (args.size() == 3) link.bit = std::stoi(args[2]);
    } catch (...) { return false; }
    link.type = input.substr(close + 1);
    while (!link.type.empty() && std::isspace(link.type.front())) link.type.erase(0, 1);
    if (link.type.empty()) return false;
    if (link.type == "BIT" || link.type == "INT8" || link.type == "UINT8") link.length = 1;
    else if (link.type == "INT16" || link.type == "UINT16") link.length = 2;
    else if (link.type == "INT32" || link.type == "UINT32" || link.type == "FLOAT32") link.length = 4;
    else if (link.type == "INT64" || link.type == "UINT64" || link.type == "FLOAT64") link.length = 8;
    else return false;
    if (link.type == "BIT") {
        if (link.bit < 0 || link.bit > 15) return false;
        link.length = link.bit > 7 ? 2 : 1;
    }
    return true;
}

extern "C" bool genericEipReadField(const char*, std::size_t, std::size_t, const char*, int, double*);
extern "C" bool genericEipWriteField(const char*, std::size_t, std::size_t, const char*, int, double);
extern "C" IOSCANPVT genericEipGetScanPvt(const char*);

static long initLink(dbCommon* record, DBLINK* dbLink)
{
    auto* link = new EipFieldLink;
    if (dbLink->type != INST_IO || !parseLink(dbLink->value.instio.string, *link)) {
        delete link;
        return -1;
    }
    record->dpvt = link;
    return 0;
}

static long getIoIntInfo(int, dbCommon* record, IOSCANPVT* scan)
{
    const auto* link = static_cast<const EipFieldLink*>(record->dpvt);
    *scan = link ? genericEipGetScanPvt(link->port.c_str()) : nullptr;
    return *scan ? 0 : -1;
}

static long getStatusIoIntInfo(int, dbCommon* record, IOSCANPVT* scan)
{
    const auto* link = static_cast<const EipStatusLink*>(record->dpvt);
    *scan = link ? genericEipGetScanPvt(link->port.c_str()) : nullptr;
    return *scan ? 0 : -1;
}

static long readValue(dbCommon* record, double* value)
{
    const auto* link = static_cast<const EipFieldLink*>(record->dpvt);
    return link && genericEipReadField(link->port.c_str(), link->offset, link->length,
                                       link->type.c_str(), link->bit, value) ? 0 : -1;
}

static long writeValue(dbCommon* record, double value)
{
    const auto* link = static_cast<const EipFieldLink*>(record->dpvt);
    return link && genericEipWriteField(link->port.c_str(), link->offset, link->length,
                                        link->type.c_str(), link->bit, value) ? 0 : -1;
}

static long initLongin(dbCommon* r) { return initLink(r, &reinterpret_cast<longinRecord*>(r)->inp); }
static long initLongout(dbCommon* r) { return initLink(r, &reinterpret_cast<longoutRecord*>(r)->out); }
static long initAi(dbCommon* r) { return initLink(r, &reinterpret_cast<aiRecord*>(r)->inp); }
static long initAo(dbCommon* r) { return initLink(r, &reinterpret_cast<aoRecord*>(r)->out); }
static long initBi(dbCommon* r) { return initLink(r, &reinterpret_cast<biRecord*>(r)->inp); }
static long initBo(dbCommon* r) { return initLink(r, &reinterpret_cast<boRecord*>(r)->out); }
static long initMbbi(dbCommon* r) { return initLink(r, &reinterpret_cast<mbbiRecord*>(r)->inp); }
static long initMbbo(dbCommon* r) { return initLink(r, &reinterpret_cast<mbboRecord*>(r)->out); }

static long initStringin(dbCommon* record)
{
    auto* link = new EipStatusLink;
    const auto& io = reinterpret_cast<stringinRecord*>(record)->inp;
    std::string text = io.value.instio.string ? io.value.instio.string : "";
    const std::string prefix = text.compare(0, 9, "@etherip(") == 0 ? "@etherip(" : "etherip(";
    if (io.type != INST_IO || text.compare(0, prefix.size(), prefix) != 0) { delete link; return -1; }
    const auto close = text.find(')', prefix.size());
    if (close == std::string::npos || text.substr(close + 1) != "CONNECTION_STATE") { delete link; return -1; }
    link->port = text.substr(prefix.size(), close - prefix.size());
    if (link->port.empty()) { delete link; return -1; }
    record->dpvt = link;
    return 0;
}

static long readStringin(stringinRecord* r)
{
    const auto* link = static_cast<const EipStatusLink*>(r->dpvt);
    char value[sizeof(r->val)] = {};
    const long status = link && genericEipReadConnectionState(link->port.c_str(), value, sizeof(value)) ? 0 : -1;
    if (!status) std::snprintf(r->val, sizeof(r->val), "%s", value);
    return status;
}

static dbCommon* common(void* record) { return reinterpret_cast<dbCommon*>(record); }
static long readLongin(longinRecord* r) { double v; const long s = readValue(common(r), &v); if (!s) r->val = static_cast<epicsInt32>(v); return s; }
static long readAi(aiRecord* r) { double v; const long s = readValue(common(r), &v); if (!s) r->val = v; return s; }
static long readBi(biRecord* r) { double v; const long s = readValue(common(r), &v); if (!s) r->rval = v != 0.0; return s; }
static long writeLongout(longoutRecord* r) { return writeValue(common(r), r->val); }
static long writeAo(aoRecord* r) { return writeValue(common(r), r->val); }
static long writeBo(boRecord* r) { return writeValue(common(r), r->val); }
static long readMbbi(mbbiRecord* r) { double v; const long s = readValue(common(r), &v); if (!s) r->rval = static_cast<epicsUInt32>(v); return s; }
static long writeMbbo(mbboRecord* r) { return writeValue(common(r), r->rval); }

longindset devEipLongin = {{5, nullptr, nullptr, initLongin, getIoIntInfo}, readLongin};
longoutdset devEipLongout = {{5, nullptr, nullptr, initLongout, nullptr}, writeLongout};
aidset devEipAi = {{5, nullptr, nullptr, initAi, getIoIntInfo}, readAi};
aodset devEipAo = {{5, nullptr, nullptr, initAo, nullptr}, writeAo};
bidset devEipBi = {{5, nullptr, nullptr, initBi, getIoIntInfo}, readBi};
bodset devEipBo = {{5, nullptr, nullptr, initBo, nullptr}, writeBo};
mbbidset devEipMbbi = {{5, nullptr, nullptr, initMbbi, getIoIntInfo}, readMbbi};
mbbodset devEipMbbo = {{5, nullptr, nullptr, initMbbo, nullptr}, writeMbbo};
stringindset devEipStringin = {{5, nullptr, nullptr, initStringin, getStatusIoIntInfo}, readStringin};

epicsExportAddress(dset, devEipLongin);
epicsExportAddress(dset, devEipLongout);
epicsExportAddress(dset, devEipAi);
epicsExportAddress(dset, devEipAo);
epicsExportAddress(dset, devEipBi);
epicsExportAddress(dset, devEipBo);
epicsExportAddress(dset, devEipMbbi);
epicsExportAddress(dset, devEipMbbo);
epicsExportAddress(dset, devEipStringin);
