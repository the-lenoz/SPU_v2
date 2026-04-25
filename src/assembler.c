#include "assembler.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "spec.h"

enum
{
  MAX_LINE_LENGTH = 2048,
  MAX_LABEL_LENGTH = 128,
  MAX_MNEMONIC_LENGTH = 64,
  MAX_OPERAND_LENGTH = 256,
};

typedef struct SourceLine
{
  char *text;
  size_t line_number;
} SourceLine;

typedef struct ParsedLine
{
  int has_label;
  char label[MAX_LABEL_LENGTH];
  size_t label_column;

  int has_instruction;
  char mnemonic[MAX_MNEMONIC_LENGTH];
  size_t mnemonic_column;

  int has_operand;
  char operand[MAX_OPERAND_LENGTH];
  size_t operand_column;

  size_t line_number;
} ParsedLine;

typedef struct LabelEntry
{
  char name[MAX_LABEL_LENGTH];
  uint32_t address;
  size_t line_number;
} LabelEntry;

typedef struct AsmRecord
{
  ParsedLine parsed;
  const InstructionInfo *instruction;
  uint32_t address;
} AsmRecord;

typedef struct AsmError
{
  size_t line_number;
  size_t column;
  char message[256];
} AsmError;

static void make_error(AsmError *error, size_t line_number, size_t column, const char *message)
{
  error->line_number = line_number;
  error->column = column;
  snprintf(error->message, sizeof(error->message), "%s", message);
}

static void free_source_lines(SourceLine *lines, size_t count)
{
  if (lines == NULL)
  {
    return;
  }

  for (size_t i = 0; i < count; ++i)
  {
    free(lines[i].text);
  }

  free(lines);
}

static int read_source_lines(const char *path, SourceLine **out_lines, size_t *out_count)
{
  FILE *file = NULL;
  SourceLine *lines = NULL;
  size_t count = 0;
  size_t capacity = 0;
  char line_buffer[MAX_LINE_LENGTH];

  if (path == NULL || out_lines == NULL || out_count == NULL)
  {
    return -1;
  }

  file = fopen(path, "rb");
  if (file == NULL)
  {
    return -1;
  }

  while (fgets(line_buffer, (int)sizeof(line_buffer), file) != NULL)
  {
    SourceLine *new_lines = NULL;
    char *copy = NULL;
    size_t read = strlen(line_buffer);

    if (read == sizeof(line_buffer) - 1u && line_buffer[read - 1u] != '\n')
    {
      fclose(file);
      free_source_lines(lines, count);
      return -1;
    }

    if (count == capacity)
    {
      size_t new_capacity = (capacity == 0) ? 32u : capacity * 2u;
      new_lines = (SourceLine *)realloc(lines, new_capacity * sizeof(*lines));
      if (new_lines == NULL)
      {
        fclose(file);
        free_source_lines(lines, count);
        return -1;
      }
      lines = new_lines;
      capacity = new_capacity;
    }

    copy = (char *)malloc(read + 1u);
    if (copy == NULL)
    {
      fclose(file);
      free_source_lines(lines, count);
      return -1;
    }

    memcpy(copy, line_buffer, read);
    copy[read] = '\0';

    lines[count].text = copy;
    lines[count].line_number = count + 1u;
    ++count;
  }

  fclose(file);

  *out_lines = lines;
  *out_count = count;
  return 0;
}

static int is_identifier_start_char(int c)
{
  return isalpha(c) || c == '_';
}

static int is_identifier_char(int c)
{
  return isalnum(c) || c == '_';
}

static size_t parse_identifier(const char *start)
{
  size_t length = 0;
  if (!is_identifier_start_char((unsigned char)start[0]))
  {
    return 0;
  }

  while (is_identifier_char((unsigned char)start[length]))
  {
    ++length;
  }

  return length;
}

static char *skip_spaces(char *cursor)
{
  while (*cursor != '\0' && isspace((unsigned char)*cursor))
  {
    ++cursor;
  }

  return cursor;
}

static void rstrip(char *line)
{
  size_t length = strlen(line);
  while (length > 0u && isspace((unsigned char)line[length - 1u]))
  {
    line[length - 1u] = '\0';
    --length;
  }
}

static const InstructionInfo *find_instruction_by_name(const char *name)
{
  size_t count = spu_instruction_count();
  for (size_t i = 0; i < count; ++i)
  {
    if (strcasecmp(kInstructionTable[i].name, name) == 0)
    {
      return &kInstructionTable[i];
    }
  }

  return NULL;
}

