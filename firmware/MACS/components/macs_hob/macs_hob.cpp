#include "macs_hob.h"
#include "esphome/core/hal.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome {
namespace macs_hob {

static const char *const TAG = "macs_hob";
static const char *const REVISION = "macs-r5-20261029";

// Board provisioning data, byte-identical across three cold boots 2026-09-18.
static const BoardConfig BOARDS[NUM_BOARDS] = {
    {ADDR_GEN_A, true,
     {{0xD0, 0xAA, 0x6C, 0xD2, 0xCD, 0x1E, 0x4B, 0x73},
      {0xD2, 0xCD, 0x1E, 0x4B, 0x73, 0x0B, 0x98, 0x98},
      {0x7D, 0xBF, 0xB8, 0x1E, 0x44, 0x33, 0x33, 0x64},
      {0x55, 0x10, 0x10, 0x00, 0x0C, 0x00, 0x0A, 0x6A}}},
    {ADDR_GEN_B, false,
     {{0xD0, 0xAA, 0x6C, 0xD2, 0xCD, 0x1E, 0x4B, 0x73},
      {0xD2, 0xCD, 0x1E, 0x4B, 0x73, 0x0B, 0x98, 0x98},
      {0x7D, 0xBF, 0xB8, 0x1E, 0x44, 0x33, 0x33, 0x64},
      {0x55, 0x10, 0x10, 0x00, 0x0C, 0x00, 0x0A, 0x6A}}},
    {ADDR_GEN_C, true,
     {{0x90, 0xAA, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00},
      {0xD2, 0xCF, 0x1A, 0x4B, 0xE6, 0x0B, 0xAE, 0x05},
      {0x3D, 0xBF, 0xB8, 0x1E, 0x66, 0x33, 0x33, 0x44},
      {0x70, 0x00, 0x20, 0x00, 0x00, 0x00, 0xCD, 0x8A}}},
    {ADDR_GEN_D, false,
     {{0x98, 0xAA, 0x6C, 0xCD, 0xC8, 0x0E, 0x70, 0xE6},
      {0xCD, 0xC8, 0x0E, 0x70, 0xE6, 0x0B, 0x1E, 0x36},
      {0x7D, 0xBF, 0xF8, 0x1E, 0x66, 0x33, 0x33, 0x64},
      {0x77, 0x20, 0x20, 0x00, 0x00, 0x00, 0xC8, 0xFA}}},
};

// ---------------------------------------------------------------------------
// helpers

uint8_t MacsHob::xor_of_(const uint8_t *d, size_t n) {
  uint8_t x = 0;
  for (size_t i = 0; i < n; i++)
    x ^= d[i];
  return x;
}

// CRC-16/CCITT, poly 0x1021, MSB-first, init 0x0000, xorout 0x235F.
uint16_t MacsHob::crc_ccitt_(const uint8_t *d, size_t n) {
  uint16_t c = 0;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t) d[i] << 8;
    for (int b = 0; b < 8; b++)
      c = (c & 0x8000) ? (uint16_t) ((c << 1) ^ 0x1021) : (uint16_t) (c << 1);
  }
  return c ^ 0x235F;
}

std::string MacsHob::hex_(const uint8_t *d, size_t n) {
  std::string s;
  char b[4];
  for (size_t i = 0; i < n; i++) {
    snprintf(b, sizeof(b), "%02X", d[i]);
    if (i)
      s += ' ';
    s += b;
  }
  return s;
}

std::string MacsHob::describe_(const uint8_t *f, size_t total) {
  (void) total;
  const uint8_t dst = f[1], src = f[2], len = f[3];
  const uint8_t *p = f + 4;
  char b[96];
  if (len == 18 && p[0] == MSG_SETPOINT && p[1] == 0x00) {
    snprintf(b, sizeof(b), "setpoint table, entries %u-%u, hob %s", p[4] >> 4,
             (p[4] >> 4) + 5, p[2] == 0x0B ? "on" : "off");
  } else if (len >= 4 && p[0] == MSG_SETPOINT && p[1] == 0x02) {
    snprintf(b, sizeof(b), "setpoint query from 0x%02X (ctr 0x%02X%02X)", src, p[2], p[3]);
  } else if (len >= 3 && p[0] == MSG_SETPOINT && p[1] == 0x03) {
    snprintf(b, sizeof(b), "state broadcast: hob %s", p[2] == 0x09 ? "ON" : "off");
  } else if (len >= 9 && p[0] == MSG_STATUS && p[1] == 0x03) {
    snprintf(b, sizeof(b), "section status from 0x%02X: %02X %02X %02X %02X (ctr 0x%02X%02X)", src,
             p[3], p[4], p[5], p[6], p[7], p[8]);
  } else if (len >= 4 && p[0] == MSG_CONFIG && p[1] == 0x02) {
    snprintf(b, sizeof(b), "board 0x%02X configuration request (ctr 0x%02X%02X)", src, p[2], p[3]);
  } else if (len >= 3 && p[0] == MSG_CONFIG && p[1] == 0x03) {
    snprintf(b, sizeof(b), "config block 0x%02X -> board 0x%02X", p[2], dst);
  } else if (len >= 1 && p[0] == MSG_COMMIT) {
    snprintf(b, sizeof(b), "commit -> board 0x%02X", dst);
  } else {
    snprintf(b, sizeof(b), "unknown %02X %02X, 0x%02X -> 0x%02X", p[0], len > 1 ? p[1] : 0, src, dst);
  }
  return std::string(b);
}

const BoardConfig *MacsHob::board_for_(uint8_t addr) const {
  for (auto &b : BOARDS)
    if (b.addr == addr)
      return &b;
  return nullptr;
}

int8_t MacsHob::board_index_(uint8_t addr) const {
  for (uint8_t i = 0; i < NUM_BOARDS; i++)
    if (BOARDS[i].addr == addr)
      return (int8_t) i;
  return -1;
}

// ---------------------------------------------------------------------------
// setup / loop

void MacsHob::setup() {
  this->rx_.reserve(256);
  this->boot_at_ = millis();
  if (mode_ == MODE_AUTO) {
    auto_enabled_=true; auto_pending_=true; detect_at_=millis(); mode_=MODE_LISTEN;
  }
  if (mode_ == MODE_MASTER) begin_master_();
  ESP_LOGI(TAG, "Firmware revision: %s", REVISION);
  ESP_LOGCONFIG(TAG, "MACS hob starting in %s mode", this->mode_name().c_str());
}

