/**
   Copyright (c) 2021 WIZnet Co.,Ltd

   SPDX-License-Identifier: BSD-3-Clause
*/

//#define STATIC_IP
#define USE_DHCP

#if defined(TCP_SERVER)
#pragma message("building SERVER")
#endif	/* SERVER */

#if defined(TCP_CLIENT)
#pragma message("building CLIENT")
#endif	/* CLIENT */


#define RTK_RECV
/**
   ----------------------------------------------------------------------------------------------------
   Includes
   ----------------------------------------------------------------------------------------------------
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
//#include <stdbool.h>

#include "port_common.h"
extern "C" {
#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "timer/timer.h"
}
#include "loopback.h"
#include "socket.h"
#include "pico/unique_id.h"
#include "pico/rand.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/structs/sio.h"

#include "cfg.h"
#define GPS_LIB
#if defined(GPS_LIB)
#include "gpsLib.h"
#endif  /* GPS_LIB */

#if defined(USE_DHCP)
#include "dhcp.h"
#include "dns.h"

//#include "timer.h"
#endif

#define RTK_SEND

#if !defined(GPS_LIB)

#define DBG0_PIN 28
#define DBG1_PIN 27

#define sio_hw ((sio_hw_t *)SIO_BASE)

inline void dbg0Set()
{
 sio_hw->gpio_set = (1 << DBG0_PIN);
}

inline void dbg0Clr()
{
 sio_hw->gpio_clr = (1 << DBG0_PIN);
}

inline void dbg1Set()
{
 sio_hw->gpio_set = (1 << DBG1_PIN);
}

inline void dbg1Clr()
{
 sio_hw->gpio_clr = (1 << DBG1_PIN);
}

inline uint32_t usTime()
{
 return timer_hw->timerawl;
}

#endif	/* GPS_LIB */

/**
   ----------------------------------------------------------------------------------------------------
   Macros
   ----------------------------------------------------------------------------------------------------
*/

#if defined(USE_DHCP)
/* Retry count */
#define DHCP_RETRY_COUNT 5
#define DNS_RETRY_COUNT 5
#endif

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_TCP_SERVER 0
#define SOCKET_TCP_CLIENT 1
#if 0
#define SOCKET_UDP 2
#define SOCKET_TCP_SERVER6 3
#define SOCKET_TCP_CLIENT6 4
#define SOCKET_UDP6 5
#define SOCKET_TCP_SERVER_DUAL 6
#define SOCKET_DHCP 7
#endif

#if defined(USE_DHCP)
/* Socket */
#define SOCKET_DHCP 2
#define SOCKET_DNS 3
#endif

/* Port */
#define PORT_TCP_SERVER 8088
#define PORT_TCP_CLIENT 8088

#define PORT_TCP_CLIENT_DEST 8088
#define PORT_UDP 5003

#define PORT_TCP_SERVER6 5004
#define PORT_TCP_CLIENT6 5005
#define PORT_TCP_CLIENT6_DEST 5006
#define PORT_UDP6 5007

#define PORT_TCP_SERVER_DUAL 5008

#define IPV4
// #define IPV6

#ifdef IPV4
// #define TCP_SERVER
// #define TCP_CLIENT
// #define UDP
#endif

#ifdef IPV6
// #define TCP_SERVER6
// #define TCP_CLIENT6
// #define UDP6
#endif

#if defined IPV4 && defined IPV6
// #define TCP_SERVER_DUAL
#endif

#define RETRY_CNT   10000

// #if defined(TCP_SERVER)
// #define HOST_NAME "Server2"
// #endif
//
// #if defined(TCP_CLIENT)
// #define HOST_NAME "Client2"
// #define SERVER_NAME "Server1"
// //#define SERVER_NAME "mac-mini"
// #endif

/**
   ----------------------------------------------------------------------------------------------------
   Variables
   ----------------------------------------------------------------------------------------------------
*/
/* Network */
static wiz_NetInfo g_net_info = {
 .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
 .ip = {192, 168, 0, 180},                    // IP address
 .sn = {255, 255, 255, 0},                    // Subnet Mask
 .gw = {192, 168, 0, 1},                      // Gateway
 .dns = {192, 168, 0, 18},                    // DNS server
#if _WIZCHIP_ > W5500
 .lla = {
  0xfe, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x02, 0x08, 0xdc, 0xff,
  0xfe, 0x57, 0x57, 0x25
 },             // Link Local Address
 .gua = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
 },             // Global Unicast Address
 .sn6 = {
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
 },             // IPv6 Prefix
 .gw6 = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
 },             // Gateway IPv6 Address
 .dns6 = {
  0x20, 0x01, 0x48, 0x60,
  0x48, 0x60, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x88, 0x88
 },             // DNS6 server
 .ipmode = NETINFO_STATIC_ALL
#else
#if defined(STATIC_IP)
 .dhcp = NETINFO_STATIC
#endif
#if defined(USE_DHCP)
 .dhcp = NETINFO_DHCP,
#endif
#endif
};

// enum RCV_STATE {RCV_IDLE, RCV_GET_LEN, RCV_GET_DATA, RCV_TEXT};
//
// #define RTK_BUF_SIZE 1024
// uint32_t crcBuf[1024];
//
// typedef struct S_RTK_DATA
// {
//  enum RCV_STATE state;
//  unsigned int t0;
//  uint64_t startTime;
//  uint32_t crc;
//  int count;
//  int len;
//  int fil;
//  unsigned char buf[RTK_BUF_SIZE];
//  unsigned int t0Accum;
//  int rxAccum;
//  unsigned int tData;
//  //int rxCount;
// } T_RTK_DATA, *P_RTK_DATA;
//
// T_RTK_DATA rtk;

