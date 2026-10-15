#include "metatron_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CRC-8 table from GRAVFLYER/firmware/common/protocol.cpp. Polynomial 0x07. */
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

struct mt_topology {
    char slot[6][16];
    char tower[6][16];
    char firmware_id[6][16];
    int count;
};

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint8_t mt_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    size_t i;
    if (data == NULL && len > 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

int mt_frame_crc(const uint8_t *frame, size_t len)
{
    uint8_t declared;
    uint8_t received;
    if (frame == NULL || len < 5 || frame[0] != 0xAA || frame[len - 1] != 0x55) {
        return 0;
    }
    declared = frame[1];
    if (declared < 1 || (size_t)declared + 4 != len) {
        return 0;
    }
    received = frame[len - 2];
    if (received == mt_crc8(frame + 1, len - 3)) {
        return MT_CRC_CANONICAL;
    }
    if (received == mt_crc8(frame + 2, len - 4)) {
        return MT_CRC_LEGACY;
    }
    return 0;
}

int mt_build_frame(uint8_t type, const uint8_t *payload, size_t len,
                   uint8_t *out, size_t cap, size_t *out_len)
{
    if (type == 0x04 || type == 0x09) {
        return MT_ERR_BAD_ARG;
    }
    if (out == NULL || out_len == NULL || (payload == NULL && len > 0) || len > 128) {
        return MT_ERR_BAD_ARG;
    }
    if (cap < len + 5) {
        return MT_ERR_BAD_ARG;
    }
    out[0] = 0xAA;
    out[1] = (uint8_t)(len + 1);
    out[2] = type;
    if (len > 0) {
        memcpy(out + 3, payload, len);
    }
    out[len + 3] = mt_crc8(out + 1, len + 2);
    out[len + 4] = 0x55;
    *out_len = len + 5;
    return MT_OK;
}

static int json_string(const char *json, size_t len, const char *key, char *out, size_t cap)
{
    char pattern[24];
    size_t key_len;
    size_t i;
    const char *hit;
    const char *end;
    size_t n;
    if (cap == 0) {
        return 0;
    }
    out[0] = '\0';
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    key_len = strlen(pattern);
    hit = NULL;
    for (i = 0; i + key_len <= len; i++) {
        if (memcmp(json + i, pattern, key_len) == 0) {
            hit = json + i + key_len;
            break;
        }
    }
    if (hit == NULL) {
        return 0;
    }
    end = json + len;
    while (hit < end && (*hit == ' ' || *hit == ':' )) {
        hit++;
    }
    if (hit >= end || *hit != '"') {
        return 0;
    }
    hit++;
    n = 0;
    while (hit < end && *hit != '"' && n + 1 < cap) {
        out[n++] = *hit++;
    }
    out[n] = '\0';
    return 1;
}

static int json_uint(const char *json, size_t len, const char *key, unsigned *out)
{
    char text[24];
    char pattern[24];
    size_t key_len;
    size_t i;
    const char *hit;
    const char *end;
    unsigned value = 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    key_len = strlen(pattern);
    hit = NULL;
    for (i = 0; i + key_len <= len; i++) {
        if (memcmp(json + i, pattern, key_len) == 0) {
            hit = json + i + key_len;
            break;
        }
    }
    if (hit == NULL) {
        return 0;
    }
    end = json + len;
    while (hit < end && (*hit == ' ' || *hit == ':')) {
        hit++;
    }
    i = 0;
    while (hit < end && *hit >= '0' && *hit <= '9' && i + 1 < sizeof(text)) {
        text[i++] = *hit++;
    }
    text[i] = '\0';
    if (i == 0) {
        return 0;
    }
    value = (unsigned)strtoul(text, NULL, 10);
    *out = value;
    return 1;
}

int mt_parse_ident(const uint8_t *json, size_t len, mt_identity *out)
{
    unsigned type_value;
    unsigned channels;
    if (json == NULL || out == NULL) {
        return MT_ERR_BAD_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!json_string((const char *)json, len, "id", out->firmware_id, sizeof(out->firmware_id))) {
        return MT_ERR_BAD_ARG;
    }
    if (!json_uint((const char *)json, len, "type", &type_value) || type_value > 255u) {
        return MT_ERR_BAD_ARG;
    }
    if (!json_string((const char *)json, len, "fw", out->firmware_version, sizeof(out->firmware_version))) {
        return MT_ERR_BAD_ARG;
    }
    if (!json_uint((const char *)json, len, "channels", &channels) || channels > 255u) {
        return MT_ERR_BAD_ARG;
    }
    if (!json_string((const char *)json, len, "health", out->health, sizeof(out->health))) {
        return MT_ERR_BAD_ARG;
    }
    out->module_type = (uint8_t)type_value;
    out->channel_count = (uint8_t)channels;
    if (json_string((const char *)json, len, "uid", out->uid, sizeof(out->uid)) && out->uid[0] != '\0') {
        out->has_uid = 1;
    }
    return MT_OK;
}

int mt_hall_bit(uint8_t pattern, int sensor)
{
    if (sensor < 0 || sensor >= MT_HALL_COUNT) {
        return 0;
    }
    return (pattern & (uint8_t)(1u << sensor)) != 0;
}

static int pillar_number(const char *firmware_id)
{
    if (firmware_id == NULL) {
        return 0;
    }
    if (strcmp(firmware_id, "PILLAR_01") == 0) return 1;
    if (strcmp(firmware_id, "PILLAR_02") == 0) return 2;
    if (strcmp(firmware_id, "PILLAR_03") == 0) return 3;
    if (strcmp(firmware_id, "PILLAR_04") == 0) return 4;
    if (strcmp(firmware_id, "PILLAR_05") == 0) return 5;
    if (strcmp(firmware_id, "PILLAR_06") == 0) return 6;
    return 0;
}

static void set_reason(mt_sample *out, const char *reason)
{
    snprintf(out->reason, sizeof(out->reason), "%s", reason);
}

int mt_parse_data(const char *firmware_id, uint8_t ident_type, int has_ident,
                  const uint8_t *data_payload, size_t len, mt_sample *out)
{
    uint16_t body_len;
    const uint8_t *body;
    int pillar;
    int i;
    if (out == NULL || data_payload == NULL || len < 8) {
        return MT_ERR_BAD_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->sensor_index = -1;
    if (firmware_id != NULL) {
        snprintf(out->firmware_id, sizeof(out->firmware_id), "%s", firmware_id);
    }
    out->firmware_millis = rd_u32(data_payload);
    out->channel = data_payload[4];
    out->module_type = data_payload[5];
    body_len = rd_u16(data_payload + 6);
    if ((size_t)body_len + 8 != len || body_len > sizeof(out->body)) {
        return MT_ERR_BAD_ARG;
    }
    body = data_payload + 8;
    out->body_len = body_len;
    memcpy(out->body, body, body_len);
    pillar = pillar_number(firmware_id);
    if (!has_ident || pillar < 1 || pillar > 3 || ident_type != MT_MOD_HALL_ULTRA ||
        data_payload[5] != MT_MOD_HALL_ULTRA) {
        out->kind = MT_KIND_RAW;
        if (pillar >= 4 && pillar <= 6) {
            set_reason(out, "firmware_payload_raw");
        }
        return MT_OK;
    }
    if (out->channel == 0 && body_len == 6) {
        out->kind = MT_KIND_HALL_PATTERN;
        out->pattern = body[0];
        out->active_count = body[1];
        for (i = 0; i < MT_HALL_COUNT; i++) {
            out->hall_bit[i] = (uint8_t)mt_hall_bit(out->pattern, i);
        }
        out->fields_valid = 1;
        return MT_OK;
    }
    if (out->channel >= 1 && out->channel <= 4 && body_len == 12) {
        out->kind = MT_KIND_HALL;
        out->sensor_index = (int32_t)out->channel - 1;
        out->magnet_detected = body[0];
        out->hall_state = body[1];
        out->trigger_count = rd_u16(body + 2);
        out->raw_analog = rd_u16(body + 4);
        out->baseline = rd_u16(body + 6);
        out->threshold = rd_u16(body + 8);
        out->quality = body[10];
        out->fields_valid = 1;
        return MT_OK;
    }
    if ((out->channel == 4 || out->channel == 5) && body_len == 8) {
        uint32_t bits = rd_u32(body);
        float distance;
        memcpy(&distance, &bits, sizeof(distance));
        out->kind = MT_KIND_ULTRASONIC;
        out->sensor_index = out->channel == 4 ? 0 : 1;
        out->distance_cm = distance;
        out->quality = body[4];
        out->ultra_valid = body[5];
        out->stddev_x100 = rd_u16(body + 6);
        out->fields_valid = 1;
        return MT_OK;
    }
    out->kind = MT_KIND_AMBIGUOUS;
    if (out->channel == 4) {
        set_reason(out, "channel4_length");
    } else {
        set_reason(out, "schema_mismatch");
    }
    return MT_OK;
}

int mt_topology_load(const char *path, mt_topology **out)
{
    FILE *file;
    char line[160];
    mt_topology *topo;
    if (path == NULL || out == NULL) {
        return MT_ERR_BAD_ARG;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        return MT_ERR_IO;
    }
    topo = (mt_topology *)calloc(1, sizeof(*topo));
    if (topo == NULL) {
        fclose(file);
        return MT_ERR_IO;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char kind[16];
        char slot[16];
        char tower[16];
        char firmware_id[16];
        char *hash = strchr(line, '#');
        int n;
        if (hash != NULL) {
            *hash = '\0';
        }
        n = sscanf(line, "%15s %15s %15s %15s", kind, slot, tower, firmware_id);
        if (n <= 0) {
            continue;
        }
        if (n != 4 || strcmp(kind, "pillar") != 0 || topo->count >= 6) {
            free(topo);
            fclose(file);
            return MT_ERR_BAD_ARG;
        }
        snprintf(topo->slot[topo->count], 16, "%s", slot);
        snprintf(topo->tower[topo->count], 16, "%s", tower);
        snprintf(topo->firmware_id[topo->count], 16, "%s", firmware_id);
        topo->count++;
    }
    fclose(file);
    *out = topo;
    return MT_OK;
}

void mt_topology_free(mt_topology *topo)
{
    free(topo);
}

int mt_topology_assign(const mt_topology *topo,
                       const mt_observed *found, int found_count,
                       mt_assignment *out, int out_cap, int *out_count)
{
    int used[6];
    int match[16];
    int i;
    int n = 0;
    if (topo == NULL || out == NULL || out_count == NULL || found_count < 0 || out_cap < 1) {
        return MT_ERR_BAD_ARG;
    }
    memset(used, 0, sizeof(used));
    for (i = 0; i < found_count && i < 16; i++) {
        int s;
        match[i] = -1;
        for (s = 0; s < topo->count; s++) {
            if (strcmp(topo->firmware_id[s], found[i].identity.firmware_id) == 0) {
                match[i] = s;
            }
        }
    }
    for (i = 0; i < found_count && i < 16; i++) {
        int dup = 0;
        int j;
        mt_assignment *row;
        if (n >= out_cap) {
            return MT_ERR_BAD_ARG;
        }
        row = &out[n++];
        memset(row, 0, sizeof(*row));
        row->identity = found[i].identity;
        if (match[i] < 0) {
            row->status = MT_STATUS_UNASSIGNED;
            continue;
        }
        for (j = 0; j < found_count && j < 16; j++) {
            if (j != i && match[j] == match[i]) {
                dup = 1;
            }
        }
        snprintf(row->slot, sizeof(row->slot), "%s", topo->slot[match[i]]);
        snprintf(row->tower, sizeof(row->tower), "%s", topo->tower[match[i]]);
        row->status = dup ? MT_STATUS_DUPLICATE : MT_STATUS_ASSIGNED;
        if (!dup) {
            used[match[i]] = 1;
        }
    }
    for (i = 0; i < topo->count; i++) {
        mt_assignment *row;
        if (used[i] || n >= out_cap) {
            continue;
        }
        row = &out[n++];
        memset(row, 0, sizeof(*row));
        snprintf(row->slot, sizeof(row->slot), "%s", topo->slot[i]);
        snprintf(row->tower, sizeof(row->tower), "%s", topo->tower[i]);
        snprintf(row->identity.firmware_id, sizeof(row->identity.firmware_id), "%s", topo->firmware_id[i]);
        row->status = MT_STATUS_MISSING;
    }
    *out_count = n;
    return MT_OK;
}

const char *mt_kind_name(int kind)
{
    switch (kind) {
    case MT_KIND_HALL_PATTERN: return "hall_pattern";
    case MT_KIND_HALL: return "hall";
    case MT_KIND_ULTRASONIC: return "ultrasonic";
    case MT_KIND_AMBIGUOUS: return "ambiguous";
    default: return "raw";
    }
}

const char *mt_status_name(int status)
{
    switch (status) {
    case MT_STATUS_ASSIGNED: return "assigned";
    case MT_STATUS_MISSING: return "missing";
    case MT_STATUS_DUPLICATE: return "duplicate";
    case MT_STATUS_UNASSIGNED: return "unassigned";
    default: return "unknown";
    }
}

size_t mt_sizeof_identity(void) { return sizeof(mt_identity); }
size_t mt_sizeof_sample(void) { return sizeof(mt_sample); }
size_t mt_sizeof_frame(void) { return sizeof(mt_frame); }
size_t mt_sizeof_assignment(void) { return sizeof(mt_assignment); }

int mt_ident_request(uint8_t *out, size_t cap, size_t *out_len)
{
    return mt_build_frame(MT_MSG_IDENT, NULL, 0, out, cap, out_len);
}

int mt_parse_heartbeat(const uint8_t *payload, size_t len, uint32_t *uptime, uint8_t *health)
{
    if (payload == NULL || uptime == NULL || health == NULL || len != 5) {
        return MT_ERR_BAD_ARG;
    }
    *uptime = ((uint32_t)payload[0] << 24) | ((uint32_t)payload[1] << 16) |
              ((uint32_t)payload[2] << 8) | (uint32_t)payload[3];
    *health = payload[4];
    return MT_OK;
}

int mt_parse_ack(const uint8_t *payload, size_t len, uint8_t *acked_type, uint8_t *code)
{
    if (payload == NULL || acked_type == NULL || code == NULL || len != 2) {
        return MT_ERR_BAD_ARG;
    }
    *acked_type = payload[0];
    *code = payload[1];
    return MT_OK;
}

int mt_parse_error(const uint8_t *payload, size_t len, uint8_t *code, char *message, size_t cap)
{
    size_t i;
    if (payload == NULL || code == NULL || message == NULL || cap == 0 || len < 2) {
        return MT_ERR_BAD_ARG;
    }
    *code = payload[0];
    i = 0;
    while (i + 1 < len && i + 1 < cap && payload[1 + i] != 0) {
        message[i] = (char)payload[1 + i];
        i++;
    }
    message[i] = '\0';
    return MT_OK;
}

struct mt_parser {
    uint8_t buf[MT_MAX_FRAME * 2];
    size_t len;
};

mt_parser *mt_parser_new(void)
{
    mt_parser *parser = (mt_parser *)calloc(1, sizeof(*parser));
    return parser;
}

void mt_parser_free(mt_parser *parser)
{
    free(parser);
}

void mt_parser_reset(mt_parser *parser)
{
    if (parser != NULL) {
        parser->len = 0;
    }
}

static void parser_drop(mt_parser *parser, size_t n)
{
    if (n >= parser->len) {
        parser->len = 0;
        return;
    }
    memmove(parser->buf, parser->buf + n, parser->len - n);
    parser->len -= n;
}

int mt_parser_push(mt_parser *parser, const uint8_t *data, size_t len,
                   uint64_t host_unix_ms, mt_frame *frame, int *got)
{
    if (parser == NULL || frame == NULL || got == NULL || (data == NULL && len > 0)) {
        return MT_ERR_BAD_ARG;
    }
    *got = 0;
    if (len > 0) {
        if (parser->len + len > sizeof(parser->buf)) {
            return MT_ERR_BAD_ARG;
        }
        memcpy(parser->buf + parser->len, data, len);
        parser->len += len;
    }
    while (parser->len > 0 && parser->buf[0] != 0xAA) {
        parser_drop(parser, 1);
    }
    if (parser->len < 2) {
        return MT_OK;
    }
    if (parser->buf[1] < 1 || parser->buf[1] > MT_MAX_PAYLOAD) {
        parser_drop(parser, 1);
        return mt_parser_push(parser, NULL, 0, host_unix_ms, frame, got);
    }
    {
        size_t total = (size_t)parser->buf[1] + 4;
        int window;
        if (parser->len < total) {
            return MT_OK;
        }
        window = mt_frame_crc(parser->buf, total);
        if (window == 0) {
            parser_drop(parser, 1);
            return mt_parser_push(parser, NULL, 0, host_unix_ms, frame, got);
        }
        memset(frame, 0, sizeof(*frame));
        frame->host_unix_ms = host_unix_ms;
        frame->crc_window = window;
        frame->type = parser->buf[2];
        frame->payload_len = (uint16_t)(parser->buf[1] - 1);
        frame->raw_len = (uint16_t)total;
        memcpy(frame->raw, parser->buf, total);
        if (frame->payload_len > 0) {
            memcpy(frame->payload, parser->buf + 3, frame->payload_len);
        }
        parser_drop(parser, total);
        *got = 1;
    }
    return MT_OK;
}

struct mt_record {
    FILE *file;
    int writing;
};

static void store_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void store_u64(uint8_t *p, uint64_t v)
{
    store_u32(p, (uint32_t)v);
    store_u32(p + 4, (uint32_t)(v >> 32));
}

int mt_record_create(const char *path, mt_record **out)
{
    mt_record *record;
    if (path == NULL || out == NULL) {
        return MT_ERR_BAD_ARG;
    }
    record = (mt_record *)calloc(1, sizeof(*record));
    if (record == NULL) {
        return MT_ERR_IO;
    }
    record->file = fopen(path, "wb");
    if (record->file == NULL) {
        free(record);
        return MT_ERR_IO;
    }
    record->writing = 1;
    *out = record;
    return MT_OK;
}

int mt_record_open(const char *path, mt_record **out)
{
    mt_record *record;
    if (path == NULL || out == NULL) {
        return MT_ERR_BAD_ARG;
    }
    record = (mt_record *)calloc(1, sizeof(*record));
    if (record == NULL) {
        return MT_ERR_IO;
    }
    record->file = fopen(path, "rb");
    if (record->file == NULL) {
        free(record);
        return MT_ERR_IO;
    }
    *out = record;
    return MT_OK;
}

int mt_record_write(mt_record *record, const mt_frame *frame)
{
    uint8_t hdr[12];
    if (record == NULL || frame == NULL || record->file == NULL || !record->writing) {
        return MT_ERR_BAD_ARG;
    }
    if (frame->raw_len < 5 || frame->raw_len > MT_MAX_FRAME) {
        return MT_ERR_BAD_ARG;
    }
    store_u64(hdr, frame->host_unix_ms);
    store_u32(hdr + 8, frame->raw_len);
    if (fwrite(hdr, 1, 12, record->file) != 12) {
        return MT_ERR_IO;
    }
    if (fwrite(frame->raw, 1, frame->raw_len, record->file) != frame->raw_len) {
        return MT_ERR_IO;
    }
    return MT_OK;
}

int mt_record_read(mt_record *record, mt_frame *frame, int *got)
{
    uint8_t hdr[12];
    size_t n;
    uint32_t raw_len;
    if (record == NULL || frame == NULL || got == NULL || record->file == NULL || record->writing) {
        return MT_ERR_BAD_ARG;
    }
    *got = 0;
    n = fread(hdr, 1, 12, record->file);
    if (n == 0) {
        return MT_OK;
    }
    if (n != 12) {
        return MT_ERR_IO;
    }
    raw_len = rd_u32(hdr + 8);
    if (raw_len < 5 || raw_len > MT_MAX_FRAME) {
        return MT_ERR_BAD_ARG;
    }
    memset(frame, 0, sizeof(*frame));
    if (fread(frame->raw, 1, raw_len, record->file) != raw_len) {
        return MT_ERR_IO;
    }
    if (mt_frame_crc(frame->raw, raw_len) == 0) {
        return MT_ERR_CRC;
    }
    frame->host_unix_ms = (uint64_t)rd_u32(hdr) | ((uint64_t)rd_u32(hdr + 4) << 32);
    frame->crc_window = mt_frame_crc(frame->raw, raw_len);
    frame->raw_len = (uint16_t)raw_len;
    frame->type = frame->raw[2];
    frame->payload_len = (uint16_t)(frame->raw[1] - 1);
    if (frame->payload_len > 0) {
        memcpy(frame->payload, frame->raw + 3, frame->payload_len);
    }
    *got = 1;
    return MT_OK;
}

void mt_record_close(mt_record *record)
{
    if (record == NULL) {
        return;
    }
    if (record->file != NULL) {
        fclose(record->file);
    }
    free(record);
}
