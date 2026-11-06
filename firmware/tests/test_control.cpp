#include "macs_hob.h"
#include <cassert>
#include <iostream>
#include "boot_fixture.h"
namespace esphome { uint32_t test_millis=1; }
using namespace esphome; using namespace esphome::macs_hob;
struct Hob:MacsHob {
using MacsHob::build_config_;using MacsHob::board_for_;using MacsHob::xor_of_;using MacsHob::crc_ccitt_;using MacsHob::rx_;
using MacsHob::waiting_ack_;using MacsHob::inflight_;using MacsHob::board_provisioned_;using MacsHob::provision_;using MacsHob::tx_;using MacsHob::build_table_;using MacsHob::hob_on_;using MacsHob::handle_frame_;using MacsHob::ready_latched_;using MacsHob::handle_status_;using MacsHob::status_seen_;using MacsHob::table_acked_;using MacsHob::initial_provision_started_;using MacsHob::pan_;using MacsHob::pan_since_;using MacsHob::pan_timing_;using MacsHob::check_pan_timers_;using MacsHob::check_safety_;using MacsHob::cancel_transaction_;using MacsHob::handle_ack_;
using MacsHob::refreshing_; using MacsHob::refresh_started_; using MacsHob::observe_counter_; using MacsHob::board_counter_; using MacsHob::counter_seen_;
void request(uint8_t src,uint16_t c){std::vector<uint8_t> f={0xC9,0x2C,src,4,0x21,2,uint8_t(c>>8),uint8_t(c)};f.push_back(xor_of_(f.data(),f.size()));handle_frame_(f.data(),f.size());}
void init(Mode m=MODE_MASTER){test_millis=1;set_initial_mode(m);set_allow_master(true);set_require_api(false);setup();}
void tick(uint32_t ms=20){test_millis+=ms;loop();}
void ack(){if(waiting_ack_){auto x=inflight_.back();input.insert(input.end(),{0x98,x,uint8_t(0x98^x)});}}
void left(uint8_t a,uint8_t b,uint8_t c,uint8_t d){uint8_t p[]={0xB5,3,3,a,b,c,d,0,1};handle_status_(p,0x24);}
void status(uint8_t a=5,uint8_t b=5,uint8_t c=5){uint8_t p[]={0xB5,3,3,0,a,b,c,0,1};handle_status_(p,0x24);handle_status_(p,0x64);}
void simulate_ready(){init();test_millis=6000;cancel_transaction_();tx_.clear();for(auto &v:board_provisioned_)v=true;table_acked_[0]=table_acked_[1]=true;initial_provision_started_=true;ready_latched_=true;status();}
void panel(){bool old=hob_on_;hob_on_=false;auto f=build_table_(0);hob_on_=old;handle_frame_(f.data(),f.size());}
};
int main(){
// No heat before ACK-confirmed startup.
{Hob h;h.init();h.tick(6000);h.set_hob_on(true);h.set_zone_level(3,5);assert(!h.commanded_on());assert(h.commanded_level(3)==0);}
// Entire configuration through real loop/parser, with simulated board ACKs.
{Hob h;h.init();for(int i=0;i<600;++i){h.ack();if(i%100==0)h.status();h.tick();}assert(h.ready());for(auto v:h.board_provisioned_)assert(v);h.set_hob_on(true);h.set_zone_level(3,4);assert(h.commanded_level(3)==4);}
// Wrong ACK doesn't complete; missing ACK bounded then latched fault.
{Hob h;h.init();h.tick(6000);assert(h.waiting_ack_);h.handle_ack_(uint8_t(h.inflight_.back()^1));assert(h.waiting_ack_);for(int i=0;i<40;++i)h.tick();assert(h.faulted());assert(!h.commanded_on());h.set_hob_on(true);assert(!h.commanded_on());}
// Duplicate announces do not enqueue a configuration train.
{Hob h;h.init();h.provision_(0x24);auto n=h.tx_.size();for(int i=0;i<30;i++)h.provision_(0x24);assert(h.tx_.size()==n);}
// No-pan timeout is independent per zone, retains other levels, no restart.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(2,3);h.set_zone_level(3,2);h.status(5,0,5);h.check_pan_timers_();test_millis+=59999;h.status(5,0,5);h.check_pan_timers_();assert(h.commanded_level(3)==2);test_millis++;h.status(5,0,5);h.check_pan_timers_();assert(h.commanded_level(3)==0);assert(h.commanded_level(2)==3);h.status();h.check_pan_timers_();assert(h.commanded_level(3)==0);}
// Pan return resets timer; changing level does not evade timeout.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(3,2);h.status(5,0,5);h.check_pan_timers_();test_millis+=40000;h.status();h.check_pan_timers_();h.status(5,0,5);h.check_pan_timers_();test_millis+=30000;h.status(5,0,5);h.set_zone_level(3,3);h.check_pan_timers_();assert(h.commanded_level(3)==3);test_millis+=30000;h.status(5,0,5);h.check_pan_timers_();assert(h.commanded_level(3)==0);}
// Unknown and stale do not masquerade as no-pan.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(3,2);h.pan_[3]=-1;h.check_pan_timers_();test_millis+=12000;h.status_seen_[3]=test_millis;h.check_pan_timers_();assert(h.faulted());}
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(3,2);test_millis+=12001;h.check_safety_();assert(h.faulted());assert(h.commanded_level(3)==0);}
// Auto never sends before decision; detects original panel and remains Dev.
{Hob h;h.init(MODE_AUTO);h.tick(30000);assert(h.output.empty());assert(h.mode_name()=="Auto");h.panel();assert(h.mode()==MODE_DEV);h.tick(60000);assert(h.mode()==MODE_DEV);assert(h.output.empty());assert(!h.set_mode(MODE_MASTER));}
// Auto with no panel chooses Master OFF; late panel yields without OFF injection.
{Hob h;h.init(MODE_AUTO);h.tick(35000);assert(h.mode()==MODE_MASTER);assert(!h.commanded_on());h.panel();assert(h.mode()==MODE_DEV);assert(h.tx_.empty());assert(!h.waiting_ack_);}
// Listen never transmits.
{Hob h;h.init(MODE_LISTEN);h.tick(6000);h.set_hob_on(true);h.stop_all();h.tick();assert(h.output.empty());}
// Explicit reset clears fault but doesn't restore requested heat.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(3,5);test_millis+=12001;h.check_safety_();h.reset_fault();assert(!h.faulted());assert(!h.ready());assert(!h.commanded_on());assert(h.commanded_level(3)==0);}
// Compare captured configuration bytes directly against generated firmware frames.
{Hob h;h.init(MODE_LISTEN);assert(boot_frames.size()==156);unsigned configs=0;
 for (auto &f:boot_frames) {assert(h.xor_of_(f.data(),f.size())==0);
  if(f[4]==0x21&&f[5]==3){auto b=h.board_for_(f[1]);assert(b);assert(h.build_config_(*b,f[6]-0x21)==f);configs++;}
  if(f[4]==0xb2&&f[5]==0){assert(h.crc_ccitt_(f.data()+4,16)==((f[20]<<8)|f[21]));}
 }
 assert(configs==64);
 for(auto &chunk:boot_chunks) for(auto byte:chunk){h.input.push_back(byte);h.tick(1);}
 assert(h.rx_.empty());assert(h.output.empty());
}
// Captured forward configuration request is refreshed without clearing heating.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(0,1);
 h.observe_counter_(0x24,0x09E2);h.request(0x24,0x0A6A);
 assert(!h.faulted()&&h.refreshing_[0]&&!h.ready());assert(h.commanded_level(0)==1);
 h.set_zone_level(0,2);assert(h.commanded_level(0)==1);
 for(int i=0;i<150;i++){h.ack();h.tick();}
 assert(!h.faulted()&&h.ready()&&!h.refreshing_[0]);assert(h.commanded_level(0)==1);
}
// Regression faults; rollover does not.
{Hob h;h.simulate_ready();h.observe_counter_(0x24,0x09E2);h.request(0x24,0x0011);
 assert(h.faulted());assert(!h.commanded_on());}
{Hob h;h.simulate_ready();h.board_counter_[0]=0xFFF0;
 assert(h.observe_counter_(0x24,0x0010));assert(!h.faulted());}
