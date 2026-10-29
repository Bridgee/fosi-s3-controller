#include "intro_ai_gate.h"
#if ORBIT_AI_ENABLED || ORBIT_AI_TRANSPORT_CHECK
namespace orbit_intro_async {NetworkGate &networkGate(){static NetworkGate gate;return gate;}}
#endif
