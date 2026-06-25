#include <string.h>

#include <unity.h>

#include "ce_cube/feature_flags.hpp"
#include "ce_cube/crc16.hpp"
#include "ce_cube/protocol.hpp"
#include "ce_cube/protocol_store.hpp"
#include "ce_cube/rf24_framing.hpp"

namespace {

size_t CountRf24Frames(ce_cube::MessageKind kind, uint16_t seq,
                       const char* json) {
  ce_cube::Rf24Fragmenter fragmenter;
  if (!fragmenter.Begin(kind, seq, json)) {
    return 0U;
  }

  size_t frame_count = 0U;
  while (!fragmenter.Done()) {
    uint8_t frame[ce_cube::kMaxRf24FrameSize] = {0U};
    size_t frame_length = 0U;
    if (!fragmenter.NextFrame(frame, sizeof(frame), &frame_length)) {
      return 0U;
    }
    ++frame_count;
  }

  return frame_count;
}

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
void test_parse_protocol_begin_command_v1() {
  static const char kJson[] =
      "{\"v\":1,\"kind\":\"command\",\"seq\":7,\"cmd\":\"protocol.begin\","
      "\"slot_count\":12,\"bge1_slot\":0,\"bge2_slot\":1,\"sample_1_slot\":5,"
      "\"sample_count\":2,\"repetitions\":3,\"injection_mode\":\"hydro\","
      "\"collection_duration_ms\":1200,\"injection_duration_ms\":3400,"
      "\"droplet_duration_ms\":5600,\"wait_0_ms\":180000,"
      "\"wait_1_ms\":3600000,\"wait_2_ms\":60000}";
  ce_cube::ParsedCommand command = {};
  const ce_cube::ParseResult result = ce_cube::ParseCommandJson(kJson, &command);

  TEST_ASSERT_TRUE(result.ok);
  TEST_ASSERT_EQUAL_UINT16(7U, command.seq);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::CommandType::kProtocolBegin),
                        static_cast<int>(command.type));
  TEST_ASSERT_EQUAL_UINT8(12U, command.protocol_metadata.slot_count);
  TEST_ASSERT_EQUAL_UINT8(5U, command.protocol_metadata.sample_1_slot);
  TEST_ASSERT_EQUAL_UINT8(2U, command.protocol_metadata.sample_count);
  TEST_ASSERT_EQUAL_UINT8(3U, command.protocol_metadata.repetitions);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ce_cube::InjectionMode::kHydrodynamic),
      static_cast<int>(command.protocol_metadata.injection_mode));
  TEST_ASSERT_EQUAL_UINT32(1200UL,
                           command.protocol_metadata.collection_duration_ms);
  TEST_ASSERT_EQUAL_UINT32(3400UL,
                           command.protocol_metadata.injection_duration_ms);
  TEST_ASSERT_EQUAL_UINT32(5600UL,
                           command.protocol_metadata.droplet_duration_ms);
  TEST_ASSERT_EQUAL_UINT16(1800U, command.protocol_metadata.wait_times_100ms[0]);
  TEST_ASSERT_EQUAL_UINT16(36000U,
                           command.protocol_metadata.wait_times_100ms[1]);
  TEST_ASSERT_EQUAL_UINT16(600U, command.protocol_metadata.wait_times_100ms[2]);
}
#endif

void test_parse_protocol_begin_command_v2() {
  static const char kJson[] =
      "{\"v\":2,\"k\":2,\"s\":7,\"c\":16,\"sc\":12,\"b1\":0,\"b2\":1,"
      "\"s1\":5,\"sn\":2,\"rr\":3,\"im\":0,\"cd\":1200,\"jd\":3400,"
      "\"dd\":5600,\"w0\":180000,\"w1\":3600000,\"w2\":60000}";
  ce_cube::ParsedCommand command = {};
  const ce_cube::ParseResult result = ce_cube::ParseCommandJson(kJson, &command);

  TEST_ASSERT_TRUE(result.ok);
  TEST_ASSERT_EQUAL_UINT16(7U, command.seq);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::CommandType::kProtocolBegin),
                        static_cast<int>(command.type));
  TEST_ASSERT_EQUAL_UINT8(12U, command.protocol_metadata.slot_count);
  TEST_ASSERT_EQUAL_UINT8(5U, command.protocol_metadata.sample_1_slot);
  TEST_ASSERT_EQUAL_UINT8(2U, command.protocol_metadata.sample_count);
  TEST_ASSERT_EQUAL_UINT8(3U, command.protocol_metadata.repetitions);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(ce_cube::InjectionMode::kHydrodynamic),
      static_cast<int>(command.protocol_metadata.injection_mode));
  TEST_ASSERT_EQUAL_UINT32(1200UL,
                           command.protocol_metadata.collection_duration_ms);
  TEST_ASSERT_EQUAL_UINT32(3400UL,
                           command.protocol_metadata.injection_duration_ms);
  TEST_ASSERT_EQUAL_UINT32(5600UL,
                           command.protocol_metadata.droplet_duration_ms);
  TEST_ASSERT_EQUAL_UINT16(1800U, command.protocol_metadata.wait_times_100ms[0]);
  TEST_ASSERT_EQUAL_UINT16(36000U,
                           command.protocol_metadata.wait_times_100ms[1]);
  TEST_ASSERT_EQUAL_UINT16(600U, command.protocol_metadata.wait_times_100ms[2]);
}

