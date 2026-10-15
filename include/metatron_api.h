#ifndef METATRON_API_H
#define METATRON_API_H

/*
 * Public client for the pillar firmware already in this tree.
 * Frame bytes, CRC table, and the hall bitmap come from that firmware.
 * The only host frame this library will write is an empty MSG_IDENT.
 * It will not build MSG_CALIBRATE or MSG_COMMAND.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MT_OK 0
#define MT_ERR_BAD_ARG 1
#define MT_ERR_CRC 2
#define MT_ERR_IO 3
#define MT_ERR_PORT_IN_USE 4
#define MT_ERR_TIMEOUT 5

#define MT_MSG_IDENT 0x01
#define MT_MSG_DATA 0x02
#define MT_MSG_STATUS 0x03
#define MT_MSG_ERROR 0x06
#define MT_MSG_HEARTBEAT 0x07
#define MT_MSG_ACK 0x08

#define MT_MAX_PAYLOAD 128
#define MT_MAX_FRAME 133

#define MT_MOD_HALL_ULTRA 0x17
#define MT_HALL_COUNT 4
#define MT_ULTRA_COUNT 2

#define MT_CRC_CANONICAL 1
#define MT_CRC_LEGACY 2

#define MT_KIND_RAW 0
#define MT_KIND_HALL_PATTERN 1
#define MT_KIND_HALL 2
#define MT_KIND_ULTRASONIC 3
#define MT_KIND_AMBIGUOUS 4

#define MT_STATUS_ASSIGNED 1
#define MT_STATUS_MISSING 2
#define MT_STATUS_DUPLICATE 3
#define MT_STATUS_UNASSIGNED 4

#pragma pack(push, 1)

typedef struct mt_identity {
    char firmware_id[16];
    uint8_t module_type;
    char firmware_version[12];
    uint8_t channel_count;
    char health[8];
    char uid[20];
    uint8_t has_uid;
} mt_identity;

typedef struct mt_sample {
    char firmware_id[16];
    uint8_t module_type;
    uint8_t channel;
    uint16_t body_len;
    uint8_t body[120];
    uint32_t firmware_millis;
    int32_t kind;
    int32_t sensor_index;
    uint8_t pattern;
    uint8_t active_count;
    uint8_t hall_bit[MT_HALL_COUNT];
    uint8_t magnet_detected;
    uint8_t hall_state;
    uint16_t trigger_count;
    uint16_t raw_analog;
    uint16_t baseline;
    uint16_t threshold;
    uint8_t quality;
    float distance_cm;
    uint8_t ultra_valid;
    uint16_t stddev_x100;
    uint8_t fields_valid;
    char reason[32];
} mt_sample;

typedef struct mt_observed {
    mt_identity identity;
} mt_observed;

typedef struct mt_assignment {
    mt_identity identity;
    char slot[16];
    char tower[16];
    int32_t status;
} mt_assignment;

typedef struct mt_frame {
    uint64_t host_unix_ms;
    int32_t crc_window;
    uint16_t payload_len;
    uint16_t raw_len;
    uint8_t type;
    uint8_t payload[MT_MAX_PAYLOAD];
    uint8_t raw[MT_MAX_FRAME];
} mt_frame;

#pragma pack(pop)

typedef struct mt_topology mt_topology;
typedef struct mt_parser mt_parser;
typedef struct mt_port mt_port;
typedef struct mt_record mt_record;

size_t mt_sizeof_identity(void);
size_t mt_sizeof_sample(void);
size_t mt_sizeof_frame(void);
size_t mt_sizeof_assignment(void);

uint8_t mt_crc8(const uint8_t *data, size_t len);
int mt_frame_crc(const uint8_t *frame, size_t len);

int mt_build_frame(uint8_t type, const uint8_t *payload, size_t len,
                   uint8_t *out, size_t cap, size_t *out_len);

int mt_parse_ident(const uint8_t *json, size_t len, mt_identity *out);

int mt_hall_bit(uint8_t pattern, int sensor);

int mt_parse_data(const char *firmware_id, uint8_t ident_type, int has_ident,
                  const uint8_t *data_payload, size_t len, mt_sample *out);

int mt_topology_load(const char *path, mt_topology **out);
void mt_topology_free(mt_topology *topo);
int mt_topology_assign(const mt_topology *topo,
                       const mt_observed *found, int found_count,
                       mt_assignment *out, int out_cap, int *out_count);

const char *mt_kind_name(int kind);
const char *mt_status_name(int status);

int mt_ident_request(uint8_t *out, size_t cap, size_t *out_len);
int mt_parse_heartbeat(const uint8_t *payload, size_t len, uint32_t *uptime, uint8_t *health);
int mt_parse_ack(const uint8_t *payload, size_t len, uint8_t *acked_type, uint8_t *code);
int mt_parse_error(const uint8_t *payload, size_t len, uint8_t *code, char *message, size_t cap);

mt_parser *mt_parser_new(void);
void mt_parser_free(mt_parser *parser);
void mt_parser_reset(mt_parser *parser);
int mt_parser_push(mt_parser *parser, const uint8_t *data, size_t len,
                   uint64_t host_unix_ms, mt_frame *frame, int *got);

uint64_t mt_now_unix_ms(void);

int mt_port_path(const char *name, char *out, size_t cap);
int mt_port_list(char names[][16], int cap, int *count);
int mt_port_open(const char *name, mt_port **out);
void mt_port_close(mt_port *port);
int mt_port_write(mt_port *port, const uint8_t *data, size_t len);
int mt_port_read(mt_port *port, uint8_t *buf, size_t cap, size_t *nread, int timeout_ms);

int mt_record_create(const char *path, mt_record **out);
int mt_record_open(const char *path, mt_record **out);
int mt_record_write(mt_record *record, const mt_frame *frame);
int mt_record_read(mt_record *record, mt_frame *frame, int *got);
void mt_record_close(mt_record *record);

#ifdef __cplusplus
}
#endif

#endif
