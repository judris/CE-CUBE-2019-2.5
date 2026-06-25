#include <unity.h>

#include "ce_cube/crc16.hpp"
#include "ce_cube/instrument_controller.hpp"
#include "ce_cube/protocol_store.hpp"

namespace {

ce_cube::ParsedCommand MakeCommand(uint16_t seq, ce_cube::CommandType type) {
  ce_cube::ParsedCommand command = {};
  command.seq = seq;
  command.type = type;
  return command;
}

void test_status_get_requests_telemetry() {
  ce_cube::ProtocolStore::ResetTestStorage();
  ce_cube::InstrumentController controller;
  controller.Initialize(0U);

  ce_cube::ParsedCommand status = MakeCommand(1U, ce_cube::CommandType::kStatusGet);
  const ce_cube::ReplyMessage reply = controller.HandleCommand(status, 0U);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::MessageKind::kAck),
                        static_cast<int>(reply.kind));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::AckCode::kStatus),
                        static_cast<int>(reply.ack_code));
  TEST_ASSERT_EQUAL_UINT32(0U, reply.uptime_ms);
  TEST_ASSERT_TRUE(controller.TakeStatusRequest());
  TEST_ASSERT_FALSE(controller.TakeStatusRequest());
}

void test_protocol_info_is_unavailable_when_disabled() {
  ce_cube::ProtocolStore::ResetTestStorage();
  ce_cube::InstrumentController controller;
  controller.Initialize(0U);

  ce_cube::ParsedCommand info = MakeCommand(2U, ce_cube::CommandType::kProtocolInfo);
  const ce_cube::ReplyMessage reply = controller.HandleCommand(info, 0U);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::MessageKind::kError),
                        static_cast<int>(reply.kind));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ErrorCode::kUnavailable),
                        static_cast<int>(reply.error_code));
  TEST_ASSERT_EQUAL_UINT32(0U, reply.uptime_ms);
}

void test_idle_telemetry_has_no_analysis_time() {
  ce_cube::ProtocolStore::ResetTestStorage();
  ce_cube::InstrumentController controller;
  controller.Initialize(0U);

  const ce_cube::TelemetrySnapshot snapshot =
      controller.BuildTelemetry(1U, 123U);
  TEST_ASSERT_EQUAL_UINT32(123U, snapshot.uptime_ms);
  TEST_ASSERT_FALSE(snapshot.analysis_time_valid);
  TEST_ASSERT_EQUAL_UINT32(0U, snapshot.analysis_time_ms);
}

void test_empty_storage_is_invalid_until_protocol_is_uploaded() {
  ce_cube::ProtocolStore::ResetTestStorage();
  ce_cube::InstrumentController controller;
  controller.Initialize(0U);

  const ce_cube::TelemetrySnapshot snapshot = controller.BuildTelemetry(7U, 0U);
  TEST_ASSERT_FALSE(snapshot.protocol_valid);
  TEST_ASSERT_EQUAL_UINT16(ce_cube::kFaultProtocolStore, snapshot.faults);

  ce_cube::ParsedCommand start = MakeCommand(8U, ce_cube::CommandType::kRunStart);
  const ce_cube::ReplyMessage reply = controller.HandleCommand(start, 0U);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::MessageKind::kError),
                        static_cast<int>(reply.kind));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ErrorCode::kProtocolInvalid),
                        static_cast<int>(reply.error_code));
  TEST_ASSERT_EQUAL_UINT32(0U, reply.uptime_ms);
}