void test_mixed_schema_command_is_rejected() {
  static const char kJson[] =
      "{\"v\":2,\"k\":2,\"s\":7,\"c\":16,\"slot_count\":12}";
  ce_cube::ParsedCommand command = {};
  const ce_cube::ParseResult result = ce_cube::ParseCommandJson(kJson, &command);

  TEST_ASSERT_FALSE(result.ok);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ErrorCode::kInvalidField),
                        static_cast<int>(result.error));
}

void test_encode_ack_and_error_use_compact_v2_schema() {
  char json[64] = "";
  size_t length = 0U;
  ce_cube::ReplyMessage ack = {ce_cube::MessageKind::kAck, 7U,
                               ce_cube::AckCode::kStatus,
                               ce_cube::ErrorCode::kNone};
  ce_cube::ReplyMessage error = {ce_cube::MessageKind::kError, 9U,
                                 ce_cube::AckCode::kAccepted,
                                 ce_cube::ErrorCode::kBusy};

  TEST_ASSERT_TRUE(ce_cube::EncodeAckJson(ack, json, sizeof(json), &length));
  TEST_ASSERT_EQUAL_STRING("{\"v\":2,\"k\":3,\"s\":7,\"a\":1}", json);
  TEST_ASSERT_EQUAL_UINT(strlen(json), length);

  TEST_ASSERT_TRUE(
      ce_cube::EncodeErrorJson(error, json, sizeof(json), &length));
  TEST_ASSERT_EQUAL_STRING("{\"v\":2,\"k\":4,\"s\":9,\"e\":4}", json);
  TEST_ASSERT_EQUAL_UINT(strlen(json), length);
}

void test_encode_standard_event_meets_compact_budget() {
  char json[ce_cube::kMaxEventJsonLength + 1U] = "";
  size_t length = 0U;
  ce_cube::EventSnapshot event = {};
  event.code = ce_cube::EventCode::kRunStart;
  event.uptime_ms = 1234U;
  event.analysis_time_ms = 0U;
  event.analysis_time_valid = true;
  event.sample_index = 1U;
  event.sample_slot = 5U;
  event.repetition = 2U;

  TEST_ASSERT_TRUE(
      ce_cube::EncodeEventJson(event, 12U, json, sizeof(json), &length));
  TEST_ASSERT_EQUAL_STRING(
      "{\"v\":2,\"k\":5,\"s\":12,\"ev\":1,\"gt\":1234,\"at\":0,\"si\":1,"
      "\"sl\":5,\"ri\":2}",
      json);
  TEST_ASSERT_EQUAL_UINT(strlen(json), length);
  TEST_ASSERT_TRUE(length <= 72U);
  TEST_ASSERT_TRUE(
      CountRf24Frames(ce_cube::MessageKind::kEvent, 12U, json) <= 3U);
}

void test_encode_bare_telemetry_meets_compact_budget() {
  char json[ce_cube::kMaxTelemetryJsonLength + 1U] = "";
  size_t length = 0U;
  const ce_cube::TelemetrySnapshot snapshot = {
      1U,
      0.0F,
      0.0F,
      ce_cube::SensorStatusCode::kBusError,
      0U,
      ce_cube::PressureStatusCode::kUnavailable,
      0L,
      false,
      false,
      false,
      false,
      true,
      ce_cube::LiftStateCode::kDown,
      0U,
      ce_cube::RunStateCode::kIdle,
      0U,
      0U,
      0U,
      0U,
      0U,
      false,
      36U,
  };

  TEST_ASSERT_TRUE(
      ce_cube::EncodeTelemetryJson(snapshot, json, sizeof(json), &length));
  TEST_ASSERT_TRUE(length <= 216U);
  TEST_ASSERT_NULL(strstr(json, "\"at\""));
  TEST_ASSERT_EQUAL_STRING(
      "{\"v\":2,\"k\":1,\"s\":1,\"cp\":0.000000,\"tc\":0.000,\"ss\":2,"
      "\"pp\":0,\"ps\":1,\"cu\":0,\"hv\":0,\"pm\":0,\"v1\":0,\"v2\":0,\"pv\":1,"
      "\"lf\":0,\"cs\":0,\"rs\":0,\"pc\":0,\"si\":0,\"sl\":0,\"ri\":0,"
      "\"ff\":36}",
      json);
  TEST_ASSERT_TRUE(
      CountRf24Frames(ce_cube::MessageKind::kTelemetry, snapshot.seq, json) <=
      10U);
}

