#include <unity.h>

#include "ce_cube/controllers.hpp"
#include "ce_cube/crc16.hpp"
#include "ce_cube/instrument_controller.hpp"
#include "ce_cube/protocol_store.hpp"

namespace {

void test_lift_motion_completes_after_deadline() {
  ce_cube::LiftController lift;
  lift.Configure({35U, 100U, 400U});

  TEST_ASSERT_TRUE(lift.CommandMove(ce_cube::LiftPosition::kUp, 1000U));
  TEST_ASSERT_TRUE(lift.busy());
  lift.Tick(1200U);
  TEST_ASSERT_TRUE(lift.busy());
  lift.Tick(1400U);
  TEST_ASSERT_FALSE(lift.busy());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::LiftStateCode::kUp),
                        static_cast<int>(lift.state()));
}

void test_carousel_adjust_keeps_slot_and_finishes() {
  ce_cube::CarouselController carousel;
  carousel.Configure({4076U, ce_cube::kDefaultSlotCount, 1U});

  TEST_ASSERT_TRUE(carousel.CommandGotoSlot(3U, 0U));
  for (uint16_t tick = 0U; tick < 2000U; ++tick) {
    carousel.Tick(tick);
    ce_cube::CarouselHardwareAction action = {0};
    (void)carousel.TakeHardwareAction(&action);
    if (!carousel.busy()) {
      break;
    }
  }

  TEST_ASSERT_EQUAL_UINT8(3U, carousel.slot());
  TEST_ASSERT_TRUE(carousel.CommandAdjust(1, 2001U));
  for (uint16_t tick = 2001U; tick < 2600U; ++tick) {
    carousel.Tick(tick);
    ce_cube::CarouselHardwareAction action = {0};
    (void)carousel.TakeHardwareAction(&action);
    if (!carousel.busy()) {
      break;
    }
  }

  TEST_ASSERT_FALSE(carousel.busy());
  TEST_ASSERT_EQUAL_UINT8(3U, carousel.slot());
}

void test_run_controller_advances_samples_and_repetitions() {
  ce_cube::ProtocolStore::ResetTestStorage();
  ce_cube::ProtocolStore store;
  store.Begin();

  ce_cube::ProtocolMetadata metadata = {};
  metadata.slot_count = ce_cube::kDefaultSlotCount;
  metadata.bge1_slot = 0U;
  metadata.bge2_slot = 1U;
  metadata.sample_1_slot = 5U;
  metadata.sample_count = 2U;
  metadata.repetitions = 3U;
  metadata.injection_mode = ce_cube::InjectionMode::kElectrokinetic;
  metadata.collection_duration_ms = 1000UL;
  metadata.injection_duration_ms = 1000UL;
  metadata.droplet_duration_ms = 1000UL;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ProtocolStoreStatus::kOk),
                        static_cast<int>(store.BeginUpload(metadata)));

  const uint8_t program[] = {
      static_cast<uint8_t>(ce_cube::ProtocolOpcode::kEndSection),
      static_cast<uint8_t>(ce_cube::ProtocolOpcode::kEndSection),
  };
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ProtocolStoreStatus::kOk),
                        static_cast<int>(store.WriteChunk(
                            0U, program, static_cast<uint8_t>(sizeof(program)))));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ce_cube::ProtocolStoreStatus::kOk),
      static_cast<int>(store.Commit(
          1U, static_cast<uint16_t>(sizeof(program)),
          ce_cube::ComputeCrc16Ccitt(program, sizeof(program)))));

  ce_cube::ProtocolInfo info = {};
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ProtocolStoreStatus::kOk),
                        static_cast<int>(store.LoadInfo(&info)));

  ce_cube::LiftController lift;
  ce_cube::CarouselController carousel;
  ce_cube::ReplenishController replenish;
  ce_cube::FluidController fluid;
  ce_cube::BinaryOutputController hv;
  ce_cube::BinaryOutputController pump;
  ce_cube::BinaryOutputController valve1;
  ce_cube::BinaryOutputController valve2;
  ce_cube::RunExecutionContext context = {
      &store, &info, &lift,      &carousel, &replenish,
      &fluid, &hv,   &pump,      &valve1,   &valve2};
  ce_cube::RunController run;
  ce_cube::RunStepRequest request = {};

  TEST_ASSERT_TRUE(run.Start(info));
  run.Tick(0U, context, &request);
  TEST_ASSERT_EQUAL_UINT8(1U, run.progress().repetition);
  TEST_ASSERT_EQUAL_UINT8(1U, run.progress().sample_index);

  run.Tick(0U, context, &request);
  TEST_ASSERT_EQUAL_UINT8(2U, run.progress().repetition);
  TEST_ASSERT_EQUAL_UINT8(1U, run.progress().sample_index);

  run.Tick(0U, context, &request);
  TEST_ASSERT_EQUAL_UINT8(3U, run.progress().repetition);
  TEST_ASSERT_EQUAL_UINT8(1U, run.progress().sample_index);

  run.Tick(0U, context, &request);
  TEST_ASSERT_EQUAL_UINT8(1U, run.progress().repetition);
  TEST_ASSERT_EQUAL_UINT8(2U, run.progress().sample_index);
  TEST_ASSERT_EQUAL_UINT8(6U, run.progress().sample_slot);

  run.Tick(0U, context, &request);
  run.Tick(0U, context, &request);
  run.Tick(0U, context, &request);
  run.Tick(0U, context, &request);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::RunStateCode::kComplete),
                        static_cast<int>(run.progress().state));
}

void test_manual_commands_rejected_while_run_is_active() {
  ce_cube::InstrumentController controller;
  controller.Initialize(0U);

  ce_cube::ParsedCommand start = {};
  start.seq = 1U;
  start.type = ce_cube::CommandType::kRunStart;

  ce_cube::ParsedCommand move = {};
  move.seq = 2U;
  move.type = ce_cube::CommandType::kLiftMove;
  move.lift_position = ce_cube::LiftPosition::kUp;

  const ce_cube::ReplyMessage start_reply = controller.HandleCommand(start, 0U);
  const ce_cube::ReplyMessage move_reply = controller.HandleCommand(move, 0U);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::MessageKind::kAck),
                        static_cast<int>(start_reply.kind));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::MessageKind::kError),
                        static_cast<int>(move_reply.kind));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ErrorCode::kBusy),
                        static_cast<int>(move_reply.error_code));
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_lift_motion_completes_after_deadline);
  RUN_TEST(test_carousel_adjust_keeps_slot_and_finishes);
  RUN_TEST(test_run_controller_advances_samples_and_repetitions);
  RUN_TEST(test_manual_commands_rejected_while_run_is_active);
  return UNITY_END();
}
