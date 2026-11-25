#include <gtest/gtest.h>

#include "esphome/components/microchip_atsha204a/microchip_atsha204a.h"

#include <algorithm>
#include <initializer_list>
#include <vector>

namespace esphome::microchip_atsha204a::testing {

class TestTransport : public Transport {
 public:
  bool setup() override { return true; }
  void dump_config() override {}
  bool is_single_wire() const override { return this->single_wire; }
  bool wake() override {
    this->wakes++;
    return true;
  }
  bool command(const uint8_t *packet, size_t size) override {
    this->commands.emplace_back(packet, packet + size);
    return true;
  }
  bool request_response() override {
    this->requests++;
    return true;
  }
  Receive receive(uint8_t *packet) override {
    if (this->reply.empty())
      return Receive::RECEIVE_PENDING;
    std::copy(this->reply.begin(), this->reply.end(), packet);
    this->reply.clear();
    return Receive::RECEIVE_COMPLETE;
  }
  bool power_down(bool idle) override {
    this->low_power_modes.push_back(idle);
    return true;
  }

  bool single_wire{false};
  int wakes{0};
  int requests{0};
  std::vector<uint8_t> reply;
  std::vector<std::vector<uint8_t>> commands;
  std::vector<bool> low_power_modes;
};

class TestChip : public ATSHA204A {
 public:
  explicit TestChip(TestTransport &transport) : ATSHA204A(&transport), transport_(transport) {}

  // Advance one transaction phase without waiting for the hardware timer.
  void advance(std::initializer_list<uint8_t> reply = {}) {
    this->transport_.reply = reply;
    this->wait_us_ = 0;
    this->loop();
  }

 protected:
  TestTransport &transport_;
};

TEST(ATSHA204A, ValidatesWakeAndChainsCommandWithoutWakingAgain) {
  TestTransport transport;
  TestChip chip(transport);
  uint32_t held_session = 0;
  int last_status = BUSY;
  std::vector<uint8_t> result;
  const auto finished = [&](int status, const CommandResponse &data, uint32_t session) {
    last_status = status;
    result.assign(data.begin(), data.end());
    EXPECT_EQ(session, 0U);
  };
  chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_KEEP_AWAKE, 0, true,
               [&](int status, const CommandResponse &data, uint32_t session) {
                 EXPECT_EQ(status, 0);
                 EXPECT_EQ(data.size(), 4U);
                 held_session = session;
                 chip.command(Opcode::OPCODE_DEV_REV, 0, 0, {}, EndMode::END_MODE_SLEEP, session, true, finished);
               });
  chip.advance({0x04, 0x11, 0x33, 0x43});
  ASSERT_EQ(transport.commands.size(), 1U);
  EXPECT_EQ(transport.commands.front(), (std::vector<uint8_t>{0x07, 0x30, 0, 0, 0, 0x03, 0x5D}));

  chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
  ASSERT_NE(held_session, 0U);
  ASSERT_EQ(transport.commands.size(), 2U);
  EXPECT_EQ(transport.wakes, 1);
  chip.advance({0x07, 0, 0, 0, 0, 0x03, 0xAD});
  EXPECT_EQ(last_status, 0);
  EXPECT_EQ(result, (std::vector<uint8_t>{0, 0, 0, 0}));
  EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{false}));
}

TEST(ATSHA204A, RejectsBadResponseCrcWithoutRetryingCommand) {
  TestTransport transport;
  TestChip chip(transport);
  int last_status = BUSY;
  chip.command(Opcode::OPCODE_RANDOM, 0, 0, {}, EndMode::END_MODE_IDLE, 0, true,
               [&](int status, const CommandResponse &data, uint32_t session) {
                 last_status = status;
                 EXPECT_TRUE(data.empty());
                 EXPECT_EQ(session, 0U);
               });
  chip.advance({0x04, 0x11, 0x33, 0x43});
  chip.advance({0x04, 0, 0, 0});
  EXPECT_EQ(last_status, BAD_CRC);
  EXPECT_EQ(transport.commands.size(), 1U);
  EXPECT_EQ(transport.low_power_modes, (std::vector<bool>{false}));
}

TEST(ATSHA204A, PauseWithoutResponseDoesNotReturnSession) {
  TestTransport transport;
  transport.single_wire = true;
  TestChip chip(transport);
  int last_status = BUSY;
  const auto finished = [&](int status, const CommandResponse &data, uint32_t session) {
    last_status = status;
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(session, 0U);
  };
  chip.command(Opcode::OPCODE_PAUSE, 0, 0, {}, EndMode::END_MODE_KEEP_AWAKE, 0, false, finished);
  EXPECT_EQ(last_status, INVALID_ARGUMENT);
  EXPECT_EQ(transport.wakes, 0);

  chip.command(Opcode::OPCODE_PAUSE, 0, 0, {}, EndMode::END_MODE_SLEEP, 0, false, finished);
  chip.advance();
  chip.advance();
  EXPECT_EQ(last_status, 0);
  EXPECT_EQ(transport.requests, 0);
  EXPECT_EQ(transport.commands.size(), 1U);
  EXPECT_TRUE(transport.low_power_modes.empty());
}

}  // namespace esphome::microchip_atsha204a::testing
