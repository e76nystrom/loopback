/**
   Copyright (c) 2021 WIZnet Co.,Ltd

   SPDX-License-Identifier: BSD-3-Clause
*/

//#define STATIC_IP
#define USE_DHCP
#define U8X8

#if defined(TCP_SERVER)
#pragma message("building SERVER")
#define HOST_NAME SERVER_NAME
#endif	/* SERVER */

#if defined(TCP_CLIENT)
#pragma message("building CLIENT " CLIENT_NAME)
#pragma message("server " SERVER_NAME)
#define HOST_NAME CLIENT_NAME
#endif	/* CLIENT */

/**
   ----------------------------------------------------------------------------------------------------
   Includes
   ----------------------------------------------------------------------------------------------------
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
//#include <stdbool.h>

#include "hardware/uart.h"
#include "hardware/structs/uart.h"
#include "pico/stdlib.h"
#include "RP2040.h"
#include "pico/time.h"

#if defined(MULTI_CORE)
#include "pico/multicore.h"
[[noreturn]] void core1_entry();
#endif	/* MULTI_CORE */

#include "port_common.h"
#if defined(U8X8)
#include "oledLib.h"
#endif  /* U8X8 */

extern "C"
{
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
#if defined(U8X8)
#include "hardware/i2c.h"
#endif  /* U8X8 */

#define GPS_LIB
#if defined(GPS_LIB)
#include "gpsLib.h"
#endif  /* GPS_LIB */

#if defined(USE_DHCP)
#include "dhcp.h"
#include "dns.h"
#endif  /* USE_DHCP */

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

#if defined(USE_DHCP)
/* Socket */
#define SOCKET_DHCP 2
#define SOCKET_DNS 3
#endif  /* USE_DHCP */

#define IPV4

#define RETRY_CNT   10000

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

uint16_t tcp_client_destport = PORT;

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
#endif	/* USE_DHCP */

#if defined(U8X8)

#define I2C_PORT i2c1
#define I2C_SDA  26
#define I2C_SCL  27

// ReSharper disable once CppParameterMayBeConstPtrOrRef
extern "C" uint8_t u8x8_byte_pico_hw_i2c(u8x8_t *u8x8V, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
 static uint8_t buffer[32];
 static uint8_t buf_idx;

 switch (msg)
 {
 case U8X8_MSG_BYTE_SEND:
 {
  auto data = static_cast<uint8_t*>(arg_ptr);
  while (arg_int > 0)
  {
   buffer[buf_idx++] = *data++;
   arg_int--;
  }
  break;
 }

 case U8X8_MSG_BYTE_START_TRANSFER:
  buf_idx = 0;
  break;

 case U8X8_MSG_BYTE_END_TRANSFER:
  i2c_write_blocking(I2C_PORT, u8x8_GetI2CAddress(u8x8V) >> 1,
                     buffer, buf_idx, false);
  break;

 case U8X8_MSG_BYTE_INIT:
 case U8X8_MSG_BYTE_SET_DC:
  break;

 default:
  return 0;
 }

 return 1;
}

uint8_t u8x8_gpio_and_delay_pico(u8x8_t *u8x8V, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
 switch (msg)
 {
 case U8X8_MSG_GPIO_AND_DELAY_INIT:
  break; // I2C peripheral already set up in main()
 case U8X8_MSG_DELAY_MILLI:
  sleep_ms(arg_int);
  break;
 case U8X8_MSG_DELAY_10MICRO:
  sleep_us(arg_int * 10);
  break;
 case U8X8_MSG_DELAY_100NANO:
  sleep_us(1);
  break;
 case U8X8_MSG_GPIO_I2C_CLOCK:
 case U8X8_MSG_GPIO_I2C_DATA:
  break; // hardware I2C drives these lines itself
 default:
  return 0;
 }
 return 1;
}

void u8x8Init()
{
 i2c_init(I2C_PORT, 400 * 1000);
 gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
 gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
 gpio_pull_up(I2C_SDA);
 gpio_pull_up(I2C_SCL);

 u8x8_Setup(&u8x8, u8x8_d_sh1106_128x64_noname,
            u8x8_cad_ssd13xx_i2c,
            u8x8_byte_pico_hw_i2c,
            u8x8_gpio_and_delay_pico);

 u8x8_SetI2CAddress(&u8x8, 0x3C << 1); // 7-bit 0x3C shifted, u8x8 uses 8-bit convention

 u8x8_InitDisplay(&u8x8);
 u8x8_SetPowerSave(&u8x8, 0);
 u8x8_ClearDisplay(&u8x8);

 u8x8_SetFont(&u8x8, u8x8_font_chroma48medium8_r);
 u8x8_DrawString(&u8x8, 0, 6, "Hello " HOST_NAME);
 u8x8_DrawString(&u8x8, 0, 7, "0123456789012345");
 printf("U8x8Init done\n");
}

#endif	/* U8X8 */

#define UART1_ID   uart1
#define UART1_TX   8
#define UART1_RX   9
#define UART1_BAUD 115200

static void uart_init_hw()
{
 uart_init(UART1_ID, UART1_BAUD);
 gpio_set_function(UART1_TX, GPIO_FUNC_UART);
 gpio_set_function(UART1_RX, GPIO_FUNC_UART);
}

void printMac(const char *mac_address)
{
 printf("%02x:%02x:%02x:%02x:%02x:%02x",
	mac_address[0], mac_address[1], mac_address[2],
	mac_address[3], mac_address[4], mac_address[5]);
}

void get_mac_from_board_id(uint8_t mac[6])
{
 pico_unique_board_id_t id;
 pico_get_unique_board_id(&id); // 8 bytes, id.id[0..7]

 mac[0] = 0x02; // locally administered, unicast
 mac[1] = id.id[2];
 mac[2] = id.id[3];
 mac[3] = id.id[5];
 mac[4] = id.id[6];
 mac[5] = id.id[7];
}

#if !defined(GPS_LIB)
void processSerial(int sock);
#endif	/* GPS_LIB */

char serialBuf[256];
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
    retval = static_cast<unsigned char>(DNS_run(g_net_info.dns, g_dns_target_domain,
                                                g_dns_target_ip));
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

void wizchip_rst()
{
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

void wizchip_spi_init()
{
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

 // chip select is active-low, so we'll initialize it to a driven-high state
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

 // We set the inbound DMA to transfer from the SPI receive FIFO to a memory buffer paced by the SPI RX FIFO DREQ.
 // We configure the read address to remain unchanged for each element, but the write
 // address to increment (so data is written throughout the buffer.)
 dma_channel_config_rx = dma_channel_get_default_config(dma_rx);
 channel_config_set_transfer_data_size(&dma_channel_config_rx, DMA_SIZE_8);
 channel_config_set_dreq(&dma_channel_config_rx, DREQ_SPI0_RX);
 channel_config_set_read_increment(&dma_channel_config_rx, false);
 channel_config_set_write_increment(&dma_channel_config_rx, true);
#endif
#endif
}

#if defined(U8X8)

#include "hardware/adc.h"

void initReadTemp()
{
 adc_init();
 adc_set_temp_sensor_enabled(true);  // enable the internal sensor
 adc_select_input(4);                // channel 4 = temp sensor
}

float readTemp()
{
 const float raw = static_cast<float>(adc_read()) * (3.3f / 4096.0f);
 const float tempC = 27.0f - (raw - 0.706f) * 581.06f;
 return tempC;
}

void displayTemp()
{
 static uint64_t tmr0;
 if (const auto t0 = timer_time_us_64(timer_hw);
     (t0 - tmr0) > 1000 * 1000)
 {
  tmr0 = t0;

  char buf[20];
  snprintf(buf, sizeof(buf), "    %4.1f %4d ", readTemp(), rtk.rxCount);
  drawString(0, 1, buf);
 }
}

#endif	/* USE_U8X8 */

/**
   ----------------------------------------------------------------------------------------------------
   Main
   ----------------------------------------------------------------------------------------------------
*/

int serverLoop();
void clientLoop();

int main()
{
#if defined(DBG0_PIN)
 dbg0Clr();
 gpio_init(DBG0_PIN);
 gpio_set_dir(DBG0_PIN, GPIO_OUT);
#endif	/* DBG0_PIN */

#if defined(DBG1_PIN)
 dbg1Clr();
 gpio_init(DBG1_PIN);
 gpio_set_dir(DBG1_PIN, GPIO_OUT);
#endif	/* DBG1_PIN */

#if defined(DBG2_PIN)
 dbg2Clr();
 gpio_init(DBG2_PIN);
 gpio_set_dir(DBG2_PIN, GPIO_OUT);
#endif	/* DBG2_PIN */

#if defined(DBG3_PIN)
 dbg3Clr();
 gpio_init(DBG3_PIN);
 gpio_set_dir(DBG3_PIN, GPIO_OUT);
#endif	/* DBG3_PIN */

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

// #if defined(UART_RING)
 uart_init_hw();
 uart_puts(uart1, "uart1 started\n");
// #endif

 buildCRC24qTable();

 wizchip_rst();
 bool is_output = !gpio_get_dir(PIN_RST);
 printf("is_output %d\n", is_output);

 // gpio_init(PIN_RST);
 // gpio_set_dir(PIN_RST, GPIO_OUT);

#if defined(U8X8)
 u8x8Init();
 initReadTemp();
#endif	/* U8X8 */

 wizchip_spi_init();
 wizchip_cris_initialize();

 dbg2Set();
 networkInit(true);
 dbg2Clr();

#if defined(U8X8)
 char tmp[20];
 snprintf(tmp, sizeof(tmp), "%d.%d.%d.%d %c", g_net_info.ip[0], g_net_info.ip[1],
	  g_net_info.ip[2], g_net_info.ip[3], HOST_NAME[0]);
 drawString(0, 0, tmp);
#endif	/* USE_U8X8 */

 is_output = !(gpio_get_dir(PIN_RST));
 printf("is_output %d\n", is_output);

 /*Choose IPv4 / IPv6*/
#if _WIZCHIP_ > W5500
#ifdef IPV6
 set_loopback_mode_W6x00(AS_IPV6);
#endif
#endif

#if defined(MULTI_CORE)
 multicore_launch_core1(core1_entry);
#endif	/* MULTI_CORE */

#if defined(UART1_ISR)

 printf("enable interrupts\n");
 uart_set_irqs_enabled(uart1, true, false); /* sets fifo to 4 bytes */
 hw_write_masked(&uart_get_hw(uart1)->ifls,
                 2 << UART_UARTIFLS_RXIFLSEL_LSB,   // 0b010 = 1/2 full (16 bytes)
                 UART_UARTIFLS_RXIFLSEL_BITS);
 irq_set_enabled(UART1_IRQ, true);

#endif	/* UART1_ISR */

 // ***** SERVER ******

#if defined(TCP_SERVER)

 serverLoop();

#endif	/* TCP_SERVER */

// ***** CLIENT *****

#if defined(TCP_CLIENT)

 clientLoop();

#endif	/* TCP_CLIENT */

}  /* main */

#if defined(MULTI_CORE)

inline void readUart()
{
 rtk.isrCount += 1;

 uint32_t fil = rtk.iFil.load(std::memory_order_relaxed);
 const uint32_t emp = rtk.iEmp.load(std::memory_order_acquire);
 uint32_t free = (RTK_BUF_SIZE - 1) - ((fil - emp) & (RTK_BUF_SIZE - 1));
 while (!(uart1_hw->fr & UART_UARTFR_RXFE_BITS))
 {
  const char c = static_cast<char>(uart1_hw->dr);
  if (free > 0)
  {
   free -= 1;
   rtk.iBuf[fil++] = c;
   fil &= RTK_BUF_SIZE - 1;
   rtk.isrByteCount += 1;
  }
  else
   rtk.isrOverflowCount += 1;
 }
 rtk.iFil.store(fil, std::memory_order_release);
}

inline void writeUart();

bool queueUart(const char c)
{
 uint32_t fil = rtk.tFil.load(std::memory_order_relaxed);
 const uint32_t emp = rtk.tEmp.load(std::memory_order_acquire);
 if (const uint32_t free = (ISR_BUF_SIZE - 1) - (fil - emp) & (ISR_BUF_SIZE - 1);
     free != 0)
 {
  rtk.tBuf[fil++] = c;
  fil &= ISR_BUF_SIZE - 1;
  rtk.tFil.store(fil, std::memory_order_release);
  hw_set_bits(&uart1_hw->imsc, UART_UARTIMSC_TXIM_BITS);
  if (const volatile uint32_t check = uart1_hw->imsc;
      (check & UART_UARTIMSC_TXIM_BITS) == 0)
   printf("check %08x\n", static_cast<unsigned int>(check));
  writeUart();
  return true;
 }
 return false;
}

inline void writeUart()
{
 uint32_t emp = rtk.tEmp.load(std::memory_order_relaxed);
 const uint32_t fil = rtk.tFil.load(std::memory_order_acquire);
 while (emp != fil && uart_is_writable(uart1))
 {
  uart1_hw->dr = rtk.tBuf[emp++];
  emp &= ISR_BUF_SIZE - 1;
 }
 rtk.tEmp.store(emp, std::memory_order_release);
 if (emp == fil)
  hw_clear_bits(&uart1_hw->imsc, UART_UARTIMSC_TXIM_BITS);
}

void core1_entry()
{
 printf("core 1 started core %d\n", get_core_num());
#if 1
 uart_set_irqs_enabled(uart1, true, false);
 irq_set_enabled(UART1_IRQ, true);
 hw_write_masked(&uart_get_hw(uart1)->ifls,
                2 << UART_UARTIFLS_RXIFLSEL_LSB,   // 0b010 = 1/2 full (16 bytes)
                UART_UARTIFLS_RXIFLSEL_BITS);
 while (true)
  __WFI();
 // asm volatile ("wfi");
#endif
}

extern "C" void UART1_IRQ_Handler(void)
{
 const uint32_t status = uart1_hw->mis;
 if (status & (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS))
 {
  uart1_hw->icr = UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS;
  dbg3Set();
  readUart();
  dbg3Clr();
 }

 if (status & UART_UARTMIS_TXMIS_BITS)
 {
  rtk.tIsrCount += 1;
  dbg2Set();
  writeUart();
  dbg2Clr();
  uart1_hw->icr = UART_UARTMIS_TXMIS_BITS;
 }
}

#endif	/* MULTI_CORE */

#if defined(TCP_SERVER)

//#define DHCP_INTERVAL (3600 * 1000000ULL)
#define DHCP_INTERVAL (300 * 1000000ULL)

int serverLoop()
{
 dhcpT0 = timer_time_us_64(timer_hw);

 static uint32_t secTimer;
 static bool phyDown = false;

 setSn_KPALVTR(SOCKET_TCP_SERVER, 30); /* keep alive timer */

 while (true)  // server main loop
 {
  const auto t64 = timer_time_us_64(timer_hw);
  const uint32_t t32 = t64;

#if defined(U8X8)
  static uint64_t tmr0;
  if ((t64 - tmr0) > 1000 * 1000)
  {
   tmr0 = t64;

   displayTemp();
  }
#endif	/* USE_U8X8 */

  pollSerial();

  const uint8_t status = getSn_SR(SOCKET_TCP_SERVER);

  if (status == SOCK_ESTABLISHED)
  {
   rtk.tData = t32;	    /* temporary add keep alive from client */
   processSerial(SOCKET_TCP_SERVER);

#if defined(U8X8)

   if (gpsInfo.update)
   {
    gpsInfo.update = false;
    char buf[20];
    drawString(0, 2, gpsInfo.timeBuf);
    snprintf(buf, sizeof(buf), "%d %2d   ", gpsInfo.fix, gpsInfo.sats);
    drawString(9, 2, buf);  // 9 10 11 12 13 14 15

    snprintf(buf, sizeof(buf), " %13.10f", gpsInfo.lat);
    drawString(0, 3, buf);
    snprintf(buf, sizeof(buf), "%14.10f", gpsInfo.lon);
    drawString(0, 4, buf);
   }

#endif	/* USE_U8X8 */

  }
  else				/* flush data if no connection */
  {
   const uint32_t fil = rtk.iFil.load(std::memory_order_acquire);
   rtk.iEmp.store(fil, std::memory_order_release);
  }

  if ((t64 - dhcpT0) > DHCP_INTERVAL)
  {
   dhcpT0 = t64;
   DHCP_run();
  }

  if (t32 - secTimer > 1000000)
  {
   secTimer = t32;

   if (status == SOCK_ESTABLISHED)
   {
    printf("t %u\n", static_cast<unsigned int>(t32 - rtk.tData));
    if ((t32 - rtk.tData) > (10 * 1000000))
    {
     rtk.tData = t32;
     uint8_t temp;
     if (ctlwizchip(CW_GET_PHYLINK, &temp) == -1)
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

  if (const int retval = loopback_tcps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT);
      retval < 0)
  {
   printf(" loopback_tcps error : %d\n", retval);
   // ReSharper disable once CppDFAEndlessLoop
   while (true)
   {}
  }
 }
 // ReSharper disable once CppDFAUnreachableCode
 printf("exit main\n");
}

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
   processRemData(buf, size);
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

#endif	/* TCP_SERVER */

#if defined(TCP_CLIENT)

void clientLoop()
{
 dhcpT0 = timer_time_us_64(timer_hw);

 lastTime = timer_time_us_64(timer_hw);

 while (true)  // client main loop
 {
  const uint64_t t = timer_time_us_64(timer_hw);

#if defined(U8X8)
  static uint64_t tmr0;
  if ((t - tmr0) > 1000 * 1000)
  {
   tmr0 = t;

   displayTemp();
  }
#endif	/* USE_U8X8 */

  if ((t - dhcpT0) > (3600 * 1000000ULL))
  {
   dhcpT0 = t;
   DHCP_run();
  }
  //const uint32_t t32 = t;
  if ((t - rtk.lan.t) > (100 * 1000))
  {
   rtk.lan.state = RCV_IDLE;
  }

  int retval = loopback_tcpc(SOCKET_TCP_CLIENT, g_tcp_client_buf,
			     g_dns_target_ip, tcp_client_destport);
  if (retval < 0)
  {
   printf(" loopback_tcpc error : %d\n", retval);

   // ReSharper disable once CppDFAEndlessLoop
   while (true);
  }
 }
}

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

  pollSerial();
  processSerial(SOCKET_TCP_CLIENT);

#if defined(U8X8)

  if (gpsInfo.update)
  {
   gpsInfo.update = false;
   char tmp[20];
   drawString(0, 2, gpsInfo.timeBuf);
   snprintf(tmp, sizeof(tmp), "%d %2d   ", gpsInfo.fix, gpsInfo.sats);
   drawString(9, 2, tmp);  // 9 10 11 12 13 14 15

   snprintf(tmp, sizeof(tmp), " %13.10f", gpsInfo.lat);
   drawString(0, 3, tmp);
   snprintf(tmp, sizeof(tmp), "%14.10f", gpsInfo.lon);
   drawString(0, 4, tmp);
  }

#endif	/* USE_U8X8 */

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
   // printf("write %ld bytes\n", ret);
   // uart_write_blocking(UART1_ID, buf, ret);
   rtk.tData = usTime();
   processRemData(buf, size);
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

#endif	/* TCP_CLIENT */

#if defined(USE_DHCP)

static void wizchip_dhcp_init()
{
 printf(" DHCP client running\n");

 DHCP_init1(SOCKET_DHCP, g_ethernet_buf, HOST_NAME);

 reg_dhcp_cbfunc(wizchip_dhcp_assign, wizchip_dhcp_assign, wizchip_dhcp_conflict);
}

static void wizchip_dhcp_assign()
{
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

static void wizchip_dhcp_conflict()
{
 printf(" Conflict IP from DHCP\n");

 // halt or reset or any...
 // ReSharper disable once CppDFAEndlessLoop
 while (true); // this example is halt.
}

/* Timer */
static void repeating_timer_callback()
{
 g_msec_cnt++;

 if (g_msec_cnt >= 1000 - 1)
 {
  g_msec_cnt = 0;

  DHCP_time_handler();
  DNS_time_handler();
 }
}

#endif	/* USE_DHCP */
