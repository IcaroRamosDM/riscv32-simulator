#include "program_io.h"
#include <stdlib.h>
#include <string.h>

// ELF32 fields are decoded from bytes, independent of host alignment and endian.
enum { ELF_HEADER_SIZE = 52, ELF_PH_SIZE = 32, ELF_SH_SIZE = 40,
  ELF_FILE_LIMIT = 64 * MEMORY_MIB, ELF_MAX_SEGMENTS = 1024 };
static uint16_t u16(const uint8_t *p)
{
  return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}
static uint32_t u32(const uint8_t *p)
{
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
    (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool range(size_t offset, size_t length, size_t size)
{
  return offset <= size && length <= size - offset;
}
static bool uleb(const uint8_t **cursor, const uint8_t *end, uint32_t *value)
{
  uint32_t result = 0;
  for (unsigned shift = 0; shift < 35 && *cursor < end; shift += 7)
  {
    uint8_t byte = *(*cursor)++;
    if (shift == 28 && (byte & 0xF0) != 0) return false;
    result |= (uint32_t)(byte & 127) << shift;
    if ((byte & 128) == 0) { *value = result; return true; }
  }
  return false;
}
static bool attributes(const uint8_t *data, size_t size, ProgramError *error)
{
  if (size < 1 || data[0] != 'A')
    return program_error(error, 0, "invalid RISC-V attributes");
  size_t offset = 1;
  while (offset < size)
  {
    if (!range(offset, 4, size)) goto malformed;
    uint32_t length = u32(data + offset);
    if (length < 5 || !range(offset, length, size)) goto malformed;
    const uint8_t *end = data + offset + length;
    const uint8_t *vendor = data + offset + 4;
    const uint8_t *nul = memchr(vendor, 0, (size_t)(end - vendor));
    if (nul == NULL) goto malformed;
    if (strcmp((const char *)vendor, "riscv") == 0)
    {
      const uint8_t *p = nul + 1;
      while (p < end)
      {
        const uint8_t *start = p;
        uint32_t scope;
        if (!uleb(&p, end, &scope) || end - p < 4) goto malformed;
        uint32_t subsize = u32(p);
        p += 4;
        if (subsize < (size_t)(p - start) || subsize > (size_t)(end - start))
          goto malformed;
        const uint8_t *subend = start + subsize;
        if (scope != 1) return program_error(error, 0, "unsupported RISC-V attribute scope");
        while (p < subend)
        {
          uint32_t tag;
          if (!uleb(&p, subend, &tag)) goto malformed;
          if ((tag & 1) != 0)
          {
            const uint8_t *text = p;
            p = memchr(p, 0, (size_t)(subend - p));
            if (p == NULL) goto malformed;
            if (tag == 5 && strcmp((const char *)text, "rv32i2p1") != 0
                && strcmp((const char *)text, "rv32i2p0") != 0
                && strcmp((const char *)text, "rv32i") != 0)
              return program_error(error, 0, "ELF architecture requires instructions beyond RV32I");
            ++p;
          }
          else
          {
            uint32_t value;
            if (!uleb(&p, subend, &value)) goto malformed;
            if (tag == 4 && value != 16)
              return program_error(error, 0, "ELF requires an unsupported stack alignment");
            if (tag == 6 && value != 0)
              return program_error(error, 0, "ELF declares unaligned memory accesses");
          }
        }
      }
    }
    offset += length;
  }
  return true;
malformed:
  return program_error(error, 0, "malformed RISC-V attributes");
}

static bool validate_sections(const uint8_t *data, size_t size, ProgramError *error)
{
  uint32_t offset = u32(data + 32);
  uint16_t count = u16(data + 48), names = u16(data + 50);
  if (count == 0 && offset == 0 && names == 0) return true;
  if (count == 0 || u16(data + 46) != ELF_SH_SIZE ||
      !range(offset, (size_t)count * ELF_SH_SIZE, size) || names >= count)
    return program_error(error, 0, "invalid or unsupported ELF section table");
  for (size_t i = 0; i < count; ++i)
  {
    const uint8_t *section = data + offset + i * ELF_SH_SIZE;
    uint32_t type = u32(section + 4), start = u32(section + 16), length = u32(section + 20);
    if ((u32(section + 8) & 0x400) != 0)
      return program_error(error, 0, "ELF thread-local storage is unsupported");
    if (type != 8 && !range(start, length, size))
      return program_error(error, 0, "ELF section exceeds the file");
    if (type == 0x70000003 && !attributes(data + start, length, error)) return false;
  }
  return true;
}

static bool load_elf(Program *next, size_t ram_size, ProgramError *error)
{
  const uint8_t *data = next->elf_data;
  size_t size = next->elf_size;
  if (size < ELF_HEADER_SIZE || memcmp(data, "\177ELF", 4) != 0)
    return program_error(error, 0, "not an ELF file or truncated ELF header");
  if (data[4] != 1 || data[5] != 1 || data[6] != 1 ||
      u16(data + 16) != 2 || u16(data + 18) != 243 || u32(data + 20) != 1)
    return program_error(error, 0, "expected ELF32 little-endian RISC-V static executable");
  if (u32(data + 36) != 0)
    return program_error(error, 0, "ELF flags require an ABI or extension beyond RV32I/ILP32");
  if (u16(data + 40) != ELF_HEADER_SIZE || u16(data + 42) != ELF_PH_SIZE)
    return program_error(error, 0, "invalid ELF header sizes");
  uint32_t table = u32(data + 28), entry = u32(data + 24);
  uint16_t count = u16(data + 44);
  if (count == 0 || count > ELF_MAX_SEGMENTS ||
      !range(table, (size_t)count * ELF_PH_SIZE, size))
    return program_error(error, 0, "invalid ELF program header table");
  if (!validate_sections(data, size, error)) return false;
  size_t lowest = ram_size, highest = 0;
  bool executable_entry = false;
  for (size_t i = 0; i < count; ++i)
  {
    const uint8_t *ph = data + table + i * ELF_PH_SIZE;
    uint32_t type = u32(ph);
    if (type == 2 || type == 3 || type == 7)
      return program_error(error, 0, "dynamic linking, interpreters, and TLS are unsupported");
    if (type != 1) continue;
    uint32_t offset = u32(ph + 4), address = u32(ph + 8), physical = u32(ph + 12);
    uint32_t filesz = u32(ph + 16), memsz = u32(ph + 20);
    uint32_t flags = u32(ph + 24), align = u32(ph + 28);
    if (filesz > memsz || !range(offset, filesz, size))
      return program_error(error, 0, "ELF segment exceeds file or memory size");
    if (physical != address)
      return program_error(error, 0, "ELF requires different physical and virtual addresses");
    if (align > 1 && ((align & (align - 1)) != 0 || address % align != offset % align))
      return program_error(error, 0, "invalid ELF segment alignment");
    if (!range(address, memsz, ram_size))
      return program_error(error, 0, "ELF segment does not fit in configured RAM");
    if (memsz == 0) continue;
    for (size_t j = 0; j < i; ++j)
    {
      const uint8_t *prior = data + table + j * ELF_PH_SIZE;
      uint32_t a = u32(prior + 8), n = u32(prior + 20);
      if (u32(prior) == 1 && n != 0 && (uint64_t)address < (uint64_t)a + n
          && (uint64_t)a < (uint64_t)address + memsz)
        return program_error(error, 0, "overlapping ELF load segments");
    }
    if (address < lowest) lowest = address;
    if ((size_t)address + memsz > highest) highest = (size_t)address + memsz;
    if ((flags & 1) != 0 && filesz >= 4 && entry >= address
        && entry - address <= filesz - 4 && entry % 4 == 0)
      executable_entry = true;
  }
  if (!executable_entry || lowest >= highest)
    return program_error(error, 0, "ELF entry must address an aligned file-backed executable instruction");
  if (!memory_init(&next->memory, ram_size))
    return program_error(error, 0, "cannot allocate program RAM");
  for (size_t i = 0; i < count; ++i)
  {
    const uint8_t *ph = data + table + i * ELF_PH_SIZE;
    if (u32(ph) == 1 && u32(ph + 16) != 0)
      memcpy(next->memory.bytes + u32(ph + 8), data + u32(ph + 4), u32(ph + 16));
  }
  next->load_address = (uint32_t)lowest;
  next->entry = entry;
  next->image_size = highest - lowest;
  return true;
}

bool program_read_elf(FILE *input, size_t ram_size, Program *output, ProgramError *error)
{
  if (error != NULL) *error = (ProgramError){0};
  if (input == NULL || output == NULL)
    return program_error(error, 0, "invalid argument");
  if (!memory_size_valid(ram_size)) return program_error(error, 0, "invalid RAM size");
  Program next = {.format = PROGRAM_ELF};
  if (!program_input_bytes(input, ELF_FILE_LIMIT, &next.elf_data, &next.elf_size, error))
    return false;
  if (!load_elf(&next, ram_size, error))
  {
    program_destroy(&next);
    return false;
  }
  program_destroy(output);
  *output = next;
  return true;
}
