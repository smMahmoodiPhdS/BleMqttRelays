#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <math.h>
#include <string>
#include <vector>
typedef uint8_t byte;
#define HEX 16
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1
#define PROGMEM
struct String {
    std::string s;
private:
    void fmtI(long long v, unsigned char base) {
        char b[48];
        if (base == 16) { snprintf(b, sizeof b, "%llx", (unsigned long long)v); }
        else            { snprintf(b, sizeof b, "%lld", v); }
        s = b;
    }
    void fmtU(unsigned long long v, unsigned char base) {
        char b[48];
        if (base == 16) { snprintf(b, sizeof b, "%llx", v); }
        else            { snprintf(b, sizeof b, "%llu", v); }
        s = b;
    }
    void fmtF(double v, unsigned int dec) {
        char b[64];
        snprintf(b, sizeof b, "%.*f", (int)dec, v);
        s = b;
    }
public:
    String() {}
    String(const char* c) : s(c ? c : "") {}
    String(const std::string& v) : s(v) {}
    // ---------------------------------------------------------------------
    // The numeric constructors MIRROR framework-arduinoespressif32's WString.h
    // EXACTLY — same types, same defaults, same `explicit`. Do not "simplify"
    // them.
    //
    // This matters more than it looks. An earlier version of this stub declared
    // `String(float, int)` and omitted `String(unsigned char, unsigned char)`.
    // Under that overload set `String(aFloat, aUint8)` resolved cleanly, so the
    // host tests passed — and the real build then failed with "call of
    // overloaded 'String(float&, uint8_t&)' is ambiguous", because the genuine
    // set offers `String(float, unsigned int)` (exact + promotion) against
    // `String(unsigned char, unsigned char)` (conversion + exact) and neither
    // wins on both arguments.
    //
    // A stub that is more permissive than the real thing does not just miss
    // bugs; it actively certifies broken code. If you add a String method here,
    // copy its signature from WString.h rather than writing what is convenient.
    // ---------------------------------------------------------------------
    explicit String(char c) { s = std::string(1, c); }
    explicit String(unsigned char v, unsigned char base = 10) { fmtU((unsigned long long)v, base); }
    explicit String(int v, unsigned char base = 10) { fmtI((long long)v, base); }
    explicit String(unsigned int v, unsigned char base = 10) { fmtU((unsigned long long)v, base); }
    explicit String(long v, unsigned char base = 10) { fmtI((long long)v, base); }
    explicit String(unsigned long v, unsigned char base = 10) { fmtU((unsigned long long)v, base); }
    explicit String(long long v, unsigned char base = 10) { fmtI(v, base); }
    explicit String(unsigned long long v, unsigned char base = 10) { fmtU(v, base); }
    explicit String(float v, unsigned int decimalPlaces = 2) { fmtF((double)v, decimalPlaces); }
    explicit String(double v, unsigned int decimalPlaces = 2) { fmtF(v, decimalPlaces); }
    const char* c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    float toFloat() const { return strtof(s.c_str(), nullptr); }
    bool startsWith(const String& p) const { return s.rfind(p.s,0)==0; }
    bool endsWith(const String& p) const {
        return s.size()>=p.s.size() && s.compare(s.size()-p.s.size(), p.s.size(), p.s)==0; }
    int indexOf(char c) const { auto n=s.find(c); return n==std::string::npos?-1:(int)n; }
    String substring(size_t a) const { return String(a<=s.size()?s.substr(a):std::string()); }
    String substring(size_t a, size_t b) const { return String(s.substr(a, b-a)); }
    String& operator+=(const String& o){ s+=o.s; return *this; }
    String& operator+=(char c){ s+=c; return *this; }
    bool operator==(const String& o) const { return s==o.s; }
    bool operator!=(const String& o) const { return s!=o.s; }
};
inline String operator+(const String& a, const String& b){ return String(a.s+b.s); }
inline String operator+(const String& a, const char* b){ return String(a.s+(b?b:"")); }
inline String operator+(const char* a, const String& b){ return String(std::string(a?a:"")+b.s); }
struct SerialCls {
    void begin(unsigned long){} 
    template<class T> void print(T){} 
    void print(const String&){} 
    template<class T> void println(T){} 
    void println(const String&){} 
    void println(){} 
    template<class...A> void printf(const char*, A...){}
};
extern SerialCls Serial;
inline void delay(unsigned long){}
inline void delayMicroseconds(unsigned long){}
extern unsigned long g_millis;
inline unsigned long millis(){ return g_millis; }
inline void pinMode(uint8_t,uint8_t){}
inline void digitalWrite(uint8_t,uint8_t){}
extern int g_dipLevel[40];
inline int digitalRead(uint8_t p){ return g_dipLevel[p]; }
inline void randomSeed(unsigned long){}
inline long random(long hi){ return hi/2; }
inline long random(long lo, long hi){ return (lo+hi)/2; }
