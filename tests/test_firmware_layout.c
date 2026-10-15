#include "metatron_api.h"

#include <stdio.h>
#include <string.h>

static int g_failed;

static void expect(int cond, const char *message)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", message);
        g_failed++;
    }
}

static void test_crc_table(void)
{
    static const uint8_t prefix[8] = {0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15};
    int i;
    for (i = 0; i < 8; i++) {
        uint8_t b = (uint8_t)i;
        expect(mt_crc8(&b, 1) == prefix[i], "firmware crc table");
    }
}

static void test_bitmap(void)
{
    uint8_t body[6];
    uint8_t payload[14];
    mt_sample sample;
    body[0] = 0x05;
    body[1] = 2;
    body[2] = 10;
    body[3] = 0;
    body[4] = 0;
    body[5] = 0;
    payload[0] = 1;
    payload[1] = 0;
    payload[2] = 0;
    payload[3] = 0;
    payload[4] = 0;
    payload[5] = MT_MOD_HALL_ULTRA;
    payload[6] = 6;
    payload[7] = 0;
    memcpy(payload + 8, body, 6);
    expect(mt_hall_bit(0x05, 0) == 1, "hall0 bit");
    expect(mt_hall_bit(0x05, 1) == 0, "hall1 bit");
    expect(mt_hall_bit(0x05, 2) == 1, "hall2 bit");
    expect(mt_hall_bit(0x05, 3) == 0, "hall3 bit");
    expect(mt_parse_data("PILLAR_01", MT_MOD_HALL_ULTRA, 1, payload, 14, &sample) == MT_OK, "pattern parse");
    expect(sample.kind == MT_KIND_HALL_PATTERN, "pattern kind");
    expect(sample.hall_bit[0] == 1 && sample.hall_bit[2] == 1, "bitmap copied");
    expect(sample.hall_bit[1] == 0 && sample.hall_bit[3] == 0, "clear bits");
}

static void test_channel4(void)
{
    uint8_t hall[20];
    uint8_t ultra[16];
    mt_sample sample;
    float distance = 12.5f;
    memset(hall, 0, sizeof(hall));
    hall[4] = 4;
    hall[5] = MT_MOD_HALL_ULTRA;
    hall[6] = 12;
    hall[8] = 1;
    expect(mt_parse_data("PILLAR_01", MT_MOD_HALL_ULTRA, 1, hall, 20, &sample) == MT_OK, "hall ch4");
    expect(sample.kind == MT_KIND_HALL && sample.sensor_index == 3, "hall sensor 3");
    memset(ultra, 0, sizeof(ultra));
    ultra[4] = 4;
    ultra[5] = MT_MOD_HALL_ULTRA;
    ultra[6] = 8;
    memcpy(ultra + 8, &distance, 4);
    ultra[12] = 80;
    ultra[13] = 1;
    expect(mt_parse_data("PILLAR_02", MT_MOD_HALL_ULTRA, 1, ultra, 16, &sample) == MT_OK, "ultra ch4");
    expect(sample.kind == MT_KIND_ULTRASONIC && sample.sensor_index == 0, "ultra sensor 0");
    hall[6] = 1;
    expect(mt_parse_data("PILLAR_01", MT_MOD_HALL_ULTRA, 1, hall, 9, &sample) == MT_OK, "bad length");
    expect(sample.kind == MT_KIND_AMBIGUOUS, "not guessed");
}

static void test_pillar4_raw_and_towers(void)
{
    uint8_t payload[12];
    mt_sample sample;
    mt_topology *topo = NULL;
    mt_observed found[2];
    mt_assignment rows[8];
    int n = 0;
    int i;
    memset(payload, 0, sizeof(payload));
    payload[5] = 0x1D;
    payload[6] = 4;
    expect(mt_parse_data("PILLAR_04", 0x1D, 1, payload, 12, &sample) == MT_OK, "pillar4");
    expect(sample.kind == MT_KIND_RAW && sample.fields_valid == 0, "pillar4 raw");
    expect(mt_topology_load("config/towers.txt", &topo) == MT_OK, "map");
    memset(found, 0, sizeof(found));
    snprintf(found[0].identity.firmware_id, 16, "PILLAR_01");
    found[0].identity.module_type = MT_MOD_HALL_ULTRA;
    snprintf(found[1].identity.firmware_id, 16, "PILLAR_04");
    found[1].identity.module_type = 0x1D;
    expect(mt_topology_assign(topo, found, 2, rows, 8, &n) == MT_OK, "assign");
    for (i = 0; i < n; i++) {
        if (strcmp(rows[i].identity.firmware_id, "PILLAR_01") == 0) {
            expect(strcmp(rows[i].tower, "Tower_1") == 0 && rows[i].status == MT_STATUS_ASSIGNED, "tower 1 pillar 1");
        }
        if (strcmp(rows[i].identity.firmware_id, "PILLAR_04") == 0) {
            expect(strcmp(rows[i].tower, "Tower_1") == 0 && strcmp(rows[i].slot, "Pillar_04") == 0, "tower 1 pillar 4");
        }
    }
    mt_topology_free(topo);
}