void MacsHob::loop() {
  const uint32_t now = millis();

  if (!this->rx_.empty() && now - this->last_rx_byte_ > 500) this->rx_.clear();
  while (this->available()) {
    uint8_t c;
    if (!this->read_byte(&c)) break;
    this->rx_.push_back(c);
    this->last_rx_byte_ = now;
    this->last_rx_ = now;
    if (this->rx_.size() > 512)
      this->rx_.erase(this->rx_.begin(), this->rx_.begin() + 256);
  }

  size_t pos = 0, keep = this->rx_.size();
  while (pos < this->rx_.size()) {
    const uint8_t c = this->rx_[pos];
    if (c == MACS_ACK) {
      if (this->rx_.size() - pos < 3) {
        keep = std::min(keep, pos);
        break;
      }
      if (this->rx_[pos + 2] == (uint8_t) (MACS_ACK ^ this->rx_[pos + 1])) {
        const uint8_t ack[3] = {rx_[pos], rx_[pos+1], rx_[pos+2]};
        if (!is_echo_(ack, 3)) handle_ack_(ack[1]);
        pos += 3;
        keep = this->rx_.size();
        continue;
      }
      pos++;
      continue;
    }
    if (c != MACS_MARK) {
      if (this->mode_ != MODE_LISTEN)
        ESP_LOGV(TAG, "RX stray byte %02X", c);
      pos++;
      continue;
    }
    if (this->rx_.size() - pos < 4) {
      keep = std::min(keep, pos);
      break;
    }
    const size_t len = this->rx_[pos + 3];
    if (len == 0 || len > 64) {
      pos++;
      continue;
    }
    const size_t total = len + 5;
    if (this->rx_.size() - pos < total) {
      keep = std::min(keep, pos);
      break;
    }
    if (xor_of_(this->rx_.data() + pos, total) != 0) {
      pos++;
      continue;
    }
    this->handle_frame_(this->rx_.data() + pos, total);
    pos += total;
    keep = this->rx_.size();
  }
  if (keep >= this->rx_.size())
    this->rx_.clear();
  else if (keep > 0)
    this->rx_.erase(this->rx_.begin(), this->rx_.begin() + keep);

  if (!this->rx_.empty() && now - this->last_rx_byte_ > 500)
    this->rx_.clear();

  // expire the echo record
  while (!this->sent_.empty() && millis() - this->sent_.front().second > ECHO_MS)
    this->sent_.pop_front();

  if (auto_pending_ && millis()-detect_at_ >= 35000) {
    auto_pending_=false;
    mode_=MODE_MASTER;
    begin_master_();
    ESP_LOGW(TAG, "Auto: no panel observed in 35s; initializing Master OFF");
  }
  if (this->mode_ == MODE_MASTER) {
    this->check_safety_();
    service_transaction_();
    if (!this->transitioning_ && now - this->boot_at_ >= BOOT_QUIET_MS) this->service_master_();
  }

  if (this->mode_ == MODE_LISTEN) {
    if (!this->tx_.empty())
      this->tx_.clear();  // belt and braces: nothing may be pending in listen
    return;
  }

  if (!this->tx_.empty() && now - this->boot_at_ >= BOOT_QUIET_MS) {
    const PendingFrame &f = this->tx_.front();
    const uint32_t tx_now = millis();
    const bool due = (int32_t) (tx_now - f.due) >= 0;
    const bool quiet = (tx_now - this->last_rx_byte_) >= 12 && (tx_now - this->last_tx_) >= 30;
    if (due && quiet && !waiting_ack_ &&
        (f.prio == PRIO_STOP || (int32_t)(tx_now - ack_guard_until_) >= 0)) {
      auto frame = f.data;
      this->tx_.pop_front();
      this->transmit_(frame);
      if (mode_ == MODE_MASTER && frame[1] != ADDR_BROADCAST) {
        inflight_ = frame; waiting_ack_ = true; attempts_ = 1; ack_started_ = millis();
      }
    }
  }
  if (this->transitioning_ && !waiting_ack_ && this->tx_.empty() && millis() - this->last_tx_ >= 100) {
    this->mode_ = this->pending_mode_;
    if (mode_ == MODE_LISTEN) auto_enabled_=false;
    this->transitioning_ = false;
  }
}

// ---------------------------------------------------------------------------
// receive

bool MacsHob::is_echo_(const uint8_t *f, size_t total) {
  for (auto it = this->sent_.begin(); it != this->sent_.end(); ++it) {
    if (millis() - it->second <= ECHO_MS && it->first.size() == total && memcmp(it->first.data(), f, total) == 0) {
      this->sent_.erase(it);
      return true;
    }
  }
  return false;
}

void MacsHob::handle_frame_(const uint8_t *f, size_t total) {
  const uint8_t dst = f[1];
  const uint8_t src = f[2];
  const uint8_t len = f[3];
  const uint8_t *p = f + 4;
  const uint8_t checksum = f[total - 1];

  // Our own transmissions come back on the shared wire. They are not
  // observations of the panel and must not update decoded state.
  if (this->is_echo_(f, total)) {
    if (this->mode_ != MODE_LISTEN)
      ESP_LOGD(TAG, "ECHO %s", hex_(f, total).c_str());
    return;
  }

  ESP_LOGD(TAG, "RX  %s  | %s", hex_(f, total).c_str(), describe_(f, total).c_str());

  // The original panel remains authoritative in Dev. Cancel pending tests
  // when its off broadcast arrives; never try to counteract its shutdown.
  if (this->mode_ == MODE_DEV && src == ADDR_PANEL && dst == ADDR_BROADCAST &&
      len == 5 && p[0] == MSG_SETPOINT && p[1] == 0x03 && p[2] == 0x00) {
    this->tx_.clear();
  }

  if (auto_enabled_ && src == ADDR_PANEL) {
    // CRC-valid table, known state broadcast, or known config/commit only.
    const bool known = (len==18 && p[0]==MSG_SETPOINT && p[1]==0 &&
                        crc_ccitt_(p,16)==((uint16_t(p[16])<<8)|p[17])) ||
                       (len==5 && p[0]==MSG_SETPOINT && p[1]==3) ||
                       (len==11 && p[0]==MSG_CONFIG && p[1]==3) ||
                       (len==4 && p[0]==MSG_COMMIT);
    if (known) {
      if (!panel_detected_) ESP_LOGW(TAG, "Auto: original panel detected; yielding to Dev");
      panel_detected_=true; auto_pending_=false;
      // Yield silently. Do not send an OFF sequence into the panel's traffic.
      cancel_transaction_(); tx_.clear(); transitioning_=false;
      hob_on_=false; ready_latched_=false;
      for (auto &v:levels_) v=0;
      mode_=MODE_DEV;
    }
  }
  if (mode_ == MODE_MASTER && src == ADDR_PANEL) {
    latch_fault_("Another panel is transmitting; select Dev");
    return;
  }
  const int8_t bi = this->board_index_(src);
  if (bi >= 0)
    this->board_seen_[bi] = millis();

  if (dst == ADDR_STATUS && len == 9 && p[0] == MSG_STATUS && p[1] == 0x03) {
    this->handle_status_(p, src);
    return;
  }

  // Mirror the panel's setpoint tables, but only if the inner CRC is good.
  if (len == 18 && p[0] == MSG_SETPOINT && p[1] == 0x00 && src == ADDR_PANEL) {
    const uint16_t want = (uint16_t) (p[16] << 8) | p[17];
    if (crc_ccitt_(p, 16) != want) {
      ESP_LOGW(TAG, "Setpoint table failed inner CRC; ignoring");
      return;
    }
    const uint8_t base = p[4] >> 4;
    for (uint8_t i = 0; i < 6; ++i)
      if ((p[4 + i * 2] >> 4) != base + i) return;
    if (base == 0 || base == 6) {
      const uint8_t slot = base ? 1 : 0;
      memcpy(this->obs_table_[slot], p, 16);
      this->obs_table_seen_[slot] = millis();
      this->decode_table_(p);
    }
    return;
  }

  if (this->transitioning_ || this->mode_ != MODE_MASTER || dst != ADDR_PANEL)
    return;

  if (millis() - boot_at_ < BOOT_QUIET_MS || faulted()) return;
  this->send_ack_(checksum);

  if (len == 4 && p[0] == MSG_CONFIG && p[1] == 0x02) {
    const uint16_t counter = (uint16_t(p[2]) << 8) | p[3];
    if (bi < 0) { latch_fault_("Unknown board configuration request"); return; }
    // A 21 02 packet alone is not evidence of a reboot: the original panel
    // answers these during operation too. Require fresh status and a known,
    // non-regressing per-board counter before allowing an online refresh.
    if (ready_latched_ && (!status_fresh_() || !counter_seen_[bi])) {
      latch_fault_("Configuration request without continuity evidence"); return;
    }
    if (!observe_counter_(src, counter)) return;
    mains_counter_ = counter;
    ESP_LOGI(TAG, "Board %02X configuration request (ctr %04X); %s", src,
             counter, ready_latched_ ? "refresh with status supervision" : "startup");
    this->provision_(src);
    return;
  }

  if (len == 4 && p[0] == MSG_SETPOINT && p[1] == 0x02) {
    this->mains_counter_ = (uint16_t) (p[2] << 8) | p[3];
    if (!waiting_ack_ && this->tx_.empty()) this->push_tables_(40, PRIO_SETPOINT);
    return;
  }
}