void test_temporary_protocol_run_emits_events_and_completes() {
  ce_cube::ProtocolStore::ResetTestStorage();
  ce_cube::InstrumentController controller;
  controller.Initialize(0U);

  ce_cube::ParsedCommand begin = MakeCommand(10U, ce_cube::CommandType::kProtocolBegin);
  begin.protocol_metadata.slot_count = ce_cube::kDefaultSlotCount;
  begin.protocol_metadata.bge1_slot = 0U;
  begin.protocol_metadata.bge2_slot = 1U;
  begin.protocol_metadata.sample_1_slot = 5U;
  begin.protocol_metadata.sample_count = 1U;
  begin.protocol_metadata.repetitions = 1U;
  begin.protocol_metadata.injection_mode = ce_cube::InjectionMode::kElectrokinetic;
  begin.protocol_metadata.collection_duration_ms = 1000UL;
  begin.protocol_metadata.injection_duration_ms = 1000UL;
  begin.protocol_metadata.droplet_duration_ms = 1000UL;
  begin.protocol_metadata.wait_times_100ms[0] = 10U;

  uint8_t program[ce_cube::kProtocolChunkDataBytes] = {0U};
  program[0] = static_cast<uint8_t>(ce_cube::ProtocolOpcode::kEndSection);
  program[1] = static_cast<uint8_t>(ce_cube::ProtocolOpcode::kEventRunStart);
  program[2] = static_cast<uint8_t>(ce_cube::ProtocolOpcode::kWaitBase);
  program[3] = static_cast<uint8_t>(ce_cube::ProtocolOpcode::kEventRunStop);
  program[4] = static_cast<uint8_t>(ce_cube::ProtocolOpcode::kEndSection);

  ce_cube::ParsedCommand chunk = MakeCommand(11U, ce_cube::CommandType::kProtocolChunk);
  chunk.offset = 0U;
  chunk.chunk_length = static_cast<uint8_t>(ce_cube::kProtocolChunkDataBytes);
  for (uint8_t index = 0U; index < ce_cube::kProtocolChunkDataBytes; ++index) {
    chunk.chunk_data[index] = program[index];
  }

  ce_cube::ParsedCommand commit =
      MakeCommand(12U, ce_cube::CommandType::kProtocolCommit);
  commit.prepare_length = 1U;
  commit.program_length = 5U;
  commit.crc16 = ce_cube::ComputeCrc16Ccitt(program, 5U);

  ce_cube::ParsedCommand start = MakeCommand(13U, ce_cube::CommandType::kRunStart);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ce_cube::MessageKind::kAck),
      static_cast<int>(controller.HandleCommand(begin, 0U).kind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ce_cube::MessageKind::kAck),
      static_cast<int>(controller.HandleCommand(chunk, 0U).kind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ce_cube::MessageKind::kAck),
      static_cast<int>(controller.HandleCommand(commit, 0U).kind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ce_cube::MessageKind::kAck),
      static_cast<int>(controller.HandleCommand(start, 0U).kind));

  ce_cube::EventSnapshot event = {};
  TEST_ASSERT_FALSE(controller.TakePendingEvent(&event));

  controller.Tick(0U);
  TEST_ASSERT_FALSE(controller.TakePendingEvent(&event));

  controller.Tick(0U);
  TEST_ASSERT_TRUE(controller.TakePendingEvent(&event));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::EventCode::kRunStart),
                        static_cast<int>(event.code));
  TEST_ASSERT_EQUAL_UINT8(1U, event.sample_index);
  TEST_ASSERT_EQUAL_UINT8(5U, event.sample_slot);
  TEST_ASSERT_EQUAL_UINT8(1U, event.repetition);
  TEST_ASSERT_EQUAL_UINT32(0U, event.uptime_ms);
  TEST_ASSERT_TRUE(event.analysis_time_valid);
  TEST_ASSERT_EQUAL_UINT32(0U, event.analysis_time_ms);

  const ce_cube::ParsedCommand status = MakeCommand(14U, ce_cube::CommandType::kStatusGet);
  const ce_cube::ReplyMessage status_reply = controller.HandleCommand(status, 500U);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::MessageKind::kAck),
                        static_cast<int>(status_reply.kind));
  TEST_ASSERT_EQUAL_UINT32(500U, status_reply.uptime_ms);

  controller.Tick(0U);
  TEST_ASSERT_FALSE(controller.TakePendingEvent(&event));

  controller.Tick(1000U);
  TEST_ASSERT_FALSE(controller.TakePendingEvent(&event));

  controller.Tick(1000U);
  TEST_ASSERT_TRUE(controller.TakePendingEvent(&event));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::EventCode::kRunStop),
                        static_cast<int>(event.code));
  TEST_ASSERT_EQUAL_UINT32(1000U, event.uptime_ms);
  TEST_ASSERT_TRUE(event.analysis_time_valid);
  TEST_ASSERT_EQUAL_UINT32(1000U, event.analysis_time_ms);

  controller.Tick(1000U);
  const ce_cube::TelemetrySnapshot snapshot =
      controller.BuildTelemetry(1U, 1000U);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::RunStateCode::kComplete),
                        static_cast<int>(snapshot.run_state));
  TEST_ASSERT_EQUAL_UINT32(1000U, snapshot.uptime_ms);
  TEST_ASSERT_TRUE(snapshot.analysis_time_valid);
  TEST_ASSERT_EQUAL_UINT32(1000U, snapshot.analysis_time_ms);
  TEST_ASSERT_EQUAL_UINT16(0U, snapshot.faults);
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_status_get_requests_telemetry);
  RUN_TEST(test_protocol_info_is_unavailable_when_disabled);
  RUN_TEST(test_idle_telemetry_has_no_analysis_time);
  RUN_TEST(test_empty_storage_is_invalid_until_protocol_is_uploaded);
  RUN_TEST(test_temporary_protocol_run_emits_events_and_completes);
  return UNITY_END();
}
