#pragma once

#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>

#ifdef USE_MICROCHIP_ATSHA204A_I2C
#include "esphome/components/i2c/i2c.h"
#endif
namespace esphome::microchip_atsha204a {

inline constexpr size_t MAX_COMMAND = 84;
inline constexpr size_t MAX_RESPONSE = 35;

enum class Receive : uint8_t { RECEIVE_PENDING, RECEIVE_COMPLETE, RECEIVE_INVALID_LENGTH, RECEIVE_IO_ERROR };

class Transport {
 public:
  virtual ~Transport() = default;
  virtual bool setup() = 0;
  virtual void dump_config() = 0;
  virtual bool wake() = 0;
  virtual bool command(const uint8_t *packet, size_t size) = 0;
  virtual bool request_response() = 0;
  virtual Receive receive(uint8_t *packet) = 0;
  virtual bool power_down(bool idle, bool allow_nack = false) = 0;
};

#ifdef USE_MICROCHIP_ATSHA204A_I2C
class I2CTransport : public Transport, public i2c::I2CDevice {
 public:
  bool setup() override { return true; }
  void dump_config() override;
  bool wake() override;
  bool command(const uint8_t *packet, size_t size) override;
  bool request_response() override { return true; }
  Receive receive(uint8_t *packet) override;
  bool power_down(bool idle, bool allow_nack) override;
};
#endif

}  // namespace esphome::microchip_atsha204a
