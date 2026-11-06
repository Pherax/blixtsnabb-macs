#pragma once
namespace esphome { namespace api {
struct APIServer { bool connected=false; bool is_connected_with_state_subscription() const{return connected;} };
inline APIServer *global_api_server=nullptr;
}}