static int parse_line(const SourceLine *source, ParsedLine *parsed, AsmError *error)
{
  char work[MAX_LINE_LENGTH];
  char *comment = NULL;
  char *cursor = NULL;
  size_t ident_length = 0;

  if (source == NULL || parsed == NULL || error == NULL)
  {
    return -1;
  }

  memset(parsed, 0, sizeof(*parsed));
  parsed->line_number = source->line_number;

  if (strlen(source->text) >= sizeof(work))
  {
    make_error(error, source->line_number, 1u, "line is too long");
    return -1;
  }

  snprintf(work, sizeof(work), "%s", source->text);

  comment = strchr(work, ';');
  if (comment != NULL)
  {
    *comment = '\0';
  }

  rstrip(work);

  cursor = skip_spaces(work);
  if (*cursor == '\0')
  {
    return 0;
  }

  ident_length = parse_identifier(cursor);
  if (ident_length > 0u && cursor[ident_length] == ':')
  {
    if (ident_length >= sizeof(parsed->label))
    {
      make_error(error,
                 source->line_number,
                 (size_t)(cursor - work) + 1u,
                 "label is too long");
      return -1;
    }

    parsed->has_label = 1;
    parsed->label_column = (size_t)(cursor - work) + 1u;
    memcpy(parsed->label, cursor, ident_length);
    parsed->label[ident_length] = '\0';

    cursor += ident_length + 1u;
    cursor = skip_spaces(cursor);

    if (*cursor == '\0')
    {
      return 0;
    }
  }

  ident_length = parse_identifier(cursor);
  if (ident_length == 0u)
  {
    make_error(error,
               source->line_number,
               (size_t)(cursor - work) + 1u,
               "expected instruction mnemonic");
    return -1;
  }

  if (ident_length >= sizeof(parsed->mnemonic))
  {
    make_error(error,
               source->line_number,
               (size_t)(cursor - work) + 1u,
               "instruction mnemonic is too long");
    return -1;
  }

  parsed->has_instruction = 1;
  parsed->mnemonic_column = (size_t)(cursor - work) + 1u;
  memcpy(parsed->mnemonic, cursor, ident_length);
  parsed->mnemonic[ident_length] = '\0';

  cursor += ident_length;
  cursor = skip_spaces(cursor);

  if (*cursor == '\0')
  {
    return 0;
  }

  parsed->has_operand = 1;
  parsed->operand_column = (size_t)(cursor - work) + 1u;

  ident_length = 0;
  while (cursor[ident_length] != '\0' && !isspace((unsigned char)cursor[ident_length]))
  {
    ++ident_length;
  }

  if (ident_length >= sizeof(parsed->operand))
  {
    make_error(error,
               source->line_number,
               (size_t)(cursor - work) + 1u,
               "operand is too long");
    return -1;
  }

  memcpy(parsed->operand, cursor, ident_length);
  parsed->operand[ident_length] = '\0';

  cursor += ident_length;
  cursor = skip_spaces(cursor);

  if (*cursor != '\0')
  {
    make_error(error,
               source->line_number,
               (size_t)(cursor - work) + 1u,
               "too many operands");
    return -1;
  }

  return 0;
}

static int add_label(LabelEntry **labels,
                     size_t *label_count,
                     size_t *label_capacity,
                     const char *name,
                     uint32_t address,
                     size_t line_number,
                     AsmError *error)
{
  LabelEntry *new_labels = NULL;

  for (size_t i = 0; i < *label_count; ++i)
  {
    if (strcmp((*labels)[i].name, name) == 0)
    {
      make_error(error, line_number, 1u, "duplicate label");
      return -1;
    }
  }

  if (*label_count == *label_capacity)
  {
    size_t new_capacity = (*label_capacity == 0u) ? 32u : (*label_capacity * 2u);
    new_labels = (LabelEntry *)realloc(*labels, new_capacity * sizeof(**labels));
    if (new_labels == NULL)
    {
      make_error(error, line_number, 1u, "out of memory while adding label");
      return -1;
    }

    *labels = new_labels;
    *label_capacity = new_capacity;
  }

  snprintf((*labels)[*label_count].name, sizeof((*labels)[*label_count].name), "%s", name);
  (*labels)[*label_count].address = address;
  (*labels)[*label_count].line_number = line_number;
  (*label_count)++;

  return 0;
}

static const LabelEntry *find_label(const LabelEntry *labels, size_t label_count, const char *name)
{
  for (size_t i = 0; i < label_count; ++i)
  {
    if (strcmp(labels[i].name, name) == 0)
    {
      return &labels[i];
    }
  }

  return NULL;
}

static int token_is_label(const char *token)
{
  size_t length = strlen(token);
  if (length == 0u)
  {
    return 0;
  }

  if (!is_identifier_start_char((unsigned char)token[0]))
  {
    return 0;
  }

  for (size_t i = 1; i < length; ++i)
  {
    if (!is_identifier_char((unsigned char)token[i]))
    {
      return 0;
    }
  }

  return 1;
}

