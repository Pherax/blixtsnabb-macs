#pragma once
#include <vector>
#include <deque>
#include <cstdint>
#include <cstddef>
namespace esphome { namespace uart {
constexpr int UART_CONFIG_PARITY_EVEN=0;
class UARTDevice { public:
uint32_t write_delay_ms=0;
std::deque<uint8_t> input; std::vector<std::vector<uint8_t>> output;
int available(){return input.size();}
bool read_byte(uint8_t *v){if(input.empty())return false;*v=input.front();input.pop_front();return true;}
void write_array(const uint8_t*d,size_t n){output.emplace_back(d,d+n); esphome::test_millis += write_delay_ms;}
void flush() {}
void check_uart_settings(int,int,int,int){}
};}}
