#include <initializer_list>
#include "discovery_policy.h"
#include <assert.h>
#include <string>
#include <iostream>
using namespace s3;
int main() {
    for(const char *s:{"S3","FOSI-S3","Fosi Audio S3","FosiS3Stream1832"})assert(s3_model(s));
    for(const char *s:{"S5","S30","Fosi S35","Chromecast","Apple TV","Samsung S3",static_cast<const char*>(nullptr)})assert(!s3_model(s));
    assert(!s3_model(std::string(100,'s').c_str()));
    const char *id="00112233445566778899aabbccddeeff";
    assert(cast_id(id));assert(!cast_id("short"));assert(!cast_id("<script>not-a-device</script>"));
    assert(!cast_id(std::string(65,'a').c_str()));
    assert(local_candidate(0xc0000217,0xc0000250,0xffffff00));
    assert(!local_candidate(0xc6336417,0xc0000250,0xffffff00));
    assert(!local_candidate(0xc0000250,0xc0000250,0xffffff00));
    assert(!local_candidate(0xc00002ff,0xc0000250,0xffffff00));
    assert(!local_candidate(0x7f000001,0x7f000002,0xff000000));
    assert(!local_candidate(0xe0000001,0xe0000002,0xff000000));
    assert(!local_candidate(0xc0000217,0xc0000250,0));
    DiscoveryChoice c;c.consider("192.0.2.23",id,"");assert(c.unique());
    c.consider("192.0.2.23",id,"");assert(c.unique()); // Duplicate interfaces.
    c.consider("192.0.2.24","11112233445566778899aabbccddeeff","");assert(!c.unique());
    DiscoveryChoice moved;moved.consider("192.0.2.24","11112233445566778899aabbccddeeff",id);assert(!moved.unique());
    moved.consider("192.0.2.99",id,id);assert(moved.unique()); // DHCP moved, identity retained.
    assert(same_id(id,"00112233445566778899AABBCCDDEEFF"));
    std::cout<<"PASS: S3 model filtering, bounded identifiers, LAN-only candidates, duplicates/multiple devices and DHCP identity pinning\n";
}
