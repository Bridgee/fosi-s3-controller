#pragma once
#include "intro_ai.h"
namespace orbit_intro_ai {
// Strict, incremental HTTP response framing. No socket, JSON, String or heap.
class HttpResponse {
public:
    void reset(char *output,size_t capacity,uint64_t utcSeconds=0);
    bool feed(const uint8_t *,size_t);
    void eof();
    bool done()const{return state_==Done;}
    Outcome error()const{return error_;}
    bool headersReceived()const{return headersDone_;}
    HttpResult result()const;
private:
    enum State:uint8_t {Status,Headers,LengthBody,CloseBody,ChunkSize,ChunkData,ChunkCr,ChunkLf,Trailers,Done,Failed};
    State state_=Status;
    char line_[1024]={};char *output_=nullptr;
    size_t capacity_=0,received_=0,lineSize_=0,headerBytes_=0,frameBytes_=0,remaining_=0;
    uint64_t utc_=0;
    int status_=0;
    uint32_t retry_=0;
    Outcome error_=Outcome::Idle;
    bool cr_=false,headersDone_=false,haveLength_=false,haveEncoding_=false,haveType_=false,haveRetry_=false,haveContentEncoding_=false;
    bool chunked_=false;
    void fail(Outcome);
    bool lineByte(uint8_t);
    void lineReady();
    void header(bool trailer);
};
}
