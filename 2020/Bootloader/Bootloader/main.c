/*
 * Bootloader.c
 *
 * Created: 9/11/2020 4:07:15 PM
 * Author : Connor
 */ 


#include "sam.h"
#include <stdint.h>
#include <stdbool.h>

#define LITTLE_ENDIAN

// CAN config
#define CAN0_TX_PIN PIN_PA24
#define CAN0_TX_MUX MUX_PA24G_CAN0_TX
#define CAN0_RX_PIN PIN_PA25
#define CAN0_RX_MUX MUX_PA25G_CAN0_RX
#ifdef CAN1
#define CAN1_TX_PIN PIN_PB10
#define CAN1_TX_MUX MUX_PB10G_CAN1_TX
#define CAN1_RX_PIN PIN_PB11
#define CAN1_RX_MUX MUX_PB11G_CAN1_RX
#endif

// CAN message IDs for the UDS communication
#define UDS_COMMAND_ID 0x69
#define UDS_RESPONSE_ID 0x70

#define BOOTLOADER_DELAY 1000

// CAN data storage
static CanMramXifde can0_extended_filters[1];
static volatile CanMramRxbe can0_rx_buffers[1];
static volatile CanMramTxbe can0_tx_buffers[1];

static volatile uint32_t milliseconds = 0;

static void init_cpu(void);
static void init_can(void);
static void init_timer(void);

static uint8_t * can_get_response_buffer(void);
static void can_queue_response_buffer(const void * buffer);

static void isotp_send_fc(void);
static void isotp_receive(const uint8_t * data);
static void isotp_transmit(const uint8_t * data, int length);

static void uds_receive(const uint8_t * data, int length, bool final);

int main(void)
{
    /* Initialize the SAM system */
    SystemInit();
	
	init_cpu();
	init_can();
	init_timer();
	
	// call user code
	PM->SLEEPCFG.reg = PM_SLEEPCFG_SLEEPMODE_IDLE0;
	while (1)
	{
		asm("wfi\r\n");
	}
}


static void init_cpu(void)
{
	// increase flash wait states from 0 to 1. if this is not done, the
	// CPU has a seizure trying to run at 48MHz
	NVMCTRL->CTRLB.bit.RWS = 1;
	// up speed to 48MHz
	OSCCTRL->OSC48MDIV.reg = OSCCTRL_OSC48MDIV_DIV(0b0000);
	// wait for synchronization
	while(OSCCTRL->OSC48MSYNCBUSY.reg) ;
	// update CMSIS config for new clock rate
	SystemCoreClock = 48000000;
}