#if defined(TCP_SERVER)
/* Loopback */
static uint8_t g_tcp_server_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
};
#endif
#if defined(TCP_CLIENT)
uint8_t tcp_client_destip[] = {
 192, 168, 50, 103
};

uint16_t tcp_client_destport = PORT_TCP_CLIENT_DEST;

static uint8_t g_tcp_client_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
};
#endif
#if defined(UDP_CLIENT)
static uint8_t g_udp_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
};
#endif
#if defined(TCP_SERVER6)
static uint8_t g_tcp_server6_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
};
#endif
#if defined(TCP_CLIENT6)
uint8_t tcp_client_destip6[] = {
 0x20, 0x01, 0x02, 0xb8,
 0x00, 0x10, 0xff, 0xff,
 0x71, 0x48, 0xcb, 0x27,
 0x36, 0xb9, 0x99, 0x2e
};

uint16_t tcp_client_destport6 = PORT_TCP_CLIENT6_DEST;

static uint8_t g_tcp_client6_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
};
#endif
#if defined(UDP_CLIENT6)
static uint8_t g_udp6_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
};
#endif
#if defined(TCP_SERVER_DUAL)
static uint8_t g_tcp_server_dual_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
};
#endif
#if defined(USE_DHCP)
static uint8_t g_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {
 0,
}; // common buffer

/* DHCP */
static uint8_t g_dhcp_get_ip_flag = 0;

/* DNS */
#if defined(TCP_SERVER)
auto g_dns_target_domain = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(HOST_NAME));
#endif
#if defined(TCP_CLIENT)
auto g_dns_target_domain = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(SERVER_NAME));
#endif

static uint8_t g_dns_target_ip[4] = {
 0,
};
static uint8_t g_dns_get_ip_flag = 0;


/* Timer */
static volatile uint16_t g_msec_cnt = 0;
#endif

/**
   ----------------------------------------------------------------------------------------------------
   Functions
   ----------------------------------------------------------------------------------------------------
*/

#if defined(USE_DHCP)
/* DHCP */
static void wizchip_dhcp_init();
static void wizchip_dhcp_assign();
static void wizchip_dhcp_conflict();

/* Timer */
static void repeating_timer_callback();
#endif

// static void buildCRC24qTable();

#define UART_RING
#define UART1_RING

/* ---------- UART ring buffer (IRQ-driven RX, non-blocking reads) ---------- */

#if defined(UART_RING)

/* ---------- UART configuration ---------- */
#define UART0_ID   uart0
#define UART0_TX   0
#define UART0_RX   1
#define UART0_BAUD 115200

#define UART1_ID   uart1
#define UART1_TX   8
#define UART1_RX   9
#define UART1_BAUD 115200

#define UART_RING_SIZE 256

typedef struct {
 uint8_t  buf[UART_RING_SIZE];
 volatile uint32_t head; /* next slot to write */
 volatile uint32_t tail; /* next slot to read */
} uart_ring_t;

static void ring_push(uart_ring_t *ring, uint8_t byte) {
 auto next = (uint32_t) ((ring->head + 1) % UART_RING_SIZE);
 if (next != ring->tail) {        /* drop the byte if the ring is full */
  ring->buf[ring->head] = byte;
  ring->head = next;
 }
}

static bool ring_pop(uart_ring_t *ring, uint8_t *byte) {
 if (ring->head == ring->tail)
  return false; /* empty */
 *byte = ring->buf[ring->tail];
 ring->tail = (uint32_t) ((ring->tail + 1) % UART_RING_SIZE);
 return true;
}
#endif

#if defined(UART0_RING)
static uart_ring_t uart0_rx_ring;

static void on_uart0_rx(void) {
 while (uart_is_readable(UART0_ID)) {
  ring_push(&uart0_rx_ring, uart_getc(UART0_ID));
 }
}

static size_t uart0_read_available(uint8_t *data, size_t max_len) {
 size_t count = 0;
 uint8_t byte;
 while (count < max_len && ring_pop(&uart0_rx_ring, &byte)) {
  data[count++] = byte;
 }
 return count;
}
#endif

#if 0 && defined(UART1_RING)
static uart_ring_t uart1_rx_ring;

static void on_uart1_rx(void) {
 while (uart_is_readable(UART1_ID)) {
  ring_push(&uart1_rx_ring, uart_getc(UART1_ID));
 }
}

static size_t uart1_read_available(uint8_t *data, size_t max_len) {
 size_t count = 0;
 while ((count < max_len) &&
        !ring_pop(&uart1_rx_ring, data))
 {
  data += 1;
  count += 1;
 }
 return count;
}
#endif

#if 0
/* ---------- Multi-byte UART helpers ---------- */

/* Blocking: sends/receives exactly len bytes, blocks until done. */
static void uart0_write(const uint8_t *data, size_t len) { uart_write_blocking(UART0_ID, data, len); }
static void uart0_read_exact(uint8_t *data, size_t len)  { uart_read_blocking(UART0_ID, data, len); }

static void uart1_write(const uint8_t *data, size_t len) { uart_write_blocking(UART1_ID, data, len); }
static void uart1_read_exact(uint8_t *data, size_t len)  { uart_read_blocking(UART1_ID, data, len); }

