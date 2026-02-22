#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
  uint8_t *buffer;
  uint32_t size;
  volatile uint32_t head;
  volatile uint32_t tail;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint32_t size);
void ring_buffer_reset(ring_buffer_t *rb);
uint32_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint32_t len);
uint32_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint32_t len);
uint32_t ring_buffer_count(const ring_buffer_t *rb);
uint32_t ring_buffer_free(const ring_buffer_t *rb);

#endif
