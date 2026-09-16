#include "monitor.h"
#include "machine.h"
#include "source_view.h"
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { DEFAULT_LIMIT = 100000, MAX_INSPECTION_BYTES = 4096, MAX_DISASSEMBLY_WORDS = 256 };
static volatile sig_atomic_t interrupted;

typedef struct Monitor
{
  Machine *machine;
  DebugInfo *debug;
  const DebugLine *last_source;
  bool source_seen;
  uint64_t limit;
  bool trace;
  bool color;
  bool has_record;
  CpuStepRecord last;
} Monitor;

static void interrupt_handler(int signal_number)
{
  (void)signal_number;
  interrupted = 1;
}

static bool number(const char *text, uint64_t maximum, uint64_t *value)
{
  if (text == NULL || *text == '\0' || *text == '+' || *text == '-' ||
      isspace((unsigned char)*text)) return false;
  int base = 10;
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
  {
    base = 16;
    text += 2;
    if (!isxdigit((unsigned char)*text)) return false;
  }
  for (const char *p = text; *p; ++p)
    if (base == 10 ? !isdigit((unsigned char)*p) : !isxdigit((unsigned char)*p)) return false;
  errno = 0;
  char *end;
  unsigned long long parsed = strtoull(text, &end, base);
  if (errno == ERANGE || *end != '\0' || parsed > maximum) return false;
  *value = (uint64_t)parsed;
  return true;
}

static bool parse_ram_size(const char *text, size_t *value)
{
  size_t length = strlen(text);
  size_t unit = 0;
  size_t suffix = 0;
  if (length > 3 && strcmp(text + length - 3, "KiB") == 0)
  {
    unit = MEMORY_KIB;
    suffix = 3;
  }
  else if (length > 3 && strcmp(text + length - 3, "MiB") == 0)
  {
    unit = MEMORY_MIB;
    suffix = 3;
  }
  else if (length > 1 && text[length - 1] == 'B')
  {
    unit = MEMORY_BYTE_SIZE;
    suffix = 1;
  }
  else return false;

  char digits[32];
  size_t count = length - suffix;
  if (count >= sizeof digits) return false;
  memcpy(digits, text, count);
  digits[count] = '\0';

  uint64_t amount;
  if (!number(digits, MEMORY_MAX_SIZE / unit, &amount)) return false;
  size_t size = (size_t)amount * unit;
  if (!memory_size_valid(size)) return false;
  *value = size;
  return true;
}

static void usage(FILE *out)
{
  fputs("RISC-V Studio - RV32I teaching monitor\n"
    "Usage: riscv32-studio (--program FILE | --elf FILE | --bin FILE) [options]\n"
    "  --program FILE     Text words: one 32-bit hex word per line, # comments\n"
    "  --elf FILE         ELF32 RV32I/ILP32 executable, symbols, and DWARF source mapping\n"
    "  --bin FILE         Raw bytes; requires --load-address AND --entry\n"
    "  --ram SIZE         RAM size with B, KiB, or MiB suffix (default: 64KiB)\n"
    "                     Range: 4B through 256MiB, multiples of 4 bytes\n"
    "  --run              Run in batch mode; otherwise open the monitor\n"
    "  --trace            Show each instruction and its effects\n"
    "  --max-steps N      Maximum attempts per run (default: 100000)\n"
    "  --load-address N   First instruction address (default: 0)\n"
    "  --entry N          Initial PC (default: load address)\n"
    "  --color            Highlight changes with ANSI colors\n"
    "  --no-color         Disable colors (automatic on terminals by default)\n"
    "  --help             Show this help\n"
    "Numbers are decimal or 0x-prefixed hexadecimal; negative values are invalid.\n"
    "RAM requires an explicit case-sensitive unit, e.g. 65536B, 64KiB, or 1MiB.\n"
    "Commands: step [N], run [N], stop, reset, regs, mem ADDRESS [BYTES],\n"
    "          disasm ADDRESS [COUNT], where [ADDRESS], sources, source [LINE] [FILE_INDEX],\n"
    "          symbols [NAME], help [formats|runtime|source|MNEMONIC], quit\n"
    "Ctrl-C interrupts execution at an instruction boundary.\n"
    "Environment: a7=93 + ECALL exits with a0. EBREAK pauses at its instruction.\n"
    "Batch status: exit a0 & 255; trap=1; input error=2; limit=124; Ctrl-C=130.\n"
    "Examples:\n"
    "  ./build/debug/riscv32-studio --program demos/sum.words --run --trace\n"
    "  ./build/debug/riscv32-studio --program demos/call.words --ram 1MiB\n"
    "Guide and flowcharts: docs/HELP.md, docs/FLOWCHARTS.md\n", out);
}

