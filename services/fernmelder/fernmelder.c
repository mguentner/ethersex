#include <avr/pgmspace.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "core/eeprom.h"
#include "protocols/uip/uip.h"
#include "config.h"
#include "protocols/uip/parse.h"
#include "services/clock/clock.h"
#include "services/clock/clock_lib.h"
#include "hardware/lcd/hd44780.h"

#include "services/fernmelder/fernmelder.h"
#include "services/fernmelder/scrollstring.h"
#include "protocols/ecmd/ecmd-base.h"

/* ADC Values */
#define KEYNO 1023
#define KEY1 583
#define KEY2 406
#define KEY3 0

#define BUF ((struct uip_udpip_hdr *) (uip_appdata - UIP_IPUDPH_LEN))


#define KNOCK_PORT_EEPROM_BASE EEPROM_CONFIG_BASE + 3*sizeof(struct eeprom_config_t)
#define KNOCK_PORT_EEPROM_MAGIC 42

const char* s_state_names[] = {
  "ST_PREINIT",
  "ST_INIT",
  "ST_IDLE",
  "ST_ALARM_ON",
  "ST_ALARM_OFF",
  "ST_ALARM_DISABLED",
  "ST_SETTING1",
  "ST_SETTING2"
  };

/* globals */

enum event_t external_event = NONE;
uip_udp_conn_t* knock_sockets[KNOCK_SOCKET_COUNT];
uint16_t knock_ports[KNOCK_SOCKET_COUNT] = {0};
struct knock_record_t knock_records[KNOCK_RECORD_HISTORY_SIZE];
uint8_t knock_records_current;

uint16_t scroll_state[4] = {0};
char knock_message_text[KNOCK_MESSAGE_TEXT_SIZE] = { '\0' };
uint16_t knock_message_text_size = 0;
uip_ipaddr_t knock_remote_addr;
uint8_t sirene_state;

void restore_ports() {
  struct knock_port_eeprom_record_t local;
  for(uint8_t i=0; i<sizeof(struct knock_port_eeprom_record_t); i++) {
    *(((uint8_t*)&local)+i) = eeprom_read_byte(KNOCK_PORT_EEPROM_BASE+i);
  }
  if (local.magic == KNOCK_PORT_EEPROM_MAGIC) {
    memcpy(knock_ports, local.knock_ports, sizeof(uint16_t)*KNOCK_SOCKET_COUNT);
    SIRDEBUG("Restored ports!\n");
  } else {
    SIRDEBUG("Could not restored ports, randomizing! magic: %d sizeof():%d offset: %d\n", local.magic, sizeof(struct knock_port_eeprom_record_t), KNOCK_PORT_EEPROM_BASE);
    randomize_ports();
  }
}

void save_ports() {
  struct knock_port_eeprom_record_t local;
  memcpy(local.knock_ports, knock_ports, sizeof(uint16_t)*KNOCK_SOCKET_COUNT);
  local.magic = KNOCK_PORT_EEPROM_MAGIC;
  for(uint8_t i=0; i<sizeof(struct knock_port_eeprom_record_t); i++) {
    eeprom_write_byte(KNOCK_PORT_EEPROM_BASE+i, *(((uint8_t*)&local)+i));
  }
  SIRDEBUG("Saved ports!");
}

void increase_knock_record_current() {
  if (knock_records_current < KNOCK_RECORD_HISTORY_SIZE-1)
    knock_records_current++;
  else {
    knock_records_current=0;
  }
}

void debug_knock_record_buffer() {
  for (uint8_t i=0; i < KNOCK_RECORD_HISTORY_SIZE; i++) {
    char line[40];
    char ip[60];
    print_ipaddr(&knock_records[i].addr, ip, 60);
    sprintf(line, "[%u] addr: %s, port: %u\n", (knock_records_current == i), ip, knock_records[i].port);
    SIRDEBUG("%s", line);
  }
}

void save_knock_record(uip_ipaddr_t *a, uint16_t port) {
  increase_knock_record_current();
  uip_ipaddr_copy(&(knock_records[knock_records_current].addr), a);
  knock_records[knock_records_current].port = port;
  debug_knock_record_buffer();
}

struct knock_record_t* get_last_n_knock_record(uint8_t n) {
  uint8_t real_index;
  if (n > knock_records_current) {
    real_index = KNOCK_RECORD_HISTORY_SIZE-(n-knock_records_current);
  } else {
    real_index = knock_records_current-n;
  }
  // DELETE  if (real_index > KNOCK_RECORD_HISTORY_SIZE-1) {
  // DELETE    SIRDEBUG("PROGRAMMING_FNORD: n: %d ci: %d ri: %d KRH: %d\n", n, knock_records_current, real_index, KNOCK_RECORD_HISTORY_SIZE);
  // DELETE  }
  // DELETE  SIRDEBUG("get_last: n: %d ci: %d ri: %d\n", n, knock_records_current, real_index);
  return &(knock_records[real_index]);
}

