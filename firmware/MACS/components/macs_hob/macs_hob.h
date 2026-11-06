#pragma once
//
// Electrolux MACS hob interface for ESPHome.
// Reverse engineered from an IKEA BLIXTSNABB five-zone induction hob.
//
// Three explicit software modes:
//
//   LISTEN  decode only. Never transmits, for any reason, from any caller.
//   DEV     decode plus explicit one-shot injection. The touch panel stays
//           master and may command shutdown after injection. We cancel pending
//           tests on panel off, with no automatic retries, ACKs or provisioning.
//   MASTER  we are the panel. Requires allow_master: true in configuration.
//
// Commands are refused for the first few seconds after boot so that entity
// state restoration cannot put frames on the bus.
//
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace macs_hob {

// ---- protocol constants ----------------------------------------------------
static const uint8_t MACS_MARK = 0xC9;
static const uint8_t MACS_ACK = 0x98;

static const uint8_t ADDR_BROADCAST = 0x00;
static const uint8_t ADDR_STATUS = 0x0C;
static const uint8_t ADDR_PANEL = 0x2C;
static const uint8_t ADDR_GEN_A = 0x24;
static const uint8_t ADDR_GEN_B = 0x44;
static const uint8_t ADDR_GEN_C = 0x64;
static const uint8_t ADDR_GEN_D = 0x84;

static const uint8_t MSG_CONFIG = 0x21;
static const uint8_t MSG_COMMIT = 0x31;
static const uint8_t MSG_SETPOINT = 0xB2;
static const uint8_t MSG_STATUS = 0xB5;

static const uint16_t ENTRY_IDLE = 0x7E1;
static const uint16_t ENTRY_ABSENT = 0x000;
static const uint16_t GROUP_BOTTOM_LEFT = 0x7D1;
static const uint16_t GROUP_TOP_LEFT = 0x7D3;

static const uint8_t NUM_ZONES = 5;
static const uint8_t NUM_BOARDS = 4;

// Readings older than this report as unknown rather than as fact.
static const uint32_t STALE_MS = 60000;
// No frame may leave the device until this long after boot.
static const uint32_t BOOT_QUIET_MS = 5000;
// A frame we sent, seen back on the shared wire within this window, is an echo.
static const uint32_t ECHO_MS = 600;

static const uint8_t ZONE_ENTRY[NUM_ZONES] = {0, 2, 5, 6, 7};

static const uint16_t LEVEL_LEFT[11] = {ENTRY_IDLE, 0x008, 0x010, 0x01E, 0x02D,
                                        0x03C, 0x058, 0x082, 0x0B7, 0x11F, 0x190};
static const uint16_t LEVEL_RIGHT[11] = {ENTRY_IDLE, 0x00B, 0x020, 0x03C, 0x05A,
                                         0x078, 0x0B1, 0x104, 0x16F, 0x23F, 0x320};

struct BoardConfig {
  uint8_t addr;
  bool commit;
  uint8_t blocks[4][8];
};

// Lower value wins: a stop frame overtakes queued heating commands.
enum Priority : uint8_t { PRIO_STOP = 0, PRIO_CONFIG = 1, PRIO_SETPOINT = 2 };

struct PendingFrame {
  std::vector<uint8_t> data;
  uint32_t due;
  Priority prio;
  bool is_table;  // superseded by any later table
};

enum Mode : uint8_t { MODE_LISTEN = 0, MODE_DEV = 1, MODE_MASTER = 2, MODE_AUTO = 3 };

enum SafetyStop : uint8_t { STOP_NONE = 0, STOP_API_LOST, STOP_TIMEOUT, STOP_MANUAL };

class MacsHob : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // configuration
  void set_initial_mode(uint8_t m) { this->mode_ = (Mode) m; }
  void set_allow_master(bool v) { this->allow_master_ = v; }
  void set_max_level(uint8_t v) { this->max_level_ = v; }
  void set_command_timeout(uint32_t ms) { this->cmd_timeout_ = ms; }
  void set_require_api(bool v) { this->require_api_ = v; }
  void set_table_interval(uint32_t ms) { this->table_interval_ = ms; }

  // mode
  bool set_mode(Mode m);  // false if refused
  bool set_mode_by_name(const std::string &s);
  Mode mode() const { return this->mode_; }
  std::string mode_name() const;
  bool transitioning() const { return this->transitioning_; }
  int commanded_level(uint8_t z) const { return this->mode_ == MODE_MASTER && z < NUM_ZONES ? this->levels_[z] : -1; }
  bool commanded_on() const { return this->hob_on_; }
  uint8_t max_level() const { return this->max_level_; }

  // commands. All refuse in LISTEN and during the boot quiet window.
  void set_hob_on(bool on);
  void set_zone_level(uint8_t zone, uint8_t level);
  void stop_all();  // clears the queue, then sends zeros
  void send_probe();

  void reset_fault();
  bool ready() const;
  bool faulted() const { return !fault_.empty(); }
  int pan_timeout_remaining(uint8_t z) const;
  void set_pan_timeout(uint32_t ms) { pan_timeout_ = ms; }
  void set_comm_timeout(uint32_t ms) { comm_timeout_ = ms; }
  void on_shutdown() override;

  // observed state
  bool observed_hob_on() const;
  int8_t observed_hob_state() const;  // -1 unknown, 0 off, 1 on
  int8_t observed_level(uint8_t z) const;
  std::string observed_setting(uint8_t z) const;
  std::string observed_raw(uint8_t z) const;
  int8_t zone_pan(uint8_t z) const;
  int8_t zone_hot(uint8_t z) const;
  bool board_present(uint8_t i) const;
  bool board_provisioned(uint8_t i) const;
  bool all_boards_provisioned() const;
  std::string status_text() const;

 protected:
  void handle_frame_(const uint8_t *f, size_t total);
  void handle_status_(const uint8_t *p, uint8_t src);
  void decode_table_(const uint8_t *p);
  bool is_echo_(const uint8_t *f, size_t total);
  void send_ack_(uint8_t checksum);

  void queue_(std::vector<uint8_t> frame, uint32_t delay_ms, Priority prio, bool is_table);
  void drop_superseded_tables_();
  void transmit_(const std::vector<uint8_t> &frame);
  bool may_transmit_(const char *what) const;

  static uint8_t xor_of_(const uint8_t *d, size_t n);
  static uint16_t crc_ccitt_(const uint8_t *d, size_t n);
  static std::string hex_(const uint8_t *d, size_t n);
  static std::string describe_(const uint8_t *f, size_t total);
  bool fresh_(uint32_t stamp) const { return stamp != 0 && millis() - stamp <= STALE_MS; }
  const BoardConfig *board_for_(uint8_t addr) const;
  int8_t board_index_(uint8_t addr) const;

  std::vector<uint8_t> build_table_(uint8_t base);
  std::vector<uint8_t> build_state_();
  std::vector<uint8_t> build_config_(const BoardConfig &b, uint8_t idx);
  std::vector<uint8_t> build_commit_(uint8_t addr);
  bool build_injected_(uint8_t entry, uint16_t raw, std::vector<uint8_t> &out);
  bool build_injected_off_(uint8_t base, std::vector<uint8_t> &out);
  uint16_t entry_value_(uint8_t entry) const;

  void provision_(uint8_t addr);
  void push_tables_(uint32_t delay_ms, Priority prio);
  void service_master_();
  void check_safety_();

  void begin_master_();
  void latch_fault_(const char *reason);
  void handle_ack_(uint8_t checksum);
  void service_transaction_();
  void complete_transaction_();
  void cancel_transaction_();
  bool status_fresh_() const;
  bool observe_counter_(uint8_t src, uint16_t counter);
  void check_pan_timers_();
  std::string fault_;
  bool auto_enabled_{false}, auto_pending_{false}, panel_detected_{false};
  uint32_t detect_at_{0};
  bool ready_latched_{false};
  bool initial_provision_started_{false};
  uint32_t startup_at_{0}, on_at_{0};
  bool provisioning_[NUM_BOARDS]{};
  uint8_t config_step_[NUM_BOARDS]{};
  bool refreshing_[NUM_BOARDS]{};
  uint32_t refresh_started_[NUM_BOARDS]{};
  bool counter_seen_[NUM_BOARDS]{};
  uint16_t board_counter_[NUM_BOARDS]{};

  bool table_acked_[2]{};
  bool status_valid_[2]{};
  bool waiting_ack_{false};
  std::vector<uint8_t> inflight_;
  uint32_t ack_started_{0};
  uint8_t attempts_{0};
  uint32_t ack_guard_until_{0};
  uint32_t pan_timeout_{60000}, comm_timeout_{12000};
  bool pan_timing_[NUM_ZONES]{};
  uint32_t pan_since_[NUM_ZONES]{};
  bool unknown_timing_[NUM_ZONES]{};
  uint32_t unknown_since_[NUM_ZONES]{};
  std::string last_event_;

  std::vector<uint8_t> rx_;
  uint32_t last_rx_{0};
  uint32_t last_rx_byte_{0};

  std::deque<PendingFrame> tx_;
  std::deque<std::pair<std::vector<uint8_t>, uint32_t>> sent_;
  uint32_t last_tx_{0};
  uint32_t boot_at_{0};

  Mode mode_{MODE_LISTEN};
  bool allow_master_{false};
  bool transitioning_{false};
  Mode pending_mode_{MODE_LISTEN};
  bool hob_on_{false};
  uint8_t levels_[NUM_ZONES]{0, 0, 0, 0, 0};
  uint32_t last_command_{0};
  SafetyStop last_stop_{STOP_NONE};

  bool observed_on_{false};
  uint32_t observed_on_seen_{0};
  int8_t obs_level_[NUM_ZONES]{-1, -1, -1, -1, -1};
  int16_t obs_raw_[NUM_ZONES]{-1, -1, -1, -1, -1};
  uint32_t level_seen_[NUM_ZONES]{0, 0, 0, 0, 0};
  int8_t pan_[NUM_ZONES]{-1, -1, -1, -1, -1};
  int8_t hot_[NUM_ZONES]{-1, -1, -1, -1, -1};
  int16_t status_[NUM_ZONES]{-1, -1, -1, -1, -1};
  int16_t status_second_[NUM_ZONES]{-1, -1, -1, -1, -1};
  uint32_t status_seen_[NUM_ZONES]{0, 0, 0, 0, 0};
  uint32_t board_seen_[NUM_BOARDS]{0, 0, 0, 0};
  bool board_provisioned_[NUM_BOARDS]{false, false, false, false};
  uint8_t obs_table_[2][16]{};
  uint32_t obs_table_seen_[2]{0, 0};
  uint16_t mains_counter_{0};

  uint32_t next_table_{0};
  uint32_t next_state_{0};
  uint32_t api_lost_since_{0};

  uint8_t max_level_{9};
  uint32_t cmd_timeout_{1800000};
  uint32_t table_interval_{4000};
  bool require_api_{true};
};

}  // namespace macs_hob
}  // namespace esphome