static void command_help(const char *topic)
{
  if (topic == NULL)
  {
    puts("step [N]                Execute N instructions with effects (default 1).\n"
      "run [N]                 Run with a bounded number of attempts.\n"
      "stop                    Mark the CPU stopped; step/run resume it.\n"
      "reset                   Restore the loaded image and reset CPU state.\n"
      "regs                    Show x0..x31 and PC; mark the last changed register.\n"
      "mem ADDRESS [BYTES]     Inspect up to 4096 bytes (default 32).\n"
      "disasm ADDRESS [COUNT]  Decode up to 256 words (default 8).\n"
      "where [ADDRESS]         Show source location (default: current PC).\n"
      "sources                 List source file indices from DWARF.\n"
      "source [LINE] [INDEX]   Show source context; mark lines without mapped instructions.\n"
      "symbols [NAME]          List ELF symbols or find an exact name.\n"
      "help runtime            Explain RAM, stack, C libraries, and ELF/binary input.\n"
      "help source             Explain instruction-to-source mapping and limitations.\n"
      "help formats            Explain instruction layouts and immediates.\n"
      "help MNEMONIC           Show syntax and behavior, e.g. help addi.\n"
      "quit                    Exit the monitor. Ctrl-C stops a running program.\n"
      "EBREAK stays at the breakpoint; use reset to restart the program.");
    return;
  }
  if (strcmp(topic, "runtime") == 0)
  {
    puts("C target: RV32I / ILP32, freestanding C17, int main(void).\n"
      "Default layout: code at 0x1000, globals after code, 4KiB stack at the RAM top.\n"
      "Startup sets sp/gp, clears BSS, calls main, then exits with a7=93 + ECALL.\n"
      "libgcc provides software arithmetic; memcpy/memmove/memset/memcmp are included.\n"
      "There is no hosted libc, printf, malloc, file I/O, or OS. Unsupported ECALLs trap.\n"
      "RAM is flat: no MMU, memory permissions, or protected stack guard.\n"
      "ELF retains load addresses, entry, symbols and available debug data.\n"
      "Raw binary retains bytes only; supply load address, entry and enough RAM.\n"
      "Build: make guest SOURCE=demos/c/learning.c RAM=64KiB");
    return;
  }
  if (strcmp(topic, "source") == 0)
  {
    puts("The PC and assembly instruction are the execution authority.\n"
      "One C line can map to many instructions; the Source label stays until it changes.\n"
      "DWARF rows map address ranges, not a one-to-one translation of C statements.\n"
      "Declarations, comments, optimized code and other lines may have no mapped instruction.\n"
      "Unmapped instructions and missing source files are reported explicitly.\n"
      "Source text is read from its recorded path; rebuild after editing a source file.\n"
      "The startup and library routines may map to assembly or runtime C, not your main file.\n"
      "Use sources, then source LINE FILE_INDEX to inspect any recorded source.\n"
      "Local-variable evaluation and source-level stepping belong to the later debugger.");
    return;
  }
  if (strcmp(topic, "formats") == 0)
  {
    puts("Every RV32I instruction occupies 4 bytes. The PC selects the next word.\n"
      "R: rd, rs1, rs2       add x1, x2, x3      values come from two registers\n"
      "I: rd, rs1, imm       addi x1, x2, 7      constant comes from the instruction\n"
      "S: rs1, rs2, offset   sw x3, 0(x2)        write a register to RAM\n"
      "B: rs1, rs2, offset   beq x1, x2, -8      conditional PC-relative branch\n"
      "U: rd, upper imm     lui x1, 0x12345     place immediate in bits 31..12\n"
      "J: rd, offset        jal x1, 16          jump relative to PC; save PC+4\n"
      "I/S offsets: signed 12 bits (-2048..2047). B/J offsets include a zero low bit.\n"
      "B: -4096..4094; J: -1048576..1048574. Taken targets must align to 4 bytes.\n"
      "Shift immediates are 0..31. Raw rs2 slices in I-format are not register operands.\n"
      "x0 always reads zero; writing it discards the result. A load into x0 can still fault.");
    return;
  }
  char lower[32];
  size_t length = strlen(topic);
  if (length >= sizeof lower) { puts("Unknown help topic."); return; }
  for (size_t i = 0; i <= length; ++i) lower[i] = (char)tolower((unsigned char)topic[i]);
  for (unsigned kind = 1; kind < INSTRUCTION_COUNT; ++kind)
  {
    const InstructionInfo *info = instruction_info((InstructionKind)kind);
    if (strcmp(lower, info->name) == 0)
    {
      printf("%s\n%s\n", info->syntax, info->description);
      return;
    }
  }
  puts("Unknown help topic. Use help formats or an RV32I mnemonic.");
}

