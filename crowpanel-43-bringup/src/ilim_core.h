#pragma once

// Pure current-limit (ILIM) logic for the CrowPanel Iset editor: exact decimal parsing, keypad entry
// rules, and the single-write transaction state machine. No Arduino/LVGL dependency and everything
// is constexpr so ilim_core_checks.h can verify it with static_assert at build time (no host
// compiler or test framework is available on this PC).
//
// Canonical unit is integer mA. Ranges mirror the STM32 UDI host (CH1 0..3000, CH2 0..2000 mA);
// they are protocol ranges, not proven hardware ratings. Zero writes are blocked by user decision
// (2026-10-06); a host-reported zero is still shown as-is.

#include <stddef.h>
#include <stdint.h>

namespace ilim {

constexpr uint16_t kMaxMa[2] = {3000, 2000};
constexpr uint16_t kSliderStepMa = 10;
constexpr uint16_t kMinWriteMa = 1;
constexpr uint8_t kMaxIntDigits = 2;
constexpr uint8_t kMaxDecimals = 3;
constexpr size_t kEntryCap = 8;  // "99.999" plus NUL, one spare
constexpr uint32_t kAckTimeoutMs = 1500;
constexpr uint32_t kReconcileRetryMs = 2000;

enum class Parse : uint8_t {
  Ok,
  Empty,
  Malformed,
  Negative,
  TooManyDecimals,
  Zero,
  OutOfRange,
};

struct ParseResult {
  Parse status;
  uint16_t mA;
};

// Exact decimal-amps -> mA. Never rounds, clamps or converts bad text to zero.
constexpr ParseResult parseAmps(const char* text, uint8_t ch) {
  if (text == nullptr || text[0] == '\0') return {Parse::Empty, 0};
  if (text[0] == '-') return {Parse::Negative, 0};
  uint32_t int_part = 0;
  uint32_t frac = 0;
  uint8_t frac_digits = 0;
  uint8_t digits = 0;
  bool point = false;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    const char c = text[i];
    if (c == '.') {
      if (point) return {Parse::Malformed, 0};
      point = true;
    } else if (c >= '0' && c <= '9') {
      ++digits;
      if (!point) {
        if (int_part < 100000u) int_part = int_part * 10u + static_cast<uint32_t>(c - '0');
      } else {
        if (frac_digits >= kMaxDecimals) return {Parse::TooManyDecimals, 0};
        frac = frac * 10u + static_cast<uint32_t>(c - '0');
        ++frac_digits;
      }
    } else {
      return {Parse::Malformed, 0};
    }
  }
  if (digits == 0) return {Parse::Malformed, 0};
  for (uint8_t k = frac_digits; k < kMaxDecimals; ++k) frac *= 10u;
  const uint32_t mA = int_part * 1000u + frac;
  if (mA == 0u) return {Parse::Zero, 0};
  if (ch > 1u || mA > kMaxMa[ch]) return {Parse::OutOfRange, 0};
  return {Parse::Ok, static_cast<uint16_t>(mA)};
}

// Exact "A.mmm" text for any mA value (host-reported values above the editor range included).
constexpr void formatAmps(uint32_t mA, char* out, size_t cap) {
  if (cap == 0) return;
  char tmp[12] = {};
  size_t n = 0;
  uint32_t whole = mA / 1000u;
  do {
    tmp[n++] = static_cast<char>('0' + (whole % 10u));
    whole /= 10u;
  } while (whole != 0u && n < 6);
  size_t o = 0;
  while (n > 0 && o + 1 < cap) out[o++] = tmp[--n];
  const char frac[4] = {'.', static_cast<char>('0' + (mA / 100u) % 10u),
                        static_cast<char>('0' + (mA / 10u) % 10u),
                        static_cast<char>('0' + mA % 10u)};
  for (size_t k = 0; k < 4 && o + 1 < cap; ++k) out[o++] = frac[k];
  out[o] = '\0';
}

// ── keypad entry ──────────────────────────────────────────────────────────
enum class Entry : uint8_t { Ok, Full, SecondPoint, TooManyDecimals, TooManyIntDigits, BadKey };

constexpr size_t entryLen(const char* buf) {
  size_t n = 0;
  while (buf[n] != '\0') ++n;
  return n;
}

// Adds one digit or '.'; a rejected key leaves the text untouched and says why.
constexpr Entry entryAppend(char* buf, size_t cap, char c) {
  const bool is_digit = (c >= '0' && c <= '9');
  if (!is_digit && c != '.') return Entry::BadKey;
  size_t len = entryLen(buf);
  uint8_t int_digits = 0;
  uint8_t decimals = 0;
  bool point = false;
  for (size_t i = 0; i < len; ++i) {
    if (buf[i] == '.') point = true;
    else if (point) ++decimals;
    else ++int_digits;
  }
  if (c == '.') {
    if (point) return Entry::SecondPoint;
  } else if (point) {
    if (decimals >= kMaxDecimals) return Entry::TooManyDecimals;
  } else if (int_digits >= kMaxIntDigits) {
    return Entry::TooManyIntDigits;
  }
  if (len + 1 >= cap) return Entry::Full;
  buf[len] = c;
  buf[len + 1] = '\0';
  return Entry::Ok;
}

constexpr void entryBackspace(char* buf) {
  const size_t len = entryLen(buf);
  if (len > 0) buf[len - 1] = '\0';
}