/* Non-blocking: pulls whatever is already in the ring buffer (up to
 * max_len bytes) and returns how many bytes were actually copied.
 * Safe to call every loop iteration without stalling the socket code. */
#endif

/* ---------- Init helpers ---------- */
static void uart_init_hw() {
#if defined(UART0_RING)
 memset((void *) uart0_rx_ring.buf, 0, sizeof(uart_ring_t));
 uart_init(UART0_ID, UART0_BAUD);
 gpio_set_function(UART0_TX, GPIO_FUNC_UART);
 gpio_set_function(UART0_RX, GPIO_FUNC_UART);

 irq_set_exclusive_handler(UART0_IRQ, on_uart0_rx);
 irq_set_enabled(UART0_IRQ, true);
 uart_set_irq_enables(UART0_ID, true /* rx irq */, false /* tx irq */);
#endif

#if defined(UART1_RING)
#if 0
 memset((void *) uart1_rx_ring.buf, 0, sizeof(uart_ring_t));
#endif
 uart_init(UART1_ID, UART1_BAUD);
 gpio_set_function(UART1_TX, GPIO_FUNC_UART);
 gpio_set_function(UART1_RX, GPIO_FUNC_UART);

#if 0
 irq_set_exclusive_handler(UART1_IRQ, on_uart1_rx);
 irq_set_enabled(UART1_IRQ, true);
 uart_set_irq_enables(UART1_ID, true, false);
#endif
#endif
}

void printMac(const char *mac_address)
{
 printf("%02x:%02x:%02x:%02x:%02x:%02x",
	mac_address[0], mac_address[1], mac_address[2],
	mac_address[3], mac_address[4], mac_address[5]);
}

void get_mac_from_board_id(uint8_t mac[6]) {
 pico_unique_board_id_t id;
 pico_get_unique_board_id(&id);  // 8 bytes, id.id[0..7]

 mac[0] = 0x02;              // locally administered, unicast
 mac[1] = id.id[2];
 mac[2] = id.id[3];
 mac[3] = id.id[5];
 mac[4] = id.id[6];
 mac[5] = id.id[7];
}

#if !defined(GPS_LIB)
void processSerial(int sock);
#endif	/* GPS_LIB */

uint8_t serialBuf[256];
uint64_t dhcpT0;

void networkInit(bool first)
{
 printf("networkInit\n");
 int retval = 0;

 wizchip_reset();
 wizchip_initialize();
 wizchip_check();

 get_mac_from_board_id(g_net_info.mac);
 network_initialize(g_net_info);
 print_network_information(g_net_info);

#if defined(USE_DHCP)

 int dhcp_retry = 0;
 int dns_retry = 0;

 if (first)
  wizchip_1ms_timer_initialize(repeating_timer_callback);

 wizchip_dhcp_init();

 DNS_init(SOCKET_DNS, g_ethernet_buf);

 g_dhcp_get_ip_flag = 0;
 while (g_dhcp_get_ip_flag == 0)
 {
  if (g_net_info.dhcp == NETINFO_DHCP)
  {
   retval = DHCP_run();
   if (retval == DHCP_IP_LEASED)
   {
    if (g_dhcp_get_ip_flag == 0)
    {
     printf(" DHCP success\n");
     g_dhcp_get_ip_flag = 1;
    }
   }
   else if (retval == DHCP_FAILED)
   {
    g_dhcp_get_ip_flag = 0;
    dhcp_retry++;

    if (dhcp_retry <= DHCP_RETRY_COUNT)
    {
     printf(" DHCP timeout occurred and retry %d\n", dhcp_retry);
    }
   }

   if (dhcp_retry > DHCP_RETRY_COUNT)
   {
    printf(" DHCP failed\n");
    DHCP_stop();
    // ReSharper disable once CppDFAEndlessLoop
    while (true)
    {
    }
   }

   wizchip_delay_ms(1000); // wait for 1 second
  }

  /* Get IP through DNS */
  if ((g_dns_get_ip_flag == 0) && (retval == DHCP_IP_LEASED))
  {
   printf(" starting DNS loop %s\n", reinterpret_cast<const char*>(g_dns_target_domain));
   // ReSharper disable once CppDFAEndlessLoop
   while (true)
   {
    retval = static_cast<unsigned char>(DNS_run(g_net_info.dns, g_dns_target_domain, g_dns_target_ip));
    if (retval > 0)
    {
     printf(" DNS success\n");
     printf(" Target domain : %s\n", reinterpret_cast<const char*>(g_dns_target_domain));
     printf(" IP of target domain : %d.%d.%d.%d\n", g_dns_target_ip[0], g_dns_target_ip[1],
	    g_dns_target_ip[2], g_dns_target_ip[3]);
     g_dns_get_ip_flag = 1;
     break;
    }
    else
    {
     dns_retry++;
     if (dns_retry <= DNS_RETRY_COUNT)
     {
      printf(" DNS timeout occurred and retry %d\n", dns_retry);
     }
    }

    if (dns_retry > DNS_RETRY_COUNT)
    {
     printf(" DNS failed\n");
     // ReSharper disable once CppDFAEndlessLoop
     while (true);
    }

    wizchip_delay_ms(1000); // wait for 1 second
   }
  }
 }

#endif

 /* Get network information */
 print_network_information(g_net_info);
}

uint16_t any_port;
uint32_t lastTime;
int lastStatus;

