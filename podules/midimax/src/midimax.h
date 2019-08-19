void midimax_log(const char *format, ...);

void midimax_set_irq(void *p);
void midimax_clear_irq(void *p);

void midimax_midi_send(void *p, uint8_t val);
