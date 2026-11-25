#include "transport.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::microchip_atsha204a {

ESPHOME_LOG_TAG(TAG, "microchip_atsha204a");
static constexpr uint8_t MIN_RESPONSE_SIZE = 4;

#ifdef USE_MICROCHIP_ATSHA204A_I2C
static constexpr uint8_t I2C_COMMAND = 0x03;
static constexpr uint8_t I2C_IDLE = 0x02;
static constexpr uint8_t I2C_SLEEP = 0x01;
// Keep transfers within the smallest supported Wire buffer (32 bytes).
static constexpr size_t I2C_CHUNK_SIZE = 30;

void I2CTransport::dump_config() {
  ESP_LOGCONFIG(TAG, "  Transport: I2C");
  LOG_I2C_DEVICE(this);
}

bool I2CTransport::wake() {
  // The address byte holds SDA low for eight clocks. A sleeping chip is expected to NACK.
  const uint8_t zero = 0;
  const auto status = this->bus_->write(0, &zero, 1);
  return status == i2c::ERROR_OK || status == i2c::ERROR_NOT_ACKNOWLEDGED;
}

bool I2CTransport::command(const uint8_t *packet, size_t size) {
  uint8_t data[I2C_CHUNK_SIZE + 1]{I2C_COMMAND};
  while (size != 0) {
    // Each I2C write repeats the word address and continues at the chip's input counter.
    const size_t count = std::min(size, I2C_CHUNK_SIZE);
    std::copy_n(packet, count, data + 1);
    if (this->write(data, count + 1) != i2c::ERROR_OK)
      return false;
    packet += count;
    size -= count;
  }
  return true;
}

Receive I2CTransport::receive(uint8_t *packet) {
  const auto status = this->read(packet, 1);
  if (status == i2c::ERROR_NOT_ACKNOWLEDGED)
    return Receive::RECEIVE_PENDING;
  if (status != i2c::ERROR_OK)
    return Receive::RECEIVE_IO_ERROR;
  if (packet[0] < MIN_RESPONSE_SIZE || packet[0] > MAX_RESPONSE)
    return Receive::RECEIVE_INVALID_LENGTH;
  for (size_t offset = 1; offset < packet[0];) {
    const size_t count = std::min<size_t>(packet[0] - offset, I2C_CHUNK_SIZE);
    if (this->read(packet + offset, count) != i2c::ERROR_OK)
      return Receive::RECEIVE_IO_ERROR;
    offset += count;
  }
  return Receive::RECEIVE_COMPLETE;
}

bool I2CTransport::power_down(bool idle, bool allow_nack) {
  const uint8_t flag = idle ? I2C_IDLE : I2C_SLEEP;
  const auto status = this->write(&flag, 1);
  return status == i2c::ERROR_OK || (allow_nack && status == i2c::ERROR_NOT_ACKNOWLEDGED);
}
#endif

}  // namespace esphome::microchip_atsha204a
