#ifndef BKTIME_H
#define BKTIME_H
void epochToLongString(time_t epoch, char* longString);
void epochToShortString(time_t epoch, char* shortString);
void longStringToEpoch(const char* longString, time_t* epoch);
#endif