static void init_can(void)
{
	{ // set up CAN supplying clock generator at 8MHz
		GCLK_GENCTRL_Type clock = {
			.bit = {
				.GENEN = 1,
				.SRC = GCLK_GENCTRL_SRC_OSC48M_Val,
				.DIV = 6,
				.DIVSEL = 0,
				.RUNSTDBY = 1,
			}
		};
		GCLK->GENCTRL[8] = clock;
	}
	
	// sync
	while (GCLK->SYNCBUSY.bit.GENCTRL8) {};
	
	{ // set up CAN peripheral clocks
		GCLK_PCHCTRL_Type pclock = {
			.bit = {
				.CHEN = 1,
				.GEN = 8
			}
		};
		GCLK->PCHCTRL[CAN0_GCLK_ID] = pclock;
		#ifdef CAN1
		GCLK->PCHCTRL[CAN1_GCLK_ID] = pclock;
		#endif
	}
	
	// set up MCLK AHB for CAN (synchronized peripheral clock)
	MCLK->AHBMASK.bit.CAN0_ = 1;
	#ifdef CAN1
	MCLK->AHBMASK.bit.CAN1_ = 1;
	#endif
	
	// set up pin mux for CAN pins
	PORT->Group[0].PMUX[CAN0_TX_PIN/2].bit.PMUXE = CAN0_TX_MUX;
	PORT->Group[0].PMUX[CAN0_RX_PIN/2].bit.PMUXO = CAN0_RX_MUX;
	PORT->Group[0].PINCFG[CAN0_TX_PIN].bit.PMUXEN = 1;
	PORT->Group[0].PINCFG[CAN0_RX_PIN].bit.PMUXEN = 1;
	#ifdef CAN1
	PORT->Group[1].PMUX[CAN1_TX_PIN/2].bit.PMUXE = CAN1_TX_MUX;
	PORT->Group[1].PMUX[CAN1_RX_PIN/2].bit.PMUXO = CAN1_RX_MUX;
	PORT->Group[1].PINCFG[CAN1_TX_PIN].bit.PMUXEN = 1;
	PORT->Group[1].PINCFG[CAN1_RX_PIN].bit.PMUXEN = 1;
	#endif
	
	// Enable interrupt
	NVIC->ISER[0] = (1 << 15) | (1 << 16);
		
	// put CAN module into configuration mode
	CAN0->CCCR.bit.INIT = 1;
	while (CAN0->CCCR.bit.INIT != 1) {}; // wait
	CAN0->CCCR.bit.CCE = 1;
	while (CAN0->CCCR.bit.CCE != 1) {}; // wait
		
	// set up TX buffer
	CanMramTxbe uds_response_buf = {
		.TXBE_0 = {
			.bit = {
				.ID = UDS_RESPONSE_ID,
				.RTR = 0,
				.XTD = 1,
			}
		},
		.TXBE_1 = {
			.bit = {
				.DLC = 8,
				.EFC = 0,
			}
		}
	};
	can0_tx_buffers[0].TXBE_0 = uds_response_buf.TXBE_0;
	can0_tx_buffers[0].TXBE_1 = uds_response_buf.TXBE_1;
		

	// set up CAN filter
	CanMramXifde uds_command_fil = {
		.XIDFE_0 = {
			.bit = {
				.EFEC = CAN_XIDFE_0_EFEC_STRXBUF_Val,
				.EFID1 = UDS_COMMAND_ID,
			}
		},
		.XIDFE_1 = {
			.bit = {
				.EFID2 = 0, // store in only buffer
			}
		}
	};
	can0_extended_filters[0].XIDFE_0 = uds_command_fil.XIDFE_0;
	can0_extended_filters[0].XIDFE_1 = uds_command_fil.XIDFE_1;
		
	{ // Standard filters storage
		CAN_SIDFC_Type sidfc = {
			.bit = {
				.FLSSA = 0,
				.LSS = 0,
			}
		};
		CAN0->SIDFC.reg = sidfc.reg;
	}
	{ // Extended filters storage
		CAN_XIDFC_Type xidfc = {
			.bit = {
				.FLESA = (uint32_t) can0_extended_filters,
				.LSE = 1,
			}
		};
		CAN0->XIDFC.reg = xidfc.reg;
	}
	{ // RX FIFO 0 storage
		CAN_RXF0C_Type rxf0c = {
			.bit = {
				.F0SA = 0,
				.F0S = 0,
			}
		};
		CAN0->RXF0C.reg = rxf0c.reg;
	}
	{ // RX FIFO 1 storage
		CAN_RXF1C_Type rxf1c = {
			.bit = {
				.F1SA = 0,
				.F1S = 0,
			}
		};
		CAN0->RXF1C.reg = rxf1c.reg;
	}
	{ // RX buffers storage
		CAN_RXBC_Type rxbc = {
			.bit = {
				.RBSA = (uint32_t) can0_rx_buffers,
			}
		};
		CAN0->RXBC.reg = rxbc.reg;
	}
	{ // RX element size (for FD mode) configuration
		CAN_RXESC_Type rxesc = {
			.bit = {
				.F0DS = CAN_RXESC_F0DS_DATA8_Val,
				.F1DS = CAN_RXESC_F1DS_DATA8_Val,
				.RBDS = CAN_RXESC_RBDS_DATA8_Val,
			}
		};
		CAN0->RXESC.reg = rxesc.reg;
	}
	{ // TX buffers storage
		CAN_TXBC_Type txbc = {
			.bit = {
				.TFQM = 0, /* fifo mode */
				.TFQS = 0,
				.NDTB = 1,
				.TBSA = (uint32_t) can0_tx_buffers,
			}
		};
		CAN0->TXBC.reg = txbc.reg;
	}
	{ // TX event FIFO storage
		CAN_TXEFC_Type txefc = {
			.bit = {
				.EFS = 0,
				.EFSA = 0,
			}
		};
		CAN0->TXEFC.reg = txefc.reg;
	}
	{ // TX element size (for FD mode) configuration
		CAN_TXESC_Type txesc = {
			.bit = {
				.TBDS = CAN_TXESC_TBDS_DATA64_Val,
			}
		};
		CAN0->TXESC.reg = txesc.reg;
	}
	{ // bit rate timing (currently unused)
		// With a GCLK_CAN of 8MHz, the reset value 0x00000A33 configures the CAN for a fast bit rate of 500 kBits/s.
		//CAN_DBTP_Type dbtp;
	}
	{ // nominal bit rate timing and prescaler
		// With a CAN clock (GCLK_CAN) of 8MHz, the reset value 0x06000A03 configures the CAN for a bit rate of 500 kBits/s.
		//CAN_NBTP_Type nbtp;
	}
	{ // timestamping. we should reset TSCV every ms so we know correct microsecond timing or something like that
		CAN_TSCC_Type tscc = {
			.bit = {
				.TCP = 0, /* prescaler = 1 (times the CAN bit time) */
				.TSS = 1, /* enable tscc counter */
			}
		};
		CAN0->TSCC.reg = tscc.reg;
	}
	{ // enable interrupts. probably want to
		CAN_IE_Type ie = {
			.bit = {
				.RF1NE = 1, /* interrupt on new FIFO message */
				.RF0NE = 1, /* interrupt on new FIFO message */
				.DRXE  = 1, /* interrupt on new Rx buffer message */
			}
		};
		CAN0->IE.reg = ie.reg;
	}
	{ // enable interrupt line 0. this is the default for all interrupts unless changed
		CAN_ILE_Type ile = {
			.bit = {
				.EINT0 = 1,
			}
		};
		CAN0->ILE.reg = ile.reg;
	}
	{ // general filter configuration
		CAN_GFC_Type gfc = {
			.bit = {
				.ANFS = CAN_GFC_ANFS_REJECT_Val, /* reject nonmatching standard */
				.ANFE = CAN_GFC_ANFE_REJECT_Val, /* reject nonmatching extended */
				.RRFS = 1, /* reject standard remote frames */
				.RRFE = 1, /* reject extended remote frames */
			}
		};
		CAN0->GFC.reg = gfc.reg;
	}
		
	// drop out of configuration mode and start
	CAN0->CCCR.bit.INIT = 0;
}