void wizchip_rst() {
 gpio_init(PIN_RST);

#if defined(USE_PIO) && (_WIZCHIP_ == W5500)
 gpio_pull_up(PIN_RST);
 gpio_set_dir(PIN_RST, GPIO_OUT);
 sleep_ms(5);
#else
 gpio_set_dir(PIN_RST, GPIO_OUT);
#endif
 unsigned int tmp = sio_hw->gpio_oe;
 printf("output enable %08x %08x\n", tmp, tmp & (1 << PIN_RST));
 gpio_put(PIN_RST, false);
 sleep_ms(100);

 gpio_put(PIN_RST, true);
 sleep_ms(100);

 bi_decl(bi_1pin_with_name(PIN_RST, "WIZCHIP RESET"));
}

void wizchip_spi_init() {
#ifdef USE_PIO
 spi_handle = wiznet_spi_pio_open(&g_spi_config);
 (*spi_handle)->set_active(spi_handle);
#else
 // this example will use SPI0 at 5MHz
 spi_init(SPI_PORT, 1 * 1000 * 1000);

 gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
 gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
 gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

 // make the SPI pins available to picotool
 bi_decl(bi_3pins_with_func(PIN_MISO, PIN_MOSI, PIN_SCK, GPIO_FUNC_SPI));

 // chip select is active-low, so we'll initialise it to a driven-high state
 gpio_init(PIN_CS);
 gpio_set_dir(PIN_CS, GPIO_OUT);
 gpio_put(PIN_CS, true);

 // make the SPI pins available to picotool
 bi_decl(bi_1pin_with_name(PIN_CS, "W5x00 CHIP SELECT"));

#ifdef USE_SPI_DMA
 dma_tx = dma_claim_unused_channel(true);
 dma_rx = dma_claim_unused_channel(true);

 dma_channel_config_tx = dma_channel_get_default_config(dma_tx);
 channel_config_set_transfer_data_size(&dma_channel_config_tx, DMA_SIZE_8);
 channel_config_set_dreq(&dma_channel_config_tx, DREQ_SPI0_TX);

 // We set the inbound DMA to transfer from the SPI receive FIFO to a memory buffer paced by the SPI RX FIFO DREQ
 // We configure the read address to remain unchanged for each element, but the write
 // address to increment (so data is written throughout the buffer)
 dma_channel_config_rx = dma_channel_get_default_config(dma_rx);
 channel_config_set_transfer_data_size(&dma_channel_config_rx, DMA_SIZE_8);
 channel_config_set_dreq(&dma_channel_config_rx, DREQ_SPI0_RX);
 channel_config_set_read_increment(&dma_channel_config_rx, false);
 channel_config_set_write_increment(&dma_channel_config_rx, true);
#endif
#endif
}

/**
   ----------------------------------------------------------------------------------------------------
   Main
   ----------------------------------------------------------------------------------------------------
*/
int main() {
 /* Initialize */
 int retval = 0;

 dbg0Clr();
 gpio_init(DBG0_PIN);
 gpio_set_dir(DBG0_PIN, GPIO_OUT);

 dbg1Clr();
 gpio_init(DBG1_PIN);
 gpio_set_dir(DBG1_PIN, GPIO_OUT);

 stdio_init_all();

#if defined(LIB_PICO_STDIO_USB)
 sleep_ms(3000);
#endif

 printf("==========================================================\n");
 printf("Compiled @ %s, %s\n", __DATE__, __TIME__);
 printf("==========================================================\n");

 printf("hostname %s\n", HOST_NAME);

 lastStatus = -1;
 any_port = 50000 + (get_rand_32() & 0x3ff);
 printf("using port %d\n", any_port);

#if defined(UART_RING)
 uart_init_hw();
#endif
    
 buildCRC24qTable();

 wizchip_rst();
 bool is_output = !(gpio_get_dir(PIN_RST));
 printf("is_output %d\n", is_output);

 // gpio_init(PIN_RST);
 // gpio_set_dir(PIN_RST, GPIO_OUT);

 wizchip_spi_init();
 wizchip_cris_initialize();

 networkInit(true);

 is_output = !(gpio_get_dir(PIN_RST));
 printf("is_output %d\n", is_output);

 /*Choose IPv4 / IPv6*/
#if _WIZCHIP_ > W5500
#ifdef IPV6
 set_loopback_mode_W6x00(AS_IPV6);
#endif
#endif

#ifdef TCP_SERVER
 dhcpT0 = timer_time_us_64(timer_hw);

//#define DHCP_INTERVAL (3600 * 1000000ULL)
#define DHCP_INTERVAL (300 * 1000000ULL)

 static uint32_t secTimer;
 static bool phyDown = false;
 while (true)
 {
  const uint8_t status = getSn_SR(SOCKET_TCP_SERVER);

  const uint64_t t = timer_time_us_64(timer_hw);
  if ((t - dhcpT0) > DHCP_INTERVAL)
  {
   dhcpT0 = t;
   DHCP_run();
  }

  const uint32_t t32 = t;
  if (t32 - secTimer > 1000000)
  {
   secTimer = t32;

   if (rtk.state != RCV_IDLE &&
       (t32 - rtk.t0) > (100 * 1000))
   {
    rtk.state = RCV_IDLE;
   }

   if ((status == SOCK_ESTABLISHED))
   {
    printf("t %u\n", static_cast<unsigned int>(t32 - rtk.tData));
    if ((t32 - rtk.tData) > (10 * 1000000))
    {
     rtk.tData = t32;
     uint8_t temp;
     if (ctlwizchip(CW_GET_PHYLINK, (void*)&temp) == -1)
     {
      printf(" Unknown PHY link status\n");
     }
     else
     {
      printf(" PHY link status %d\n", temp);
      if (temp == PHY_LINK_OFF)
      {
       phyDown = true;
       printf("phy link off\n");
      }
      else
      {
       if (phyDown)
       {
        phyDown = false;
        networkInit(false);
       }
       else
       {
        printf("disconnect\n");
        disconnect(SOCKET_TCP_SERVER);
       }
      }
     }
    }
   }
  }

  char* p = reinterpret_cast<char*>(serialBuf);
  int size = 0;
  while (uart_is_readable(uart1) &&
	 size < sizeof(serialBuf))
  {
   *p++ = uart_getc(uart1);
   size++;
  }

  if (size != 0)
  {
   if (status == SOCK_ESTABLISHED)
   {
    const int ret = send(SOCKET_TCP_SERVER, serialBuf, size);
    if (ret < 0)
    {
     printf("send returned %d\n", ret);
     close(SOCKET_TCP_SERVER);
     return ret;
    }
   }
  }

  retval = loopback_tcps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT_TCP_SERVER);
  if (retval < 0)
  {
   printf(" loopback_tcps error : %d\n", retval);

   // ReSharper disable once CppDFAEndlessLoop
   while (true);
  }
 }
 // ReSharper disable once CppDFAUnreachableCode
 printf("exit main\n");
