#include "lyrics_parser.h"
#include "metadata_text.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <utility>
#include <initializer_list>
namespace orbit_lyrics {
namespace {
bool decode(const char *s,size_t length,size_t &at,uint32_t &cp){
    unsigned c=static_cast<unsigned char>(s[at++]);unsigned n=c<128?1:c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
    if(!n || n-1>length-at)return false;cp=n==1?c:c&((1u<<(7-n))-1);
    for(unsigned i=1;i<n;++i){unsigned b=static_cast<unsigned char>(s[at++]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}
    return !((n==2&&cp<0x80)||(n==3&&cp<0x800)||(n==4&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff));
}
bool allowed(uint32_t cp,bool multiline){return (cp>=32 || cp==9 || (multiline&&(cp==10||cp==13))) && !(cp>=0x7f&&cp<=0x9f) && !(cp>=0x200b&&cp<=0x200f) && !(cp>=0x202a&&cp<=0x202e) && !(cp>=0x2060&&cp<=0x206f);}
bool canonical(const char *source,char (&out)[160]){
    if(!source)return false;size_t n=strnlen(source,160);if(!n || n>=159 || !valid_text(source,n))return false;
    char composed[160];orbit_metadata::compose_latin(composed,sizeof(composed),source,n);
    size_t written=0;bool space=false;
    for(size_t i=0;composed[i];++i){auto c=static_cast<unsigned char>(composed[i]);
        if(c==' '||c=='\t'){space=written>0;continue;}
        if(space){out[written++]=' ';space=false;}
        out[written++]=c>='A'&&c<='Z'?char(c+32):char(c);
    }
    out[written]=0;return written>0;
}
bool remaster_note(const char *p,size_t n){
    bool marker=false;if(!n||n>100)return false;
    for(size_t i=0;i<n;){unsigned c=static_cast<unsigned char>(p[i]);
        if(c==' '||c=='\t'||c=='-'||c=='.'||c==':'){++i;continue;}
        if(c>='0'&&c<='9'){unsigned year=0,count=0;while(i<n&&p[i]>='0'&&p[i]<='9'){year=year*10+unsigned(p[i++]-'0');if(++count>4)return false;}if(count!=4||year<1900||year>2099)return false;continue;}
        if(c<128&&isalpha(c)){char word[24];unsigned used=0;while(i<n&&static_cast<unsigned char>(p[i])<128&&isalpha(static_cast<unsigned char>(p[i]))){if(used+1>=sizeof(word))return false;word[used++]=char(tolower(static_cast<unsigned char>(p[i++])));}word[used]=0;
            if(!strcmp(word,"remaster")||!strcmp(word,"remastered")||!strcmp(word,"remastering"))marker=true;
            else if(strcmp(word,"digital")&&strcmp(word,"digitally")&&strcmp(word,"version")&&strcmp(word,"the"))return false;continue;}
        bool known=false;
        for(const char *term:{"重制版","重製版","重制","重製","リマスター","リマスタリング"}){size_t bytes=strlen(term);if(bytes<=n-i&&!memcmp(p+i,term,bytes)){marker=known=true;i+=bytes;break;}}
        if(known)continue;
        if(i+3<=n&&!memcmp(p+i,"年",3)){i+=3;continue;}
        return false;
    }
    return marker;
}
bool version_note(const char *p,size_t n){
    if(n>100)return true;char tag[101];memcpy(tag,p,n);tag[n]=0;
    for(size_t i=0;i<n;++i)if(tag[i]>='A'&&tag[i]<='Z')tag[i]+=32;
    for(size_t i=0;i<n;){unsigned c=static_cast<unsigned char>(tag[i]);if(c>=128||!isalpha(c)){++i;continue;}
        size_t end=i;while(end<n&&static_cast<unsigned char>(tag[end])<128&&isalpha(static_cast<unsigned char>(tag[end])))++end;
        const size_t bytes=end-i;
        for(const char *word:{"live","liveat","remix","mix","edit","acoustic","acoustique","instrumental","karaoke","demo","extended","part","pt","session","rehearsal","alternate","take","rerecorded","rerecording","feat","featuring","ft","vivo","directo","dub"})
            if(bytes==strlen(word)&&!memcmp(tag+i,word,bytes))return true;
        if((bytes>=4&&!memcmp(tag+i,"live",4)) || (bytes>=4&&!memcmp(tag+i,"feat",4)) || (bytes>=3&&!memcmp(tag+end-3,"mix",3)) || (bytes>=4&&!memcmp(tag+end-4,"edit",4)))return true;
        i=end;
    }
    for(const char *word:{"现场","現場","ライブ","アコースティック","カラオケ","混音","伴奏","纯音乐","純音樂","中文版","日文版","国语版","國語版","粤语版","粵語版"})if(strstr(tag,word))return true;
    if(strstr(tag,"version")&&!remaster_note(p,n))return true;
    return false;
}
bool strip_notes(const char *source,char (&dest)[160]){
    if(!source){dest[0]=0;return false;}
    const size_t length=strnlen(source,160);if(length>=159){dest[0]=0;return false;}
    size_t used=0;bool changed=false;
    for(size_t i=0;i<length;){const char *open=nullptr,*close=nullptr;
        for(const auto &pair:{std::pair<const char*,const char*>("(",")"),{"[","]"},{"{","}"},{"（","）"},{"【","】"},{"「","」"},{"〈","〉"},{"《","》"}}){size_t bytes=strlen(pair.first);if(bytes<=length-i&&!memcmp(source+i,pair.first,bytes)){open=pair.first;close=pair.second;break;}}
        if(open){size_t start=i+strlen(open);const char *end=strstr(source+start,close);
            if(end&&!version_note(source+start,size_t(end-(source+start)))){changed=true;i=size_t(end-source)+strlen(close);if(used&&dest[used-1]!=' ')dest[used++]=' ';continue;}}
        size_t separator=0;
        if(i && source[i]=='-' && ((source[i-1]==' ')||(i+1<length&&source[i+1]==' ')||remaster_note(source+i+1,length-i-1)))separator=1;
        for(const char *dash:{"–","—"})if(i&&strlen(dash)<=length-i&&!memcmp(source+i,dash,strlen(dash)))separator=strlen(dash);
        if(separator&&!version_note(source+i+separator,length-i-separator)){changed=true;break;}
        if(i && (source[i]==' '||source[i]==','||source[i]==':') && remaster_note(source+i+1,length-i-1)){changed=true;break;}
        dest[used++]=source[i++];
    }
    if(changed){while(used&&(dest[used-1]==' '||dest[used-1]=='\t'||dest[used-1]=='-'))--used;}
    dest[used]=0;if(!used){memcpy(dest,source,length+1);return false;}return changed;
}

bool stamp(const char *p,size_t n,uint32_t &ms){
    size_t i=0;unsigned minute=0,digits=0;
    while(i<n && p[i]>='0'&&p[i]<='9'){minute=minute*10+unsigned(p[i++]-'0');if(++digits>3)return false;}
    if(!digits || i+3>n || p[i++]!=':' || p[i]<'0'||p[i]>'5'||p[i+1]<'0'||p[i+1]>'9')return false;
    unsigned second=unsigned(p[i]-'0')*10+unsigned(p[i+1]-'0');i+=2;unsigned fraction=0,count=0;
    if(i<n){if(p[i]!='.'&&p[i]!=':')return false;++i;while(i<n && p[i]>='0'&&p[i]<='9'){fraction=fraction*10+unsigned(p[i++]-'0');if(++count>3)return false;}if(!count)return false;}
    if(i!=n)return false;while(count<3){fraction*=10;++count;}ms=(minute*60+second)*1000+fraction;return true;
}
bool literal(const char *p,size_t n,const char *key){size_t len=strlen(key);if(n!=len)return false;for(size_t i=0;i<n;++i)if(tolower(static_cast<unsigned char>(p[i]))!=key[i])return false;return true;}
bool signed_offset(const char *p,size_t n,int &offset){
    if(!n || n>7)return false;char b[8];memcpy(b,p,n);b[n]=0;char *end=nullptr;long value=strtol(b,&end,10);
    if(end!=b+n || value< -10000 || value>10000)return false;offset=int(value);return true;
}
bool append(Document &out,uint32_t time,const char *text,size_t bytes){
    if(out.count>=MaxLines || bytes>MaxLineBytes || size_t(out.used)+bytes+1>TextBytes)return false;
    auto &line=out.lines[out.count++];line.at=time;line.offset=out.used;memcpy(out.text+out.used,text,bytes);out.used+=bytes;out.text[out.used++]=0;return true;
}
}
bool valid_text(const char *text,size_t length,bool multiline){
    if(!text)return false;for(size_t at=0;at<length;){uint32_t cp;if(!decode(text,length,at,cp)||!allowed(cp,multiline))return false;}return true;
}
bool same_name(const char *a,const char *b){char x[160],y[160];return canonical(a,x)&&canonical(b,y)&&!strcmp(x,y);}
bool same_title(const char *a,const char *b){if(!a||!b)return false;if(same_name(a,b))return true;char x[160],y[160];strip_notes(a,x);strip_notes(b,y);return same_name(x,y);}
bool relaxed_track(const Track &source,Track &result){result=source;const bool title=strip_notes(source.title,result.title),album=strip_notes(source.album,result.album);return title||album;}
bool same_track(const Track &a,const Track &b){return a.duration==b.duration&&!strcmp(a.title,b.title)&&!strcmp(a.artist,b.artist)&&!strcmp(a.album,b.album);}
bool make_track(const s3::Snapshot &s,Track &out){
    if(s.input<0||s.input>1 || s.duration<1||s.duration>3600)return false;
    Track next;const char *sources[]={s.title,s.artist,s.album};char *dest[]={next.title,next.artist,next.album};
    for(unsigned i=0;i<3;++i){size_t n=strnlen(sources[i],160);if(n>=159 || (!n&&i<2) || !valid_text(sources[i],n))return false;memcpy(dest[i],sources[i],n+1);}
    next.duration=s.duration;out=next;return true;
}
bool parse_lrc(const char *source,size_t size,const Track &track,Document &out,bool signature){
    out.count=out.used=0;out.track=track;
    if(!source || !size || size>32768 || !valid_text(source,size,true))return false;
    bool haveTitle=false,haveArtist=false,haveAlbum=false,haveLength=false,haveOffset=false;int offset=0;unsigned nonempty=0;
    for(size_t at=(size>=3 && !memcmp(source,"\xef\xbb\xbf",3))?3:0;at<size;){
        size_t end=at;while(end<size && source[end]!='\n')++end;size_t stop=end;if(stop>at&&source[stop-1]=='\r')--stop;
        const char *p=source+at;size_t n=stop-at;at=end<size?end+1:end;
        uint32_t stamps[8];unsigned count=0;size_t cursor=0;
        while(cursor<n&&p[cursor]=='['){size_t close=cursor+1;while(close<n&&p[close]!=']')++close;if(close==n)return false;
            const char *tag=p+cursor+1;size_t bytes=close-cursor-1;uint32_t value;
            if(stamp(tag,bytes,value)){if(count>=8)return false;stamps[count++]=value;cursor=close+1;continue;}
            if(bytes && isdigit(static_cast<unsigned char>(tag[0])))return false;
            const char *colon=static_cast<const char*>(memchr(tag,':',bytes));
            if(colon){size_t key=colon-tag,amount=bytes-key-1;const char *v=colon+1;
                if(literal(tag,key,"offset")){int next;if(haveOffset || !signed_offset(v,amount,next))return false;offset=next;haveOffset=true;}
                else if(literal(tag,key,"length")){uint32_t duration;if(haveLength || !stamp(v,amount,duration) || labs(long(duration)-long(track.duration)*1000)>2000)return false;haveLength=true;}
                else if(literal(tag,key,"ti")||literal(tag,key,"ar")||literal(tag,key,"al")){
                    if(amount>=159)return false;char name[160];memcpy(name,v,amount);name[amount]=0;
                    if(literal(tag,key,"ti")){if(!same_title(name,track.title))return false;haveTitle=true;}
                    else if(literal(tag,key,"ar")){if(!same_name(name,track.artist))return false;haveArtist=true;}
                    else {if(track.album[0]&&!same_title(name,track.album))return false;haveAlbum=true;}
                }
            }
            cursor=close+1;
        }
        if(!count)continue;
        while(cursor<n&&(p[cursor]==' '||p[cursor]=='\t'))++cursor;while(n>cursor&&(p[n-1]==' '||p[n-1]=='\t'))--n;
        char line[MaxLineBytes+1];size_t written=0;
        while(cursor<n){
            if(p[cursor]=='<'){size_t close=cursor+1;while(close<n && close-cursor<18 && p[close]!='>')++close;uint32_t unused;
                if(close<n && p[close]=='>' && stamp(p+cursor+1,close-cursor-1,unused)){cursor=close+1;continue;}}
            if(written>=MaxLineBytes)return false;line[written++]=p[cursor]=='\t'?' ':p[cursor];++cursor;
        }
        line[written]=0;if(written)nonempty+=count;
        for(unsigned i=0;i<count;++i)if(stamps[i]>uint32_t(track.duration)*1000+2000 || !append(out,stamps[i],line,written))return false;
    }
    if(signature && (!haveLength || (track.album[0]&&!haveAlbum)))return false;
    (void)haveTitle;(void)haveArtist;
    if(out.count<2 || nonempty<2)return false;
    // Positive LRC offset advances lyric display. Sort repeated chorus tags;
    // stable equal-time order also preserves translations supplied together.
    for(unsigned i=0;i<out.count;++i){int64_t t=int64_t(out.lines[i].at)-offset;out.lines[i].at=t<0?0:uint32_t(t);}
    for(unsigned i=1;i<out.count;++i){Line value=out.lines[i];unsigned j=i;while(j && out.lines[j-1].at>value.at){out.lines[j]=out.lines[j-1];--j;}out.lines[j]=value;}
    unsigned result=0;bool distinct=false;
    for(unsigned i=0;i<out.count;++i){
        if(result && out.lines[result-1].at==out.lines[i].at){
            auto &previous=out.lines[result-1];const char *a=out.text+previous.offset,*b=out.text+out.lines[i].offset;
            if(!b[0] || !strcmp(a,b))continue;if(!a[0]){previous=out.lines[i];continue;}
            size_t x=strlen(a),y=strlen(b);if(x+y+1>MaxLineBytes || size_t(out.used)+x+y+2>TextBytes)return false;
            uint16_t joined=out.used;memcpy(out.text+out.used,a,x);out.used+=x;out.text[out.used++]='\n';memcpy(out.text+out.used,b,y+1);out.used+=y+1;previous.offset=joined;
        }else{if(result)distinct=true;out.lines[result++]=out.lines[i];}
    }
    out.count=result;return distinct && out.count>=2;
}
int current_line(const Document &document,uint64_t position){
    unsigned low=0,high=document.count;while(low<high){unsigned mid=(low+high)/2;if(document.lines[mid].at<=position)low=mid+1;else high=mid;}return low?int(low)-1:-1;
}
const char *provider_name(Provider p){return p==Provider::Lrclib?"LRCLIB":"LrcAPI";}
}
