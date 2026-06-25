#include <unity.h>

#include "ce_cube/crc16.hpp"

void test_crc16_matches_known_vector() {
  static const uint8_t kVector[] = {'1', '2', '3', '4', '5',
                                    '6', '7', '8', '9'};
  const uint16_t crc = ce_cube::ComputeCrc16Ccitt(kVector, sizeof(kVector));
  TEST_ASSERT_EQUAL_HEX16(0x29B1U, crc);
}

void setUp() {}

void tearDown() {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_crc16_matches_known_vector);
  return UNITY_END();
}
