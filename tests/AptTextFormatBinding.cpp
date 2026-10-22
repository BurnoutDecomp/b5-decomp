#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include "SDKs/EATech/Apt/AptTextFormatMembersIndex.h"

using s32 = int32_t;
class EAStringC {
    std::string mText;
public:
    EAStringC() = default;
    EAStringC(const char* text) : mText(text) {}
    const char* GetBuffer() const { return mText.c_str(); }
    unsigned GetLength() const { return static_cast<unsigned>(mText.size()); }
    bool IsEmpty() const { return mText.empty(); }
};
using AptNativeString = EAStringC;
class AptValue {
public:
    double mNumber = 0;
    EAStringC mString;
    bool mStringValue = false;
    virtual ~AptValue() = default;
    virtual bool objectMemberSet(AptValue* const, const AptNativeString* const, AptValue* const) { return false; }
    virtual AptValue* objectMemberLookup(AptValue* const, const AptNativeString* const) const { return nullptr; }
    float toFloat() const { return static_cast<float>(mNumber); }
    int toInteger() const { return static_cast<int>(mNumber); }
    void toString(EAStringC* result) const { *result = mString; }
};
static std::vector<std::unique_ptr<AptValue>> gValues;
template <typename T> static T* Keep(T* value) { gValues.emplace_back(value); return value; }
class AptFloat : public AptValue { public: static AptFloat* Create(float x) { auto* p=Keep(new AptFloat);p->mNumber=x;return p; } };
class AptInteger : public AptValue { public: static AptInteger* Create(int x) { auto* p=Keep(new AptInteger);p->mNumber=x;return p; } };
class AptBoolean : public AptValue { public: static AptBoolean* Create(bool x) { auto* p=Keep(new AptBoolean);p->mNumber=x;return p; } };
class AptString : public AptValue {
public:
    static AptString* Create(const char* text) { auto* p=Keep(new AptString);p->mString=EAStringC(text);p->mStringValue=true;return p; }
    EAStringC* GetInternalString() { return &mString; }
};
static AptValue gUndefined;
AptValue* gpUndefinedValue = &gUndefined;
struct TextFormat {
    EAStringC mFontName;
    float mfSize=-1;
    s32 mnColor=-1, mnAlign=3, mnStyleFlags=2, mnIndent=-1, mnLeftMargin=-1, mnRightMargin=-1;
    enum {KU_BOLD_VALUE=1, KU_ITALIC_VALUE=0x10, KU_UNDERLINE_VALUE=0x100,
          KU_BOLD_DEFINED=0x10000, KU_ITALIC_DEFINED=0x100000, KU_UNDERLINE_DEFINED=0x1000000};
};
class AptTextFormat : public AptValue {
public:
    TextFormat mFormat;
#if !defined(APT_TEXTFORMAT_OLD_SETTER)
    bool objectMemberSet(AptValue* const, const AptNativeString* const, AptValue* const) override;
#endif
    AptValue* objectMemberLookup(AptValue* const, const AptNativeString* const) const override;
};

#include "apt_textformat_binding_bodies.inc"

static unsigned gChecks, gFailures;
static void Check(bool ok, const char* message) { ++gChecks; if(!ok){++gFailures;std::printf("FAIL %s\n",message);} }
static bool Same(const TextFormat& a,const TextFormat& b) {
    return std::strcmp(a.mFontName.GetBuffer(),b.mFontName.GetBuffer())==0 && a.mfSize==b.mfSize &&
        a.mnColor==b.mnColor && a.mnAlign==b.mnAlign && a.mnStyleFlags==b.mnStyleFlags &&
        a.mnIndent==b.mnIndent && a.mnLeftMargin==b.mnLeftMargin && a.mnRightMargin==b.mnRightMargin;
}
static bool Set(AptValue* object,const char* key,AptValue* value) {
    EAStringC name(key);return object->objectMemberSet(object,&name,value);
}
static AptValue* Get(AptValue* object,const char* key) { EAStringC name(key);return object->objectMemberLookup(object,&name); }
static float Number(AptValue* value) { return value ? value->toFloat() : -999.0f; }
static const char* String(AptValue* value) { return value ? value->mString.GetBuffer() : "<missing>"; }

