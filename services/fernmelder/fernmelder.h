#ifndef _FERNMELDER_H
#define _FERNMELDER_H

#define KNOCK_MESSAGE_TEXT_SIZE 421
#define KNOCK_SOCKET_COUNT 5
#define KNOCK_PORT_RANGE_START  500
#define KNOCK_PORT_RANGE_END  20000
/* This is allows for more than one IP knocking at a time */
#define KNOCK_RECORD_HISTORY_SCALE 2
#define KNOCK_RECORD_HISTORY_SIZE KNOCK_SOCKET_COUNT*KNOCK_RECORD_HISTORY_SCALE

enum event_t {NONE, KEY_UP, KEY_DOWN, KEY_SELECT, ALARM_TIMER_EXPR, RIGHT_SEQUENCE};

struct knock_port_eeprom_record_t {
  uint16_t knock_ports[KNOCK_SOCKET_COUNT];
  uint8_t magic;
} __attribute__((__packed__));

struct knock_record_t {
  /* From which the knock came */
  uip_ipaddr_t addr;
  /* On which port the addr knocked */
  uint16_t port;
};

enum s_state_t {ST_PREINIT, ST_INIT, ST_IDLE, ST_ALARM_ON, ST_ALARM_OFF, ST_ALARM_DISABLED, ST_SETTING1, ST_SETTING2};

int16_t fernmelder_periodic(void);
int16_t fernmelder_init(void);
void restore_ports();
void save_ports();
void get_n_random(uint16_t *a, uint8_t n, uint16_t start, uint16_t end);
void increase_knock_record_current();
void debug_knock_record_buffer();
void save_knock_record(uip_ipaddr_t *a, uint16_t port);
struct knock_record_t* get_last_n_knock_record(uint8_t n);
void burn_knock_records();
void sockets_down();
void sockets_up();
void randomize_ports();
uint8_t check_sequence_for_ip(uip_ipaddr_t *ip);
void sirene_handle_knock();
void set_state(enum s_state_t s);
void activate_alarm();
void sirene_state_machine(enum event_t e);
void process_alarm();
void print_line(char *s, uint16_t line, uint16_t length);


#include "config.h"
#if DEBUG_FERNMELDER_
# include "core/debug.h"
# define SIRDEBUG(a...)  debug_printf("fernmelder: " a)
#else
# define SIRDEBUG(a...)  
#endif

#endif