static int parse_u32_token(const char *token, uint32_t *value)
{
  char *end = NULL;
  errno = 0;

  if (token[0] == '-')
  {
    long long v = strtoll(token, &end, 0);
    if (errno != 0 || *end != '\0' || v < INT32_MIN || v > INT32_MAX)
    {
      return -1;
    }

    *value = (uint32_t)(int32_t)v;
    return 0;
  }

  unsigned long long v = strtoull(token, &end, 0);
  if (errno != 0 || *end != '\0' || v > UINT32_MAX)
  {
    return -1;
  }

  *value = (uint32_t)v;
  return 0;
}

static int add_record(AsmRecord **records,
                      size_t *record_count,
                      size_t *record_capacity,
                      const ParsedLine *parsed,
                      const InstructionInfo *instruction,
                      uint32_t address,
                      AsmError *error)
{
  AsmRecord *new_records = NULL;

  if (*record_count == *record_capacity)
  {
    size_t new_capacity = (*record_capacity == 0u) ? 64u : (*record_capacity * 2u);
    new_records = (AsmRecord *)realloc(*records, new_capacity * sizeof(**records));
    if (new_records == NULL)
    {
      make_error(error, parsed->line_number, 1u, "out of memory while adding record");
      return -1;
    }

    *records = new_records;
    *record_capacity = new_capacity;
  }

  (*records)[*record_count].parsed = *parsed;
  (*records)[*record_count].instruction = instruction;
  (*records)[*record_count].address = address;
  (*record_count)++;

  return 0;
}

static void print_asm_error(const char *path,
                            const SourceLine *lines,
                            size_t line_count,
                            const AsmError *error)
{
  fprintf(stderr,
          "%s:%zu:%zu: error: %s\n",
          path,
          error->line_number,
          error->column,
          error->message);

  if (error->line_number == 0u || error->line_number > line_count)
  {
    return;
  }

  const char *text = lines[error->line_number - 1u].text;
  size_t text_len = strlen(text);
  while (text_len > 0u && (text[text_len - 1u] == '\n' || text[text_len - 1u] == '\r'))
  {
    --text_len;
  }

  fprintf(stderr, "%.*s\n", (int)text_len, text);
  for (size_t i = 1u; i < error->column; ++i)
  {
    fputc(' ', stderr);
  }
  fputc('^', stderr);
  fputc('\n', stderr);
}

static int first_pass(const SourceLine *lines,
                      size_t line_count,
                      LabelEntry **labels,
                      size_t *label_count,
                      AsmRecord **records,
                      size_t *record_count,
                      uint32_t *program_size,
                      AsmError *error)
{
  size_t label_capacity = 0;
  size_t record_capacity = 0;
  uint32_t address = SPU_CODE_BASE;

  for (size_t i = 0; i < line_count; ++i)
  {
    ParsedLine parsed = {0};
    const InstructionInfo *instruction = NULL;

    if (parse_line(&lines[i], &parsed, error) != 0)
    {
      return -1;
    }

    if (parsed.has_label)
    {
      if (add_label(labels,
                    label_count,
                    &label_capacity,
                    parsed.label,
                    address,
                    parsed.line_number,
                    error) != 0)
      {
        return -1;
      }
    }

    if (!parsed.has_instruction)
    {
      continue;
    }

    instruction = find_instruction_by_name(parsed.mnemonic);
    if (instruction == NULL)
    {
      make_error(error,
                 parsed.line_number,
                 parsed.mnemonic_column,
                 "unknown instruction");
      return -1;
    }

    if (instruction->immediate_size == 0u && parsed.has_operand)
    {
      make_error(error,
                 parsed.line_number,
                 parsed.operand_column,
                 "instruction does not take operand");
      return -1;
    }

    if (instruction->immediate_size != 0u && !parsed.has_operand)
    {
      make_error(error,
                 parsed.line_number,
                 parsed.mnemonic_column,
                 "instruction requires an immediate operand");
      return -1;
    }

    if (instruction->immediate_size == 1u)
    {
      uint32_t value = 0;
      if (token_is_label(parsed.operand))
      {
        make_error(error,
                   parsed.line_number,
                   parsed.operand_column,
                   "label operand is not allowed for 8-bit immediate");
        return -1;
      }

      if (parse_u32_token(parsed.operand, &value) != 0)
      {
        make_error(error,
                   parsed.line_number,
                   parsed.operand_column,
                   "invalid numeric immediate");
        return -1;
      }

      if ((int32_t)value < -128 || (int32_t)value > 127)
      {
        make_error(error,
                   parsed.line_number,
                   parsed.operand_column,
                   "8-bit immediate is out of range (-128..127)");
        return -1;
      }
    }
    else if (instruction->immediate_size == 4u && !token_is_label(parsed.operand))
    {
      uint32_t value = 0;
      if (parse_u32_token(parsed.operand, &value) != 0)
      {
        make_error(error,
                   parsed.line_number,
                   parsed.operand_column,
                   "invalid numeric immediate");
        return -1;
      }
    }

    if (add_record(records,
                   record_count,
                   &record_capacity,
                   &parsed,
                   instruction,
                   address,
                   error) != 0)
    {
      return -1;
    }

    address += 1u + (uint32_t)instruction->immediate_size;
  }

  *program_size = address - SPU_CODE_BASE;
  return 0;
}

