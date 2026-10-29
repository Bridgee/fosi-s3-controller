#include "wifi_recovery.h"
#include <assert.h>
#include <stdio.h>
int main() {
    s3::WifiRecovery r;r.connected(100);
    assert(!r.due(20099,false));assert(r.due(20100,false));
    assert(!r.due(20100,true));assert(!r.due(60099,true));assert(r.due(60100,true));
    r.attempted(20100);assert(!r.due(60099,false));assert(r.due(60100,false));
    r.attempted(60100);assert(r.interval(false)==60000);assert(r.due(120100,false));
    for(int i=0;i<10;++i)r.attempted(120100+i*60000);
    assert(r.interval(false)==60000);r.connected(900000);assert(r.interval(false)==20000);
    r.connected(0xffffff00u);assert(!r.due(0xffffff00u+19999u,false));assert(r.due(0xffffff00u+20000u,false));
    puts("PASS: stalled Wi-Fi retry backoff, DHCP grace, reset after connection, cap and tick wrap");
}
