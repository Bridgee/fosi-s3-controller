#pragma once
// Copy to ai_local.h on your private machine ONLY when preparing a live test.
// This file never enables networking. ORBIT_AI_ENABLED stays0 by default.
// A separate explicit opt-in in config_local.h is required after authorization.
// Do not paste a key into browser JavaScript, prompts, logs or source archives.
#define ORBIT_AI_API_KEY ""
// Verified PEM root CA(s) for api.openai.com; no insecure fallback is provided.
// Obtain the appropriate CA chain independently before enabling live requests.
// The local tests intentionally do not contact OpenAI to inspect its certificate.
#define ORBIT_AI_CA_CERT ""
/* Portable PEM syntax: replace the empty macro above with verified public roots.
   Keep the backslash after each continued macro line and explicit newline escapes.

#define ORBIT_AI_CA_CERT \
    "-----BEGIN CERTIFICATE-----\n" \
    "...verified base64 data...\n" \
    "-----END CERTIFICATE-----\n"
*/