#endif	/* TCP_SERVER */

#ifdef TCP_CLIENT
 dhcpT0 = timer_time_us_64(timer_hw);

 lastTime = timer_time_us_64(timer_hw);

 while (true)
 {
  const uint64_t t = timer_time_us_64(timer_hw);
  if ((t - dhcpT0) > (3600 * 1000000ULL))
  {
   dhcpT0 = t;
   DHCP_run();
  }
  //const uint32_t t32 = t;
  if ((t - rtk.t0) > (100 * 1000))
  {
   rtk.state = RCV_IDLE;
  }

  retval = loopback_tcpc(SOCKET_TCP_CLIENT, g_tcp_client_buf,
			 g_dns_target_ip, tcp_client_destport);
  if (retval < 0)
  {
   printf(" loopback_tcpc error : %d\n", retval);

   // ReSharper disable once CppDFAEndlessLoop
   while (true)
    ;
  }
 }
#endif	/* TCP_CLIENT */
    
#ifdef UDP
 /* UDP loopback test */
 if ((retval = loopback_udps(SOCKET_UDP, g_udp_buf, PORT_UDP)) < 0)
 {
  printf(" loopback_udps error : %d\n", retval);

  while (1);
 }
#endif
#ifdef IPV6_AVAILABLE
#ifdef TCP_SERVER6
 /* TCP server loopback test */
 if ((retval = loopback_tcps(SOCKET_TCP_SERVER6, g_tcp_server6_buf, PORT_TCP_SERVER6)) < 0)
 {
  printf(" loopback_tcps IPv6 error : %d\n", retval);

  while (1);
 }
#endif
#ifdef TCP_CLIENT6
 /* TCP client loopback test */
 if ((retval = loopback_tcpc(SOCKET_TCP_CLIENT6, g_tcp_client6_buf, tcp_client_destip6, tcp_client_destport6)) < 0)
 {
  printf(" loopback_tcpc IPv6 error : %d\n", retval);

  while (1);
 }
#endif
#ifdef UDP6
 /* UDP loopback test */
 if ((retval = loopback_udps(SOCKET_UDP6, g_udp6_buf, PORT_UDP6)) < 0)
 {
  printf(" loopback_udps IPv6 error : %d\n", retval);

  while (1);
 }
#endif
#ifdef TCP_SERVER_DUAL
 /* TCP server dual loopback test */
 if ((retval = loopback_tcps(SOCKET_TCP_SERVER_DUAL, g_tcp_server_dual_buf, PORT_TCP_SERVER_DUAL, AS_IPDUAL)) < 0)
 {
  printf(" loopback_tcps IPv6 error : %d\n", retval);

  while (1);
 }
#endif
#endif
}

/**
   ----------------------------------------------------------------------------------------------------
   Functions
   ----------------------------------------------------------------------------------------------------
*/

// /* ── CRC-24Q constants ───────────────────────────────────────────────────── */
//
// #define CRC24Q_POLY      0x1864CFBu  /* Generator polynomial                 */
// #define RTCM3_PREAMBLE   0xD3u       /* Mandatory first byte of every frame  */
// #define RTCM3_HDR_LEN    3           /* Preamble + 2 length/reserved bytes   */
// #define RTCM3_CRC_LEN    3           /* 24-bit CRC appended at end           */
// #define RTCM3_MIN_FRAME  (RTCM3_HDR_LEN + RTCM3_CRC_LEN)
//
// /* ── CRC-24Q lookup table (generated once on first use) ─────────────────── */
//
// static uint32_t crc24qTable[256];
//
// static void buildCRC24qTable()
// {
//  for (uint32_t i = 0; i < 256; i++)
//  {
//   uint32_t crc = i << 16;
//   for (int j = 0; j < 8; j++)
//   {
//    crc <<= 1;
//    if (crc & 0x1000000u)
//     crc ^= CRC24Q_POLY;
//   }
//   crc24qTable[i] = crc & 0xFFFFFFu;
//  }
// }
//
// uint32_t crc24(const uint32_t crc, const unsigned char c)
// {
//  return ((crc << 8) ^ crc24qTable[((crc >> 16) ^ c) & 0xFFu]) & 0xFFFFFFu;
// }

#ifdef TCP_SERVER