// ── slider mapping (step units of kSliderStepMa) ──────────────────────────
constexpr int32_t sliderMin() { return 1; }
constexpr int32_t sliderMax(uint8_t ch) { return kMaxMa[ch > 1u ? 1u : ch] / kSliderStepMa; }

// Nearest slider position for display only; the draft itself is never rounded by this.
constexpr int32_t sliderFromMa(uint32_t mA, uint8_t ch) {
  int32_t v = static_cast<int32_t>((mA + kSliderStepMa / 2u) / kSliderStepMa);
  if (v < sliderMin()) v = sliderMin();
  if (v > sliderMax(ch)) v = sliderMax(ch);
  return v;
}

constexpr uint16_t maFromSlider(int32_t pos) {
  return static_cast<uint16_t>(pos * static_cast<int32_t>(kSliderStepMa));
}

// ── write transaction (one outstanding ILIM write at a time) ──────────────
enum class TxState : uint8_t {
  Idle,         // nothing outstanding
  Pending,      // write sent, waiting for a matching ACK/EVT
  Unconfirmed,  // outcome unknown (timeout / link loss / dropped line): readback required
};

enum class TxResult : uint8_t {
  None,
  NotSent,      // refused before sending (link, state, range)
  SendFailed,   // UART not ready
  Confirmed,    // matching ACK/EVT
  ConfirmedLate,  // matching report after an unconfirmed period
  Rejected,     // host ERR for ILIM
  NotApplied,   // readback after an unconfirmed period differs from the request
};

enum class Why : uint8_t { None, Timeout, LinkLost, Dropped };

struct Tx {
  TxState state;
  TxResult result;
  Why why;
  uint8_t ch;
  uint16_t requested_mA;
  uint16_t reported_mA;
  uint32_t sent_ms;
  uint32_t reconcile_ms;
  bool reconcile_sent;
  char err[48];
};

constexpr bool startsWith(const char* s, const char* prefix) {
  for (size_t i = 0; prefix[i] != '\0'; ++i) {
    if (s[i] != prefix[i]) return false;
  }
  return true;
}

constexpr void noteNotSent(Tx& t, uint8_t ch, uint16_t mA) {
  if (t.state != TxState::Idle) return;  // never overwrite an unresolved write's state
  t.result = TxResult::NotSent;
  t.ch = ch;
  t.requested_mA = mA;
}

constexpr void noteSendFailed(Tx& t, uint8_t ch, uint16_t mA) {
  if (t.state != TxState::Idle) return;
  t.result = TxResult::SendFailed;
  t.ch = ch;
  t.requested_mA = mA;
}

constexpr void begin(Tx& t, uint8_t ch, uint16_t mA, uint32_t now_ms) {
  t.state = TxState::Pending;
  t.result = TxResult::None;
  t.why = Why::None;
  t.ch = ch;
  t.requested_mA = mA;
  t.reported_mA = 0;
  t.sent_ms = now_ms;
  t.reconcile_ms = 0;
  t.reconcile_sent = false;
  t.err[0] = '\0';
}

// Any host ILIM report (ACK or EVT). Returns true when it resolved the transaction.
constexpr bool onReport(Tx& t, uint8_t ch, uint16_t mA) {
  if (t.state == TxState::Idle || ch != t.ch) return false;
  if (mA == t.requested_mA) {
    t.result = (t.state == TxState::Pending) ? TxResult::Confirmed : TxResult::ConfirmedLate;
    t.state = TxState::Idle;
    return true;
  }
  // A different value only counts as the answer once it follows our own readback request.
  if (t.state == TxState::Unconfirmed && t.reconcile_sent) {
    t.reported_mA = mA;
    t.result = TxResult::NotApplied;
    t.state = TxState::Idle;
    return true;
  }
  return false;
}

// Host ERR line. Only ILIM errors are claimed; others stay unrelated (returns false).
constexpr bool onErr(Tx& t, const char* text) {
  if (t.state == TxState::Idle || text == nullptr || !startsWith(text, "ILIM")) return false;
  t.result = TxResult::Rejected;
  t.state = TxState::Idle;
  size_t i = 0;
  for (; text[i] != '\0' && i + 1 < sizeof(t.err); ++i) t.err[i] = text[i];
  t.err[i] = '\0';
  return true;
}

constexpr void markUnconfirmed(Tx& t, Why why) {
  if (t.state != TxState::Pending) return;
  t.state = TxState::Unconfirmed;
  t.why = why;
  t.reconcile_sent = false;
  t.reconcile_ms = 0;
}

// Call every loop. A write that was not answered in time or lost its link becomes Unconfirmed.
constexpr void tick(Tx& t, uint32_t now_ms, bool link_live) {
  if (t.state != TxState::Pending) return;
  if (!link_live) markUnconfirmed(t, Why::LinkLost);
  else if (now_ms - t.sent_ms > kAckTimeoutMs) markUnconfirmed(t, Why::Timeout);
}

// True when a read-only GET ILIM should be sent now to learn the real value (never a retransmit).
constexpr bool reconcileDue(const Tx& t, uint32_t now_ms, bool link_live, bool force) {
  if (t.state != TxState::Unconfirmed || !link_live) return false;
  if (force || !t.reconcile_sent) return true;
  return (now_ms - t.reconcile_ms) >= kReconcileRetryMs;
}

constexpr void markReconcileSent(Tx& t, uint32_t now_ms) {
  t.reconcile_sent = true;
  t.reconcile_ms = now_ms;
}

}  // namespace ilim
