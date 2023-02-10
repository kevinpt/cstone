#ifndef PROP_SERIALIZE_H
#define PROP_SERIALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned varint_encoded_bytes(uint32_t n);
int varint_encode(uint32_t n, uint8_t *buf, size_t buf_size);
int varint_decode(uint32_t *n, uint8_t *buf);

int uint32_encode(uint32_t n, uint8_t *buf, size_t buf_size);
int uint32_decode(uint32_t *n, uint8_t *buf);

int string_encode(char *str, uint8_t *buf, size_t buf_size);
int string_decode(char *str, size_t str_size, uint8_t *buf);

int blob_encode(uint8_t *data, size_t data_size, uint8_t *buf, size_t buf_size);

unsigned prop_encoded_bytes(uint32_t prop, PropDBEntry *entry);
int prop_encode(uint32_t prop, PropDBEntry *entry, uint8_t *buf, size_t buf_size);
int prop_decode(uint32_t *prop, PropDBEntry *entry, uint8_t *buf);


#ifdef __cplusplus
}
#endif

#endif // PROP_SERIALIZE_H
