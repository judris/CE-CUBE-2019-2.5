#include <unity.h>

#include "ce_cube/ad7745_math.hpp"

void test_capacitance_formula_matches_legacy_reference() {
  const float capacitance = ce_cube::CapacitanceFromRawBytes(0x80U, 0x00U, 0x00U);
  TEST_ASSERT_FLOAT_WITHIN(0.0005F, 0.0F, capacitance);
}

void test_temperature_formula_matches_legacy_reference() {
  const float temperature = ce_cube::TemperatureFromRawBytes(0x80U, 0x00U, 0x00U);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, temperature);
}

void setUp() {}

void tearDown() {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_capacitance_formula_matches_legacy_reference);
  RUN_TEST(test_temperature_formula_matches_legacy_reference);
  return UNITY_END();
}
