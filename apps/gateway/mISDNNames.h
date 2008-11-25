#ifndef _MISDNNAMES_H_
#define _MISDNNAMES_H_
class mISDNNames {
public:
    static const char* Message(int i);
    static const char* NPI(int i);
    static const char* TON(int i);
    static const char* Presentation(int i);
    static const char* Screening(int i);
    static const char* isdn_prim[4];
    static const char* IE_Names[37];
};
#endif


