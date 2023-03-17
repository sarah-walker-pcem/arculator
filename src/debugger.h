#ifndef __DEBUGGER_H__
#define __DEBUGGER_H__

enum
{
	DEBUG_TRAP_PREF_ABORT = 0,
	DEBUG_TRAP_DATA_ABORT,
	DEBUG_TRAP_ADDR_EXCEP,
	DEBUG_TRAP_UNDEF,
	DEBUG_TRAP_SWI
};

void debug_start(void);
void debug_kill(void);
void debug_end(void);
//void debug_read(uint16_t addr);
//void debug_write(uint16_t addr, uint8_t val);
void debugger_do();
void debug_out(char *s);
void debug_trap(int trap, uint32_t opcode);
void debug_writememb(uint32_t a, uint8_t v);
void debug_writememl(uint32_t a, uint32_t v);

void debugger_start_reset(void);
void debugger_end_reset(void);

extern int debug, debugon;
extern int debugger_in_reset;
extern int indebug;

void console_output(char *s);
int console_input_get(char *s);
void console_input_disable();
void console_input_enable();

#define CONSOLE_INPUT_GET_ERROR_WINDOW_CLOSED -1
#define CONSOLE_INPUT_GET_ERROR_IN_RESET -2

#endif /*__DEBUGGER_H__*/