static uint8_t * can_get_response_buffer(void)
{
	// TODO: check if buffer has yet to be transmitted, and if so, get a FIFO instead
	return (uint8_t *)can0_tx_buffers[0].TXBE_DATA;
}
static void can_queue_response_buffer(const void * buffer)
{
	// TODO: after implementing the FIFO technique above, find which buffer this is and queue appropriately
	CAN0->TXBAR.reg = 1;
}

static void init_timer(void)
{
	// Call SysTick_Handler every millisecond
	milliseconds = 0;
	SysTick_Config(SystemCoreClock / 1000);
}

enum isotp_state {
	ISOTP_STATE_IDLE,
	ISOTP_STATE_RECEIVING,
	
	ISOTP_STATE_COUNT	
};

enum isotp_type {
	ISOTP_FRAME_TYPE_SINGLE = 0,
	ISOTP_FRAME_TYPE_FIRST = 1,
	ISOTP_FRAME_TYPE_CONSECUTIVE = 2,
	ISOTP_FRAME_TYPE_FC = 3,
};

struct isotp_frame_single {
	struct {
	#ifdef LITTLE_ENDIAN
		uint8_t size:4;
		uint8_t type:4;
	#endif
	};
	uint8_t data[7];
};

struct isotp_frame_first {
	struct {
	#ifdef LITTLE_ENDIAN
		uint16_t size:12;
		uint16_t type:4;
	#endif
	};
	uint8_t data[6];
};

struct isotp_frame_consecutive {
	struct {
	#ifdef LITTLE_ENDIAN
		uint8_t index:4;
		uint8_t type:4;
	#endif
	};
	uint8_t data[7];
};

struct isotp_frame_fc {
#ifdef LITTLE_ENDIAN
	uint8_t st:8;
	uint8_t bsize:8;
	uint8_t flag:4;
	uint8_t type:4;
#endif
};

union isotp_frame {
	uint8_t raw_data[8];
	struct isotp_frame_single single;
	struct isotp_frame_first first;
	struct isotp_frame_consecutive consecutive;
	struct isotp_frame_fc fc;
};