void MacsHob::handle_status_(const uint8_t *p, uint8_t src) {
  if (src != ADDR_GEN_A && src != ADDR_GEN_C)
    return;
  if (mode_ == MODE_MASTER && hob_on_ && millis() - on_at_ > 3000 && p[2] != 0x03) {
    latch_fault_("Generator not in operating state");
  }
  if (!observe_counter_(src, (uint16_t(p[7]) << 8) | p[8])) return;
  status_valid_[src == ADDR_GEN_A ? 0 : 1] = (p[2] == 0 || p[2] == 3);
  const uint8_t start = (src == ADDR_GEN_A) ? 0 : 2;
  const uint8_t count = (src == ADDR_GEN_A) ? 2 : 3;
  const uint32_t now = millis();
  this->mains_counter_ = (uint16_t) (p[7] << 8) | p[8];
  for (uint8_t j = 0; j < count; j++) {
    const uint8_t z = start + j;
    if (z >= NUM_ZONES)
      break;
    // Generator A has four physical sections, paired in default two-zone mode.
    // Generator C retains the established middle/right mapping at p[4..6].
    const bool paired = src == ADDR_GEN_A;
    const uint8_t s = p[paired ? 3 + 2 * j : 4 + j];
    const uint8_t second = paired ? p[4 + 2 * j] : s;
    this->status_[z] = s;
    this->status_second_[z] = paired ? second : -1;
    this->status_seen_[z] = now;
    // Aggregate heat flags, independently of pan presence. Left-side 0x80
    // interpretation remains a hardware-validation item.
    this->hot_[z] = ((s | second) & 0x80) ? 1 : 0;
    const auto pan_state = [](uint8_t value) -> int8_t {
      const uint8_t code = value & 0x6F;  // exclude heat and undecoded 0x10 flag
      return code == 0x05 ? 1 : (code == 0x00 ? 0 : -1);
    };
    const int8_t first_pan = pan_state(s), second_pan = pan_state(second);
    this->pan_[z] = p[2] != 0x03 ? -1 :
        (first_pan == 1 || second_pan == 1) ? 1 :
        (first_pan == 0 && second_pan == 0) ? 0 : -1;
    if (pan_[z] != 0) pan_timing_[z] = false;
  }
}

void MacsHob::decode_table_(const uint8_t *p) {
  const uint8_t base = p[4] >> 4;
  for (uint8_t i = 0; i < 6; i++)
    if ((p[4 + i * 2] >> 4) != base + i)
      return;

  const bool off = p[2] == 0x00 && p[3] == 0x00;
  const bool on = p[2] == 0x0B && p[3] == 0x00;
  if (off && this->mode_ == MODE_DEV) this->tx_.clear();
  const uint32_t now = millis();
  if (on || off) {
    this->observed_on_ = on;
    this->observed_on_seen_ = now;
  }

  for (uint8_t i = 0; i < 6; i++) {
    const uint8_t entry = base + i;
    int8_t zone = -1;
    for (uint8_t z = 0; z < NUM_ZONES; z++)
      if (ZONE_ENTRY[z] == entry)
        zone = (int8_t) z;
    if (zone < 0)
      continue;

    const uint16_t raw = (uint16_t) ((p[4 + i * 2] & 0x0F) << 8) | p[5 + i * 2];
    int8_t level = -1;
    if (off && raw == ENTRY_ABSENT)
      level = 0;
    if (on) {
      bool grouping_ok = true;
      if (zone < 2) {
        const uint16_t comp = (uint16_t) ((p[6 + i * 2] & 0x0F) << 8) | p[7 + i * 2];
        grouping_ok = (raw == ENTRY_IDLE)
                          ? (comp == ENTRY_IDLE)
                          : (comp == (zone == 0 ? GROUP_BOTTOM_LEFT : GROUP_TOP_LEFT));
      }
      if (grouping_ok) {
        const uint16_t *tbl = (zone < 2) ? LEVEL_LEFT : LEVEL_RIGHT;
        for (uint8_t l = 0; l <= 10; l++)
          if (raw == tbl[l])
            level = (int8_t) l;
      }
    }
    this->obs_raw_[zone] = (int16_t) raw;
    this->obs_level_[zone] = level;
    this->level_seen_[zone] = now;
  }

  if (off)
    for (uint8_t z = 0; z < NUM_ZONES; z++)
      this->pan_[z] = -1;
}

// ---------------------------------------------------------------------------
// transmit

bool MacsHob::may_transmit_(const char *what) const {
  if (this->transitioning_) return false;
  if (this->mode_ == MODE_LISTEN) {
    ESP_LOGW(TAG, "Refusing %s: listen mode never transmits", what);
    return false;
  }
  if (millis() - this->boot_at_ < BOOT_QUIET_MS) {
    ESP_LOGW(TAG, "Refusing %s: still in the boot quiet window", what);
    return false;
  }
  return true;
}