void burn_knock_records() {
  for (uint8_t i=0; i < KNOCK_RECORD_HISTORY_SCALE; i++) {
    uip_ipaddr_copy(&knock_records[i].addr, all_zeroes_addr);
    knock_records[i].port = 0;
  }
}

void sockets_down() {
  for (uint8_t i=0; i < KNOCK_SOCKET_COUNT; i++) {
    if (knock_sockets[i]) {
      uip_udp_remove(knock_sockets[i]);
    }
  }
}

void randomize_ports() {
  srand(clock_get_time());
  get_n_random(knock_ports , KNOCK_SOCKET_COUNT, KNOCK_PORT_RANGE_START, KNOCK_PORT_RANGE_END);
  SIRDEBUG("randomized ports\n");
  for (uint8_t i=0; i < KNOCK_SOCKET_COUNT; i++) {
    SIRDEBUG("port %d \t %d\n", i, knock_ports[i]);
  }
  save_ports();
}

uint8_t check_sequence_for_ip(uip_ipaddr_t *ip) {
  int8_t in_sequence = KNOCK_SOCKET_COUNT-1;
  for (uint8_t i=0; i < KNOCK_RECORD_HISTORY_SIZE; i++) {
    struct knock_record_t *k = get_last_n_knock_record(i);
    if (uip_ipaddr_cmp(k->addr, ip)) {
      /* Same IP */
      SIRDEBUG("same ip\n");
      if (knock_ports[in_sequence] == k->port) {
        SIRDEBUG("same port\n");
        /* Same Port */
        in_sequence--;
      } else {
        /* the sequence has started and ports differ */
        if (in_sequence < KNOCK_SOCKET_COUNT-1) {
          break;
        }
      }
    }
  }
  if (in_sequence == -1) {
    return 1;
  } else {
    return 0;
  }
}

/*
   From RFC 768:
   Length  is the length  in octets  of this user datagram  including  this
   header  and the data.   (This  means  the minimum value of the length is
   eight.)
*/

void sirene_handle_knock() {
  if (uip_newdata()) {
//TODO ignore in certain modes
    SIRDEBUG("Received knock\n");
    uip_ipaddr_t addr;
    uint16_t port;
    uip_ipaddr_copy(addr, BUF->srcipaddr);
    port = HTONS(BUF->destport);
    save_knock_record(&addr, port);
    if (check_sequence_for_ip(&addr)) {
      external_event = RIGHT_SEQUENCE;
      /* Copy buffer */
      memset(knock_message_text, '\0', KNOCK_MESSAGE_TEXT_SIZE*sizeof(char));
      size_t size = fmin(KNOCK_MESSAGE_TEXT_SIZE-1, HTONS(BUF->udplen)-8 );
      for (size_t i=0; i < size; i++) {
        char c = *((uint8_t*)uip_appdata+i);
        if ((c >= 0x20 && c <= 0x7E)) {
          knock_message_text[i] = c;
        } else if (c == '\0') {
          knock_message_text[i] = '\0';
        } else {
          knock_message_text[i] = '?';
        }
      }
      knock_message_text[size] = '\0';
      knock_message_text_size = size;
      uip_ipaddr_copy(&knock_remote_addr, addr);
    }
  }
}

void sockets_up() {
  for (uint8_t i=0; i<KNOCK_SOCKET_COUNT; i++) {
    uip_ipaddr_t ip;
    uip_udp_conn_t *conn;
    uip_ipaddr_copy(&ip, all_ones_addr);
    conn = uip_udp_new(&ip, 0, sirene_handle_knock);
    if (conn == 0) {
      SIRDEBUG("Could not create conn\n");
      return;
    }
    uip_udp_bind(conn, HTONS(knock_ports[i]));
    knock_sockets[i] = conn;
  }
}

void get_n_random(uint16_t *a, uint8_t n, uint16_t start, uint16_t end) {
  uint16_t range=end-start;
  for (uint8_t i=0; i<n ; i++) {
    for (;;) {
      uint16_t v = start+rand()%range;
      uint8_t same = 0;
      if (i > 0) {
        for (uint8_t k=0; k<i; k++) {
          if (a[k] == v) {
            same = 1;
          }
        }
      }
      if (same == 0) {
        a[i] = v;
        break;
      }
    }
  }
}

