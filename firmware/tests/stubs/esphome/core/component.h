#pragma once
#include <cstdint>
namespace esphome {
extern uint32_t test_millis;
inline uint32_t millis(){return test_millis;}
namespace setup_priority { constexpr float LATE=0; }
class Component { public: virtual void on_shutdown() {}
virtual void setup(){} virtual void loop(){} virtual void dump_config(){} virtual float get_setup_priority() const{return 0;} };
}