#if !defined(GPS_LIB)
static void processData(const unsigned char *ptr, size_t len);
#endif	/* GPS_LIB */

#define _LOOPBACK_DEBUG_ // NOLINT(*-reserved-identifier)

int32_t loopback_tcps(uint8_t sn, uint8_t* buf, uint16_t port)
{
 int ret;
 int size;
 switch (getSn_SR(sn))
 {
 case SOCK_ESTABLISHED:
  if (getSn_IR(sn) & Sn_IR_CON)
  {
#ifdef _LOOPBACK_DEBUG_
   uint8_t destip[4];
   getSn_DIPR(sn, destip);
   uint16_t destport = getSn_DPORT(sn);
   printf("%d:Connected - %d.%d.%d.%d : %d\n",
	  sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
   setSn_IR(sn, Sn_IR_CON);
  }
	
  size = getSn_RX_RSR(sn);
  if (size > 0) // Don't need to check SOCKERR_BUSY because it doesn't occur.
  {
   if (size > DATA_BUF_SIZE)
    size = DATA_BUF_SIZE;
   ret = static_cast<int>(recv(sn, buf, size));
   if (ret <= 0)
    return ret; // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
   size = static_cast<uint16_t>(ret);

   rtk.tData = usTime();
#if !defined(GPS_LIB)
   processData(buf, size);
#else
   processRemData(buf, size);
#endif	/* GPS_LIB */
  }
  break;

 case SOCK_CLOSE_WAIT:
#ifdef _LOOPBACK_DEBUG_
   printf("%d:Socket Closed\n", sn);
#endif
   ret = static_cast<int>(static_cast<unsigned char>(disconnect(sn)));
   if (ret != SOCK_OK)
    return ret;
  break;

 case SOCK_INIT:
#ifdef _LOOPBACK_DEBUG_
  printf("%d:Listen, TCP server loopback, port [%d]\n", sn, port);
#endif
  ret = static_cast<int>(static_cast<unsigned char>(listen(sn)));
  if (ret != SOCK_OK)
   return ret;
  break;

 case SOCK_CLOSED:
#ifdef _LOOPBACK_DEBUG_
  printf("%d:Socket close\n", sn);
#endif
  ret = static_cast<int>(static_cast<unsigned char>(socket(sn, Sn_MR_TCP, port, 0x00)));
  if (ret != sn)
   return ret;
  break;

 default:
  break;
 }
 return 1;
}

#if !defined(GPS_LIB)

static void processData(const unsigned char *ptr, size_t len)
{
 uart_write_blocking(UART0_ID, ptr, len);
 while (len > 0)
 {
  len -= 1;
  const unsigned char ch = *ptr;
  //uart_putc(UART0_ID, ch);
  switch (rtk.state)
  {
  case RCV_IDLE:
   if (ch == 0xd3)
   {
    rtk.count = 2;
    uart_putc(UART1_ID, ch);
    rtk.buf[0] = ch;
    rtk.crc = crc24qTable[static_cast<int>(ch)];
    crcBuf[0] = rtk.crc;
    rtk.fil = 1;
    rtk.t0 = usTime();
    rtk.state = RCV_GET_LEN;
   }
   else //if (ch == '$')
   {
    rtk.t0 = usTime();
    rtk.state = RCV_TEXT;
    uart_putc(UART1_ID, ch);
#if 0
    Serial.print(ch);
    Serial.flush();
#endif
   }
   break;

  case RCV_GET_LEN:
   uart_putc(UART1_ID, ch);
   rtk.crc = ((rtk.crc << 8) ^ crc24qTable[((rtk.crc >> 16) ^ ch) & 0xFFu]) & 0xFFFFFFu;
   crcBuf[rtk.fil] = rtk.crc;
   rtk.len = (rtk.len << 8) + ch;
   rtk.buf[rtk.fil] = ch;
   rtk.fil += 1;
   rtk.count -= 1;
   if (rtk.count == 0)
   {
    rtk.state = RCV_GET_DATA;
    rtk.len &= 0x3ff;
    rtk.len += 3;
   }
   break;

  case RCV_GET_DATA:
   uart_putc(UART1_ID, ch);
   rtk.crc = ((rtk.crc << 8) ^ crc24qTable[((rtk.crc >> 16) ^ ch) & 0xFFu]) & 0xFFFFFFu;
   crcBuf[rtk.fil] = rtk.crc;
   rtk.buf[rtk.fil] = ch;
   rtk.fil += 1;
   rtk.len -= 1;
   if (rtk.len == 0)
   {
#if 0
    const int type = (rtk.buf[3] << 4) | (rtk.buf[4] >> 4);
    printf("len %4d type %4d CRC %08x\n", rtk.fil, type, rtk.crc);
#endif
#if defined(DBG_PRT)
    if (prt == 1)
    {
     printHex(rtk.buf, rtk.fil);
     printHex(crcBuf, rtk.fil << 2);
     prt = 0;
    }
#endif	/* DBG_PRT */
    rtk.state = RCV_IDLE;
   }
   break;

  case RCV_TEXT:
   uart_putc(UART1_ID, ch);
#if 0
   Serial.print(ch);
#endif
   if (ch == '\n')
   {
    rtk.state = RCV_IDLE;
   }
   break;
  }
  ptr += 1;
 }
}

#endif	/* GPS_LIB */

#endif	/* TCP_SERVER */

#if defined(TCP_CLIENT)
#define _LOOPBACK_DEBUG_ // NOLINT(*-reserved-identifier)
int32_t loopback_tcpc(uint8_t sn, uint8_t* buf, uint8_t* destip, uint16_t destport)
{
 dbg0Set();
 int32_t ret; // return value for SOCK_ERROR
 //uint16_t size = 0;

 // Socket Status Transitions
 // Check the W5500 Socket n status register
 // (Sn_SR, The 'Sn_SR' controlled by Sn_CR command or Packet send/recv status)
 const uint8_t status = getSn_SR(sn);
 const uint8_t ir = getSn_IR(sn);
 if (status != SOCK_ESTABLISHED)
 {
  while (uart_is_readable(uart1))
   uart_getc(uart1);
 }

 if (status != lastStatus)
 {
  lastStatus = status;
  const uint32_t t0 = timer_time_us_64(timer_hw);
  const unsigned int delta = t0 - lastTime;
  lastTime = t0;
  printf("status %02x ir %02x, delta us %u\n", status, ir, delta);
 }

 dbg0Clr();
 switch (status)
 {
 case SOCK_ESTABLISHED:
  if (getSn_IR(sn) & Sn_IR_CON)
  {
   // Socket n interrupt register mask; TCP CON interrupt = connection with peer is successful
#ifdef _LOOPBACK_DEBUG_
   printf("%d:Connected to - %d.%d.%d.%d : %d\n",
	  sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
   setSn_IR(sn, Sn_IR_CON); // this interrupt should write the bit cleared to '1'
  }

  processSerial(SOCKET_TCP_CLIENT);

  size_t size;
  if ((size = getSn_RX_RSR(sn)) > 0)
  {
   // Sn_RX_RSR: Socket n Received Size Register, Receiving data length
   if (size > DATA_BUF_SIZE)
   {
    size = DATA_BUF_SIZE; // DATA_BUF_SIZE means user defined buffer size (array)
   }

   ret = recv(sn, buf, size); // Data Receive process (H/W Rx socket buffer -> User's buffer)
   if (ret <= 0)
   {
    return ret; // If the received data length <= 0, receive failed and process end
   }

   uart_write_blocking(UART1_ID, buf, ret);
  }
  break;

 case SOCK_CLOSE_WAIT:
#ifdef _LOOPBACK_DEBUG_
  printf("%d:Socket Closed\n", sn);
#endif
  if ((ret = static_cast<unsigned char>(disconnect(sn))) != SOCK_OK)
  {
   return ret;
  }
  break;

 case SOCK_INIT:
#ifdef _LOOPBACK_DEBUG_
  printf("%d:Try to connect to the %d.%d.%d.%d : %d\n", sn, destip[0], destip[1], destip[2], destip[3],
	 destport);
#endif
  if ((ret = static_cast<unsigned char>(connect(sn, destip, destport))) != SOCK_OK)
  {
   return ret; //	Try to TCP connect to the TCP server (destination)
  }
  break;

 case SOCK_CLOSED:
#ifdef _LOOPBACK_DEBUG_
  printf("%d:Socket closed\n", sn);
#endif
  // const char val = close(sn);
  // printf("Socket closed status %d\n", val);
  // printf("open socket port %d\n", any_port);
  // setSIPR(g_dns_target_ip);
  uint8_t addr[4];
  getSIPR(addr);
  printf("ip %d.%d.%d.%d\n", addr[0], addr[1], addr[2], addr[3]);
  if ((ret = static_cast<unsigned char>(socket(sn, Sn_MR_TCP, any_port++, 0x00))) != sn)
  {
   printf("Socket opened status %d\n", static_cast<int>(ret));
   if (any_port == 0xffff)
   {
    any_port = 50000;
   }
   return ret; // TCP socket open with 'any_port' port number
  }
  break;
 default:
  break;
 }
 return 1;
}

#if !defined(GPS_LIB)

char* nextArg(char* p0)
{
 while (true)
 {
  const char c0 = *p0;
  if (c0 == 0)
   break;
  p0 += 1;
  if (c0 == ',')
  {
   break;
  }
 }
 return p0;
}

int getNum(char **p0, int n)
{
 char *p1 = *p0;
 int val = 0;
 while (n > 0)
 {
  const char c1 = *p1++;
  val *= 10;
  val += c1 - '0';
  n -= 1;
 }
 *p0 = p1;
 return val;
}

void processSerial(const int sock)
{
 while (uart_is_readable(uart1))
 {
  const char c = uart_getc(uart1);
  uart_putc(uart0, c);
  dbg1Set();
  switch (rtk.state)
  {
  case RCV_IDLE:
   if (c == 0xd3)
   {
    // dbg0Set();
    rtk.count = 2;
    rtk.buf[0] = c;
    rtk.crc = crc24qTable[c];
    crcBuf[0] = rtk.crc;
    rtk.fil = 1;
    rtk.t0 = usTime();
    rtk.state = RCV_GET_LEN;
    rtk.startTime = usTime();
   }
   else // if (c == '$')
   {
    rtk.t0 = usTime();
    rtk.state = RCV_TEXT;
    rtk.buf[0] = c;
    rtk.fil = 1;
   }
   break;

  case RCV_GET_LEN:
   rtk.crc = ((rtk.crc << 8) ^ crc24qTable[((rtk.crc >> 16) ^ c) & 0xFFu]) & 0xFFFFFFu;
   crcBuf[rtk.fil] = rtk.crc;
   rtk.len = (rtk.len << 8) + c;
   rtk.buf[rtk.fil] = c;
   rtk.fil += 1;
   rtk.count -= 1;
   if (rtk.count == 0)
   {
    rtk.state = RCV_GET_DATA;
    rtk.len &= 0x3ff;
    // printf("rtkLen %d\n", rtk.len);
#if 0 && defined(DBG_PRT)
    // if ((prt == 0) && (rtk.len == 19))
    if (rtk.len == 19)
    {
     prt = 1;
    }
#endif	/* DBG_PRT */
    rtk.len += 3;
   }
   break;

  case RCV_GET_DATA:
   rtk.crc = ((rtk.crc << 8) ^ crc24qTable[((rtk.crc >> 16) ^ c) & 0xFFu]) & 0xFFFFFFu;
   crcBuf[rtk.fil] = rtk.crc;
   rtk.buf[rtk.fil] = c;
   rtk.fil += 1;
   rtk.len -= 1;
   if (rtk.len == 0)
   {
    rtk.rxAccum += rtk.fil;
#if 1
    const uint32_t msgT = (usTime() - rtk.startTime);
    int type = (rtk.buf[3] << 4) | (rtk.buf[4] >> 4);
    printf("rtkLen %4d type %4d rtkCRC %08x %5d %lu\n",
	   rtk.fil, type, static_cast<unsigned int>(rtk.crc), rtk.rxAccum, msgT);
#endif
    rtk.t0Accum = usTime();
#if defined(RTK_SEND)
    const int err = send(sock, reinterpret_cast<uint8_t*>(rtk.buf), rtk.fil);
    if (err < 0)
     printf("send failed: errno %d\n", err);

#endif	/* RTK_SEND */

#if 0 && defined(DBG_PRT)
    if (prt == 1)
    {
     printHex(rtk.buf, rtk.fil);
     printHex(crcBuf, rtk.fil << 2);
     prt = 0;
    }
#endif	/* DDBG_PRT */
    // dbg0Clr();
    rtk.state = RCV_IDLE;
   }
   break;

  case RCV_TEXT:
   rtk.buf[rtk.fil] = c;
   rtk.fil += 1;
   if (c == '\n')
   {
    rtk.buf[rtk.fil] = 0;
    const int err = send(sock, reinterpret_cast<uint8_t*>(rtk.buf), rtk.fil);
    if (err < 0)
     printf("send failed: errno %d\n", err);
#if 1
    if (rtk.buf[0] == '$')
    {
     /* $GNGGA, 091628.00, 3844.78718183,N, 07755.96337656,W, 7,28,0.5,135.9670,M,-33.6653,M, ,*44 */
     if (strncmp(reinterpret_cast<char*>(rtk.buf), "$GNGGA", 6) == 0)
     {
      char *p = nextArg(reinterpret_cast<char*>(rtk.buf));

      int gpsTime = getNum(&p, 2) * 60;
      gpsTime += getNum(&p, 2);
      gpsTime *= 60;
      gpsTime += getNum(&p, 2);

      p = nextArg(p);
      int tmp = getNum(&p, 2);
      const double lat = static_cast<double>(tmp) + strtod(p, &p) / 60.0;
      p = nextArg(p);
      p = nextArg(p);
      tmp = getNum(&p, 3);
      const double lon = -(static_cast<double>(tmp) + strtod(p, &p) / 60.0);
      printf("gpsTime %6d lat %13.10f lon %14.10f\n", gpsTime, lat, lon);
     }
    }
    rtk.fil = 0;
    rtk.state = RCV_IDLE;
#endif
   }

  }
 }
 dbg1Clr();
}

#endif	/* GPS_LIB */

#endif	/* TCP_CLIENT */

/**
   ----------------------------------------------------------------------------------------------------
   Functions
   ----------------------------------------------------------------------------------------------------
*/

#if defined(USE_DHCP)
/* DHCP */
static void wizchip_dhcp_init() {
 printf(" DHCP client running\n");

 DHCP_init1(SOCKET_DHCP, g_ethernet_buf, HOST_NAME);

 reg_dhcp_cbfunc(wizchip_dhcp_assign, wizchip_dhcp_assign, wizchip_dhcp_conflict);
}

static void wizchip_dhcp_assign() {
 getIPfromDHCP(g_net_info.ip);
 getGWfromDHCP(g_net_info.gw);
 getSNfromDHCP(g_net_info.sn);
 getDNSfromDHCP(g_net_info.dns);

 g_net_info.dhcp = NETINFO_DHCP;

 /* Network initialize */
 network_initialize(g_net_info); // apply from DHCP

 print_network_information(g_net_info);
 printf(" DHCP leased time : %ld seconds\n", getDHCPLeasetime());
}

static void wizchip_dhcp_conflict() {
 printf(" Conflict IP from DHCP\n");

 // halt or reset or any...
 // ReSharper disable once CppDFAEndlessLoop
 while (true)
  ; // this example is halt.
}

/* Timer */
static void repeating_timer_callback() {
 g_msec_cnt++;

 if (g_msec_cnt >= 1000 - 1) {
  g_msec_cnt = 0;

  DHCP_time_handler();
  DNS_time_handler();
 }
}
#endif	/* USE_DHCP */

#if !defined(GPS_LIB)

void printHex(const uint8_t *data, size_t len)
{
 int col = 0;
 for (size_t i = 0; i < len; i++)
 {
  if (col == 0)
  {
   printf("  %04X: ", i);
  }
  printf("%02X ", data[i]);
  col += 1;
  if (col == 16)
  {
   col = 0;
   printf("\n");
  }
 }
 if (col != 0)
  printf("\n");
}

#endif	/* GPS_LIB */