void MacsHob::send_ack_(uint8_t checksum) {
  if (this->mode_ != MODE_MASTER || millis() - this->boot_at_ < BOOT_QUIET_MS)
    return;  // in dev mode the panel does the acknowledging
  const uint8_t ack[3] = {MACS_ACK, checksum, (uint8_t) (MACS_ACK ^ checksum)};
  this->write_array(ack, 3);
  this->last_tx_ = millis();
  this->sent_.push_back({std::vector<uint8_t>(ack, ack + 3), millis()});
}

void MacsHob::queue_(std::vector<uint8_t> frame, uint32_t delay_ms, Priority prio, bool is_table) {
  if (this->tx_.size() >= 24) {
    if (prio != PRIO_STOP) { if (mode_ == MODE_MASTER) latch_fault_("TX queue overflow"); return; }
    this->tx_.clear();
  }
  const uint32_t due = millis() + delay_ms;
  PendingFrame pf{std::move(frame), due, prio, is_table};
  // Insert ahead of anything of lower priority so a stop overtakes heating.
  auto it = this->tx_.begin();
  while (it != this->tx_.end() && it->prio <= prio)
    ++it;
  this->tx_.insert(it, std::move(pf));
}

void MacsHob::drop_superseded_tables_() {
  if (waiting_ack_ && inflight_.size() == 23 && inflight_[4] == MSG_SETPOINT) cancel_transaction_();
  for (auto it = this->tx_.begin(); it != this->tx_.end();)
    it = it->is_table ? this->tx_.erase(it) : ++it;
}

void MacsHob::transmit_(const std::vector<uint8_t> &frame) {
  if (this->mode_ == MODE_LISTEN) {
    ESP_LOGE(TAG, "Blocked a transmission in listen mode: %s",
             hex_(frame.data(), frame.size()).c_str());
    return;
  }
  this->write_array(frame.data(), frame.size());
  this->last_tx_ = millis();
  this->sent_.push_back({frame, millis()});
  ESP_LOGI(TAG, "TX  %s  | %s", hex_(frame.data(), frame.size()).c_str(),
           describe_(frame.data(), frame.size()).c_str());
}

// ---------------------------------------------------------------------------
// builders

uint16_t MacsHob::entry_value_(uint8_t entry) const {
  if (!this->hob_on_)
    return ENTRY_ABSENT;
  switch (entry) {
    case 0:
      return LEVEL_LEFT[this->levels_[0]];
    case 1:
      return this->levels_[0] > 0 ? GROUP_BOTTOM_LEFT : ENTRY_IDLE;
    case 2:
      return LEVEL_LEFT[this->levels_[1]];
    case 3:
      return this->levels_[1] > 0 ? GROUP_TOP_LEFT : ENTRY_IDLE;
    case 5:
      return LEVEL_RIGHT[this->levels_[2]];
    case 6:
      return LEVEL_RIGHT[this->levels_[3]];
    case 7:
      return LEVEL_RIGHT[this->levels_[4]];
    default:
      return ENTRY_ABSENT;
  }
}

std::vector<uint8_t> MacsHob::build_table_(uint8_t base) {
  uint8_t p[18];
  p[0] = MSG_SETPOINT;
  p[1] = 0x00;
  p[2] = this->hob_on_ ? 0x0B : 0x00;
  p[3] = 0x00;
  for (uint8_t i = 0; i < 6; i++) {
    const uint8_t e = base + i;
    const uint16_t v = this->entry_value_(e);
    p[4 + i * 2] = (uint8_t) ((e << 4) | ((v >> 8) & 0x0F));
    p[5 + i * 2] = (uint8_t) (v & 0xFF);
  }
  const uint16_t crc = crc_ccitt_(p, 16);
  p[16] = crc >> 8;
  p[17] = crc & 0xFF;
  std::vector<uint8_t> f;
  f.reserve(23);
  f.insert(f.end(), {MACS_MARK, ADDR_GEN_A, ADDR_PANEL, 0x12});
  f.insert(f.end(), p, p + 18);
  f.push_back(xor_of_(f.data(), f.size()));
  return f;
}

std::vector<uint8_t> MacsHob::build_state_() {
  std::vector<uint8_t> f = {MACS_MARK,    ADDR_BROADCAST, ADDR_PANEL, 0x05, MSG_SETPOINT,
                            0x03,         (uint8_t) (this->hob_on_ ? 0x09 : 0x00), 0x00, 0xBB};
  f.push_back(xor_of_(f.data(), f.size()));
  return f;
}

std::vector<uint8_t> MacsHob::build_config_(const BoardConfig &b, uint8_t idx) {
  std::vector<uint8_t> f = {MACS_MARK, b.addr, ADDR_PANEL, 0x0B, MSG_CONFIG, 0x03,
                            (uint8_t) (0x21 + idx)};
  f.insert(f.end(), b.blocks[idx], b.blocks[idx] + 8);
  f.push_back(xor_of_(f.data(), f.size()));
  return f;
}

std::vector<uint8_t> MacsHob::build_commit_(uint8_t addr) {
  std::vector<uint8_t> f = {MACS_MARK, addr, ADDR_PANEL, 0x04, MSG_COMMIT, 0x00, 0x25, 0x00};
  f.push_back(xor_of_(f.data(), f.size()));
  return f;
}

bool MacsHob::build_injected_(uint8_t entry, uint16_t raw, std::vector<uint8_t> &out) {
  const uint8_t slot = entry < 6 ? 0 : 1;
  if (!this->fresh_(this->obs_table_seen_[slot])) {
    ESP_LOGW(TAG, "No recent panel table for entry %u; nothing sent", entry);
    return false;
  }
  uint8_t p[18];
  memcpy(p, this->obs_table_[slot], 16);
  for (const auto &pending : this->tx_)
    if (pending.is_table && pending.data.size() == 23 && (pending.data[8] >> 4) == slot * 6)
      memcpy(p, pending.data.data() + 4, 16);
  const uint8_t base = p[4] >> 4;
  const int i = (int) entry - (int) base;
  if (i < 0 || i > 5)
    return false;
  p[4 + i * 2] = (uint8_t) ((entry << 4) | ((raw >> 8) & 0x0F));
  p[5 + i * 2] = (uint8_t) (raw & 0xFF);
  if (entry == 0 || entry == 2) {
    const uint16_t comp =
        (raw == ENTRY_IDLE) ? ENTRY_IDLE : (entry == 0 ? GROUP_BOTTOM_LEFT : GROUP_TOP_LEFT);
    const int j = i + 1;
    p[4 + j * 2] = (uint8_t) (((entry + 1) << 4) | ((comp >> 8) & 0x0F));
    p[5 + j * 2] = (uint8_t) (comp & 0xFF);
  }
  const uint16_t crc = crc_ccitt_(p, 16);
  p[16] = crc >> 8;
  p[17] = crc & 0xFF;
  out.clear();
  out.insert(out.end(), {MACS_MARK, ADDR_GEN_A, ADDR_PANEL, 0x12});
  out.insert(out.end(), p, p + 18);
  out.push_back(xor_of_(out.data(), out.size()));
  return true;
}

