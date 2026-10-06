#pragma once

// Build-time checks for ilim_core.h. A failing check breaks the firmware build, which stands in for
// a host unit test (no native compiler or test framework exists on the bench PC).

#include "ilim_core.h"

namespace ilim {
namespace checks {

constexpr bool streq(const char* a, const char* b) {
  size_t i = 0;
  for (; a[i] != '\0' && b[i] != '\0'; ++i) {
    if (a[i] != b[i]) return false;
  }
  return a[i] == b[i];
}

constexpr bool parses(const char* text, uint8_t ch, Parse want, uint16_t want_mA = 0) {
  const ParseResult r = parseAmps(text, ch);
  return r.status == want && r.mA == want_mA;
}

constexpr bool formats(uint32_t mA, const char* want) {
  char b[12] = {};
  formatAmps(mA, b, sizeof(b));
  return streq(b, want);
}

// Keys: digits and '.', 'B' = backspace. Rejected keys are ignored, as on the panel.
constexpr bool keys(const char* seq, const char* want) {
  char b[kEntryCap] = {};
  for (size_t i = 0; seq[i] != '\0'; ++i) {
    if (seq[i] == 'B') entryBackspace(b);
    else entryAppend(b, sizeof(b), seq[i]);
  }
  return streq(b, want);
}

constexpr Entry lastKey(const char* seq) {
  char b[kEntryCap] = {};
  Entry e = Entry::Ok;
  for (size_t i = 0; seq[i] != '\0'; ++i) {
    if (seq[i] == 'B') entryBackspace(b);
    else e = entryAppend(b, sizeof(b), seq[i]);
  }
  return e;
}

// Exact parsing: both channels, bounds, zero policy, malformed / negative / excess precision.
static_assert(parses("1.5", 0, Parse::Ok, 1500), "1.5 A");
static_assert(parses("1.500", 0, Parse::Ok, 1500), "1.500 A");
static_assert(parses("1.678", 0, Parse::Ok, 1678), "exact, not rounded to the 10 mA step");
static_assert(parses("0.001", 0, Parse::Ok, 1), "1 mA is the smallest write");
static_assert(parses(".5", 1, Parse::Ok, 500), "leading point");
static_assert(parses("5.", 0, Parse::OutOfRange), "5 A above CH1 max");
static_assert(parses("3", 0, Parse::Ok, 3000), "CH1 max");
static_assert(parses("3.000", 0, Parse::Ok, 3000), "CH1 max, 3 decimals");
static_assert(parses("3.001", 0, Parse::OutOfRange), "CH1 above max");
static_assert(parses("2", 1, Parse::Ok, 2000), "CH2 max");
static_assert(parses("2.001", 1, Parse::OutOfRange), "CH2 above max");
static_assert(parses("2.5", 1, Parse::OutOfRange), "CH2 range differs from CH1");
static_assert(parses("99.999", 0, Parse::OutOfRange), "large value");
static_assert(parses("", 0, Parse::Empty), "empty");
static_assert(parseAmps(nullptr, 0).status == Parse::Empty, "null");
static_assert(parses(".", 0, Parse::Malformed), "lone point");
static_assert(parses("1.2.3", 0, Parse::Malformed), "two points");
static_assert(parses("1a", 0, Parse::Malformed), "letter");
static_assert(parses("1 5", 0, Parse::Malformed), "space");
static_assert(parses("-1", 0, Parse::Negative), "negative");
static_assert(parses("1.2345", 0, Parse::TooManyDecimals), "excess precision");
static_assert(parses("0", 0, Parse::Zero), "zero blocked");
static_assert(parses("0.000", 0, Parse::Zero), "zero blocked with decimals");
static_assert(parses("0.", 0, Parse::Zero), "zero blocked with point");
static_assert(parses("000", 1, Parse::Zero), "zero blocked with leading zeros");
static_assert(parses("1", 2, Parse::OutOfRange), "bad channel index");

static_assert(formats(1500, "1.500"), "format 1.500");
static_assert(formats(0, "0.000"), "format zero (host-reported zero is shown honestly)");
static_assert(formats(1, "0.001"), "format 1 mA");
static_assert(formats(10, "0.010"), "format 10 mA");
static_assert(formats(3000, "3.000"), "format 3 A");
static_assert(formats(12345, "12.345"), "format above range");

// Keypad entry rules and backspace.
static_assert(keys("1.5", "1.5"), "typing");
static_assert(keys("1.2345", "1.234"), "4th decimal rejected");
static_assert(lastKey("1.2345") == Entry::TooManyDecimals, "4th decimal reason");
static_assert(keys("123", "12"), "3rd integer digit rejected");
static_assert(lastKey("123") == Entry::TooManyIntDigits, "3rd integer digit reason");
static_assert(keys("1..2", "1.2"), "second point rejected");
static_assert(lastKey("1..") == Entry::SecondPoint, "second point reason");
static_assert(keys("1.5B", "1."), "backspace");
static_assert(keys("B", ""), "backspace on empty");
static_assert(keys("1.5BBB", ""), "backspace to empty");
static_assert(keys(".5", ".5"), "point first");
static_assert(keys("99.999", "99.999"), "longest legal entry fits the buffer");
static_assert(streq("", ""), "streq");

// Slider mapping: display position only, never a change to the exact draft.
static_assert(sliderFromMa(1678, 0) == 168, "off-grid value shows nearest step");
static_assert(maFromSlider(168) == 1680, "dragging snaps to the step");
static_assert(sliderFromMa(0, 0) == 1, "host zero shows at the minimum position");
static_assert(sliderFromMa(1, 0) == 1, "1 mA shows at the minimum position");
static_assert(sliderFromMa(5000, 0) == 300, "above max shows at the maximum position");
static_assert(sliderMax(0) == 300 && sliderMax(1) == 200, "step counts");
static_assert(sliderFromMa(2000, 1) == 200, "CH2 max");

// Transaction state machine.
constexpr Tx fresh(uint8_t ch = 0, uint16_t mA = 1500, uint32_t now = 1000) {
  Tx t = {};
  begin(t, ch, mA, now);
  return t;
}

constexpr bool txConfirm() {
  Tx t = fresh();
  if (t.state != TxState::Pending) return false;
  if (onReport(t, 1, 1500)) return false;     // other channel is not ours
  if (onReport(t, 0, 2500)) return false;     // earlier GET reply with the old value: keep waiting
  if (t.state != TxState::Pending) return false;
  if (!onReport(t, 0, 1500)) return false;
  if (t.state != TxState::Idle || t.result != TxResult::Confirmed) return false;
  if (onReport(t, 0, 1500)) return false;     // trailing EVT changes nothing
  return t.result == TxResult::Confirmed;
}
static_assert(txConfirm(), "ACK/EVT match, other channel and old value ignored");

constexpr bool txErr() {
  Tx t = fresh();
  if (onErr(t, "FORMAT need CMD:")) return false;  // unrelated ERR is not claimed
  if (t.state != TxState::Pending) return false;
  if (!onErr(t, "ILIM CH1 range 0..3000 mA")) return false;
  return t.state == TxState::Idle && t.result == TxResult::Rejected &&
         streq(t.err, "ILIM CH1 range 0..3000 mA");
}
static_assert(txErr(), "only ILIM errors reject the write");

constexpr bool txTimeoutThenReadback() {
  Tx t = fresh(0, 1500, 1000);
  tick(t, 2500, true);
  if (t.state != TxState::Pending) return false;     // exactly at the timeout: still pending
  tick(t, 2501, true);
  if (t.state != TxState::Unconfirmed || t.why != Why::Timeout) return false;
  if (onReport(t, 0, 2500)) return false;            // old value before our readback: not an answer
  if (!reconcileDue(t, 2600, true, false)) return false;
  markReconcileSent(t, 2600);
  if (reconcileDue(t, 4599, true, false)) return false;
  if (!reconcileDue(t, 4600, true, false)) return false;
  if (!reconcileDue(t, 2700, true, true)) return false;  // manual refresh
  if (!onReport(t, 0, 2500)) return false;           // readback after our GET
  return t.state == TxState::Idle && t.result == TxResult::NotApplied && t.reported_mA == 2500;
}
static_assert(txTimeoutThenReadback(), "timeout needs a readback before another write");

constexpr bool txLateAck() {
  Tx t = fresh();
  tick(t, 3000, true);
  if (t.state != TxState::Unconfirmed) return false;
  if (!onReport(t, 0, 1500)) return false;
  return t.state == TxState::Idle && t.result == TxResult::ConfirmedLate;
}
static_assert(txLateAck(), "late matching ACK after a timeout resolves the write");

constexpr bool txLinkLoss() {
  Tx t = fresh();
  tick(t, 1100, false);
  if (t.state != TxState::Unconfirmed || t.why != Why::LinkLost) return false;
  if (reconcileDue(t, 1200, false, true)) return false;  // no GET while the link is down
  if (!reconcileDue(t, 9000, true, false)) return false;
  markReconcileSent(t, 9000);
  if (!onReport(t, 0, 1500)) return false;
  return t.result == TxResult::ConfirmedLate;
}
static_assert(txLinkLoss(), "link loss is explicit and reconciled after recovery");

constexpr bool txLateErr() {
  Tx t = fresh();
  tick(t, 3000, true);
  if (!onErr(t, "ILIM CH1 range 0..3000 mA")) return false;
  return t.state == TxState::Idle && t.result == TxResult::Rejected;
}
static_assert(txLateErr(), "late ERR after a timeout rejects the write");

constexpr bool txDropped() {
  Tx t = fresh();
  markUnconfirmed(t, Why::Dropped);
  if (t.state != TxState::Unconfirmed || t.why != Why::Dropped) return false;
  markUnconfirmed(t, Why::Timeout);  // only a pending write can become unconfirmed
  return t.why == Why::Dropped;
}
static_assert(txDropped(), "dropped control line makes the outcome uncertain");

constexpr bool txNotSentKeepsUnresolved() {
  Tx t = fresh(0, 1500, 1000);
  noteNotSent(t, 1, 700);
  if (t.state != TxState::Pending || t.ch != 0 || t.requested_mA != 1500) return false;
  Tx u = {};
  noteNotSent(u, 1, 700);
  if (u.result != TxResult::NotSent || u.ch != 1) return false;
  noteSendFailed(u, 0, 100);
  return u.result == TxResult::SendFailed && u.state == TxState::Idle;
}
static_assert(txNotSentKeepsUnresolved(), "refused writes never disturb a pending one");

constexpr bool txIdleIgnoresReports() {
  Tx t = {};
  return !onReport(t, 0, 1500) && !onErr(t, "ILIM CH1 x") && !reconcileDue(t, 0, true, true);
}
static_assert(txIdleIgnoresReports(), "idle transaction ignores reports");

}  // namespace checks
}  // namespace ilim
