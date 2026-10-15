#include "metatron_api.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    mt_port *port = NULL;
    mt_parser *parser = NULL;
    mt_record *record = NULL;
    uint8_t request[8];
    size_t request_len = 0;
    uint64_t start;
    int rc;
    if (argc < 2) {
        fprintf(stderr, "usage: read_pillar COM3 [record.bin]\n");
        return 2;
    }
    rc = mt_port_open(argv[1], &port);
    if (rc == MT_ERR_PORT_IN_USE) {
        fprintf(stderr, "PORT_IN_USE %s\n", argv[1]);
        return 1;
    }
    if (rc != MT_OK) {
        fprintf(stderr, "open failed %s\n", argv[1]);
        return 1;
    }
    if (argc >= 3) {
        rc = mt_record_create(argv[2], &record);
        if (rc != MT_OK) {
            mt_port_close(port);
            return 1;
        }
    }
    rc = mt_ident_request(request, sizeof(request), &request_len);
    if (rc != MT_OK || mt_port_write(port, request, request_len) != MT_OK) {
        mt_record_close(record);
        mt_port_close(port);
        return 1;
    }
    parser = mt_parser_new();
    start = mt_now_unix_ms();
    {
    mt_identity identity;
    int has_ident = 0;
    memset(&identity, 0, sizeof(identity));
    while (mt_now_unix_ms() - start < 5000) {
        uint8_t chunk[256];
        size_t nread = 0;
        mt_frame frame;
        int got = 0;
        uint64_t host_ms;
        rc = mt_port_read(port, chunk, sizeof(chunk), &nread, 200);
        if (rc == MT_ERR_TIMEOUT) {
            continue;
        }
        if (rc != MT_OK) {
            break;
        }
        host_ms = mt_now_unix_ms();
        rc = mt_parser_push(parser, chunk, nread, host_ms, &frame, &got);
        while (rc == MT_OK && got) {
            if (record != NULL) {
                mt_record_write(record, &frame);
            }
            if (frame.type == MT_MSG_IDENT) {
                if (mt_parse_ident(frame.payload, frame.payload_len, &identity) == MT_OK) {
                    has_ident = 1;
                    printf("IDENT %s type=0x%02X fw=%s channels=%u host_ms=%llu\n",
                           identity.firmware_id, identity.module_type, identity.firmware_version,
                           identity.channel_count, (unsigned long long)frame.host_unix_ms);
                }
            } else if (frame.type == MT_MSG_DATA) {
                mt_sample sample;
                if (mt_parse_data(identity.firmware_id, identity.module_type, has_ident,
                                  frame.payload, frame.payload_len, &sample) == MT_OK) {
                    printf("DATA channel=%u kind=%s firmware_millis=%u host_ms=%llu\n",
                           sample.channel, mt_kind_name(sample.kind),
                           sample.firmware_millis,
                           (unsigned long long)frame.host_unix_ms);
                    if (sample.kind == MT_KIND_HALL_PATTERN && sample.fields_valid) {
                        printf("HALL bits %u %u %u %u\n",
                               sample.hall_bit[0], sample.hall_bit[1],
                               sample.hall_bit[2], sample.hall_bit[3]);
                    }
                }
            } else {
                printf("FRAME type=0x%02X bytes=%u host_ms=%llu\n",
                       frame.type, frame.payload_len, (unsigned long long)frame.host_unix_ms);
            }
            got = 0;
            rc = mt_parser_push(parser, NULL, 0, host_ms, &frame, &got);
        }
    }
    }
    mt_parser_free(parser);
    mt_record_close(record);
    mt_port_close(port);
    return 0;
}
