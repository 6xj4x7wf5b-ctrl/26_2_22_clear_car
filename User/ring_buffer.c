#include "ring_buffer.h"

#include "cmsis_gcc.h"

static uint32_t rb_enter_critical(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void rb_exit_critical(uint32_t primask)
{
  __set_PRIMASK(primask);
}

void ring_buffer_init(ring_buffer_t *rb, uint8_t *storage, uint32_t size)
{
  if ((rb == NULL) || (storage == NULL) || (size < 2U))
  {
    return;
  }

  rb->buffer = storage;
  rb->size = size;
  rb->head = 0U;
  rb->tail = 0U;
}

void ring_buffer_reset(ring_buffer_t *rb)
{
  if (rb == NULL)
  {
    return;
  }

  uint32_t primask = rb_enter_critical();
  rb->head = 0U;
  rb->tail = 0U;
  rb_exit_critical(primask);
}

uint32_t ring_buffer_count(const ring_buffer_t *rb)
{
  if ((rb == NULL) || (rb->size < 2U))
  {
    return 0U;
  }

  uint32_t count;
  uint32_t primask = rb_enter_critical();
  uint32_t head = rb->head;
  uint32_t tail = rb->tail;

  if (head >= tail)
  {
    count = head - tail;
  }
  else
  {
    count = (rb->size - tail) + head;
  }
  rb_exit_critical(primask);

  return count;
}

uint32_t ring_buffer_free(const ring_buffer_t *rb)
{
  if (rb == NULL)
  {
    return 0U;
  }

  return (rb->size - 1U) - ring_buffer_count(rb);
}

uint32_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint32_t len)
{
  if ((rb == NULL) || (data == NULL) || (len == 0U) || (rb->size < 2U))
  {
    return 0U;
  }

  uint32_t written = 0U;
  uint32_t primask = rb_enter_critical();

  while (written < len)
  {
    uint32_t next_head = rb->head + 1U;
    if (next_head >= rb->size)
    {
      next_head = 0U;
    }

    if (next_head == rb->tail)
    {
      break;
    }

    rb->buffer[rb->head] = data[written];
    rb->head = next_head;
    written++;
  }

  rb_exit_critical(primask);
  return written;
}

uint32_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint32_t len)
{
  if ((rb == NULL) || (data == NULL) || (len == 0U) || (rb->size < 2U))
  {
    return 0U;
  }

  uint32_t read = 0U;
  uint32_t primask = rb_enter_critical();

  while (read < len)
  {
    if (rb->tail == rb->head)
    {
      break;
    }

    data[read] = rb->buffer[rb->tail];
    rb->tail++;
    if (rb->tail >= rb->size)
    {
      rb->tail = 0U;
    }
    read++;
  }

  rb_exit_critical(primask);
  return read;
}
