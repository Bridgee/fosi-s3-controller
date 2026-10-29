#include <initializer_list>
#include "s3_protocol.h"
#include <assert.h>
#include <cmath>
#include <string>
#include <iostream>

int main() {
    // Regression for upstream ArduinoJson #2220. Host sanitizers and the
    // compile-time minimum version prevent silently testing a stale package.
    for(size_t digits: {size_t(512),size_t(9000)}) {
        const std::string tiny="0."+std::string(digits,'0')+"1";
        DynamicJsonDocument doc(16384);
        assert(!deserializeJson(doc,"[\""+tiny+"\"]"));
        double number=doc[0].as<double>();
        // v6 float's narrow exponent can wrap; the regression is memory
        // safety, not accurate conversion of unrepresentable decimal values.
        assert(std::isnan(number) || (number>=0.0 && number<1.0));
        assert(doc[0].as<int>()==0);
        auto error=deserializeJson(doc,"["+tiny+"]");
        // v6 may reject an oversized numeric token before conversion.
        assert(!error || error==DeserializationError::InvalidInput);
        if(!error) {
            number=doc[0].as<double>();assert(std::isnan(number) || (number>=0.0 && number<1.0));
        }
        assert(!deserializeJson(doc,"{\"state\":\"playing\",\"status\":{\"duration\":\""+tiny+"\"}}"));
        s3::Snapshot s;
        assert(s3::parse(s3::Node::Player,doc.as<JsonVariantConst>(),s));
        assert(s.duration==0);
    }
    std::cout<<"PASS: patched ArduinoJson numeric-string underflow regression, both raw JSON and metadata duration\n";
}
