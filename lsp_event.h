#ifndef LS_EVENT_H
#define LS_EVENT_H

#ifdef __cplusplus
#include <stdint.h>
#include <unistd.h>
#else
#include <linux/types.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
  LSP_EVENT_CODE_NONE = 0
  , LSP_EVENT_CODE_OPEN = 1
} lsp_event_code_t;

#define LSP_EVENT_MAX_SIZE 16384

typedef struct __attribute__((packed))
{
  uint32_t number; //! position number
  uint32_t size;   //! including the terminating null byte
  char value[];    //! storage
} lsp_event_field_t;

typedef struct __attribute__((packed))
{
  uint32_t code;        //! op, i.e. OPEN
  uint32_t pid;
  uint32_t uid;         //! fsuid for file ops
  uint32_t gid;         //! fsgid for file ops
  uint32_t data_size;   //! overall size of data[] field
  uint32_t field_count; //! count of ls_event_field_t elements in the data[] field
  char data[];          //! storage of ls_event_field_t
} lsp_event_t;

static inline lsp_event_field_t * lsp_event_field_first(lsp_event_t * event)
{
  return (lsp_event_field_t *)(event->data);
}

static inline const lsp_event_field_t * lsp_event_field_first_const (const lsp_event_t * event)
{
  return (const lsp_event_field_t *)(event->data);
}

static inline lsp_event_field_t * lsp_event_field_next(lsp_event_field_t * field)
{
  // since we don't know the value[] size value must be the last
  return (lsp_event_field_t *)(field->value + field->size);
}

static inline const lsp_event_field_t * lsp_event_field_next_const (const lsp_event_field_t * field)
{
  return (const lsp_event_field_t *)(field->value + field->size);
}

static inline const lsp_event_field_t * lsp_event_field_end(const lsp_event_t * event)
{
  return (const lsp_event_field_t *)(event->data + event->data_size);
}

static inline lsp_event_field_t * lsp_event_field_get(lsp_event_t * event, uint32_t number)
{
  const lsp_event_field_t * field_end = NULL;
  lsp_event_field_t * field = NULL;
  if (event)
  {
    field_end = lsp_event_field_end(event);
    field = lsp_event_field_first(event);
    while(number > 0 && field < field_end)
    {
      field = lsp_event_field_next(field);
      --number;
    }
    if (number)
      field = NULL;
  }
  return field;
}

static inline const lsp_event_field_t * lsp_event_field_get_const (const lsp_event_t * event, uint32_t number)
{
  const lsp_event_field_t * field_end = NULL;
  const lsp_event_field_t * field = NULL;
  if (event)
  {
    field_end = lsp_event_field_end(event);
    field = lsp_event_field_first_const(event);
    while(number > 0 && field < field_end)
    {
      field = lsp_event_field_next_const(field);
      --number;
    }
    if (number)
      field = NULL;
  }
  return field;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LS_EVENT_H