void test_encode_representative_telemetry_uses_compact_keys() {
  char json[ce_cube::kMaxTelemetryJsonLength + 1U] = "";
  size_t length = 0U;
  const ce_cube::TelemetrySnapshot snapshot = {
      42U,
      1.234567F,
      20.125F,
      ce_cube::SensorStatusCode::kOk,
      101325U,
      ce_cube::PressureStatusCode::kOk,
      3210L,
      true,
      false,
      true,
      false,
      true,
      ce_cube::LiftStateCode::kUp,
      5U,
      ce_cube::RunStateCode::kRunning,
      9U,
      1U,
      5U,
      2U,
      3456U,
      true,
      3U,
  };

  TEST_ASSERT_TRUE(
      ce_cube::EncodeTelemetryJson(snapshot, json, sizeof(json), &length));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"v\":2"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"k\":1"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"cp\":1.234567"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"pp\":101325"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"cu\":3210"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"pc\":9"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"at\":3456"));
  TEST_ASSERT_NULL(strstr(json, "\"kind\""));
  TEST_ASSERT_NULL(strstr(json, "\"protocol_valid\""));
  TEST_ASSERT_TRUE(length <= 240U);
  TEST_ASSERT_TRUE(
      CountRf24Frames(ce_cube::MessageKind::kTelemetry, snapshot.seq, json) <=
      11U);
  TEST_ASSERT_EQUAL_UINT(strlen(json), length);
}

void test_protocol_store_commit_and_load() {
  ce_cube::ProtocolStore::ResetTestStorage();
  ce_cube::ProtocolStore store;
  store.Begin();

  ce_cube::ProtocolMetadata metadata = {};
  metadata.slot_count = ce_cube::kDefaultSlotCount;
  metadata.bge1_slot = 0U;
  metadata.bge2_slot = 1U;
  metadata.sample_1_slot = 5U;
  metadata.sample_count = 2U;
  metadata.repetitions = 2U;
  metadata.injection_mode = ce_cube::InjectionMode::kHydrodynamic;
  metadata.collection_duration_ms = 1200UL;
  metadata.injection_duration_ms = 3400UL;
  metadata.droplet_duration_ms = 5600UL;
  metadata.wait_times_100ms[0] = 25U;

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
          1U,
          static_cast<uint16_t>(sizeof(program)),
          ce_cube::ComputeCrc16Ccitt(program, sizeof(program)))));

  ce_cube::ProtocolInfo info = {};
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ProtocolStoreStatus::kOk),
                        static_cast<int>(store.LoadInfo(&info)));
  TEST_ASSERT_TRUE(info.valid);
  TEST_ASSERT_EQUAL_UINT8(2U, info.metadata.sample_count);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::InjectionMode::kHydrodynamic),
                        static_cast<int>(info.metadata.injection_mode));
  TEST_ASSERT_EQUAL_UINT32(1200UL, info.metadata.collection_duration_ms);
  TEST_ASSERT_EQUAL_UINT16(1U, info.prepare_length);
  TEST_ASSERT_EQUAL_UINT16(2U, info.program_length);
  TEST_ASSERT_EQUAL_UINT16(
      ce_cube::ComputeCrc16Ccitt(program, sizeof(program)), info.program_crc16);
}

void test_compact_command_frame_budgets() {
  static const char kStatusJson[] = "{\"v\":2,\"k\":2,\"s\":1,\"c\":21}";
  static const char kProtocolBeginJson[] =
      "{\"v\":2,\"k\":2,\"s\":2,\"c\":16,\"sc\":12,\"b1\":0,\"b2\":1,"
      "\"s1\":5,\"sn\":1,\"rr\":1,\"im\":1,\"cd\":1200,\"jd\":3400,"
      "\"dd\":5600,\"w0\":1000,\"w1\":0,\"w2\":0,\"w3\":0,\"w4\":0,"
      "\"w5\":0,\"w6\":0,\"w7\":0}";

  TEST_ASSERT_TRUE(strlen(kStatusJson) <= 32U);
  TEST_ASSERT_TRUE(CountRf24Frames(ce_cube::MessageKind::kCommand, 1U,
                                   kStatusJson) <= 2U);
  TEST_ASSERT_TRUE(strlen(kProtocolBeginJson) <= 176U);
  TEST_ASSERT_TRUE(CountRf24Frames(ce_cube::MessageKind::kCommand, 2U,
                                   kProtocolBeginJson) <= 8U);
}