static bool should_stop(void *context)
{
  (void)context;
  return interrupted != 0;
}

static void trace_step(const CpuStepRecord *record, void *context)
{
  Monitor *monitor = context;
  monitor->last = *record;
  monitor->has_record = true;
  if (!monitor->trace) return;
  const DebugLine *location = debug_info_lookup(monitor->debug, record->pc_before);
  bool same_source = location == NULL ? monitor->last_source == NULL :
    monitor->last_source != NULL && location->file == monitor->last_source->file
      && location->line == monitor->last_source->line;
  if (!monitor->source_seen || !same_source)
    source_view_location(monitor->debug, record->pc_before, true);
  monitor->last_source = location;
  monitor->source_seen = true;
  char assembly[128] = "<instruction unavailable>";
  if (record->fetched)
  {
    bool formatted = instruction_disassemble(record->instruction.fields.raw, assembly, sizeof assembly);
    if (!formatted) strcpy(assembly, "<disassembly truncated>");
  }
  printf("0x%08" PRIX32 "  %-32s", record->pc_before, assembly);
  if (record->reg.written)
  {
    const char *color = monitor->color && record->reg.changed ? "\033[36;1m" : "";
    const char *end = monitor->color && record->reg.changed ? "\033[0m" : "";
    printf("  %sx%u: 0x%08" PRIX32 " -> 0x%08" PRIX32 "%s%s",
      color, record->reg.index, record->reg.before, record->reg.after,
      record->reg.changed ? " [changed]" : " [unchanged]", end);
    if (record->reg.index == 0) printf(" (discarded 0x%08" PRIX32 ")", record->reg.value);
  }
  if (record->memory.attempted)
  {
    printf("  %s%u [0x%08" PRIX32 "]", record->memory.write ? "store" : "load",
      (unsigned)record->memory.width * 8, record->memory.address);
    if (record->memory.completed)
      printf(": 0x%08" PRIX32 " -> 0x%08" PRIX32, record->memory.before, record->memory.after);
  }
  printf("  PC -> 0x%08" PRIX32, record->pc_after);
  if (record->trap.raised)
    printf("  %s (cause=%u, value=0x%08" PRIX32 ")",
      cpu_step_result_name(record->result), (unsigned)record->trap.cause, record->trap.value);
  putchar('\n');
}