static int second_pass(const AsmRecord *records,
                       size_t record_count,
                       const LabelEntry *labels,
                       size_t label_count,
                       uint8_t *output,
                       uint32_t output_size,
                       AsmError *error)
{
  uint32_t offset = 0;

  for (size_t i = 0; i < record_count; ++i)
  {
    uint32_t value = 0;
    const AsmRecord *record = &records[i];

    if (offset >= output_size)
    {
      make_error(error,
                 record->parsed.line_number,
                 record->parsed.mnemonic_column,
                 "internal assembler error: output overflow");
      return -1;
    }

    output[offset++] = record->instruction->opcode;

    if (record->instruction->immediate_size == 0u)
    {
      continue;
    }

    if (record->instruction->immediate_size == 1u)
    {
      if (parse_u32_token(record->parsed.operand, &value) != 0)
      {
        make_error(error,
                   record->parsed.line_number,
                   record->parsed.operand_column,
                   "invalid numeric immediate");
        return -1;
      }

      output[offset++] = (uint8_t)(int8_t)(int32_t)value;
      continue;
    }

    if (record->instruction->immediate_size == 4u)
    {
      if (token_is_label(record->parsed.operand))
      {
        const LabelEntry *label = find_label(labels, label_count, record->parsed.operand);
        if (label == NULL)
        {
          make_error(error,
                     record->parsed.line_number,
                     record->parsed.operand_column,
                     "undefined label");
          return -1;
        }

        value = label->address;
      }
      else
      {
        if (parse_u32_token(record->parsed.operand, &value) != 0)
        {
          make_error(error,
                     record->parsed.line_number,
                     record->parsed.operand_column,
                     "invalid numeric immediate");
          return -1;
        }
      }

      if (offset + 4u > output_size)
      {
        make_error(error,
                   record->parsed.line_number,
                   record->parsed.operand_column,
                   "internal assembler error: output overflow");
        return -1;
      }

      output[offset++] = (uint8_t)(value & 0xFFu);
      output[offset++] = (uint8_t)((value >> 8u) & 0xFFu);
      output[offset++] = (uint8_t)((value >> 16u) & 0xFFu);
      output[offset++] = (uint8_t)((value >> 24u) & 0xFFu);
      continue;
    }

    make_error(error,
               record->parsed.line_number,
               record->parsed.mnemonic_column,
               "unsupported immediate size");
    return -1;
  }

  return 0;
}

static int write_binary(const char *path, const uint8_t *data, size_t size)
{
  FILE *file = fopen(path, "wb");
  if (file == NULL)
  {
    return -1;
  }

  if (size > 0u && fwrite(data, 1, size, file) != size)
  {
    fclose(file);
    return -1;
  }

  fclose(file);
  return 0;
}

int assemble_file(const char *input_path, const char *output_path)
{
  SourceLine *lines = NULL;
  size_t line_count = 0;

  LabelEntry *labels = NULL;
  size_t label_count = 0;

  AsmRecord *records = NULL;
  size_t record_count = 0;

  uint8_t *output = NULL;
  uint32_t output_size = 0;

  AsmError error = {0};
  int status = 1;

  if (read_source_lines(input_path, &lines, &line_count) != 0)
  {
    fprintf(stderr, "Failed to read source '%s'\n", input_path);
    return 1;
  }

  if (first_pass(lines,
                 line_count,
                 &labels,
                 &label_count,
                 &records,
                 &record_count,
                 &output_size,
                 &error) != 0)
  {
    print_asm_error(input_path, lines, line_count, &error);
    goto cleanup;
  }

  output = (uint8_t *)malloc(output_size == 0u ? 1u : output_size);
  if (output == NULL)
  {
    fprintf(stderr, "Out of memory\n");
    goto cleanup;
  }

  if (second_pass(records,
                  record_count,
                  labels,
                  label_count,
                  output,
                  output_size,
                  &error) != 0)
  {
    print_asm_error(input_path, lines, line_count, &error);
    goto cleanup;
  }

  if (write_binary(output_path, output, output_size) != 0)
  {
    fprintf(stderr, "Failed to write output '%s'\n", output_path);
    goto cleanup;
  }

  status = 0;

cleanup:
  free(output);
  free(records);
  free(labels);
  free_source_lines(lines, line_count);
  return status;
}