void set_state(enum s_state_t s) {
  SIRDEBUG("old state: %s\n", s_state_names[sirene_state]);
  sirene_state = s;
  SIRDEBUG("new state: %s\n", s_state_names[sirene_state]);
}

void activate_alarm() {
  burn_knock_records();
  set_state(ST_ALARM_ON);
  memset(scroll_state, 0, sizeof(uint16_t)*4);
}

void sirene_state_machine(enum event_t e) {
  switch (sirene_state) {
    case ST_PREINIT:
      DDRA |= (1<<PA5);
      restore_ports();
      set_state(ST_INIT);
      break;
    case ST_INIT:
      burn_knock_records();
      sockets_down();
      sockets_up();
      set_state(ST_IDLE);
      break;
    case ST_IDLE:
      switch (e) {
        case RIGHT_SEQUENCE:
          activate_alarm();
          break;
        case KEY_DOWN:
          set_state(ST_SETTING1);
          memset(scroll_state, 0, sizeof(uint16_t)*4);
          break;
        default:
          break;
      }
      break;
    case ST_ALARM_ON:
      switch (e) {
        case KEY_DOWN:
        case KEY_UP:
        case KEY_SELECT:
          set_state(ST_ALARM_DISABLED);
          break;
        case ALARM_TIMER_EXPR:
          set_state(ST_ALARM_OFF);
          break;
        default:
          break;
      }
      break;
    case ST_ALARM_OFF:
      switch (e) {
        case KEY_DOWN:
        case KEY_UP:
        case KEY_SELECT:
          set_state(ST_ALARM_DISABLED);
          memset(scroll_state, 0, sizeof(uint16_t)*4);
          break;
        case ALARM_TIMER_EXPR:
          set_state(ST_ALARM_ON);
          break;
        default:
          break;
      }
      break;
    case ST_ALARM_DISABLED:
      switch (e) {
        case KEY_UP:
          scroll_state[1] = scroll_string_paged_dec(knock_message_text, knock_message_text_size, 3*20, scroll_state[1]);
          break;
        case KEY_DOWN:
          scroll_state[1] = scroll_string_paged_inc(knock_message_text, knock_message_text_size, 3*20, scroll_state[1]);
          break;
        case KEY_SELECT:
          memset(scroll_state, 0, sizeof(uint16_t)*4);
          set_state(ST_IDLE);
          break;
        default:
          break;
      }
      break;
    case ST_SETTING1:
      switch (e) {
        case RIGHT_SEQUENCE:
          activate_alarm();
          break;
        case KEY_SELECT:
          randomize_ports();
          memset(scroll_state, 0, sizeof(uint16_t)*4);
          set_state(ST_INIT);
          break;
        case KEY_UP:
        case KEY_DOWN:
          memset(scroll_state, 0, sizeof(uint16_t)*4);
          set_state(ST_IDLE);
          break;
        default:
          break;
      }
      break;
  }
}

int16_t
fernmelder_init(void)
{
  sirene_state=ST_PREINIT;
  return ECMD_FINAL_OK;
}

uint16_t get_adc(uint8_t chan) {
  ADMUX = (ADMUX & 0xF0) | chan;
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC)) {}
  return ADC;
}

#define is_x_within_value(x, within, value) value-within <= x && x <= value+within
enum event_t display_key(uint16_t value)
{
  static uint8_t debounce = 0;
  enum event_t keypressed = NONE;
  if( value < KEYNO-20 )
  {
    if(debounce == 0)
    {
      if (is_x_within_value(value, 5, KEY1)) {
        keypressed = KEY_DOWN;
        SIRDEBUG("key_value: %d, key: down \n", value);
      }
      if (is_x_within_value(value, 5, KEY2)) {
        keypressed = KEY_UP;
        SIRDEBUG("key_value: %d, key: up \n", value);
      }
      if (value <= KEY3+5) {
        keypressed = KEY_SELECT;
        SIRDEBUG("key_value: %d, key: select \n", value);
      }
    }
    debounce=1;
  } else
    debounce=0;
  return keypressed;
}

/* Will be called every 100ms */
void process_alarm() {
  if (sirene_state == ST_ALARM_ON) {
    //PORTA ^= (1<<PA5);
    PORTA |= (1<<PA5);
  } else {
    PORTA &= ~(1<<PA5);
  }
}

void print_line(char *s, uint16_t line, uint16_t length) {
  char *lines = malloc(sizeof(char)*length+1);
  memset(lines, ' ', sizeof(char)*length);
  memcpy(lines, s+(length*line), length);
  lines[length] = '\0';
  hd44780_goto(line, 0);
  hd44780_puts(lines, &lcd);
  SIRDEBUG("line %d: %s\n", line, lines);
  free(lines);
}