static void show_registers(const Monitor *monitor)
{
  static const char *const aliases[32] = {"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0/fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
  for (unsigned reg = 0; reg < CPU_REGISTER_COUNT; ++reg)
  {
    uint32_t value = 0;
    bool read = cpu_read_register(&monitor->machine->cpu, reg, &value);
    if (!read) return;
    bool changed = monitor->has_record && monitor->last.reg.changed && monitor->last.reg.index == reg;
    printf("%sx%-2u %-5s 0x%08" PRIX32 "  signed=%" PRId64 "  unsigned=%" PRIu32 "%s%s\n",
      monitor->color && changed ? "\033[36;1m" : "", reg, aliases[reg], value,
      instruction_signed_value(value), value, changed ? " [changed]" : "",
      monitor->color && changed ? "\033[0m" : "");
  }
  printf("PC=0x%08" PRIX32 "  retired=%" PRIu64 "  halted=%s\n",
    monitor->machine->cpu.program_counter, monitor->machine->cpu.instruction_count,
    monitor->machine->cpu.halted ? "yes" : "no");
  printf("RAM=%zu bytes\n", monitor->machine->memory.size);
}

static void report_stop(const MachineRun *run, const Machine *machine)
{
  printf("Stopped: %s; attempts=%" PRIu64 "; retired=%" PRIu64 "; PC=0x%08" PRIX32,
    machine_stop_name(run->reason), run->attempts, run->retired, machine->cpu.program_counter);
  if (run->reason == MACHINE_EXITED) printf("; exit=%" PRIu32, machine->exit_code);
  if (run->reason == MACHINE_TRAP)
  {
    printf("; %s; instruction=0x%08" PRIX32 "; value=0x%08" PRIX32,
      cpu_step_result_name(run->last.result), run->last.trap.instruction_address, run->last.trap.value);
    if (run->last.result == CPU_STEP_ECALL)
      printf("; unsupported ECALL service a7=%" PRIu32, machine->cpu.registers[17]);
  }
  putchar('\n');
}

static int batch_status(MachineStop reason, uint32_t exit_code)
{
  if (reason == MACHINE_EXITED) return (int)(exit_code & 255);
  if (reason == MACHINE_LIMIT) return 124;
  if (reason == MACHINE_STOPPED) return 130;
  return 1;
}

static void inspect(Monitor *monitor, const char *command, char **args, size_t count)
{
  const size_t ram_size = monitor->machine->memory.size;
  uint64_t address, length = strcmp(command, "mem") == 0 ? 32 : 8;
  bool memory = strcmp(command, "mem") == 0;
  uint64_t maximum = memory ? MAX_INSPECTION_BYTES : MAX_DISASSEMBLY_WORDS;
  if (count < 2 || count > 3 || !number(args[1], ram_size - 1, &address) ||
      (count == 3 && !number(args[2], maximum, &length)) || length == 0 ||
      (memory ? length > ram_size - address : address % 4 != 0 || length > (ram_size - address) / 4))
  {
    puts("Invalid inspection range. Use mem ADDRESS [BYTES] or disasm ADDRESS [COUNT].");
    return;
  }
  for (uint64_t index = 0; index < length; ++index)
  {
    if (memory)
    {
      if (index % 16 == 0) printf("0x%08" PRIX64 ":", address + index);
      printf(" %02X", monitor->machine->memory.bytes[address + index]);
      if (index % 16 == 15 || index + 1 == length) putchar('\n');
    }
    else
    {
      uint32_t word;
      bool ok = memory_read_u32(&monitor->machine->memory, (uint32_t)(address + index * 4), &word);
      char text[128];
      if (!ok || !instruction_disassemble(word, text, sizeof text)) { puts("Inspection failed."); return; }
      printf("0x%08" PRIX64 "  %08" PRIX32 "  %s", address + index * 4, word, text);
      const DebugLine *location = debug_info_lookup(monitor->debug, (uint32_t)(address + index * 4));
      if (location != NULL) printf("  ; %s:%u", monitor->debug->files[location->file], location->line);
      else fputs("  ; <no source mapping>", stdout);
      putchar('\n');
    }
  }
}

static int interactive(Monitor *monitor)
{
  puts("Monitor ready. Type help for commands.");
  char line[512];
  for (;;)
  {
    fputs("rv32> ", stdout);
    fflush(stdout);
    interrupted = 0;
    if (fgets(line, sizeof line, stdin) == NULL)
    {
      if (interrupted) { clearerr(stdin); puts("Interrupted."); continue; }
      return ferror(stdin) ? 2 : 0;
    }
    if (strchr(line, '\n') == NULL && !feof(stdin))
    {
      int ch;
      while ((ch = getchar()) != '\n' && ch != EOF) {}
      puts("Command too long.");
      continue;
    }
    char *args[5];
    size_t count = 0;
    char *part = strtok(line, " \t\r\n");
    while (part != NULL && count < 5)
    {
      args[count++] = part;
      part = strtok(NULL, " \t\r\n");
    }
    if (count == 0) continue;
    if (count > 3) { puts("Too many arguments."); continue; }
    const char *command = args[0];
    if (strcmp(command, "quit") == 0 && count == 1) return 0;
    if (strcmp(command, "help") == 0 && count <= 2) command_help(count == 2 ? args[1] : NULL);
    else if (strcmp(command, "regs") == 0 && count == 1) show_registers(monitor);
    else if (strcmp(command, "sources") == 0 && count == 1) source_view_files(monitor->debug);
    else if (strcmp(command, "symbols") == 0 && count <= 2)
      source_view_symbols(monitor->debug, count == 2 ? args[1] : NULL);
    else if (strcmp(command, "where") == 0 && count <= 2)
    {
      uint64_t address = monitor->machine->cpu.program_counter;
      if (count == 2 && !number(args[1], UINT32_MAX, &address))
        puts("Invalid address.");
      else source_view_location(monitor->debug, (uint32_t)address, true);
      monitor->source_seen = false;
    }
    else if (strcmp(command, "source") == 0 && count <= 3)
    {
      uint64_t line_number = 0, file = SIZE_MAX;
      if ((count >= 2 && (!number(args[1], UINT_MAX, &line_number) || line_number == 0)) ||
          (count == 3 && (!number(args[2], SIZE_MAX, &file) || file == SIZE_MAX)))
        puts("Use source [positive LINE] [FILE_INDEX].");
      else source_view_lines(monitor->debug, monitor->machine->cpu.program_counter,
        (unsigned)line_number, (size_t)file);
      monitor->source_seen = false;
    }
    else if (strcmp(command, "stop") == 0 && count == 1)
    {
      machine_stop(monitor->machine);
      puts("CPU stopped. Use step or run to resume.");
    }
    else if (strcmp(command, "reset") == 0 && count == 1)
    {
      bool ok = machine_reset(monitor->machine);
      monitor->has_record = false;
      monitor->source_seen = false;
      monitor->last_source = NULL;
      puts(ok ? "Loaded image and CPU restored." : "Reset failed.");
    }
    else if (strcmp(command, "mem") == 0 || strcmp(command, "disasm") == 0)
      inspect(monitor, command, args, count);
    else if ((strcmp(command, "step") == 0 || strcmp(command, "run") == 0) && count <= 2)
    {
      bool stepping = strcmp(command, "step") == 0;
      uint64_t limit = stepping ? 1 : monitor->limit;
      if (count == 2 && (!number(args[1], UINT64_MAX, &limit) || limit == 0))
      { puts("Instruction limit must be a positive integer."); continue; }
      bool previous_trace = monitor->trace;
      monitor->trace = stepping || previous_trace;
      MachineRun run = machine_run(monitor->machine, limit, should_stop, trace_step, monitor);
      monitor->trace = previous_trace;
      report_stop(&run, monitor->machine);
    }
    else puts("Unknown command or wrong arguments. Type help.");
  }
}

int monitor_main(int argc, char **argv)
{
  const char *path = NULL;
  ProgramFormat format = PROGRAM_WORDS;
  uint64_t load_address = 0, entry = 0, limit = DEFAULT_LIMIT;
  size_t ram_size = MEMORY_DEFAULT_SIZE;
  bool entry_set = false, address_set = false, run_batch = false, trace = false;
  bool color = isatty(STDOUT_FILENO) != 0;
  for (int i = 1; i < argc; ++i)
  {
    const char *option = argv[i];
    if (strcmp(option, "--help") == 0) { usage(stdout); return 0; }
    if (strcmp(option, "--run") == 0) run_batch = true;
    else if (strcmp(option, "--trace") == 0) trace = true;
    else if (strcmp(option, "--color") == 0) color = true;
    else if (strcmp(option, "--no-color") == 0) color = false;
    else if (strcmp(option, "--program") == 0 || strcmp(option, "--elf") == 0 ||
        strcmp(option, "--bin") == 0 || strcmp(option, "--max-steps") == 0 ||
        strcmp(option, "--load-address") == 0 || strcmp(option, "--entry") == 0 ||
        strcmp(option, "--ram") == 0)
    {
      if (++i >= argc) { fprintf(stderr, "Missing value for %s.\n", option); return 2; }
      if (strcmp(option, "--program") == 0 || strcmp(option, "--elf") == 0 || strcmp(option, "--bin") == 0)
      {
        if (path != NULL) { fputs("Choose exactly one input: --program, --elf, or --bin.\n", stderr); return 2; }
        path = argv[i];
        format = strcmp(option, "--elf") == 0 ? PROGRAM_ELF :
          strcmp(option, "--bin") == 0 ? PROGRAM_BINARY : PROGRAM_WORDS;
      }
      else if (strcmp(option, "--ram") == 0)
      {
        if (!parse_ram_size(argv[i], &ram_size))
        {
          fputs("Invalid RAM size. Use B, KiB, or MiB; 4B through 256MiB in multiples of 4.\n", stderr);
          return 2;
        }
      }
      else
      {
        uint64_t value;
        if (!number(argv[i], strcmp(option, "--max-steps") == 0 ? UINT64_MAX : UINT32_MAX, &value))
        { fprintf(stderr, "Invalid number for %s: %s\n", option, argv[i]); return 2; }
        if (strcmp(option, "--max-steps") == 0)
        {
          if (value == 0) { fputs("Instruction limit must be positive.\n", stderr); return 2; }
          limit = value;
        }
        else if (strcmp(option, "--load-address") == 0) { load_address = value; address_set = true; }
        else { entry = value; entry_set = true; }
      }
    }
    else { fprintf(stderr, "Unknown option: %s\n", option); return 2; }
  }
  if (path == NULL) { usage(stderr); return 2; }
  if (format == PROGRAM_BINARY && (!address_set || !entry_set))
  { fputs("Raw binary requires both --load-address and --entry.\n", stderr); return 2; }
  if (format == PROGRAM_ELF && (address_set || entry_set))
  { fputs("ELF supplies its load addresses and entry; address overrides are unsupported.\n", stderr); return 2; }
  if (!entry_set) entry = load_address;
  FILE *input = fopen(path, "rb");
  if (input == NULL) { fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno)); return 2; }
  Program program = {0};
  ProgramError error;
  bool loaded;
  if (format == PROGRAM_ELF) loaded = program_read_elf(input, ram_size, &program, &error);
  else if (format == PROGRAM_BINARY)
    loaded = program_read_binary(input, ram_size, (uint32_t)load_address, (uint32_t)entry, &program, &error);
  else loaded = program_read_words(input, ram_size, (uint32_t)load_address, (uint32_t)entry, &program, &error);
  int closed = fclose(input);
  if (!loaded || closed != 0)
  {
    fprintf(stderr, "%s:%zu: %s\n", path, loaded ? 0 : error.line,
      loaded ? "input close error" : error.message);
    program_destroy(&program);
    return 2;
  }
  DebugInfo debug = {0};
  const char *debug_error;
  if (!debug_info_read(&program, &debug, &debug_error))
  {
    fprintf(stderr, "%s: %s\n", path, debug_error);
    program_destroy(&program);
    return 2;
  }
  Machine machine;
  machine_init(&machine);
  loaded = machine_load(&machine, &program);
  program_destroy(&program);
  if (!loaded)
  {
    machine_destroy(&machine);
    debug_info_destroy(&debug);
    fputs("Cannot create machine state from the program image.\n", stderr);
    return 2;
  }
  Monitor monitor = {.machine = &machine, .debug = &debug, .limit = limit, .trace = trace, .color = color};
  void (*previous_handler)(int) = signal(SIGINT, interrupt_handler);
  if (previous_handler == SIG_ERR)
  {
    machine_destroy(&machine);
    debug_info_destroy(&debug);
    fputs("Cannot install interrupt handler.\n", stderr);
    return 2;
  }
  interrupted = 0;
  int status;
  if (run_batch)
  {
    MachineRun run = machine_run(&machine, limit, should_stop, trace_step, &monitor);
    report_stop(&run, &machine);
    status = batch_status(run.reason, machine.exit_code);
  }
  else status = interactive(&monitor);
  signal(SIGINT, previous_handler);
  machine_destroy(&machine);
  debug_info_destroy(&debug);
  return status;
}