// Every zone in this half of the table set to idle, panel state preserved.
bool MacsHob::build_injected_off_(uint8_t base, std::vector<uint8_t> &out) {
  const uint8_t slot = base ? 1 : 0;
  if (!this->fresh_(this->obs_table_seen_[slot]))
    return false;
  uint8_t p[18];
  memcpy(p, this->obs_table_[slot], 16);
  for (uint8_t i = 0; i < 6; i++) {
    const uint8_t e = base + i;
    bool is_zone = false;
    for (uint8_t z = 0; z < NUM_ZONES; z++)
      if (ZONE_ENTRY[z] == e)
        is_zone = true;
    const bool is_companion = (e == 1 || e == 3);
    if (!is_zone && !is_companion)
      continue;
    p[4 + i * 2] = (uint8_t) ((e << 4) | ((ENTRY_IDLE >> 8) & 0x0F));
    p[5 + i * 2] = (uint8_t) (ENTRY_IDLE & 0xFF);
  }
  const uint16_t crc = crc_ccitt_(p, 16);
  p[16] = crc >> 8;
  p[17] = crc & 0xFF;
  out.clear();
  out.insert(out.end(), {MACS_MARK, ADDR_GEN_A, ADDR_PANEL, 0x12});
  out.insert(out.end(), p, p + 18);
  out.push_back(xor_of_(out.data(), out.size()));
  return true;
}

// ---------------------------------------------------------------------------
// commands

void MacsHob::send_probe() {
  if (mode_ == MODE_MASTER) { ESP_LOGW(TAG, "Probe disabled in Master"); return; }
  if (!this->may_transmit_("bus probe"))
    return;
  std::vector<uint8_t> f = {MACS_MARK, 0xF0, ADDR_PANEL, 0x02, 0x00, 0x00};
  f.push_back(xor_of_(f.data(), f.size()));
  this->queue_(std::move(f), 0, PRIO_CONFIG, false);
}

void MacsHob::set_hob_on(bool on) {
  if (mode_ == MODE_MASTER && on && !ready()) {
    ESP_LOGW(TAG, "On refused: startup incomplete or fault latched"); return;
  }
  if (!this->may_transmit_(on ? "hob on" : "hob off"))
    return;
  this->last_command_ = millis();
  this->last_stop_ = STOP_NONE;

  if (this->mode_ == MODE_MASTER) {
    if (!on) { stop_all(); return; }
    if (!hob_on_) on_at_ = millis();
    this->hob_on_ = on;
    // Replace old control requests, but finish pending board configuration.
    for (auto it = this->tx_.begin(); it != this->tx_.end();) {
      const auto &f = it->data;
      const bool config = f.size() > 4 && (f[4] == MSG_CONFIG || f[4] == MSG_COMMIT);
      it = config ? ++it : this->tx_.erase(it);
    }
    if (!on) {
      for (uint8_t z = 0; z < NUM_ZONES; z++)
        this->levels_[z] = 0;
      this->drop_superseded_tables_();
    }
    this->queue_(this->build_state_(), 0, PRIO_SETPOINT, false);
    this->push_tables_(88, PRIO_SETPOINT);
    this->next_state_ = millis() + 10000;
    ESP_LOGI(TAG, "Hob %s", on ? "on" : "off");
    return;
  }

  // Dev mode: one-shot injection mirroring the panel's own on/off sequence.
  const bool saved = this->hob_on_;
  uint8_t keep[NUM_ZONES];
  memcpy(keep, this->levels_, NUM_ZONES);
  this->hob_on_ = on;
  memset(this->levels_, 0, NUM_ZONES);
  this->tx_.clear();
  const Priority pr = on ? PRIO_SETPOINT : PRIO_STOP;
  this->queue_(this->build_state_(), 0, pr, false);
  this->queue_(this->build_table_(0), 88, pr, true);
  this->queue_(this->build_table_(6), 152, pr, true);
  this->hob_on_ = saved;
  memcpy(this->levels_, keep, NUM_ZONES);
  ESP_LOGI(TAG, "Injecting hob %s (one-shot test; original panel may command shutdown)", on ? "ON" : "off");
}

void MacsHob::set_zone_level(uint8_t zone, uint8_t level) {
  if (mode_ == MODE_MASTER && level > 0 && !ready()) {
    ESP_LOGW(TAG, "Level refused: startup incomplete or fault latched"); return;
  }
  if (zone >= NUM_ZONES)
    return;
  if (level > 10)
    level = 10;
  if (level > this->max_level_) {
    ESP_LOGW(TAG, "Level %u exceeds max_level %u; clamping", level, this->max_level_);
    level = this->max_level_;
  }
  if (!this->may_transmit_("zone level"))
    return;
  this->last_command_ = millis();
  this->last_stop_ = STOP_NONE;

  if (this->mode_ == MODE_MASTER) {
    if (level > 0 && !this->hob_on_) {
      ESP_LOGW(TAG, "Hob is off; turn it on before setting a level");
      return;
    }
    if (levels_[zone] == 0 || level == 0) {
      pan_timing_[zone] = false; unknown_timing_[zone] = false;
    }
    if (levels_[zone] != level) level_since_[zone] = millis();  // auto switch-off restarts on a change
    this->levels_[zone] = level;
    this->drop_superseded_tables_();
    this->push_tables_(0, level > 0 ? PRIO_SETPOINT : PRIO_STOP);
    ESP_LOGI(TAG, "Zone %u -> level %u", zone, level);
    return;
  }

  if (level > 0 && !this->observed_hob_on()) {
    ESP_LOGW(TAG, "Hob is off (or no recent table); turn it on first");
    return;
  }
  const uint16_t raw = (zone < 2) ? LEVEL_LEFT[level] : LEVEL_RIGHT[level];
  std::vector<uint8_t> f;
  if (!this->build_injected_(ZONE_ENTRY[zone], raw, f))
    return;
  for (auto it = this->tx_.begin(); it != this->tx_.end();)
    it = (it->is_table && it->data.size() == 23 &&
          (it->data[8] >> 4) == (ZONE_ENTRY[zone] < 6 ? 0 : 6)) ? this->tx_.erase(it) : ++it;
  this->queue_(std::move(f), 0, level > 0 ? PRIO_SETPOINT : PRIO_STOP, true);
  ESP_LOGI(TAG, "Injecting zone %u level %u (one-shot test; original panel may command shutdown)", zone, level);
}