static void test_session_bytes(void)
{
    uint8_t request[8];
    size_t request_len = 0;
    uint8_t blocked[8];
    size_t blocked_len = 0;
    const char *json = "{\"id\":\"PILLAR_01\",\"type\":23,\"fw\":\"1.0.0\",\"channels\":6,\"health\":\"OK\"}";
    uint8_t ident_frame[160];
    size_t ident_len = 0;
    mt_parser *parser;
    mt_frame frame;
    int got = 0;
    mt_identity identity;
    uint8_t beat[5] = {0x00, 0x00, 0x00, 0x64, 0x00};
    uint32_t uptime = 0;
    uint8_t health = 0xFF;
    uint8_t status_body[19];
    uint8_t status_frame[32];
    size_t status_len = 0;
    uint8_t pattern_body[6] = {0x05, 2, 10, 0, 0, 0};
    uint8_t data_payload[14];
    uint8_t data_frame[24];
    size_t data_len = 0;
    mt_sample sample;
    mt_record *record = NULL;
    char path_buf[32];
    int i;
    expect(mt_ident_request(request, sizeof(request), &request_len) == MT_OK, "ident request");
    expect(request_len == 5 && request[0] == 0xAA && request[1] == 1 && request[2] == MT_MSG_IDENT && request[4] == 0x55, "empty ident frame");
    expect(mt_build_frame(0x04, NULL, 0, blocked, sizeof(blocked), &blocked_len) == MT_ERR_BAD_ARG, "no calibrate");
    expect(mt_build_frame(0x09, NULL, 0, blocked, sizeof(blocked), &blocked_len) == MT_ERR_BAD_ARG, "no command");
    expect(mt_build_frame(MT_MSG_IDENT, (const uint8_t *)json, strlen(json), ident_frame, sizeof(ident_frame), &ident_len) == MT_OK, "ident frame");
    parser = mt_parser_new();
    expect(parser != NULL, "parser");
    expect(mt_parser_push(parser, ident_frame, ident_len, 1000, &frame, &got) == MT_OK && got == 1, "parse ident frame");
    expect(frame.host_unix_ms == 1000 && frame.crc_window == MT_CRC_CANONICAL, "host time separate");
    expect(mt_parse_ident(frame.payload, frame.payload_len, &identity) == MT_OK, "ident fields");
    expect(strcmp(identity.firmware_id, "PILLAR_01") == 0 && identity.module_type == MT_MOD_HALL_ULTRA, "pillar 01");
    expect(identity.has_uid == 0 && identity.channel_count == 6, "no uid");
    expect(mt_parse_heartbeat(beat, 5, &uptime, &health) == MT_OK && uptime == 100 && health == 0, "heartbeat");
    for (i = 0; i < 19; i++) {
        status_body[i] = (uint8_t)(0xA0 + i);
    }
    expect(mt_build_frame(MT_MSG_STATUS, status_body, 19, status_frame, sizeof(status_frame), &status_len) == MT_OK, "status frame");
    got = 0;
    expect(mt_parser_push(parser, status_frame, status_len, 2000, &frame, &got) == MT_OK && got == 1, "status raw");
    expect(frame.payload_len == 19 && memcmp(frame.payload, status_body, 19) == 0, "status bytes unchanged");
    memset(data_payload, 0, sizeof(data_payload));
    data_payload[0] = 1;
    data_payload[5] = MT_MOD_HALL_ULTRA;
    data_payload[6] = 6;
    memcpy(data_payload + 8, pattern_body, 6);
    expect(mt_build_frame(MT_MSG_DATA, data_payload, 14, data_frame, sizeof(data_frame), &data_len) == MT_OK, "data frame");
    got = 0;
    expect(mt_parser_push(parser, data_frame, data_len, 3000, &frame, &got) == MT_OK && got == 1, "data parsed");
    expect(mt_parse_data(identity.firmware_id, identity.module_type, 1, frame.payload, frame.payload_len, &sample) == MT_OK, "hall from frame");
    expect(sample.kind == MT_KIND_HALL_PATTERN && sample.hall_bit[0] == 1 && sample.hall_bit[2] == 1, "frame bitmap");
    expect(sample.firmware_millis == 1 && frame.host_unix_ms == 3000, "two clocks");
    expect(mt_record_create("build/layout_record.bin", &record) == MT_OK, "record create");
    expect(mt_record_write(record, &frame) == MT_OK, "record write");
    mt_record_close(record);
    record = NULL;
    expect(mt_record_open("build/layout_record.bin", &record) == MT_OK, "record open");
    got = 0;
    expect(mt_record_read(record, &frame, &got) == MT_OK && got == 1, "record read");
    expect(frame.host_unix_ms == 3000 && frame.type == MT_MSG_DATA, "record kept host time");
    mt_record_close(record);
    mt_parser_free(parser);
    expect(mt_port_path("COM12", path_buf, sizeof(path_buf)) == MT_OK, "port path");
    expect(strcmp(path_buf, "\\\\.\\COM12") == 0, "win32 path");
    expect(mt_port_path("LPT1", path_buf, sizeof(path_buf)) == MT_ERR_BAD_ARG, "not a com port");
}

int main(void)
{
    test_crc_table();
    test_bitmap();
    test_channel4();
    test_pillar4_raw_and_towers();
    test_session_bytes();
    if (g_failed != 0) {
        fprintf(stderr, "%d failed\n", g_failed);
        return 1;
    }
    printf("firmware layout checks passed\n");
    return 0;
}