int main() {
    AptTextFormat format; AptValue* object=&format;
    AptValue input;input.mNumber=23;input.mString=EAStringC("true");
    EAStringC size("size");
    auto before=format.mFormat;
    Check(!object->objectMemberSet(nullptr,&size,&input),"null context rejects a format assignment");
    Check(Same(before,format.mFormat),"null context leaves embedded format unchanged");
    Check(!Set(object,"notAProperty",&input),"unknown property is not consumed");
    Check(Set(object,"size",&input),"size assignment dispatches through the value virtual interface");
    Check(Number(Get(object,"size"))==23,"size read sees the embedded format assignment");
    input.mNumber=-1;Set(object,"size",&input);
    Check(Get(object,"size")==gpUndefinedValue,"size inherit sentinel remains undefined");
    const char* ignored[]={"blockIndent","bullet","leading","tabStops","target","url"};
    for(auto key:ignored) {
        before=format.mFormat;
        Check(Set(object,key,&input),"original recognized ignored property is consumed");
        Check(Same(before,format.mFormat),"recognized ignored property leaves all fields unchanged");
    }
    input.mNumber=static_cast<int32_t>(0xFE123456u);
    Check(Set(object,"color",&input),"color assignment is consumed");
    Check(format.mFormat.mnColor==0x123456,"color assignment masks to RGB");
    Check(Number(Get(object,"color"))==0x123456,"color getter sees the field");
    input.mString=EAStringC("FixtureFont");
    Check(Set(object,"font",&input),"font assignment is consumed");
    Check(std::strcmp(format.mFormat.mFontName.GetBuffer(),"FixtureFont")==0,"font field holds the converted value");
    Check(std::strcmp(String(Get(object,"font")),"FixtureFont")==0,"font getter sees the field");
    const char* numeric[]={"indent","leftMargin","rightMargin"};
    for(int i=0;i<3;++i) {
        input.mNumber=7+i;
        Check(Set(object,numeric[i],&input),"integer format property is consumed");
        const s32 actual=i==0?format.mFormat.mnIndent:i==1?format.mFormat.mnLeftMargin:format.mFormat.mnRightMargin;
        Check(actual==7+i,"integer format property updates its own field");
        Check(Number(Get(object,numeric[i]))==7+i,"integer property getter sees the field");
    }
    const char* align[]={"left","true","center","right","false","none","unknown"};
    const int expectedAlign[]={0,0,2,1,3,3,3};
    for(int i=0;i<7;++i) {
        input.mString=EAStringC(align[i]);
        Check(Set(object,"align",&input),"alignment assignment is consumed");
        Check(format.mFormat.mnAlign==expectedAlign[i],"alignment keywords preserve original enum and unknown behavior");
    }
    const char* alignRead[]={"left","right","center"};
    for(int i=0;i<3;++i) {
        format.mFormat.mnAlign=i;
        Check(std::strcmp(String(Get(object,"align")),alignRead[i])==0,"alignment getter uses the original enum strings");
    }
    format.mFormat.mnAlign=3;
    Check(Get(object,"align")==gpUndefinedValue,"alignment inherit sentinel stays undefined");
    format.mFormat.mnAlign=-1;
    Check(Get(object,"align")==gpUndefinedValue,"negative alignment follows the original unsigned switch");
    format.mFormat.mnAlign=1;input.mString=EAStringC("unknown");
    Check(Set(object,"align",&input),"unknown alignment text is consumed");
    Check(format.mFormat.mnAlign==1,"unknown alignment preserves an existing non-default value");
    Check(std::strcmp(String(Get(object,"align")),"right")==0,"preserved alignment reads back correctly");
    const char* booleans[]={"true","false","unknown"};
    const s32 bold[]={0x10001,0x10000,0};
    const s32 italic[]={0x100010,0x100000,0};
    for(int i=0;i<3;++i) {
        format.mFormat.mnStyleFlags=0;input.mString=EAStringC(booleans[i]);
        Check(Set(object,"bold",&input),"bold assignment is consumed");
        Check(format.mFormat.mnStyleFlags==bold[i],"bold conversion preserves original flags");
    }
    for(int i=0;i<3;++i) {
        format.mFormat.mnStyleFlags=0x10;input.mString=EAStringC(booleans[i]);
        Check(Set(object,"italic",&input),"italic assignment is consumed");
        Check(format.mFormat.mnStyleFlags==italic[i],"italic unknown clears value without defining it");
    }
    const s32 underBefore[]={0,0,0,0x100};
    const s32 underAfter[]={0x1000100,0x1000000,0x1000100,0x1000000};
    for(int i=0;i<4;++i) {
        format.mFormat.mnStyleFlags=underBefore[i];input.mString=EAStringC(booleans[i<2?i:2]);
        Check(Set(object,"underline",&input),"underline assignment is consumed");
        Check(format.mFormat.mnStyleFlags==underAfter[i],"underline unknown retains original toggle behavior");
    }
    // Reproduce the captured script's progress dependency, without a renderer or
    // an unbounded test: repeatedly write size-1 through the actual setter and
    // read through the actual getter until a size-dependent width fits.
    format.mFormat.mfSize=24;
    unsigned steps=0;
    while(Number(Get(object,"size"))*10.0f>120.0f && Number(Get(object,"size"))>12.0f && steps<16) {
        input.mNumber=Number(Get(object,"size"))-1.0f;
        Set(object,"size",&input);++steps;
    }
    Check(steps==12,"captured font-fit loop progresses and terminates before its test bound");
    Check(format.mFormat.mfSize==12,"font-fit loop changes the native embedded size");
    Check(Number(Get(object,"size"))==12,"font-fit loop reads its final assigned size");
    std::printf("AptTextFormatBinding: %u checks, %u failures\n",gChecks,gFailures);
    return gFailures?1:0;
}