void MacsHob::stop_all() {
  cancel_transaction_();
  for (uint8_t z=0; z<NUM_ZONES; ++z) { pan_timing_[z]=false; unknown_timing_[z]=false; }
  // Aborting provisioning leaves readiness false until explicit reinitialization.
  for (uint8_t i=0; i<NUM_BOARDS; ++i) {
    if (refreshing_[i]) {ready_latched_=false; startup_at_=millis();}
    provisioning_[i]=false; refreshing_[i]=false;
  }
  // Discard everything pending first: a stop must never sit behind a
  // queued heating command.
  this->hob_on_ = false;
  const size_t dropped = this->tx_.size();
  this->tx_.clear();
  for (uint8_t z = 0; z < NUM_ZONES; z++)
    this->levels_[z] = 0;
  this->last_stop_ = STOP_MANUAL;
  if (dropped)
    ESP_LOGW(TAG, "Stop: discarded %u queued frame(s)", (unsigned) dropped);

  if (this->mode_ == MODE_LISTEN) {
    ESP_LOGW(TAG, "Stop in listen mode clears internal state only; nothing was sent");
    return;
  }
  if (millis() - this->boot_at_ < BOOT_QUIET_MS)
    return;

  if (this->mode_ == MODE_MASTER) {
    this->push_tables_(0, PRIO_STOP);
    this->queue_(this->build_state_(), 140, PRIO_STOP, false);
    ESP_LOGW(TAG, "Stop: all zones commanded to zero");
    return;
  }

  std::vector<uint8_t> a, b;
  bool sent = false;
  if (this->build_injected_off_(0, a)) {
    this->queue_(std::move(a), 0, PRIO_STOP, true);
    sent = true;
  }
  if (this->build_injected_off_(6, b)) {
    this->queue_(std::move(b), 70, PRIO_STOP, true);
    sent = true;
  }
  if (sent)
    ESP_LOGW(TAG, "Stop injected once. The original panel remains master; "
                  "no automatic retry or restart will follow.");
  else
    ESP_LOGW(TAG, "Stop: no recent panel table, nothing could be injected. "
                  "Use the hob's own controls.");
}

// ---------------------------------------------------------------------------
// master behaviour

void MacsHob::provision_(uint8_t addr) {
  const int8_t i = board_index_(addr);
  if (i < 0 || faulted() || provisioning_[i]) return;
  refreshing_[i] = ready_latched_;
  refresh_started_[i] = millis();
  board_provisioned_[i] = false;
  provisioning_[i] = true;
  config_step_[i] = 0;
  // service_master_ serializes configuration for requested boards.
}

void MacsHob::push_tables_(uint32_t delay_ms, Priority prio) {
  this->drop_superseded_tables_();
  this->queue_(this->build_table_(0), delay_ms, prio, true);
  this->queue_(this->build_table_(6), delay_ms + 70, prio, true);
  this->next_table_ = millis() + this->table_interval_;
}

void MacsHob::service_master_() {
  const uint32_t now = millis();
  if (!faulted() && table_acked_[0] && table_acked_[1] && !initial_provision_started_) {
    initial_provision_started_=true;
    // Fixed appliance profile: also covers ESP reboot with boards already awake.
    for (const auto &b:BOARDS) if (!board_provisioned_[board_index_(b.addr)]) provision_(b.addr);
  }
  if (!faulted() && !waiting_ack_ && tx_.empty()) {
    for (uint8_t i=0; i<NUM_BOARDS; ++i) if (provisioning_[i]) {
      queue_(config_step_[i] < 4 ? build_config_(BOARDS[i], config_step_[i]) : build_commit_(BOARDS[i].addr),
             40, PRIO_CONFIG, false);
      break;
    }
  }
  if (!faulted() && !ready_latched_ && all_boards_provisioned() &&
      table_acked_[0] && table_acked_[1] && status_fresh_()) {
    ready_latched_ = true;
    ESP_LOGI(TAG, "READY: configuration acknowledged; fresh generator status");
  }
  if (!waiting_ack_ && this->tx_.empty() && (int32_t) (now - this->next_table_) >= 0)
    this->push_tables_(0, PRIO_SETPOINT);
  if ((int32_t) (now - this->next_state_) >= 0) {
    this->queue_(this->build_state_(), 0, PRIO_SETPOINT, false);
    this->next_state_ = now + 10000;
  }
}

void MacsHob::check_safety_() {
  if (faulted()) return;
  for (uint8_t i=0; i<NUM_BOARDS; ++i) {
    if (refreshing_[i] && millis() - refresh_started_[i] > 8000) {
      latch_fault_("Configuration refresh timeout"); return;
    }
  }
  // OFF standby gaps revoke readiness via ready(), but need no manual reset.
  // Keep polling with OFF tables; fresh valid status restores availability.
  // Never automatically clear an existing fault or resume a heating request.
  bool heat_requested = hob_on_;
  for (uint8_t z=0; z<NUM_ZONES; ++z) heat_requested |= levels_[z] > 0;
  if (ready_latched_ && !status_fresh_() && heat_requested) {
    latch_fault_("Generator status lost"); return;
  }
  if (!ready_latched_ && millis() - startup_at_ > 45000) {
    latch_fault_("Startup timeout; all boards must be configured"); return;
  }
  check_pan_timers_();
  const uint32_t now = millis();
  bool any = false;
  for (uint8_t z = 0; z < NUM_ZONES; z++)
    any |= this->levels_[z] > 0;
  if (!any && !this->hob_on_)
    return;

#ifdef USE_API
  if (this->require_api_) {
    const bool connected =
        api::global_api_server != nullptr && api::global_api_server->is_connected_with_state_subscription();
    if (!connected) {
      if (this->api_lost_since_ == 0)
        this->api_lost_since_ = now;
      else if (now - this->api_lost_since_ > 15000) {
        ESP_LOGW(TAG, "API gone for 15 s; stopping");
        latch_fault_("API connection lost");
        this->last_stop_ = STOP_API_LOST;
        return;
      }
    } else {
      this->api_lost_since_ = 0;
    }
  }
#endif

  if (this->auto_off_ && this->mode_ == MODE_MASTER) {
    for (uint8_t z = 0; z < NUM_ZONES; z++) {
      const uint8_t lv = this->levels_[z];
      if (lv > 0 && now - this->level_since_[z] > auto_off_ms(lv)) {
        ESP_LOGW(TAG, "Zone %u unchanged at level %u for %u min; automatic switch-off", z, lv,
                 unsigned((now - this->level_since_[z]) / 60000));
        latch_fault_("Automatic switch-off");
        this->last_stop_ = STOP_TIMEOUT;
        return;
      }
    }
  }
  if (any && this->cmd_timeout_ > 0 && now - this->last_command_ > this->cmd_timeout_) {
    ESP_LOGW(TAG, "Idle timeout; stopping");
    latch_fault_("Command timeout");
    this->last_stop_ = STOP_TIMEOUT;
  }
}

// ---------------------------------------------------------------------------
// mode

