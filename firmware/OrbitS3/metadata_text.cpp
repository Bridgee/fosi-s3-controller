#include "metadata_text.h"
#include "metadata_latin_composition.h"
#include <stdint.h>
namespace orbit_metadata {
namespace {
struct Mark {uint16_t cp;uint8_t ccc;};
constexpr size_t MaxMarks=MaxMetadataBytes/2+1;
uint32_t read(const char *source,size_t length,size_t &at) {
    auto lead=uint8_t(source[at++]);if(lead<0x80)return lead;
    unsigned extra=lead>=0xc2 && lead<=0xdf?1:lead>=0xe0 && lead<=0xef?2:lead>=0xf0 && lead<=0xf4?3:0;
    if(!extra || extra>length-at)return 0xfffd;
    uint32_t cp=lead&((1u<<(6-extra))-1);size_t next=at;
    for(unsigned i=0;i<extra;++i){auto b=uint8_t(source[next++]);if((b&0xc0)!=0x80)return 0xfffd;cp=(cp<<6)|(b&63);}
    if((extra==1 && cp<0x80)||(extra==2 && cp<0x800)||(extra==3 && cp<0x10000)||
       (cp>=0xd800 && cp<=0xdfff)||cp>0x10ffff)return 0xfffd;
    at=next;return cp;
}
bool emit(char *dest,size_t capacity,size_t &out,uint32_t cp) {
    unsigned n=cp<0x80?1:cp<0x800?2:cp<0x10000?3:4;
    if(n>=capacity-out)return false;
    if(n==1)dest[out++]=char(cp);
    else {
        dest[out++]=char((n==2?0xc0:n==3?0xe0:0xf0)|(cp>>(6*(n-1))));
        for(unsigned i=n-1;i>0;--i)dest[out++]=char(0x80|((cp>>(6*(i-1)))&63));
    }
    return true;
}
uint32_t compose(uint32_t base,uint32_t mark) {
    const uint32_t key=(base<<16)|mark;
    size_t lo=0,hi=sizeof(latin_compositions)/sizeof(latin_compositions[0]);
    while(lo<hi){size_t mid=(lo+hi)/2;const auto &p=latin_compositions[mid];uint32_t k=(uint32_t(p.base)<<16)|p.mark;if(k<key)lo=mid+1;else hi=mid;}
    if(lo<sizeof(latin_compositions)/sizeof(latin_compositions[0])){
        const auto &p=latin_compositions[lo];if(p.base==base && p.mark==mark)return p.composed;
    }
    return 0;
}
uint32_t canonical_mark(uint32_t cp){return cp==0x340?0x300:cp==0x341?0x301:cp;}
uint8_t combining(uint32_t cp){return cp>=0x300 && cp<0x370?combining_classes[cp-0x300]:0;}
}
static size_t compose_bounded(char *dest,size_t capacity,const char *source,size_t length) {
    if(!dest || !capacity)return 0;
    dest[0]=0;if(!source)return 0;
    size_t at=0,out=0;
    while(at<length && source[at]) {
        uint32_t base=read(source,length,at);
        if(base==0x212a)base='K';else if(base==0x212b)base=0xc5; // Canonically equivalent Latin signs.
        const bool latin=(base>='A' && base<='Z')||(base>='a' && base<='z')||(base>=0xc0 && base<=0x17f);
        if(!latin){if(!emit(dest,capacity,out,base))break;continue;}
        Mark marks[MaxMarks];size_t count=0;
        if(base>=0xc0)for(const auto &p:latin_compositions)if(p.composed==base){base=p.base;marks[count++]={p.mark,combining(p.mark)};break;}
        while(at<length && source[at] && count<MaxMarks) {
            size_t next=at;uint32_t mark=canonical_mark(read(source,length,next));uint8_t ccc=combining(mark);
            if(!ccc)break;
            at=next;marks[count++]={uint16_t(mark),ccc};
        }
        // Stable canonical ordering, with a bounded combining-mark cluster.
        for(size_t i=1;i<count;++i){Mark m=marks[i];size_t j=i;while(j && marks[j-1].ccc>m.ccc){marks[j]=marks[j-1];--j;}marks[j]=m;}
        size_t kept=0;uint8_t previous=0;
        for(size_t i=0;i<count;++i){
            uint32_t combined=(previous==0 || previous<marks[i].ccc)?compose(base,marks[i].cp):0;
            if(combined)base=combined;
            else{previous=marks[i].ccc;marks[kept++]=marks[i];}
        }
        if(!emit(dest,capacity,out,base))break;
        bool full=false;for(size_t i=0;i<kept;++i)if(!emit(dest,capacity,out,marks[i].cp)){full=true;break;}
        if(full)break;
    }
    dest[out]=0;return out;
}
size_t compose_latin(char *dest,size_t capacity,const char *source,size_t length){return compose_bounded(dest,capacity,source,length<MaxMetadataBytes?length:MaxMetadataBytes-1);}
size_t compose_lyric(char *dest,size_t capacity,const char *source,size_t length){return compose_bounded(dest,capacity,source,length<241?length:240);}

}