/* 1 Second */
#define REFRESH_RATE_1 50
/* 100 ms */
#define REFRESH_RATE_2 5
/* 3 Seconds */
#define ALARM_ONOFFTIME 150
int16_t
fernmelder_periodic(void)
{
  static uint16_t counter=0;
  char display[4*20+1] = { '\0' };
  enum event_t e = NONE;

  if (counter % ALARM_ONOFFTIME == 0) {
    e = ALARM_TIMER_EXPR;
    sirene_state_machine(e);
  }
  if (external_event != NONE) {
    e = external_event;
    external_event = NONE;
    sirene_state_machine(e);
  }
  /* Every 100 ms */
  if (counter % REFRESH_RATE_2 == 0) {
    process_alarm();
    e = display_key(get_adc(4));
    sirene_state_machine(e);
  }
  if (counter % REFRESH_RATE_1 == 0) {
    char status_line[80] = {'\0'};
    char tmp_line[80] = {'\0'};
    clock_datetime_t dt;
    uint8_t total_pages;
    uip_ipaddr_t hostaddr;


    switch (sirene_state) {
    case ST_PREINIT:
    case ST_INIT:
      sprintf(display, "init");
      break;
    case ST_IDLE:
      clock_localtime(&dt, clock_get_time());
      sprintf(status_line, " Kybernetischer Fernmelder - IDLE ");
      scroll_state[0] = scroll_string_x_auto(status_line,
                                             strlen(status_line),
                                             display,
                                             20,
                                             scroll_state[0]);
      sprintf(display+20, "%02d:%02d:%02d %02d.%02d.%04d", dt.hour, dt.min, dt.sec, dt.day, dt.month, dt.year+1900);
      uip_gethostaddr(&hostaddr);
      print_ipaddr(&hostaddr, tmp_line, 80);
      rstrip(tmp_line, strlen(tmp_line));
      sprintf(status_line, "v4: %s ", tmp_line, strlen(tmp_line));
      if (strlen(status_line) > 20) {
        scroll_state[1] = scroll_string_x_auto(status_line,
                                               strlen(status_line),
                                               display+40,
                                               20,
                                               scroll_state[1]);
      } else {
        strncpy(display+40, status_line, 20);
      }
      sprintf(status_line, "Ports: %d,%d,%d,%d,%d  ", knock_ports[0], knock_ports[1], knock_ports[2], knock_ports[3], knock_ports[4]);
      scroll_state[2] = scroll_string_x_auto(status_line,
                                             strlen(status_line),
                                             display+60,
                                             20,
                                             scroll_state[2]);
      break;
    case ST_ALARM_ON:
    case ST_ALARM_OFF:
    case ST_ALARM_DISABLED:
      print_ipaddr(&knock_remote_addr, tmp_line, 80);
      sprintf(status_line, "%s teilt mit: ", tmp_line);
      scroll_state[0] = scroll_string_x_auto(status_line,
                                             strlen(status_line),
                                             display,
                                             18,
                                             scroll_state[0]);
      total_pages = scroll_string_paged_total(knock_message_text, strlen(knock_message_text), 3*20);
      display[18] = ' ';
      display[19] = '0' + (uint8_t)(8*(float)scroll_state[1]/total_pages);
      SIRDEBUG("scroll_state[1]: %d total_pages: %d knock_message_text_size: %d\n", scroll_state[1], total_pages, knock_message_text_size);
      scroll_state[1] = scroll_string_paged(knock_message_text, knock_message_text_size, display+20, 3*20, scroll_state[1]);
      break;
    case ST_SETTING1:
      sprintf(display, "Ports ändern?");
      sprintf(display+20, "nein: up/down");
      sprintf(display+40, "  ja: select");
      sprintf(status_line, "Ports: %d,%d,%d,%d,%d  ", knock_ports[0], knock_ports[1], knock_ports[2], knock_ports[3], knock_ports[4]);
      scroll_state[2] = scroll_string_x_auto(status_line,
                                             strlen(status_line),
                                             display+60,
                                             20,
                                             scroll_state[2]);
    }
    /* Make sure we don't have any terminator elements in display */
    replace_chr(display, '\0', ' ', 4*20+1);
    print_line(display, 0, 20);
    print_line(display, 1, 20);
    print_line(display, 2, 20);
    print_line(display, 3, 20);
  }
  counter++;
  return ECMD_FINAL_OK;
}

/*
  -- Ethersex META --
  header(services/fernmelder/fernmelder.h)
  init(fernmelder_init)
  timer(1,fernmelder_periodic())
*/