bool MacsHob::set_mode(Mode m) {
  if (m == MODE_AUTO) {
    ESP_LOGW(TAG, "Auto selection is a boot policy; reboot to detect again"); return false;
  }
  if (auto_pending_) { ESP_LOGW(TAG, "Wait for automatic panel detection"); return false; }
  if (m == MODE_MASTER && auto_enabled_ && panel_detected_) {
    ESP_LOGW(TAG, "Panel detected this session; reboot after removing panel"); return false;
  }
  if (m == this->mode_)
    return true;
  if (m == MODE_MASTER && !this->allow_master_) {
    ESP_LOGE(TAG, "Master mode refused: allow_master is not enabled in the configuration");
    return false;
  }
  if (this->transitioning_) return false;
  if (faulted()) { ESP_LOGW(TAG, "Reset fault first in Master"); return false; }
  const Mode from = this->mode_;
  cancel_transaction_();

  if (from == MODE_MASTER) {
    this->hob_on_ = false;
    for (uint8_t z = 0; z < NUM_ZONES; z++)
      this->levels_[z] = 0;
    this->tx_.clear();
    this->push_tables_(0, PRIO_STOP);
    this->queue_(this->build_state_(), 140, PRIO_STOP, false);
    this->transitioning_ = true;
    this->pending_mode_ = m;
    return true; // finish shutdown on the bus before changing modes

  }

  this->mode_ = m;
  if (m == MODE_LISTEN) auto_enabled_=false;
  this->tx_.clear();

  if (m == MODE_MASTER) {
    begin_master_();
    ESP_LOGW(TAG, "MASTER mode: this device is now the hob controller");
  } else {
    ESP_LOGW(TAG, "Mode -> %s", this->mode_name().c_str());
  }
  return true;
}

bool MacsHob::set_mode_by_name(const std::string &s) {
  if (s == "Auto" || s == "auto") return set_mode(MODE_AUTO);
  if (s == "Listen" || s == "listen")
    return this->set_mode(MODE_LISTEN);
  if (s == "Dev" || s == "dev")
    return this->set_mode(MODE_DEV);
  if (s == "Master" || s == "master")
    return this->set_mode(MODE_MASTER);
  ESP_LOGW(TAG, "Unknown mode '%s'", s.c_str());
  return false;
}

std::string MacsHob::mode_name() const {
  if (auto_pending_) return "Auto";
  switch (this->mode_) {
    case MODE_DEV:
      return "Dev";
    case MODE_MASTER:
      return "Master";
    default:
      return "Listen";
  }
}

// ---------------------------------------------------------------------------
// observed state

int8_t MacsHob::observed_hob_state() const {
  if (!this->fresh_(this->observed_on_seen_))
    return -1;
  return this->observed_on_ ? 1 : 0;
}

bool MacsHob::observed_hob_on() const { return this->observed_hob_state() == 1; }

int8_t MacsHob::observed_level(uint8_t z) const {
  if (z >= NUM_ZONES || !this->fresh_(this->level_seen_[z]))
    return -1;
  return this->obs_level_[z];
}

std::string MacsHob::observed_setting(uint8_t z) const {
  const int8_t l = this->observed_level(z);
  if (l < 0)
    return "Unknown";
  if (l == 0)
    return "Off";
  if (l == 10)
    return "P";
  return std::to_string(l);
}

std::string MacsHob::observed_raw(uint8_t z) const {
  if (z >= NUM_ZONES)
    return "---";
  char b[40];
  char lv[8], st[8];
  if (this->fresh_(this->level_seen_[z]) && this->obs_raw_[z] >= 0)
    snprintf(lv, sizeof(lv), "%03X", this->obs_raw_[z]);
  else
    snprintf(lv, sizeof(lv), "---");
  if (this->fresh_(this->status_seen_[z]) && this->status_[z] >= 0)
    {
    if (z < 2 && this->status_second_[z] >= 0)
      snprintf(st, sizeof(st), "%02X/%02X", this->status_[z], this->status_second_[z]);
    else
      snprintf(st, sizeof(st), "%02X", this->status_[z]);
  }
  else
    snprintf(st, sizeof(st), "--");
  snprintf(b, sizeof(b), "level=%s status=%s", lv, st);
  return std::string(b);
}

int8_t MacsHob::zone_pan(uint8_t z) const {
  if (z >= NUM_ZONES || !this->fresh_(this->status_seen_[z]))
    return -1;
  return this->pan_[z];
}

int8_t MacsHob::zone_hot(uint8_t z) const {
  if (z >= NUM_ZONES || !this->fresh_(this->status_seen_[z]))
    return -1;
  return this->hot_[z];
}

bool MacsHob::board_present(uint8_t i) const {
  return i < NUM_BOARDS && this->fresh_(this->board_seen_[i]);
}

bool MacsHob::board_provisioned(uint8_t i) const {
  return i < NUM_BOARDS && this->board_provisioned_[i];
}

bool MacsHob::all_boards_provisioned() const {
  for (uint8_t i = 0; i < NUM_BOARDS; i++)
    if (!this->board_provisioned(i))
      return false;
  return true;
}

std::string MacsHob::status_text() const {
  if (auto_pending_) return "Auto: listening for panel (35 s)";
  switch (this->mode_) {
    case MODE_LISTEN:
      return "Listen, decode only";
    case MODE_DEV:
      return "Dev, panel in charge";
    default:
      break;
  }
  if (faulted()) return "Fault: " + fault_;
  switch (this->last_stop_) {
    case STOP_API_LOST:
      return "Master, stopped: connection lost";
    case STOP_TIMEOUT:
      return "Master, stopped: idle timeout";
    default:
      break;
  }
  if (this->transitioning_) return "Stopping before mode change";
  for (uint8_t i=0; i<NUM_BOARDS; ++i)
    if (refreshing_[i]) return "Master, configuration refresh - new heating requests locked";
  if (ready_latched_ && !hob_on_ && !status_fresh_())
    return "Master, standby - waiting for generator status";
  if (!ready()) return "Master, initializing - heating locked";
  return (this->hob_on_ ? "Master, hob on" : "Master, hob off") + (last_event_.empty() ? std::string() : " | " + last_event_);
}

void MacsHob::dump_config() {
  ESP_LOGCONFIG(TAG, "MACS hob revision: %s", REVISION);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->mode_name().c_str());
  ESP_LOGCONFIG(TAG, "  Master mode permitted: %s", YESNO(this->allow_master_));
  ESP_LOGCONFIG(TAG, "  Maximum level: %u", this->max_level_);
  ESP_LOGCONFIG(TAG, "  Idle timeout: %u ms", (unsigned) this->cmd_timeout_);
  ESP_LOGCONFIG(TAG, "  Table interval: %u ms", (unsigned) this->table_interval_);
  ESP_LOGCONFIG(TAG, "  Requires API connection: %s", YESNO(this->require_api_));
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
}

// One outstanding unicast transaction. MACS ACKs carry only an 8-bit checksum;
// they are not authenticated and cannot identify the sender. Master must be sole panel.
void MacsHob::cancel_transaction_() {
  if (waiting_ack_) ack_guard_until_ = millis() + 250;
  waiting_ack_ = false; inflight_.clear(); attempts_ = 0;
}

void MacsHob::handle_ack_(uint8_t checksum) {
  ESP_LOGD(TAG, "RX ACK checksum=%02X", checksum);
  if (mode_ != MODE_MASTER || !waiting_ack_ || inflight_.empty() ||
      checksum != inflight_.back() || millis() - ack_started_ > 180) return;
  complete_transaction_();
}