// Duplicate requests cannot extend the refresh deadline.
{Hob h;h.simulate_ready();h.request(0x24,2);auto start=h.refresh_started_[0];
 test_millis+=7000;h.request(0x24,3);assert(h.refresh_started_[0]==start);
 test_millis+=1001;h.check_safety_();assert(h.faulted());}
// Missing continuity, stale status, and failed refresh ACK remain fail-closed.
{Hob h;h.simulate_ready();h.request(0x44,2);assert(h.faulted());}
{Hob h;h.simulate_ready();test_millis+=12001;h.request(0x24,2);assert(h.faulted());}
{Hob h;h.simulate_ready();h.request(0x24,2);for(int i=0;i<100;i++)h.tick();assert(h.faulted());}
// Stop during refresh cancels configuration and never restores heat.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(0,1);h.request(0x24,2);
 h.stop_all();assert(!h.commanded_on()&&!h.ready());assert(h.commanded_level(0)==0);
 for(int i=0;i<100;i++){h.ack();h.tick();}assert(!h.commanded_on()&&!h.ready());}
// Dev observes configuration requests without responding.
{Hob h;h.init(MODE_DEV);test_millis=6000;h.request(0x24,2);h.tick();assert(h.output.empty());}
// Stop cancels a pending heating retry; only OFF remains queued.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(3,5);for(int i=0;i<20&&!h.waiting_ack_;i++)h.tick();
 h.stop_all();assert(!h.waiting_ack_);for(auto &f:h.tx_)if(f.data.size()==23)assert(f.data[6]==0);
}
// Explicit Listen is respected even after Auto originally selected Master.
{Hob h;h.init(MODE_AUTO);h.tick(35000);assert(h.set_mode(MODE_LISTEN));
 for(int i=0;i<100;i++){h.ack();h.tick();}assert(h.mode()==MODE_LISTEN);h.panel();assert(h.mode()==MODE_LISTEN);
}

