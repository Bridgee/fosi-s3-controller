#include "lyrics_protocol.h"
#include "metadata_text.h"
#include <assert.h>
#include <memory>
#include <string>
#include <stdio.h>
using namespace orbit_lyrics;
int main(){
    std::unique_ptr<Document> d(new Document());Track track;strcpy(track.title,"Test Song");strcpy(track.artist,"Test Artist");strcpy(track.album,"Test Album");track.duration=180;
    auto parse=[&](const std::string &lrc,bool signature=false){return parse_lrc(lrc.data(),lrc.size(),track,*d,signature);};
    assert(parse("[00:01.00]First test line\n[00:02.123]Second test line\n[00:03]\n"));assert(d->count==3 && d->lines[1].at==2123);
    assert(current_line(*d,0)==-1 && current_line(*d,1000)==0 && current_line(*d,2122)==0 && current_line(*d,2123)==1 && current_line(*d,999999)==2);
    assert(parse("[00:10][00:01]Repeat line\n[00:02]Other line"));assert(d->count==3 && d->lines[0].at==1000 && d->lines[2].at==10000);
    assert(parse("[00:01]Original\n[00:01]翻译\n[00:02]Next"));assert(d->count==2 && !strcmp(d->text+d->lines[0].offset,"Original\n翻译"));
    assert(parse("[00:01]Repeat\n[00:01]Repeat\n[00:02]Next") && d->count==2);
    assert(parse("[offset:500]\n[00:01]First\n[00:02]Next") && d->lines[0].at==500);
    assert(parse("[offset:-500]\n[00:01]First\n[00:02]Next") && d->lines[0].at==1500);
    assert(parse("[00:01]<00:01.0>Word <00:01.5>times\n[00:02]Next") && !strcmp(d->text+d->lines[0].offset,"Word times"));
    assert(parse("\xef\xbb\xbf[ti:Test Song]\r\n[ar:Test Artist]\r\n[00:01]日本語の歌詞\r\n[00:02]中文歌词"));
    for(const std::string &bad:{"plain lyrics", "[00:99]Bad\n[00:02]Next", "[00:01.0000]Bad\n[00:02]Next", "[00:01]Only one", "[00:00]A\n[00:00]B", "[99:01]Beyond length\n[00:02]Next", "[00:01Unclosed\n[00:02]Next", "[offset:50000]\n[00:01]A\n[00:02]B", "[ti:Wrong song]\n[00:01]A\n[00:02]B"})assert(!parse(bad));
    std::string longLine="[00:01]"+std::string(MaxLineBytes+1,'x')+"\n[00:02]Next";assert(!parse(longLine));
    std::string many;for(unsigned i=0;i<MaxLines+1;++i){char line[64];snprintf(line,sizeof(line),"[%02u:%02u]Sample line\n",i/60,i%60);many+=line;}track.duration=360;assert(!parse(many));track.duration=180;
    assert(!parse(std::string("[00:01]A\0B\n[00:02]Next",23)));assert(!parse("[00:01]\xc0\x80\n[00:02]Next"));assert(!parse("[00:01]\xe2\x80\xae\n[00:02]Next"));
    assert(!parse("[00:01]A\n[00:02]B",true));
    assert(parse("[length:03:00]\n[al:Test Album]\n[00:01]A\n[00:02]B",true));
    assert(!parse("[length:03:00]\n[00:01]A\n[00:02]B",true));assert(!parse("[length:04:00]\n[al:Test Album]\n[00:01]A\n[00:02]B",true));
    assert(same_name("  TEST   Song ","test song"));assert(same_name("Cafe\xcc\x81","Café"));assert(!same_name("Song (Live)","Song"));assert(!same_name("Song - Remix","Song"));
    assert(same_title("Song (Remastered 2009)","song"));assert(same_title("Song（2009 Remaster）","Song"));
    assert(same_title("Song [Digitally Remastered 2011]","Song"));assert(same_title("Song（2009年リマスター）","Song"));
    assert(same_title("Song - Remastered 2009","Song"));assert(same_title("Song – Deluxe Edition","Song"));
    assert(same_title("Song [Bonus Track]","Song"));assert(same_title("Song {2020 reissue}","Song"));
    assert(same_title("Song Remastered 2009","Song"));assert(same_title("Song 「édition anniversaire」","Song"));
    assert(!same_title("Song (Japanese Version)","Song"));assert(!same_title("Song - Live","Song"));
    assert(!same_title("Song (Live)","Song"));assert(!same_title("Song (Live Remastered 2009)","Song"));assert(!same_title("Song (Remix)","Song"));
    Track tagged=track;strcpy(tagged.title,"Test Song (Remastered 2009)");strcpy(tagged.album,"Test Album（2009 Remaster）");Track stripped;
    assert(relaxed_track(tagged,stripped)&&!strcmp(stripped.title,"Test Song")&&!strcmp(stripped.album,"Test Album")&&!strcmp(stripped.artist,tagged.artist));
    s3::Snapshot source;source.input=0;source.duration=180;strcpy(source.title,"Test Song");strcpy(source.artist,"Test Artist");strcpy(source.album,"Test Album");Track next;assert(make_track(source,next)&&same_track(next,track));source.input=2;assert(!make_track(source,next));source.input=0;source.duration=3601;assert(!make_track(source,next));
    DynamicJsonDocument doc(4096);doc["trackName"]="Test Song";doc["artistName"]="Test Artist";doc["albumName"]="Test Album";doc["duration"]=180;doc["instrumental"]=false;doc["syncedLyrics"]="[00:01]A\n[00:02]B";
    assert(lrclib_record(doc.as<JsonVariantConst>(),track,*d));doc["duration"]=182;assert(lrclib_record(doc.as<JsonVariantConst>(),track,*d));doc["duration"]=182.01;assert(!lrclib_record(doc.as<JsonVariantConst>(),track,*d));doc["duration"]=true;assert(!lrclib_record(doc.as<JsonVariantConst>(),track,*d));doc["duration"]=180;
    for(const char *key:{"trackName","artistName","albumName"}){const std::string old=doc[key].as<std::string>();doc[key]="Different";assert(!lrclib_record(doc.as<JsonVariantConst>(),track,*d));doc[key]=old;}
    doc["instrumental"]=true;assert(!lrclib_record(doc.as<JsonVariantConst>(),track,*d));doc["instrumental"]=false;doc["syncedLyrics"]=nullptr;assert(!lrclib_record(doc.as<JsonVariantConst>(),track,*d));
    doc.clear();deserializeJson(doc,R"({"trackName":"Test Song\u0000bad"})");assert(!json_name(doc["trackName"],track.title));
    doc.clear();auto a=doc.to<JsonArray>();auto first=a.createNestedObject();first["title"]="Test Song";first["artist"]="Test Artist";first["lyrics"]="[length:03:00]\n[al:Test Album]\n[00:01]Original\n[00:02]Next";
    assert(lrcapi_records(doc.as<JsonVariantConst>(),track,*d));auto second=a.createNestedObject();second["title"]="Test Song";second["artist"]="Test Artist";second["lyrics"]="invalid";assert(lrcapi_records(doc.as<JsonVariantConst>(),track,*d)&&!strcmp(d->text+d->lines[0].offset,"Original"));
    second["lyrics"]=first["lyrics"];assert(!lrcapi_records(doc.as<JsonVariantConst>(),track,*d));
    std::string longText(200,'x');longText+=" Cafe\xcc\x81";char composed[241];assert(orbit_metadata::compose_lyric(composed,sizeof(composed),longText.data(),longText.size())==206 && strlen(composed)==206);
    puts("PASS: strict metadata/duration/version matching; bounded UTF-8/LRC timestamps, offset, duplicates/translations, word-tag stripping, fallback ambiguity and binary timeline lookup");
}
