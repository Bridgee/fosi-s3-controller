#include "intro_ai_http.h"
#include <string.h>
#include <limits.h>
namespace orbit_intro_ai {
namespace {
bool equal(const char *a,const char *b){for(;*a&&*b;++a,++b){char x=*a,y=*b;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return !*a&&!*b;}
bool decimal(const char *s,uint64_t max,uint64_t &v){if(!*s)return false;v=0;for(;*s;++s){if(*s<'0'||*s>'9')return false;unsigned d=unsigned(*s-'0');if(v>(max-d)/10)return false;v=v*10+d;}return true;}
bool token(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("!#$%&'*+-.^_`|~",c);}
unsigned digits(const char *s,unsigned n){unsigned out=0;for(unsigned i=0;i<n;++i){if(s[i]<'0'||s[i]>'9')return UINT_MAX;out=out*10+unsigned(s[i]-'0');}return out;}
int64_t days(unsigned year,unsigned month,unsigned day){int y=int(year)-(month<=2);const int era=y/400;const unsigned yo=unsigned(y-era*400);const unsigned mp=month>2?month-3:month+9;const unsigned doy=(153*mp+2)/5+day-1;return int64_t(era)*146097+yo*365+yo/4-yo/100+doy-719468;}
uint32_t retryAfter(const char *s,uint64_t utc){
    uint64_t value=0;if(decimal(s,UINT32_MAX,value))return uint32_t(value);
    bool allDigits=*s;for(const char *p=s;*p;++p)if(*p<'0'||*p>'9')allDigits=false;
    if(allDigits)return UINT32_MAX; // saturate an excessively large minimum wait
    if(strlen(s)!=29||!utc||s[3]!=','||s[4]!=' '||s[7]!=' '||s[11]!=' '||s[16]!=' '||s[19]!=':'||s[22]!=':'||strcmp(s+25," GMT"))return 0;
    const char *months="JanFebMarAprMayJunJulAugSepOctNovDec";unsigned month=0;
    for(unsigned i=0;i<12;++i)if(!memcmp(s+8,months+3*i,3))month=i+1;
    const unsigned day=digits(s+5,2),year=digits(s+12,4),h=digits(s+17,2),m=digits(s+20,2),sec=digits(s+23,2);
    if(!month||year<1970||year>2100||!day||h>23||m>59||sec>59)return 0;
    const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap=year%4==0&&(year%100!=0||year%400==0);
    if(day>lengths[month-1]+unsigned(month==2&&leap))return 0;
    const int64_t dayCount=days(year,month,day);
    const char *week="SunMonTueWedThuFriSat";if(memcmp(s,week+3*((dayCount+4)%7),3))return 0;
    const uint64_t at=uint64_t(dayCount)*86400+h*3600+m*60+sec;
    if(at<=utc)return 0;return at-utc>UINT32_MAX?UINT32_MAX:uint32_t(at-utc);
}
}
void HttpResponse::reset(char *out,size_t cap,uint64_t utc){
    volatile char *wipe=line_;for(size_t i=0;i<sizeof line_;++i)wipe[i]=0;
    state_=Status;output_=out;capacity_=cap;utc_=utc;received_=lineSize_=headerBytes_=frameBytes_=remaining_=0;status_=0;retry_=0;error_=Outcome::Idle;
    cr_=headersDone_=haveLength_=haveEncoding_=haveType_=haveRetry_=haveContentEncoding_=chunked_=false;
}
void HttpResponse::fail(Outcome o){error_=o;state_=Failed;}
bool HttpResponse::lineByte(uint8_t c){
    if(cr_){cr_=false;if(c!='\n'){fail(Outcome::ProtocolError);return false;}line_[lineSize_]=0;lineReady();lineSize_=0;return true;}
    if(c=='\r'){cr_=true;return true;}
    if(c=='\n'||c<32||c>=127||lineSize_+1>=sizeof line_){fail(Outcome::ProtocolError);return false;}
    line_[lineSize_++]=char(c);return true;
}
void HttpResponse::header(bool trailer){
    char *colon=strchr(line_,':');if(!colon||colon==line_){fail(Outcome::ProtocolError);return;}
    for(char *p=line_;p<colon;++p)if(!token(*p)){fail(Outcome::ProtocolError);return;}
    *colon=0;char *value=colon+1;while(*value==' ')++value;char *end=value+strlen(value);while(end>value&&end[-1]==' ')*--end=0;
    const bool length=equal(line_,"Content-Length"),encoding=equal(line_,"Transfer-Encoding");
    if(trailer){if(length||encoding||equal(line_,"Content-Type")||equal(line_,"Content-Encoding")||equal(line_,"Retry-After")||equal(line_,"Authorization"))fail(Outcome::ProtocolError);return;}
    if(status_!=200){if(equal(line_,"Retry-After")){if(haveRetry_){fail(Outcome::ProtocolError);return;}haveRetry_=true;retry_=retryAfter(value,utc_);}return;}
    if(length){uint64_t count=0;if(haveLength_||!decimal(value,SIZE_MAX,count)){fail(Outcome::ProtocolError);return;}haveLength_=true;remaining_=size_t(count);}
    else if(encoding){if(haveEncoding_||!equal(value,"chunked")){fail(Outcome::ProtocolError);return;}haveEncoding_=chunked_=true;}
    else if(equal(line_,"Content-Type")){
        if(haveType_){fail(Outcome::ProtocolError);return;}haveType_=true;char *semi=strchr(value,';');if(semi)*semi=0;
        end=value+strlen(value);while(end>value&&end[-1]==' ')*--end=0;
        if(status_==200&&!equal(value,"application/json"))fail(Outcome::ProtocolError);
    }else if(equal(line_,"Content-Encoding")){
        if(haveContentEncoding_||!equal(value,"identity")){fail(Outcome::ProtocolError);return;}haveContentEncoding_=true;
    }else if(equal(line_,"Retry-After")){if(haveRetry_){fail(Outcome::ProtocolError);return;}haveRetry_=true;retry_=retryAfter(value,utc_);}
}
void HttpResponse::lineReady(){
    if(state_==Status){
        if(lineSize_<12||strncmp(line_,"HTTP/1.",7)||(line_[7]!='0'&&line_[7]!='1')||line_[8]!=' '||digits(line_+9,3)>599||digits(line_+9,3)<200||(lineSize_>12&&line_[12]!=' ')){fail(Outcome::ProtocolError);return;}
        status_=int(digits(line_+9,3));state_=Headers;return;
    }
    if(state_==Headers){
        if(lineSize_){header(false);return;}headersDone_=true;
        if(haveLength_&&chunked_){fail(Outcome::ProtocolError);return;}
        // Error bodies can contain request echoes. Preserve status/Retry-After,
        // then close without storing/logging such bodies or following redirects.
        if(status_!=200){state_=Done;return;}
        if(!haveType_){fail(Outcome::ProtocolError);return;}
        if(haveLength_&&remaining_>capacity_){fail(Outcome::TooLarge);return;}
        state_=chunked_?ChunkSize:haveLength_?(remaining_?LengthBody:Done):CloseBody;return;
    }
    if(state_==ChunkSize){
        char *semi=strchr(line_,';');if(semi)*semi=0;
        const size_t n=strlen(line_);if(!n||n>8){fail(Outcome::ProtocolError);return;}
        uint64_t count=0;
        for(size_t i=0;i<n;++i){char c=line_[i];unsigned value=c>='0'&&c<='9'?unsigned(c-'0'):c>='a'&&c<='f'?unsigned(c-'a'+10):c>='A'&&c<='F'?unsigned(c-'A'+10):16;if(value==16){fail(Outcome::ProtocolError);return;}count=count*16+value;}
        if(count>capacity_-received_){fail(Outcome::TooLarge);return;}
        remaining_=size_t(count);state_=remaining_?ChunkData:Trailers;return;
    }
    if(state_==Trailers){if(!lineSize_)state_=Done;else header(true);return;}
    fail(Outcome::ProtocolError);
}
bool HttpResponse::feed(const uint8_t *p,size_t n){
    if((n&&!p)||!output_||capacity_>MaxResponseBytes){fail(Outcome::ProtocolError);return false;}
    for(size_t i=0;i<n;++i){
        if(state_==Failed)return false;
        if(state_==Done){if(status_!=200)return true;fail(Outcome::ProtocolError);return false;}
        if(state_==Status||state_==Headers){if(++headerBytes_>8192){fail(Outcome::TooLarge);return false;}if(!lineByte(p[i]))return false;}
        else if(state_==ChunkSize||state_==Trailers){if(++frameBytes_>8192){fail(Outcome::TooLarge);return false;}if(!lineByte(p[i]))return false;}
        else if(state_==ChunkCr){if(p[i]!='\r'){fail(Outcome::ProtocolError);return false;}state_=ChunkLf;}
        else if(state_==ChunkLf){if(p[i]!='\n'){fail(Outcome::ProtocolError);return false;}state_=ChunkSize;}
        else {
            if(received_==capacity_){fail(Outcome::TooLarge);return false;}
            output_[received_++]=char(p[i]);
            if(state_==LengthBody){if(!--remaining_)state_=Done;}
            else if(state_==ChunkData){if(!--remaining_)state_=ChunkCr;}
        }
        if(state_==Failed)return false;
    }
    return true;
}
void HttpResponse::eof(){if(state_==CloseBody)state_=Done;else if(state_!=Done&&state_!=Failed)fail(Outcome::Incomplete);}
HttpResult HttpResponse::result()const{HttpResult r;r.status=status_;r.retryAfterSeconds=retry_;r.bytes=received_;r.tooLarge=error_==Outcome::TooLarge;r.truncated=error_==Outcome::Incomplete;r.failure=error_;return r;}
}