void MacsHob::complete_transaction_() {
  auto f = inflight_;
  waiting_ack_ = false; inflight_.clear(); attempts_ = 0;
  ack_guard_until_ = millis() + 20;
  if (faulted()) return;
  if (f.size() == 23 && f[4] == MSG_SETPOINT && f[5] == 0) {
    const uint8_t slot = f[8] >> 4;
    if (slot == 0 || slot == 6) table_acked_[slot ? 1 : 0] = true;
  }
  const int8_t i = board_index_(f[1]);
  if (i < 0 || !provisioning_[i]) return;
  if (f[4] == MSG_CONFIG && f.size() == 16 && f[6] == 0x21 + config_step_[i]) {
    ++config_step_[i];
    if (config_step_[i] < 4) {
      return;
    }
    if (BOARDS[i].commit) {
      return;
    }
  } else if (!(f[4] == MSG_COMMIT && config_step_[i] == 4 && BOARDS[i].commit)) {
    return;
  }
  const bool was_refresh = refreshing_[i];
  refreshing_[i] = false;
  provisioning_[i] = false;
  board_provisioned_[i] = true;
  if (!was_refresh && (i == 0 || i == 2)) {
    const uint8_t first = i == 0 ? 0 : 2, last = i == 0 ? 2 : 5;
    for (uint8_t z=first; z<last; ++z) status_seen_[z]=0;
  }
  ESP_LOGI(TAG, "Board %02X configuration ACK complete", f[1]);
}

void MacsHob::service_transaction_() {
  if (!waiting_ack_ || millis() - ack_started_ <= 180) return;
  if (attempts_ >= 3) {
    cancel_transaction_();
    if (!faulted()) latch_fault_("ACK timeout after 3 attempts");
    return;
  }
  if (millis() - last_rx_byte_ < 12 || millis() - last_tx_ < 30) return;
  ++attempts_;
  transmit_(inflight_);
  ack_started_ = millis();
}

bool MacsHob::observe_counter_(uint8_t src, uint16_t counter) {
  const int8_t i = board_index_(src);
  if (i < 0) return false;
  // Unsigned delta handles 16-bit rollover. This is supporting evidence,
  // not a universal reboot detector; operating-state and freshness checks remain.
  if (mode_ == MODE_MASTER && ready_latched_ && counter_seen_[i] &&
      uint16_t(counter - board_counter_[i]) >= 0x8000) {
    latch_fault_("Board counter regressed; reinitialization required"); return false;
  }
  counter_seen_[i] = true; board_counter_[i] = counter;
  return true;
}

bool MacsHob::status_fresh_() const {
  if (!status_valid_[0] || !status_valid_[1]) return false;
  for (uint8_t z = 0; z < NUM_ZONES; ++z)
    if (!status_seen_[z] || millis() - status_seen_[z] > comm_timeout_) return false;
  return true;
}

bool MacsHob::ready() const {
  return mode_ == MODE_MASTER && !transitioning_ && !faulted() && ready_latched_ &&
         all_boards_provisioned() && status_fresh_();
}

void MacsHob::begin_master_() {
  cancel_transaction_(); tx_.clear();
  hob_on_ = false; ready_latched_ = false; initial_provision_started_=false;
  startup_at_ = millis(); last_command_ = millis(); api_lost_since_ = 0;
  last_stop_ = STOP_NONE; last_event_.clear();
  for (uint8_t i=0; i<NUM_BOARDS; ++i) {
    board_provisioned_[i]=false; provisioning_[i]=false; config_step_[i]=0;
    refreshing_[i]=false; counter_seen_[i]=false;
  }
  table_acked_[0]=table_acked_[1]=false;
  status_valid_[0]=status_valid_[1]=false;
  for (uint8_t z=0; z<NUM_ZONES; ++z) {
    levels_[z]=0; pan_timing_[z]=false; unknown_timing_[z]=false;
    status_seen_[z]=0; pan_[z]=-1;
  }
  push_tables_(200, PRIO_STOP);
  queue_(build_state_(), 400, PRIO_STOP, false);
  next_state_=millis()+10000;
}

void MacsHob::latch_fault_(const char *reason) {
  if (faulted()) return;
  fault_=reason; ready_latched_=false; transitioning_=false;
  ESP_LOGE(TAG, "FAULT: %s; heating requests cleared", reason);
  stop_all(); // Best effort OFF; cannot guarantee shutdown over a broken wire.
  // Keep fault text until an explicit reset, never resume previous levels.
}

void MacsHob::reset_fault() {
  if (mode_ != MODE_MASTER || hob_on_ || transitioning_) return;
  if (millis()-boot_at_ < BOOT_QUIET_MS) return;
  fault_.clear();
  begin_master_();
  ESP_LOGW(TAG, "Explicit reinitialization; levels remain zero");
}

void MacsHob::check_pan_timers_() {
  const uint32_t now=millis();
  bool changed=false;
  for (uint8_t z=0; z<NUM_ZONES; ++z) {
    if (!hob_on_ || !levels_[z]) {
      pan_timing_[z]=false; unknown_timing_[z]=false; continue;
    }
    if (!status_seen_[z] || now-status_seen_[z] > comm_timeout_) {
      latch_fault_("Zone status stale"); return;
    }
    if (pan_[z] < 0) {
      pan_timing_[z]=false;
      if (!unknown_timing_[z]) { unknown_timing_[z]=true; unknown_since_[z]=now; }
      if (now-unknown_since_[z] >= comm_timeout_) {
        latch_fault_("Pan state unknown while heating requested"); return;
      }
      continue;
    }
    unknown_timing_[z]=false;
    if (pan_[z] == 1) { pan_timing_[z]=false; continue; }
    if (!pan_timing_[z]) { pan_timing_[z]=true; pan_since_[z]=now; }
    if (now-pan_since_[z] >= pan_timeout_) {
      levels_[z]=0; pan_timing_[z]=false; changed=true;
      static const char *names[] = {"Bottom left", "Top left", "Middle", "Bottom right", "Top right"};
      last_event_=std::string(names[z])+": off after pan absence timeout";
      ESP_LOGW(TAG, "Zone %u stopped: pan absent for %u ms", z, (unsigned)pan_timeout_);
    }
  }
  if (changed) push_tables_(0, PRIO_STOP);
}

int MacsHob::pan_timeout_remaining(uint8_t z) const {
  if (z >= NUM_ZONES || !pan_timing_[z] || mode_ != MODE_MASTER) return -1;
  const uint32_t elapsed=millis()-pan_since_[z];
  return elapsed >= pan_timeout_ ? 0 : (pan_timeout_-elapsed+999)/1000;
}

void MacsHob::on_shutdown() {
  // Best effort for orderly OTA/reboot only. Power loss/crash cannot execute this.
  if (mode_ != MODE_MASTER) return;
  hob_on_=false;
  for (auto &v:levels_) v=0;
  tx_.clear(); cancel_transaction_();
  for (uint8_t base: {uint8_t(0), uint8_t(6)}) {
    auto f=build_table_(base); write_array(f.data(), f.size()); flush();
  }
  auto f=build_state_(); write_array(f.data(), f.size()); flush();
}

}  // namespace macs_hob
}  // namespace esphome