static int isotp_receive_size = 0;
static enum isotp_state isotp_current_state = ISOTP_STATE_IDLE;

static void isotp_send_fc(void)
{
	struct isotp_frame_fc * data = (struct isotp_frame_fc *)can_get_response_buffer();
	data->type = 3;
	data->flag = 0; // clear to send
	data->bsize = 0; // no more FC messages
	data->st = 5; // wait 5 ms between frames, needs to be tuned
	can_queue_response_buffer(data);
}

static void isotp_receive(const uint8_t * data)
{
	int size;
	bool final;
	const union isotp_frame * frame = (const union isotp_frame *)data;
	switch (frame->single.type)
	{
		case ISOTP_FRAME_TYPE_SINGLE:
		{
			isotp_receive_size = 0;
			isotp_current_state = ISOTP_STATE_IDLE;
			uds_receive(frame->single.data, frame->single.size, true);
			break;
		}
		case ISOTP_FRAME_TYPE_FIRST:
		{
			isotp_receive_size = frame->first.size - 6;
			isotp_current_state = ISOTP_STATE_RECEIVING;
			uds_receive(frame->first.data, 6, false);
			isotp_send_fc();
			break;
		}
		case ISOTP_FRAME_TYPE_CONSECUTIVE:
		{
			if (isotp_current_state != ISOTP_STATE_RECEIVING)
			{
				break;
			}
			if (isotp_receive_size > 7)
			{
				// more data to come
				size = 7;
				isotp_receive_size -= 7;
				final = false;
			}
			else
			{
				// we done boys
				size = isotp_receive_size;
				isotp_receive_size = 0;
				final = true;
				isotp_current_state = ISOTP_STATE_IDLE;
			}
			uds_receive(frame->consecutive.data, size, final);
			isotp_send_fc();
			break;
		}
		default:
		{
			isotp_receive_size = 0;
			isotp_current_state = ISOTP_STATE_IDLE;
			break;
		}
	}
}

static void isotp_transmit(const uint8_t * data, int length)
{
	if (length > 0)
	{
		if (length < 8)
		{
			struct isotp_frame_single * frame = (struct isotp_frame_single *)can_get_response_buffer();
			frame->size = length;
			for (int i = 0; i < length; ++i)
			{
				frame->data[i] = data[i];
			}
			can_queue_response_buffer(frame);
		}
		else
		{
			// TODO: implement multi-frame response
			while (1) {}
		}
	}
}

enum uds_state {
	UDS_STATE_IDLE,
	
	UDS_STATE_COUNT
};

enum uds_request_sid {
	UDS_REQ_SID_READ_DATA_BY_ID = 0x22,	
};

enum uds_response_sid {
	UDS_RSP_SID_READ_DATA_BY_ID = 0x62,
};

static enum uds_state uds_current_state = UDS_STATE_IDLE;

static int uds_read_data_by_id(int id)
{
	switch (id)
	{
		case 101:
		{
			return 1;
		}
		default:
		{
			return 0;
		}
	}
}

static void uds_receive(const uint8_t * data, int length, bool final)
{
	if (length < 1) return;
	
	switch (uds_current_state)
	{
		case UDS_STATE_IDLE:
		{
			enum uds_request_sid sid = (enum uds_request_sid) data[0];
			switch (sid)
			{
				case UDS_REQ_SID_READ_DATA_BY_ID:
				{
					if (length < 3) return;
					int id = (data[1] << 8) | (data[2]); // BE per udsoncan pypi
					int result = uds_read_data_by_id(id);
					uint8_t response[4];
					response[0] = UDS_RSP_SID_READ_DATA_BY_ID;
					response[1] = data[1];
					response[2] = data[2];
					response[3] = result;
					isotp_transmit(response, 4);
					break;
				}
				default:
				{
					break;
				}
			}
		}
		default:
		{
			break;
		}
	}
}

void CAN0_Handler()
{	
	if (CAN0->NDAT1.reg & 1)
	{
		isotp_receive((uint8_t *)can0_rx_buffers[0].RXBE_DATA);
		
		CAN0->NDAT1.reg = 1;
	}
	CAN0->IR.reg = 0xFFFFFFFF;
}

#ifdef CAN1
void CAN1_Handler()
{
	CAN1->IR.reg = 0xFFFFFFFF;
}
#endif

void SysTick_Handler()
{
	++milliseconds;
}