/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED && (CFG_TUSB_MCU == OPT_MCU_LPC18XX || \
                                CFG_TUSB_MCU == OPT_MCU_LPC43XX || \
                                CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "device/dcd.h"

#if CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  #include "fsl_device_registers.h"
  #define   DCD_REGS_BASE { (dcd_registers_t*) USB1_BASE, (dcd_registers_t*) USB2_BASE }
  IRQn_Type DCD_IRQn[] =  { USB_OTG1_IRQn, USB_OTG2_IRQn };

#else
  #include "chip.h"
  #define   DCD_REGS_BASE { (dcd_registers_t*) LPC_USB0_BASE, (dcd_registers_t*) LPC_USB1_BASE }
  IRQn_Type DCD_IRQn[] =  { USB0_IRQn, USB1_IRQn };
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// ENDPTCTRL
enum {
  ENDPTCTRL_STALL          = TU_BIT(0),
  ENDPTCTRL_TOGGLE_INHIBIT = TU_BIT(5), ///< used for test only
  ENDPTCTRL_TOGGLE_RESET   = TU_BIT(6),
  ENDPTCTRL_ENABLE         = TU_BIT(7)
};

// USBCMD
enum {
  USBCMD_RUN_STOP         = TU_BIT(0),
  USBCMD_RESET            = TU_BIT(1),
  USBCMD_SETUP_TRIPWIRE   = TU_BIT(13),
  USBCMD_ADD_QTD_TRIPWIRE = TU_BIT(14)  ///< This bit is used as a semaphore to ensure the to proper addition of a new dTD to an active (primed) endpoint’s linked list. This bit is set and cleared by software during the process of adding a new dTD
};
// Interrupt Threshold bit 23:16

// USBSTS, USBINTR
enum {
  INTR_USB         = TU_BIT(0),
  INTR_ERROR       = TU_BIT(1),
  INTR_PORT_CHANGE = TU_BIT(2),
  INTR_RESET       = TU_BIT(6),
  INTR_SOF         = TU_BIT(7),
  INTR_SUSPEND     = TU_BIT(8),
  INTR_NAK         = TU_BIT(16)
};

// PORTSC1
enum {
  PORTSC1_CURRENT_CONNECT_STATUS = TU_BIT(0),
  PORTSC1_FORCE_PORT_RESUME      = TU_BIT(6),
  PORTSC1_SUSPEND                = TU_BIT(7),
  PORTSC1_FORCE_FULL_SPEED       = TU_BIT(24),
};

// OTGSC
enum {
  OTGSC_VBUS_DISCHARGE          = TU_BIT(0),
  OTGSC_VBUS_CHARGE             = TU_BIT(1),
//  OTGSC_HWASSIST_AUTORESET    = TU_BIT(2),
  OTGSC_OTG_TERMINATION         = TU_BIT(3), ///< Must set to 1 when OTG go to device mode
  OTGSC_DATA_PULSING            = TU_BIT(4),
  OTGSC_ID_PULLUP               = TU_BIT(5),
//  OTGSC_HWASSIT_DATA_PULSE    = TU_BIT(6),
//  OTGSC_HWASSIT_BDIS_ACONN    = TU_BIT(7),
  OTGSC_ID                      = TU_BIT(8), ///< 0 = A device, 1 = B Device
  OTGSC_A_VBUS_VALID            = TU_BIT(9),
  OTGSC_A_SESSION_VALID         = TU_BIT(10),
  OTGSC_B_SESSION_VALID         = TU_BIT(11),
  OTGSC_B_SESSION_END           = TU_BIT(12),
  OTGSC_1MS_TOGGLE              = TU_BIT(13),
  OTGSC_DATA_BUS_PULSING_STATUS = TU_BIT(14),
};

// USBMode
enum {
  USBMODE_CM_DEVICE = 2,
  USBMODE_CM_HOST   = 3,

  USBMODE_SLOM = TU_BIT(3),
  USBMODE_SDIS = TU_BIT(4),

  USBMODE_VBUS_POWER_SELCT = TU_BIT(5), // Enable for LPC18XX/43XX in host most only
};

// Device Registers
typedef struct
{
  //------------- ID + HW Parameter Registers-------------//
  __I  uint32_t TU_RESERVED[64];    ///< For iMX RT10xx, but not used by LPC18XX/LPC43XX

  //------------- Capability Registers-------------//
  __I  uint8_t  CAPLENGTH;          ///< Capability Registers Length
  __I  uint8_t  TU_RESERVED[1];
  __I  uint16_t HCIVERSION;         ///< Host Controller Interface Version

  __I  uint32_t HCSPARAMS;          ///< Host Controller Structural Parameters
  __I  uint32_t HCCPARAMS;          ///< Host Controller Capability Parameters
  __I  uint32_t TU_RESERVED[5];

  __I  uint16_t DCIVERSION;         ///< Device Controller Interface Version
  __I  uint8_t  TU_RESERVED[2];

  __I  uint32_t DCCPARAMS;          ///< Device Controller Capability Parameters
  __I  uint32_t TU_RESERVED[6];

  //------------- Operational Registers -------------//
  __IO uint32_t USBCMD;             ///< USB Command Register
  __IO uint32_t USBSTS;             ///< USB Status Register
  __IO uint32_t USBINTR;            ///< Interrupt Enable Register
  __IO uint32_t FRINDEX;            ///< USB Frame Index
  __I  uint32_t TU_RESERVED;
  __IO uint32_t DEVICEADDR;         ///< Device Address
  __IO uint32_t ENDPTLISTADDR;      ///< Endpoint List Address
  __I  uint32_t TU_RESERVED;
  __IO uint32_t BURSTSIZE;          ///< Programmable Burst Size
  __IO uint32_t TXFILLTUNING;       ///< TX FIFO Fill Tuning
       uint32_t TU_RESERVED[4];
  __IO uint32_t ENDPTNAK;           ///< Endpoint NAK
  __IO uint32_t ENDPTNAKEN;         ///< Endpoint NAK Enable
  __I  uint32_t TU_RESERVED;
  __IO uint32_t PORTSC1;            ///< Port Status & Control
  __I  uint32_t TU_RESERVED[7];
  __IO uint32_t OTGSC;              ///< On-The-Go Status & control
  __IO uint32_t USBMODE;            ///< USB Device Mode
  __IO uint32_t ENDPTSETUPSTAT;     ///< Endpoint Setup Status
  __IO uint32_t ENDPTPRIME;         ///< Endpoint Prime
  __IO uint32_t ENDPTFLUSH;         ///< Endpoint Flush
  __I  uint32_t ENDPTSTAT;          ///< Endpoint Status
  __IO uint32_t ENDPTCOMPLETE;      ///< Endpoint Complete
  __IO uint32_t ENDPTCTRL[8];       ///< Endpoint Control 0 - 7
} dcd_registers_t;


// Queue Transfer Descriptor
typedef struct
{
  // Word 0: Next QTD Pointer
  uint32_t next; ///< Next link pointer This field contains the physical memory address of the next dTD to be processed

  // Word 1: qTQ Token
  uint32_t                      : 3  ;
  volatile uint32_t xact_err    : 1  ;
  uint32_t                      : 1  ;
  volatile uint32_t buffer_err  : 1  ;
  volatile uint32_t halted      : 1  ;
  volatile uint32_t active      : 1  ;
  uint32_t                      : 2  ;
  uint32_t iso_mult_override    : 2  ; ///< This field can be used for transmit ISOs to override the MULT field in the dQH. This field must be zero for all packet types that are not transmit-ISO.
  uint32_t                      : 3  ;
  uint32_t int_on_complete      : 1  ;
  volatile uint32_t total_bytes : 15 ;
  uint32_t                      : 0  ;

  // Word 2-6: Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the start of a 4K page
  uint32_t buffer[5]; ///< buffer1 has frame_n for TODO Isochronous

  //------------- DCD Area -------------//
  uint16_t expected_bytes;
  uint8_t reserved[2];
} dcd_qtd_t;

TU_VERIFY_STATIC( sizeof(dcd_qtd_t) == 32, "size is not correct");

// Queue Head
typedef struct
{
  // Word 0: Capabilities and Characteristics
  uint32_t                         : 15 ; ///< Number of packets executed per transaction descriptor 00 - Execute N transactions as demonstrated by the USB variable length protocol where N is computed using Max_packet_length and the Total_bytes field in the dTD. 01 - Execute one transaction 10 - Execute two transactions 11 - Execute three transactions Remark: Non-isochronous endpoints must set MULT = 00. Remark: Isochronous endpoints must set MULT = 01, 10, or 11 as needed.
  uint32_t int_on_setup            : 1  ; ///< Interrupt on setup This bit is used on control type endpoints to indicate if USBINT is set in response to a setup being received.
  uint32_t max_package_size        : 11 ; ///< This directly corresponds to the maximum packet size of the associated endpoint (wMaxPacketSize)
  uint32_t                         : 2  ;
  uint32_t zero_length_termination : 1  ; ///< This bit is used for non-isochronous endpoints to indicate when a zero-length packet is received to terminate transfers in case the total transfer length is “multiple”. 0 - Enable zero-length packet to terminate transfers equal to a multiple of Max_packet_length (default). 1 - Disable zero-length packet on transfers that are equal in length to a multiple Max_packet_length.
  uint32_t iso_mult                : 2  ; ///<
  uint32_t                         : 0  ;

  // Word 1: Current qTD Pointer
	volatile uint32_t qtd_addr;

	// Word 2-9: Transfer Overlay
	volatile dcd_qtd_t qtd_overlay;

	// Word 10-11: Setup request (control OUT only)
	volatile tusb_control_request_t setup_request;

	//--------------------------------------------------------------------+
  /// Due to the fact QHD is 64 bytes aligned but occupies only 48 bytes
	/// thus there are 16 bytes padding free that we can make use of.
  //--------------------------------------------------------------------+
	uint8_t reserved[16];
}  dcd_qhd_t;

TU_VERIFY_STATIC( sizeof(dcd_qhd_t) == 64, "size is not correct");

//--------------------------------------------------------------------+
// Variables
//--------------------------------------------------------------------+

#define QHD_MAX          12
#define QTD_NEXT_INVALID 0x01

typedef struct {
  // Must be at 2K alignment
  dcd_qhd_t qhd[QHD_MAX] TU_ATTR_ALIGNED(64);
  dcd_qtd_t qtd[QHD_MAX] TU_ATTR_ALIGNED(32);
}dcd_data_t;

static dcd_data_t _dcd_data CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(2048);
static dcd_registers_t* DCD_REGS[] = DCD_REGS_BASE;

//--------------------------------------------------------------------+
// CONTROLLER API
//--------------------------------------------------------------------+

/// follows LPC43xx User Manual 23.10.3
static void bus_reset(uint8_t rhport)
{
  dcd_registers_t* dcd_reg = DCD_REGS[rhport];

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  // direction is enabled and the paired endpoint of opposite direction is disabled, then the
  // endpoint type of the unused direction must bechanged from the control type to any other
  // type (e.g. bulk). Leaving an unconfigured endpoint control will cause undefined behavior
  // for the data PID tracking on the active endpoint.

  // USB0 has 5 but USB1 only has 3 non-control endpoints
  for( int i=1; i < (rhport ? 6 : 4); i++)
  {
    dcd_reg->ENDPTCTRL[i] = (TUSB_XFER_BULK << 2) | (TUSB_XFER_BULK << 18);
  }

  //------------- Clear All Registers -------------//
  dcd_reg->ENDPTNAK       = dcd_reg->ENDPTNAK;
  dcd_reg->ENDPTNAKEN     = 0;
  dcd_reg->USBSTS         = dcd_reg->USBSTS;
  dcd_reg->ENDPTSETUPSTAT = dcd_reg->ENDPTSETUPSTAT;
  dcd_reg->ENDPTCOMPLETE  = dcd_reg->ENDPTCOMPLETE;

  while (dcd_reg->ENDPTPRIME);
  dcd_reg->ENDPTFLUSH = 0xFFFFFFFF;
  while (dcd_reg->ENDPTFLUSH);

  // read reset bit in portsc

  //------------- Queue Head & Queue TD -------------//
  tu_memclr(&_dcd_data, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
	_dcd_data.qhd[0].zero_length_termination = _dcd_data.qhd[1].zero_length_termination = 1;
	_dcd_data.qhd[0].max_package_size = _dcd_data.qhd[1].max_package_size = CFG_TUD_ENDPOINT0_SIZE;
	_dcd_data.qhd[0].qtd_overlay.next = _dcd_data.qhd[1].qtd_overlay.next = QTD_NEXT_INVALID;

	_dcd_data.qhd[0].int_on_setup = 1; // OUT only
}

void dcd_init(uint8_t rhport)
{
  tu_memclr(&_dcd_data, sizeof(dcd_data_t));

  dcd_registers_t* const dcd_reg = DCD_REGS[rhport];

  // Reset controller
  dcd_reg->USBCMD |= USBCMD_RESET;
  while( dcd_reg->USBCMD & USBCMD_RESET ) {}

  // Set mode to device, must be set immediately after reset
  dcd_reg->USBMODE = USBMODE_CM_DEVICE;
  dcd_reg->OTGSC = OTGSC_VBUS_DISCHARGE | OTGSC_OTG_TERMINATION;

  // TODO Force fullspeed on non-highspeed port
  // dcd_reg->PORTSC1 = PORTSC1_FORCE_FULL_SPEED;

  dcd_reg->ENDPTLISTADDR = (uint32_t) _dcd_data.qhd; // Endpoint List Address has to be 2K alignment
  dcd_reg->USBSTS  = dcd_reg->USBSTS;
  dcd_reg->USBINTR = INTR_USB | INTR_ERROR | INTR_PORT_CHANGE | INTR_RESET | INTR_SUSPEND | INTR_SOF;

  dcd_reg->USBCMD &= ~0x00FF0000; // Interrupt Threshold Interval = 0
  dcd_reg->USBCMD |= TU_BIT(0); // connect
}

void dcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(DCD_IRQn[rhport]);
}

void dcd_int_disable(uint8_t rhport)
{
  NVIC_DisableIRQ(DCD_IRQn[rhport]);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  // Response with status first before changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);

  DCD_REGS[rhport]->DEVICEADDR = (dev_addr << 25) | TU_BIT(24);
}

void dcd_set_config(uint8_t rhport, uint8_t config_num)
{
  (void) rhport;
  (void) config_num;
  // nothing to do
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
// index to bit position in register
static inline uint8_t ep_idx2bit(uint8_t ep_idx)
{
  return ep_idx/2 + ( (ep_idx%2) ? 16 : 0);
}

static void qtd_init(dcd_qtd_t* p_qtd, void * data_ptr, uint16_t total_bytes)
{
  tu_memclr(p_qtd, sizeof(dcd_qtd_t));

  p_qtd->next        = QTD_NEXT_INVALID;
  p_qtd->active      = 1;
  p_qtd->total_bytes = p_qtd->expected_bytes = total_bytes;

  if (data_ptr != NULL)
  {
    p_qtd->buffer[0]   = (uint32_t) data_ptr;
    for(uint8_t i=1; i<5; i++)
    {
      p_qtd->buffer[i] |= tu_align4k( p_qtd->buffer[i-1] ) + 4096;
    }
  }
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum  = tu_edpt_number(ep_addr);
  uint8_t const dir    = tu_edpt_dir(ep_addr);

  DCD_REGS[rhport]->ENDPTCTRL[epnum] |= ENDPTCTRL_STALL << (dir ? 16 : 0);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum  = tu_edpt_number(ep_addr);
  uint8_t const dir    = tu_edpt_dir(ep_addr);

  // data toggle also need to be reset
  DCD_REGS[rhport]->ENDPTCTRL[epnum] |= ENDPTCTRL_TOGGLE_RESET << ( dir ? 16 : 0 );
  DCD_REGS[rhport]->ENDPTCTRL[epnum] &= ~(ENDPTCTRL_STALL << ( dir  ? 16 : 0));
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  // TODO not support ISO yet
  TU_VERIFY ( p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  uint8_t const epnum  = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const dir    = tu_edpt_dir(p_endpoint_desc->bEndpointAddress);
  uint8_t const ep_idx = 2*epnum + dir;

  // USB0 has 5, USB1 has 3 non-control endpoints
  TU_ASSERT( epnum <= (rhport ? 3 : 5) );

  //------------- Prepare Queue Head -------------//
  dcd_qhd_t * p_qhd = &_dcd_data.qhd[ep_idx];
  tu_memclr(p_qhd, sizeof(dcd_qhd_t));

  p_qhd->zero_length_termination = 1;
  p_qhd->max_package_size        = p_endpoint_desc->wMaxPacketSize.size;
  p_qhd->qtd_overlay.next        = QTD_NEXT_INVALID;

  // Enable EP Control
  DCD_REGS[rhport]->ENDPTCTRL[epnum] |= ((p_endpoint_desc->bmAttributes.xfer << 2) | ENDPTCTRL_ENABLE | ENDPTCTRL_TOGGLE_RESET) << (dir ? 16 : 0);

  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);
  uint8_t const ep_idx = 2*epnum + dir;

  if ( epnum == 0 )
  {
    // follows UM 24.10.8.1.1 Setup packet handling using setup lockout mechanism
    // wait until ENDPTSETUPSTAT before priming data/status in response TODO add time out
    while(DCD_REGS[rhport]->ENDPTSETUPSTAT & TU_BIT(0)) {}
  }

  dcd_qhd_t * p_qhd = &_dcd_data.qhd[ep_idx];
  dcd_qtd_t * p_qtd = &_dcd_data.qtd[ep_idx];

  //------------- Prepare qtd -------------//
  qtd_init(p_qtd, buffer, total_bytes);
  p_qtd->int_on_complete = true;
  p_qhd->qtd_overlay.next = (uint32_t) p_qtd; // link qtd to qhd

  // start transfer
  DCD_REGS[rhport]->ENDPTPRIME = TU_BIT( ep_idx2bit(ep_idx) ) ;

  return true;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+
void dcd_isr(uint8_t rhport)
{
  dcd_registers_t* const dcd_reg = DCD_REGS[rhport];

  uint32_t const int_enable = dcd_reg->USBINTR;
  uint32_t const int_status = dcd_reg->USBSTS & int_enable;
  dcd_reg->USBSTS = int_status; // Acknowledge handled interrupt

  // disabled interrupt sources
  if (int_status == 0) return;

  if (int_status & INTR_RESET)
  {
    bus_reset(rhport);
    dcd_event_bus_signal(rhport, DCD_EVENT_BUS_RESET, true);
  }

  if (int_status & INTR_SUSPEND)
  {
    if (dcd_reg->PORTSC1 & PORTSC1_SUSPEND)
    {
      // Note: Host may delay more than 3 ms before and/or after bus reset before doing enumeration.
      if ((dcd_reg->DEVICEADDR >> 25) & 0x0f)
      {
        dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
      }
    }
  }

  // TODO disconnection does not generate interrupt !!!!!!
//	if (int_status & INTR_PORT_CHANGE)
//	{
//	  if ( !(dcd_reg->PORTSC1 & PORTSC1_CURRENT_CONNECT_STATUS) )
//	  {
//      dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_UNPLUGGED };
//      dcd_event_handler(&event, true);
//	  }
//	}

  if (int_status & INTR_USB)
  {
    uint32_t const edpt_complete = dcd_reg->ENDPTCOMPLETE;
    dcd_reg->ENDPTCOMPLETE = edpt_complete; // acknowledge

    if (dcd_reg->ENDPTSETUPSTAT)
    {
      //------------- Set up Received -------------//
      // 23.10.10.2 Operational model for setup transfers
      dcd_reg->ENDPTSETUPSTAT = dcd_reg->ENDPTSETUPSTAT;// acknowledge

      dcd_event_setup_received(rhport, (uint8_t*) &_dcd_data.qhd[0].setup_request, true);
    }

    if ( edpt_complete )
    {
      for(uint8_t ep_idx = 0; ep_idx < QHD_MAX; ep_idx++)
      {
        if ( tu_bit_test(edpt_complete, ep_idx2bit(ep_idx)) )
        {
          // 23.10.12.3 Failed QTD also get ENDPTCOMPLETE set
          dcd_qtd_t * p_qtd = &_dcd_data.qtd[ep_idx];

          uint8_t result = p_qtd->halted  ? XFER_RESULT_STALLED :
              ( p_qtd->xact_err ||p_qtd->buffer_err ) ? XFER_RESULT_FAILED : XFER_RESULT_SUCCESS;

          uint8_t const ep_addr = (ep_idx/2) | ( (ep_idx & 0x01) ? TUSB_DIR_IN_MASK : 0 );
          dcd_event_xfer_complete(rhport, ep_addr, p_qtd->expected_bytes - p_qtd->total_bytes, result, true); // only number of bytes in the IOC qtd
        }
      }
    }
  }

  if (int_status & INTR_SOF)
  {
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }

  if (int_status & INTR_NAK) {}
  if (int_status & INTR_ERROR) TU_ASSERT(false, );
}

#endif