// Replay observed upward/downward pan sweep through the actual frame parser.
{Hob h;h.init(MODE_DEV);
 const uint8_t sweep[][4]={{5,0,0,0},{5,5,0,0},{0,5,0,0},{0,5,5,0},
 {0,0,5,0},{0,0,5,5},{0,0,0,5},{0x10,0x10,5,5},{0,5,5,5},
 {0,5,5,0},{0,5,0,0},{5,5,0,0},{5,0,0,0}};
 const int expected[][2]={{1,0},{1,0},{1,0},{1,1},{0,1},{0,1},{0,1},
 {0,1},{1,1},{1,1},{1,0},{1,0},{1,0}};
 for(unsigned i=0;i<13;i++){
  const auto &v=sweep[i];
  std::vector<uint8_t> f={0xC9,0x0C,0x24,9,0xB5,3,3,v[0],v[1],v[2],v[3],0,1};
  f.push_back(h.xor_of_(f.data(),f.size()));
  h.input.insert(h.input.end(),f.begin(),f.end());h.tick();
  assert(h.zone_pan(0)==expected[i][0]);assert(h.zone_pan(1)==expected[i][1]);
 }
 assert(h.observed_raw(0).find("status=05/00")!=std::string::npos);
}
// Unknown section is not absence; known presence wins; heat survives pan removal.
{Hob h;h.init(MODE_DEV);h.left(0,2,5,2);
 assert(h.zone_pan(0)==-1);assert(h.zone_pan(1)==1);
 h.left(0x80,0x85,0,0x80);
 assert(h.zone_hot(0)==1&&h.zone_pan(0)==1);
 assert(h.zone_hot(1)==1&&h.zone_pan(1)==0);
 h.left(0x80,0x80,0,0);assert(h.zone_pan(0)==0&&h.zone_hot(0)==1);
}
// Left-zone timer: moving within pair preserves presence; other zone independent.
{Hob h;h.simulate_ready();h.set_hob_on(true);h.set_zone_level(0,1);h.set_zone_level(1,1);
 h.left(5,0,0,5);h.check_pan_timers_();
 test_millis+=30000;h.left(0,5,5,0);h.check_pan_timers_();
 assert(!h.pan_timing_[0]&&!h.pan_timing_[1]);
 h.left(0,0,5,0);h.check_pan_timers_();
 test_millis+=59999;h.left(0,0,0,5);h.check_pan_timers_();
 assert(h.commanded_level(0)==1);
 test_millis++;h.left(0,0,0,5);h.check_pan_timers_();
 assert(h.commanded_level(0)==0&&h.commanded_level(1)==1);
 h.left(5,0,5,0);h.check_pan_timers_();assert(h.commanded_level(0)==0);
}
// Right generator remains unchanged, including heat and unknown state.
{Hob h;h.init(MODE_DEV);uint8_t p[]={0xB5,3,3,0x10,0x85,0x10,2,0,1};
 h.handle_status_(p,0x64);
 assert(h.zone_pan(2)==1&&h.zone_hot(2)==1);
 assert(h.zone_pan(3)==0&&h.zone_pan(4)==-1);
}
// R5: replay standby timeout boundary: late status recovers without reset.
{Hob h;h.simulate_ready();test_millis+=12001;h.check_safety_();
 assert(!h.faulted()&&!h.ready()&&!h.commanded_on());
 assert(h.status_text().find("standby")!=std::string::npos);
 h.set_hob_on(true);h.set_zone_level(3,1);
 assert(!h.commanded_on()&&h.commanded_level(3)==0);
 test_millis+=111;h.status();h.check_safety_();
 assert(h.ready()&&!h.faulted()&&!h.commanded_on());
 h.set_hob_on(true);assert(h.commanded_on());}
// Persistent standby silence never enables heat; OFF maintenance continues.
{Hob h;h.simulate_ready();test_millis+=60000;h.check_safety_();
 assert(!h.faulted()&&!h.ready());h.set_hob_on(true);assert(!h.commanded_on());
 for(int i=0;i<100;i++){h.ack();h.tick();}
 assert(!h.faulted()&&!h.ready());
 for(auto &f:h.tx_)if(f.data.size()==23)assert(f.data[6]==0);}
// ON with zero levels still requires uninterrupted status, and faults stay latched.
{Hob h;h.simulate_ready();h.set_hob_on(true);test_millis+=12001;h.check_safety_();
 assert(h.faulted()&&!h.commanded_on());h.status();h.check_safety_();
 assert(h.faulted()&&!h.ready());}
std::cout<<"31 control scenarios passed\n";
}