void test_rf24_framing_round_trip_v2() {
  static const char kJson[] = "{\"v\":2,\"k\":2,\"s\":11,\"c\":7,\"sl\":10}";
  ce_cube::Rf24Fragmenter fragmenter;
  ce_cube::Rf24Reassembler reassembler;
  char rebuilt[ce_cube::kMaxCommandJsonLength + 1U] = "";
  ce_cube::MessageKind kind = ce_cube::MessageKind::kInvalid;
  uint16_t seq = 0U;
  size_t rebuilt_length = 0U;

  TEST_ASSERT_TRUE(fragmenter.Begin(ce_cube::MessageKind::kCommand, 11U, kJson));

  while (!fragmenter.Done()) {
    uint8_t frame[ce_cube::kMaxRf24FrameSize] = {0U};
    size_t frame_length = 0U;
    TEST_ASSERT_TRUE(fragmenter.NextFrame(frame, sizeof(frame), &frame_length));
    TEST_ASSERT_EQUAL_UINT8(ce_cube::kProtocolVersion, frame[0]);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ErrorCode::kNone),
                          static_cast<int>(reassembler.PushFrame(
                              frame, frame_length, rebuilt, sizeof(rebuilt))));
  }

  TEST_ASSERT_TRUE(reassembler.HasMessage());
  TEST_ASSERT_TRUE(reassembler.TakeMessage(&kind, &seq, &rebuilt_length));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::MessageKind::kCommand),
                        static_cast<int>(kind));
  TEST_ASSERT_EQUAL_UINT16(11U, seq);
  TEST_ASSERT_EQUAL_UINT(strlen(kJson), rebuilt_length);
  TEST_ASSERT_EQUAL_STRING(kJson, rebuilt);
}

void test_rf24_reassembler_accepts_legacy_frame_version() {
  static const char kJson[] =
      "{\"v\":1,\"kind\":\"command\",\"seq\":11,\"cmd\":\"status.get\"}";
  ce_cube::Rf24Fragmenter fragmenter;
  ce_cube::Rf24Reassembler reassembler;
  char rebuilt[ce_cube::kMaxCommandJsonLength + 1U] = "";
  ce_cube::MessageKind kind = ce_cube::MessageKind::kInvalid;
  uint16_t seq = 0U;
  size_t rebuilt_length = 0U;

  TEST_ASSERT_TRUE(fragmenter.Begin(ce_cube::MessageKind::kCommand, 11U, kJson));

  while (!fragmenter.Done()) {
    uint8_t frame[ce_cube::kMaxRf24FrameSize] = {0U};
    size_t frame_length = 0U;
    TEST_ASSERT_TRUE(fragmenter.NextFrame(frame, sizeof(frame), &frame_length));
    frame[0] = ce_cube::kLegacyProtocolVersion;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ce_cube::ErrorCode::kNone),
                          static_cast<int>(reassembler.PushFrame(
                              frame, frame_length, rebuilt, sizeof(rebuilt))));
  }

  TEST_ASSERT_TRUE(reassembler.HasMessage());
  TEST_ASSERT_TRUE(reassembler.TakeMessage(&kind, &seq, &rebuilt_length));
  TEST_ASSERT_EQUAL_UINT(strlen(kJson), rebuilt_length);
  TEST_ASSERT_EQUAL_STRING(kJson, rebuilt);
}

}  // namespace

void setUp() {}

void tearDown() {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
  RUN_TEST(test_parse_protocol_begin_command_v1);
#endif
  RUN_TEST(test_parse_protocol_begin_command_v2);
  RUN_TEST(test_mixed_schema_command_is_rejected);
  RUN_TEST(test_encode_ack_and_error_use_compact_v2_schema);
  RUN_TEST(test_encode_standard_event_meets_compact_budget);
  RUN_TEST(test_encode_bare_telemetry_meets_compact_budget);
  RUN_TEST(test_encode_representative_telemetry_uses_compact_keys);
  RUN_TEST(test_protocol_store_commit_and_load);
  RUN_TEST(test_compact_command_frame_budgets);
  RUN_TEST(test_rf24_framing_round_trip_v2);
  RUN_TEST(test_rf24_reassembler_accepts_legacy_frame_version);
  return UNITY_END();
}
