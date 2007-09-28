/*

  $Id: ibus_cdc.c,v 1.8 2007/09/28 19:58:20 duke4d Exp $

Release Notes:

Version	Data		Notes

0.99za  01-12-05	- Started code clean up
0.97	23-01-05	- Added recognition of E39 Mid button 'm' to change display mode
					- Changed the default display mode to display all
*/

/*

TODO:	Split code into different files
		Better Message recognition without burden of message matching to discover size
		Different thread for text display on MID, RAD or NAV
		More display options, such as elapsed time/total time, remaining time, etc (investigate id3 tags)
*/


#define LOWDIGIT(x)	(x&0x0F)
#define HIDIGIT(x)	(x&0XF0)

// Current build version
#define CDC_EMU_VERSION	"Ver: 1.22d6"

char EmptyString63[]="                                                               ";

//#define SIM_BUILD
//#define DUMMY_BUILD

#include "system.h"
#include "kernel.h"
#include "string.h"
#include "stdio.h"
#include "button.h"
#include "lcd.h"
#include "powermgmt.h"
#include "thread.h"
#include "stdlib.h"
#include "screens.h"

#include "id3.h"

#include "mpeg.h"
#include "sound.h"
#include "audio.h"
#include "status.h"
#include "mp3_playback.h"
#include "settings.h"
#include "playlist.h"

#include "ibus_help.h"
#include "ibus.h"
#include "ibus_cdc.h"
#include "IniFile.h"

#define CDC_MAGAZINE_CAPACITY	6

// emulation play state
/*
#define EMU_IDLE      0x01
#define EMU_PAUSED    0x02
#define EMU_PLAYING   0x04
#define EMU_FF        0x08
#define EMU_FR        0x10
#define EMU_INTRO	  0x20
#define EMU_FAKE	  0x40
#define EMU_UNPOLLED  0x80
*/

// CD changer playstate reported to the radio
#define IDLE_CODE			0x0C	/* 0000 1100 */
#define PLAY_CODE			0x09	/* 0000 1001 */
#define STOP_CODE			0x02	/* 0000 0010 */
#define UNKN_CODE			0x80
#define RAND_CODE			0x20
#define SCAN_CODE			0x10

typedef struct _cdc_status_msg {
	unsigned char magic;			// This should always be 0x39
	unsigned char msg_type;			// See Reply codes above
	unsigned char playstatus;		
	unsigned char unk1;
	unsigned char magazine;
	unsigned char unk2;
	unsigned char disc;
	unsigned char track;
	unsigned char unk3;
	unsigned char total_tracks;
} cdc_status_msg;

// information owned by the changer emulation
struct
{
	cdc_status_msg status;			// Status field for CD Changer with IBUS ID=0x18
	unsigned char mag_idx;
	int disc;
	int track;
	int offset;
	bool polled;
	bool paused;
	bool firstTime;
//	int playstatus;	// current play state
    long poll_interval; // call the emu each n ticks
	long last_time;	// Time stamp for last generated tick
	long last_scroll; // Time stamp for last scrolling tick
	long scroll_period; // call the scrolling routines with this period
	unsigned char announce_delay;	// Number of seconds between announcing messages to the bus
//	int battery_delay;				// Number of seconds between battery checks
	unsigned char skip_counter;		// Countdown counter to buffer track change requests
	bool first_skip;				// Used to check if it is the first time we are pressing the prev key
	bool phone_installed;
	long intro_delay;
	unsigned char cdc_id;
	bool get_ike_date;
	unsigned int date_delay;
} gEmu;

int idle_time;

struct {
	long total_msgs;
	long total_errors;
	long overlap_errors;
	long overrun_errors;
	long framing_errors;
	long parity_errors;
	long overflow_errors;
	long symbol_errors;
	long ibus_errors;
} cdc_stats;


// ---------------
// IBUS DEVICE IDS
// ---------------
#define IBUS_MAIN1_ID			0x00
#define IBUS_CDC1_ID			0x18
#define IBUS_NAV_ID				0x3B
#define IBUS_MENU_SCR_ID		0x43
#define IBUS_STEERING_WHEEL_ID	0x50
#define IBUS_PDC_ID				0x60	
#define IBUS_RADIO_ID			0x68
#define IBUS_DSP_ID				0x6A
#define IBUS_CDC2_ID			0x76
#define IBUS_IKE_ID				0x80
#define IBUS_LCM_ID				0xBF
#define IBUS_MID_BUTTONS_ID		0xC0
#define IBUS_TELEPHONE_ID		0xC8
#define IBUS_NAV_LOCATION_ID	0xD0
#define IBUS_IRIS_ID			0xE0
#define IBUS_OBC_TEXT_ID		0xE7
#define IBUS_LIGHTS_WIPERS_ID	0xED
#define IBUS_MONITOR_BUTTONS_ID	0xF0
#define IBUS_ALL_ID				0xFF

// Protocol level message templates

// Received message types
#define RX_PKT_POLL				0x0100
#define RX_PKT_STATUS			0x0200
#define RX_PKT_SPECIAL			0x0300
#define RX_PKT_START			0x0400
#define RX_PKT_STOP				0x0500
#define RX_PKT_FWD				0x0600
#define RX_PKT_REW				0x0700
#define RX_PKT_CHANGE_CD		0x0800
#define RX_PKT_INTRO_ON			0x0900
#define RX_PKT_INTRO_OFF		0x0A00
#define RX_PKT_RAND_ON			0x0B00
#define RX_PKT_RAND_OFF			0x0C00
#define RX_PKT_NEXT				0x0D00
#define RX_PKT_PREV				0x0E00
#define RX_PKT_RADIO_ON			0x0F00
#define RX_PKT_RADIO_ANNOUNCE	0x1000

#define RX_PKT_MID_M_PRESS		0x2000
#define RX_PKT_MID_M_RELEASE	0x2100

#define RX_PKT_RT				0x8000
#define RX_PKT_TALK_PUSH		0x8100
#define RX_PKT_TALK_RELEASE		0x8200

#define RX_PKT_POLL_PHONE		0x8300
#define RX_PKT_PHONE_REPLY		0x8400

#define RX_PKT_NAV_REFRESH_INDEX	0x3000
#define RX_PKT_NAV_MENU_SET			0x3100
#define RX_PKT_NAV_MENU_TIMED		0x3200
#define RX_PKT_NAV_REFRESH_INDEX_ACK 0x3400

#define RX_PKT_NAV_MODE_PRESS		0x4000
#define RX_PKT_NAV_MENU_PRESS		0x4100
#define RX_PKT_NAV_FM_PRESS			0x4200
#define RX_PKT_NAV_AM_PRESS			0x4300
#define RX_PKT_NAV_RADIO_MENU_PRESS 0x4400
#define RX_PKT_NAV_REVERSE_PRESS	0x4500

#define RX_PKT_IKE_SET_DATE			0x5000
#define RX_PKT_IKE_SET_TIME			0x5100

#define EMU_PKT_TRACK_CHANGE	0xFF00


static char cdc_stack[DEFAULT_STACK_SIZE+0x1000];
static char id3_stack[DEFAULT_STACK_SIZE];

static char cdc_thread_name[] = "bmw-cdc";
static char id3_thread_name[] = "id3-cdc";

static struct event_queue cdc_queue;
static struct event_queue id3_queue;


// Messages that we may receive
unsigned char ibus_msg_radio_on[]			  = {IBUS_RADIO_ID, 0x04, 0xFF, 0x3B, 0x00};
unsigned char ibus_msg_radio_announce[]		  = {IBUS_RADIO_ID, 0x04, 0xFF, 0x02, 0x01};
unsigned char ibus_msg_poll_req[]			  = {IBUS_RADIO_ID, 0x03, IBUS_CDC1_ID, 0x01};

unsigned char ibus_msg_status_req[]			  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x00, 0x00};
unsigned char ibus_msg_stop_play_req[]		  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x01, 0x00};
unsigned char ibus_msg_special_req[]		  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x02, 0x00};
unsigned char ibus_msg_start_play_req[]		  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x03, 0x00};
unsigned char ibus_msg_fast_scan_back_req[]	  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x04, 0x00};
unsigned char ibus_msg_fast_scan_fwd_req[]    = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x04, 0x01};
unsigned char ibus_msg_next_track_req0[]	  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x05, 0x00};	// USA variant?
unsigned char ibus_msg_prev_track_req0[]	  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x05, 0x01};	// USA variant?
unsigned char ibus_msg_change_cd_req[]		  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x06}; // followed by disc_number
unsigned char ibus_msg_scan_intro_on_req[]    = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x07, 0x01};
unsigned char ibus_msg_scan_intro_off_req[]   = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x07, 0x00};
unsigned char ibus_msg_random_mode_on_req[]   = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x08, 0x01};
unsigned char ibus_msg_random_mode_off_req[]  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x08, 0x00};
unsigned char ibus_msg_next_track_req[]		  = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x0A, 0x00};
unsigned char ibus_msg_prev_track_req[]       = {IBUS_RADIO_ID, 0x05, IBUS_CDC1_ID, 0x38, 0x0A, 0x01};

unsigned char ibus_msg_rt_button1[]			  = {IBUS_STEERING_WHEEL_ID, 0x04, IBUS_ALL_ID, 0x3B, 0x40};	
unsigned char ibus_msg_rt_button2[]			  = {IBUS_STEERING_WHEEL_ID, 0x04, IBUS_ALL_ID, 0x3B, 0x00};	
unsigned char ibus_msg_poll_phone[]			  = {IBUS_STEERING_WHEEL_ID, 0x03, IBUS_TELEPHONE_ID, 0x01};
unsigned char ibus_msg_talk_push_button[]	  = {IBUS_STEERING_WHEEL_ID, 0x04, IBUS_TELEPHONE_ID, 0x3B, 0x80};
unsigned char ibus_msg_talk_release_button[]  = {IBUS_STEERING_WHEEL_ID, 0x04, IBUS_TELEPHONE_ID, 0x3B, 0xA0};

unsigned char ibus_msg_phone_poll_reply[]	  = {IBUS_TELEPHONE_ID, 0x04, IBUS_ALL_ID, 0x02, 0x10};

unsigned char ibus_msg_mid_m_press[]		  = {IBUS_MID_BUTTONS_ID, 0x06, IBUS_RADIO_ID, 0x31, 0x00, 0x00, 0x0E};
unsigned char ibus_msg_mid_m_release[]        = {IBUS_MID_BUTTONS_ID, 0x06, IBUS_RADIO_ID, 0x31, 0x00, 0x00, 0x4E};

unsigned char ibus_msg_nav_refresh_index[]    = {IBUS_RADIO_ID, 0x06, IBUS_NAV_ID, 0xA5, 0x60, 0x01, 0x00};
unsigned char ibus_msg_nav_menu_set[]         = {IBUS_RADIO_ID, 0x04, IBUS_NAV_ID, 0x46, 0x01};
unsigned char ibus_msg_nav_menu_timed[]       = {IBUS_RADIO_ID, 0x04, IBUS_NAV_ID, 0x02};

unsigned char ibus_msg_nav_refresh_index_ack[]= {IBUS_NAV_ID, 05, IBUS_RADIO_ID, 0x22, 0x00, 0x00};

//unsigned char ibus_msg_nav_tone_press[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x04};
//unsigned char ibus_msg_nav_knob_press[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x05};
//unsigned char ibus_msg_rad_knob_press[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x06};
//unsigned char ibus_msg_nav_1_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x11};
//unsigned char ibus_msg_nav_2_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x01};
//unsigned char ibus_msg_nav_3_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x12};
//unsigned char ibus_msg_nav_4_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x02};
//unsigned char ibus_msg_nav_5_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x13};
//unsigned char ibus_msg_nav_6_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x03};
//unsigned char ibus_msg_nav_select_press[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x20};
unsigned char ibus_msg_nav_reverse_press[]	  = {IBUS_MONITOR_BUTTONS_ID, 0X04, IBUS_RADIO_ID, 0x48, 0x14};
unsigned char ibus_msg_nav_am_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x21};
unsigned char ibus_msg_nav_mode_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x23};
unsigned char ibus_msg_nav_radio_menu_press[] = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x30};
unsigned char ibus_msg_nav_fm_press[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x31};
unsigned char ibus_msg_nav_menu_press[]       = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_ALL_ID,   0x48, 0x34};	// ?? why not to radio only?

//unsigned char ibus_msg_nav_tone_long[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x44};
//unsigned char ibus_msg_nav_knob_long[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x45};
//unsigned char ibus_msg_rad_knob_long[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x46};
//unsigned char ibus_msg_nav_1_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x51};
//unsigned char ibus_msg_nav_2_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x42};
//unsigned char ibus_msg_nav_3_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x52};
//unsigned char ibus_msg_nav_4_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x42};
//unsigned char ibus_msg_nav_5_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x53};
//unsigned char ibus_msg_nav_6_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x43};
//unsigned char ibus_msg_nav_select_long[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x60};
unsigned char ibus_msg_nav_reverse_long[]	  = {IBUS_MONITOR_BUTTONS_ID, 0X04, IBUS_RADIO_ID, 0x48, 0x54};
//unsigned char ibus_msg_nav_am_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x61};
//unsigned char ibus_msg_nav_mode_long[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x63};
//unsigned char ibus_msg_nav_radio_menu_long[]= {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x70};
//unsigned char ibus_msg_nav_fm_long[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x71};
//unsigned char ibus_msg_nav_menu_long[]      = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_ALL_ID,   0x48, 0x74};	// ?? why not to radio only?

//unsigned char ibus_msg_nav_tone_rel[]	      = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x84};
//unsigned char ibus_msg_nav_knob_rel[] 	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x85};
//unsigned char ibus_msg_rad_knob_rel[] 	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x86};
//unsigned char ibus_msg_nav_1_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x91};
//unsigned char ibus_msg_nav_2_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x81};
//unsigned char ibus_msg_nav_3_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x92};
//unsigned char ibus_msg_nav_4_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x82};
//unsigned char ibus_msg_nav_5_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x93};
//unsigned char ibus_msg_nav_6_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0x83};
//unsigned char ibus_msg_nav_select_rel[]	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0xA0};
unsigned char ibus_msg_nav_reverse_rel[]	  = {IBUS_MONITOR_BUTTONS_ID, 0X04, IBUS_RADIO_ID, 0x48, 0x94};
//unsigned char ibus_msg_nav_am_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0xA1};
//unsigned char ibus_msg_nav_mode_rel[] 	  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0xA3};
//unsigned char ibus_msg_nav_radio_menu_rel[] = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0xB0};
//unsigned char ibus_msg_nav_fm_rel[]		  = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_RADIO_ID, 0x48, 0xB1};
//unsigned char ibus_msg_nav_menu_rel[]       = {IBUS_MONITOR_BUTTONS_ID, 0x04, IBUS_ALL_ID,   0x48, 0xB4};	// ?? why not to radio only?

// 80 18 E7 23 02 20 20 44 41 54 55 4D 20 03 30 37 2E 30 37 2E 32 30 30 35 04 37 (Set Date)
unsigned char ibus_msg_ike_set_date[]		  = {IBUS_IKE_ID, 0x18, IBUS_OBC_TEXT_ID, 0x23, 0x02, 0x20, 0x20, 0x44, 0x41, 0x54, 0x55, 0x4D, 0x20, 
                                                 0x03, 0x30, 0x37, 0x2E, 0x30, 0x37, 0x2E, 0x32, 0x30, 0x30, 0x35, 0x04};
// 80 0E E7 23 01 20 03 31 32 3A 35 32 20 20 04 52 (Set Time)
unsigned char ibus_msg_ike_set_time[]		  = {IBUS_IKE_ID, 0x0E, IBUS_OBC_TEXT_ID, 0x23, 0x01, 0x20, 0x03, 0x31, 0x32, 0x3A, 0x35, 0x32, 0x20, 0x20, 0x04};
typedef struct {
	unsigned char *message;
	unsigned int size;		// size for message identification (from start)
	unsigned int size2;		// size for message retrieval (from start)
	unsigned int type;
} msg_template;

msg_template gTemplates[] = { 
#ifndef DUMMY_BUILD
{ibus_msg_radio_on,				sizeof(ibus_msg_radio_on),				sizeof(ibus_msg_radio_on),				RX_PKT_RADIO_ON},
{ibus_msg_radio_announce,		sizeof(ibus_msg_radio_announce),		sizeof(ibus_msg_radio_announce),		RX_PKT_RADIO_ANNOUNCE},

{ibus_msg_poll_req,				sizeof(ibus_msg_poll_req),				sizeof(ibus_msg_poll_req),				RX_PKT_POLL},

{ibus_msg_status_req,			sizeof(ibus_msg_status_req),			sizeof(ibus_msg_status_req),			RX_PKT_STATUS},
{ibus_msg_stop_play_req,		sizeof(ibus_msg_stop_play_req),			sizeof(ibus_msg_stop_play_req),			RX_PKT_STOP},
{ibus_msg_special_req,			sizeof(ibus_msg_special_req),			sizeof(ibus_msg_special_req),			RX_PKT_SPECIAL},
{ibus_msg_start_play_req,		sizeof(ibus_msg_start_play_req),		sizeof(ibus_msg_start_play_req),		RX_PKT_START},
{ibus_msg_fast_scan_back_req,	sizeof(ibus_msg_fast_scan_back_req),	sizeof(ibus_msg_fast_scan_back_req),	RX_PKT_REW},
{ibus_msg_fast_scan_fwd_req,	sizeof(ibus_msg_fast_scan_fwd_req),		sizeof(ibus_msg_fast_scan_fwd_req),		RX_PKT_FWD},
{ibus_msg_next_track_req0,		sizeof(ibus_msg_next_track_req0),		sizeof(ibus_msg_next_track_req0),		RX_PKT_NEXT},
{ibus_msg_prev_track_req0,		sizeof(ibus_msg_prev_track_req0),		sizeof(ibus_msg_prev_track_req0),		RX_PKT_PREV},
{ibus_msg_change_cd_req,		sizeof(ibus_msg_change_cd_req),			sizeof(ibus_msg_change_cd_req)+1,		RX_PKT_CHANGE_CD},
{ibus_msg_scan_intro_on_req,	sizeof(ibus_msg_scan_intro_on_req),		sizeof(ibus_msg_scan_intro_on_req),		RX_PKT_INTRO_ON},
{ibus_msg_scan_intro_off_req,	sizeof(ibus_msg_scan_intro_off_req),	sizeof(ibus_msg_scan_intro_off_req),	RX_PKT_INTRO_OFF},
{ibus_msg_random_mode_on_req,	sizeof(ibus_msg_random_mode_on_req),	sizeof(ibus_msg_random_mode_on_req),	RX_PKT_RAND_ON},
{ibus_msg_random_mode_off_req,	sizeof(ibus_msg_random_mode_off_req),	sizeof(ibus_msg_random_mode_off_req),	RX_PKT_RAND_OFF},
{ibus_msg_next_track_req,		sizeof(ibus_msg_next_track_req),		sizeof(ibus_msg_next_track_req),		RX_PKT_NEXT},
{ibus_msg_prev_track_req,		sizeof(ibus_msg_prev_track_req),		sizeof(ibus_msg_prev_track_req),		RX_PKT_PREV},

{ibus_msg_rt_button1,			sizeof(ibus_msg_rt_button1),			sizeof(ibus_msg_rt_button1),			RX_PKT_RT},
//{ibus_msg_rt_button2,			sizeof(ibus_msg_rt_button2),			sizeof(ibus_msg_rt_button2),			RX_PKT_RT},
{ibus_msg_talk_push_button,		sizeof(ibus_msg_talk_push_button),		sizeof(ibus_msg_talk_push_button),		RX_PKT_TALK_PUSH},
{ibus_msg_talk_release_button,	sizeof(ibus_msg_talk_release_button),	sizeof(ibus_msg_talk_release_button),	RX_PKT_TALK_RELEASE},

{ibus_msg_mid_m_press,			sizeof(ibus_msg_mid_m_press),			sizeof(ibus_msg_mid_m_press),			RX_PKT_MID_M_PRESS},
{ibus_msg_mid_m_release,        sizeof(ibus_msg_mid_m_release),			sizeof(ibus_msg_mid_m_release),			RX_PKT_MID_M_RELEASE},

{ibus_msg_poll_phone,			sizeof(ibus_msg_poll_phone),			sizeof(ibus_msg_poll_phone),			RX_PKT_POLL_PHONE},
{ibus_msg_phone_poll_reply,		sizeof(ibus_msg_phone_poll_reply),		sizeof(ibus_msg_phone_poll_reply),		RX_PKT_PHONE_REPLY},

{ibus_msg_nav_refresh_index,	sizeof(ibus_msg_nav_refresh_index),		sizeof(ibus_msg_nav_refresh_index),		RX_PKT_NAV_REFRESH_INDEX},
{ibus_msg_nav_menu_set,			sizeof(ibus_msg_nav_menu_set),			sizeof(ibus_msg_nav_menu_set),			RX_PKT_NAV_MENU_SET},
{ibus_msg_nav_menu_timed,		sizeof(ibus_msg_nav_menu_timed),		sizeof(ibus_msg_nav_menu_timed),		RX_PKT_NAV_MENU_TIMED},
{ibus_msg_nav_refresh_index_ack,sizeof(ibus_msg_nav_refresh_index_ack),	sizeof(ibus_msg_nav_refresh_index_ack),	RX_PKT_NAV_REFRESH_INDEX_ACK},

{ibus_msg_nav_mode_press,		sizeof(ibus_msg_nav_mode_press),		sizeof(ibus_msg_nav_mode_press),		RX_PKT_NAV_MODE_PRESS},
{ibus_msg_nav_menu_press,		sizeof(ibus_msg_nav_menu_press),		sizeof(ibus_msg_nav_menu_press),		RX_PKT_NAV_MENU_PRESS},
{ibus_msg_nav_fm_press,			sizeof(ibus_msg_nav_fm_press),			sizeof(ibus_msg_nav_fm_press),			RX_PKT_NAV_FM_PRESS},
{ibus_msg_nav_am_press,			sizeof(ibus_msg_nav_am_press),			sizeof(ibus_msg_nav_am_press),			RX_PKT_NAV_AM_PRESS},
{ibus_msg_nav_radio_menu_press,	sizeof(ibus_msg_nav_radio_menu_press),	sizeof(ibus_msg_nav_radio_menu_press),	RX_PKT_NAV_RADIO_MENU_PRESS},
{ibus_msg_nav_reverse_press,	sizeof(ibus_msg_nav_reverse_press),		sizeof(ibus_msg_nav_reverse_press),		RX_PKT_NAV_REVERSE_PRESS},
#endif

{ibus_msg_ike_set_date,			5,										sizeof(ibus_msg_ike_set_date),			RX_PKT_IKE_SET_DATE},
{ibus_msg_ike_set_time,			5,										sizeof(ibus_msg_ike_set_time),			RX_PKT_IKE_SET_TIME},

{NULL,							0,										0,										0}
};



// Messages that we send
unsigned char ibus_msg_announce[]             = {0x02,0x01};
unsigned char ibus_msg_poll_response[]        = {0x02,0x00};

// From RADIO/PHONE to IKE:
unsigned char ibus_msg_display_rad[]		  = {0x23,0x42,0x30,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
// From RADIO/PHONE to IKE:
unsigned char ibus_msg_display_obc[]		  = {0x23,0x62,0x30,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
// From RADIO/PHONE to IKE
unsigned char ibus_msg_display_mid[]		  = {0x23,0x41,0x30,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
// From RADIO to NAV
unsigned char ibus_msg_display_nav[]		  = {0x23,0x62,0x30,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
// From PHONE to IRIS
unsigned char ibus_msg_display_iris[]		  = {0x23,0x08,0x30,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};

// From MID to IKE
unsigned char ibus_msg_mid_clock[]			  = {0x20, 0x08, 0xE0, 0xC0};

unsigned char ibus_msg_poll_telephone[]		  = {0x01};
unsigned char ibus_msg_fake_phone_reply[]	  = {0x02,0x10};

// From Radio to NAV
unsigned char ibus_msg_refresh_nav_index[]	  = {0xA5, 0x60, 0x01, 0x00};
unsigned char ibus_msg_display_nav_index[]    = {0x21, 0x60, 0x00, 0x40, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
unsigned char ibus_msg_display_nav_area[]     = {0xA5, 0x62, 0x01, 0x40, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};

unsigned char ibus_msg_myike_set_date[]		  = {0x24, 0x02, 0x00, 0x31, 0x39, 0x2E, 0x31, 0x31, 0x2E, 0x32, 0x30, 0x30, 0x32};
// 3B 05 80 41 02 01 FC 
unsigned char ibus_msg_request_date[]			  = {0x41, 0x02, 0x01};

// Transmited message types
#define TX_PKT_STATUS		0x00
#define TX_PKT_SPECIAL		0x01
#define TX_PKT_TRACK_START	0x02
#define TX_PKT_SCAN_FWD		0x03
#define TX_PKT_SCAN_BACK	0x04
#define TX_PKT_UNK2			0x05
#define TX_PKT_UNK3			0x06
#define TX_PKT_TRACK_SEEK	0x07
#define TX_PKT_CD_SEEK		0x08
#define TX_PKT_CD_CHECK		0x09
#define TX_PKT_MAGAZINE_OUT	0x0A





#define ID3_DISPLAY_ALL		0
#define ID3_DISPLAY_TIME	1
#define ID3_DISPLAY_TITLE	2
#define ID3_DISPLAY_ALBUM	3
#define ID3_DISPLAY_ARTIST	4
#define ID3_DISPLAY_NOTHING	5
#define ID3_NUM_MODES		6	// Needs to be set to 1 + the last mode number in the previous line
#define ID3_REFRESH			100

struct {
	bool scrolling;
	int mode;
	char magazine[20];
	char title[64];
	char artist[64];
	char album[64];
	char text[192];
	unsigned int old_time;
	unsigned int total_time;
	int width;
	int offset;
	int len;
} id3Ctl;

struct {
	bool display;
	bool index;
	bool waitForId3;
	unsigned long index_time;
	unsigned long display_time;
//	unsigned long time_time;
//	int mode_cnt;
} navCtl;




void loadPlaybackCtrl(void)
{
	int fd = open("/BMW/pb.ctr", O_RDONLY);

	if( fd < 0 ) return;

	read(fd, &gEmu.mag_idx, sizeof(int));
	read(fd, &gEmu.disc, sizeof(int));
	read(fd, &gEmu.track, sizeof(int));
	read(fd, &gEmu.offset, sizeof(int));
	read(fd, &id3Ctl.mode, sizeof(int));

	close(fd);
}

void savePlaybackCtrl(void)
{
	int fd = open("/BMW/pb.ctr", O_WRONLY|O_CREAT);

	if(fd < 0) return;

	write(fd, &gEmu.mag_idx, sizeof(int));
	write(fd, &gEmu.disc, sizeof(int));
	write(fd, &gEmu.track, sizeof(int));
	write(fd, &gEmu.offset, sizeof(int));
	write(fd, &id3Ctl.mode, sizeof(int));

	fsync(fd);
	close(fd);
}


int debug_file;
char buf[256];

void WriteToDebug(char *msg, int level)
{
	if(gConf.iDebugLevel >= level) 
		fwrite(debug_file, msg, strlen(msg));
}

void WriteCurrentStatus(void)
{
	char msg[8];

	snprintf(msg, 8, "%02X ", gEmu.status.playstatus);
	WriteToDebug(msg, 2);
}

// converts to bcd representation (for track display)
unsigned char bcd_track(int v)
{
	v = (v+1)%100;
	return (v/10)<<4 | (v%10);
}

unsigned char disc_ascii(unsigned char disc)
{
	if(disc >= 36)
		return disc+13;
	else if(disc>=10)
		return disc+7;
	return disc;
}

unsigned char bcd_disc(int d)
{
	unsigned char ascii = disc_ascii(d & 0xFF);

	unsigned char hi = (ascii / 10) << 4;
	unsigned char lo = ascii % 10;

	return hi | lo;
}


// IBUS Protocol Level message wrappers


void my_ibus_send(unsigned char source, unsigned char dest, unsigned char len, unsigned char *message)
{
	char dbg_msg[128];

	ibus_send_retry(source, dest, len, message);
	if(gConf.iDebugLevel > 0) {
		// TODO: Add code to write message to debug file
		// ...
		dbg_msg[0] = source;
		dbg_msg[1] = len+2;
		dbg_msg[2] = dest;
		memcpy(dbg_msg+3, message, len);
		dbg_msg[len+3] = compute_checksum(dbg_msg, len+3);

		dump_packet(buf, sizeof(buf), dbg_msg, len+4);
		WriteToDebug(buf, 1);

		snprintf(buf, sizeof(buf), " (sent at: %d)", current_tick);
		WriteToDebug(buf, 2);

		WriteToDebug("\n", 1);
	}
}

// This is the point where most messages are sent
void cdc_report_status(unsigned char type)
{
	// Converts track number to special bcd notation
	gEmu.status.track = bcd_track(gEmu.track);
	gEmu.status.disc = bcd_disc(gEmu.disc);

	unsigned char size = sizeof(gEmu.status);
	if(!gConf.bAlternateChanger) size -= 2;

	if(gConf.bAlternateStatus) gEmu.status.playstatus |= UNKN_CODE;

	gEmu.status.msg_type = type;
	my_ibus_send(gEmu.cdc_id, IBUS_RADIO_ID, size, (unsigned char*) &gEmu.status);
}

void cdc_report_special(void)
{
	cdc_report_status(TX_PKT_SPECIAL);
}

void cdc_report_playstatus(void)
{
	unsigned char pkt = TX_PKT_TRACK_START;

	if( LOWDIGIT(gEmu.status.playstatus) == STOP_CODE) pkt = TX_PKT_STATUS;
	else if(LOWDIGIT(gEmu.status.playstatus) == IDLE_CODE) pkt = TX_PKT_STATUS;

	cdc_report_status(pkt);
}

void ibus_display_obc(char *txt)
{
	strncpy(ibus_msg_display_obc+3, txt, MIN(gConf.iDisplayWidth, 20));
	my_ibus_send(IBUS_RADIO_ID, IBUS_IKE_ID, MIN(gConf.iDisplayWidth,(int)strlen(txt))+3, ibus_msg_display_obc);
}

void ibus_display_mid(char *txt)
{
	strncpy(ibus_msg_display_mid+3, txt, MIN(gConf.iDisplayWidth, 20));
	my_ibus_send(IBUS_RADIO_ID, IBUS_MID_BUTTONS_ID, MIN(gConf.iDisplayWidth,(int)strlen(txt))+3, ibus_msg_display_mid);
}

void ibus_display_iris(char *txt)
{
	strncpy(ibus_msg_display_iris+3, txt, MIN(gConf.iDisplayWidth, 11));
	if(txt && txt[0] == ' ')
		ibus_msg_display_iris[3] = gConf.iLeadingChar;
	my_ibus_send(IBUS_TELEPHONE_ID, IBUS_IRIS_ID, MIN(gConf.iDisplayWidth,(int)strlen(txt))+3, ibus_msg_display_iris);
}

void ibus_display_nav(char *txt)
{
	strncpy(ibus_msg_display_nav+3, txt, MIN(gConf.iDisplayWidth, 15));
	my_ibus_send(IBUS_RADIO_ID, IBUS_NAV_ID, MIN(gConf.iDisplayWidth,(int)strlen(txt))+3, ibus_msg_display_nav);
}


void ibus_display_radio(char *txt)
{
	strncpy(ibus_msg_display_rad+3, txt, MIN(gConf.iDisplayWidth, 11));
	if(txt && txt[0] == ' ')
		ibus_msg_display_rad[3] = gConf.iLeadingChar;
	my_ibus_send(IBUS_TELEPHONE_ID, IBUS_IKE_ID, MIN(gConf.iDisplayWidth,(int)strlen(txt))+3, ibus_msg_display_rad);
}

void ibus_display(char *txt)
{
#ifdef SIM_BUILD
	lcd_puts_scroll(0,0,txt);
	return;
#else

	if(gConf.bDisplayRAD)		ibus_display_radio(txt);
	//else if(gConf.bDisplayNAV)	ibus_display_nav(txt);

	if(gConf.bDisplayMID)		ibus_display_mid(txt);
	else if(gConf.bDisplayOBC)	ibus_display_obc(txt);
	else if(gConf.bDisplayIRIS)	ibus_display_iris(txt);

	//lcd_puts(0,1,txt);
#endif
}

void radio_refresh_index_area(void)
{
	my_ibus_send(IBUS_RADIO_ID, IBUS_NAV_ID, sizeof(ibus_msg_refresh_nav_index), ibus_msg_refresh_nav_index);
}

void radio_write_to_area(int area, char *txt)
{
	int i;
	int len = strlen(txt);

	ibus_msg_display_nav_area[3] = (area == 7 ? 0x07 : 0x40 + area);

	// Copy text to buffer without overrun
	strncpy(ibus_msg_display_nav_area+4, txt, 15);

	// Pad with spaces
	for(i=len;i<15;i++)
		ibus_msg_display_nav_area[4+i] = 0x20;

	my_ibus_send(IBUS_RADIO_ID, IBUS_NAV_ID, sizeof(ibus_msg_display_nav_area), ibus_msg_display_nav_area);
}

void radio_write_to_index(int area, char *txt)
{
	int i;
	int len = strlen(txt);

	ibus_msg_display_nav_index[3] = area == 7 ? 0x07 : 0x40 + area;

	strncpy(ibus_msg_display_nav_index+4, txt, 20);

	for(i=len; i<20; i++)
		ibus_msg_display_nav_index[4+i] = 0x20;

	my_ibus_send(IBUS_RADIO_ID, IBUS_NAV_ID, sizeof(ibus_msg_display_nav_index), ibus_msg_display_nav_index);
}

char *stripdirs(char *filename)
{
	char *ptr = filename;

	while(*ptr++);
	--ptr;
	while(ptr > filename && *ptr!='\\' && *ptr != '/') ptr--;
	if(ptr>filename) ptr++;

	return ptr;
}

void id3_set_info(void)
{
	struct mp3entry *id3 = audio_current_track();

	if(!id3) return;

	if(id3->title) strncpy(id3Ctl.title, id3->title, 63);
	else strncpy(id3Ctl.title, stripdirs(id3->path), 63);

	if(id3->artist) strncpy(id3Ctl.artist, id3->artist, 63);
	else strcpy(id3Ctl.artist, EmptyString63);

	if(id3->album) strncpy(id3Ctl.album, id3->album, 63);
	else strcpy(id3Ctl.album, EmptyString63);

	id3Ctl.total_time = id3->length;

	ibus_display("");
	queue_post(&id3_queue, id3Ctl.mode, NULL);
}

/*
 * shows playing mode (RAND, SCAN) in info area of
 * nav display. Also "BimmerBox" is shown there
 */
void nav_display_info(void)
{
//	char txt[32];

	// Update the mode field
	radio_write_to_area(2, "  ");

	if(gEmu.status.playstatus & RAND_CODE)
		radio_write_to_area(3, "RAND");
	else if(gEmu.status.playstatus & SCAN_CODE)
		radio_write_to_area(3, "SCAN");
	else radio_write_to_area(3, "    ");

	radio_write_to_area(4, "  ");

//	snprintf(txt, sizeof(txt), "Song %d/%d", gEmu.track+1, playlist_amount());
//	radio_write_to_area(6, txt);
//	sleep(HZ);
	radio_write_to_area(6, "BimmerBox");

	ibus_display_nav("");
}

/**
 * shows elapsed minutes and seconds in the info area
 * of nav display. Also BimmerBox is show there
 */
void nav_display_time(void)
{
	char txt[32];

	struct mp3entry *id3 = audio_current_track();

	if(!id3) return;

	int elapsed_seconds = id3->elapsed / 1000;
	if(elapsed_seconds == (int) id3Ctl.old_time) return;

	id3Ctl.old_time = elapsed_seconds;

	int elapsed_minutes = elapsed_seconds/60;
	elapsed_seconds = elapsed_seconds % 60;

	snprintf(txt, sizeof(txt), "%3d:%02d", elapsed_minutes, elapsed_seconds);

	radio_write_to_area(5, txt);

	ibus_display_nav("BimmerBox");
}

/**
 * shows the title line, artits, title, album,
 * bit rate and total time in the index area of
 * nav display
 */
void nav_display_index(void)
{
	char txt[32];

	id3_set_info();
	radio_write_to_index(0, gConf.sTitleLine);
	radio_write_to_index(1, id3Ctl.artist);
	radio_write_to_index(2, id3Ctl.title);
	radio_write_to_index(3, id3Ctl.album);
	//radio_write_to_index(4, CDC_EMU_VERSION);
	//radio_write_to_index(4, "You owe me a beer Stefan!!! Ah! Ah! Ah!");
	radio_write_to_index(4, EmptyString63);

	struct mp3entry *id3 = audio_current_track();

	if(!id3) radio_write_to_index(5,"");
	else {
		int totaltime = id3->length/1000;
		snprintf(txt, sizeof(txt), "%3d:%02d %d KBPS", totaltime/60, totaltime%60, id3->bitrate);
		radio_write_to_index(5, txt);
	}

	radio_refresh_index_area();
}

void request_date_from_ike(void)
{
	my_ibus_send(IBUS_NAV_ID, IBUS_IKE_ID, sizeof(ibus_msg_request_date), ibus_msg_request_date);
}

void fake_ike_set_date(char *txt, int size)
{
	// We will need to copy the date to the message first
	strncpy(ibus_msg_myike_set_date+3, txt, size);
	my_ibus_send(IBUS_IKE_ID, IBUS_OBC_TEXT_ID, sizeof(ibus_msg_myike_set_date), ibus_msg_myike_set_date);
}

bool is_hour(char *txt)
{
	return txt[10] == 0x30 && txt[11] == 0x30;
}

void fake_mid_clock_press(void)
{
	my_ibus_send(IBUS_MID_BUTTONS_ID, IBUS_ALL_ID, sizeof(ibus_msg_mid_clock), ibus_msg_mid_clock);
}

void fake_phone_reply(void)
{
	my_ibus_send(IBUS_TELEPHONE_ID, IBUS_ALL_ID, sizeof(ibus_msg_fake_phone_reply), ibus_msg_fake_phone_reply);
}

void poll_telephone(void)
{
	my_ibus_send(IBUS_STEERING_WHEEL_ID, IBUS_TELEPHONE_ID, sizeof(ibus_msg_poll_telephone), ibus_msg_poll_telephone);
}

void cdc_adjust_id(void)
{
	ibus_msg_poll_req[2]			= IBUS_CDC2_ID;
	ibus_msg_status_req[2]			= IBUS_CDC2_ID;
	ibus_msg_stop_play_req[2]		= IBUS_CDC2_ID;
	ibus_msg_special_req[2]			= IBUS_CDC2_ID;
	ibus_msg_start_play_req[2]		= IBUS_CDC2_ID;
	ibus_msg_fast_scan_back_req[2]	= IBUS_CDC2_ID;
	ibus_msg_fast_scan_fwd_req[2]   = IBUS_CDC2_ID;
	ibus_msg_next_track_req0[2]		= IBUS_CDC2_ID;
	ibus_msg_prev_track_req0[2]		= IBUS_CDC2_ID;
	ibus_msg_change_cd_req[2]		= IBUS_CDC2_ID;
	ibus_msg_scan_intro_on_req[2]   = IBUS_CDC2_ID;
	ibus_msg_scan_intro_off_req[2]  = IBUS_CDC2_ID;
	ibus_msg_random_mode_on_req[2]  = IBUS_CDC2_ID;
	ibus_msg_random_mode_off_req[2] = IBUS_CDC2_ID;
	ibus_msg_next_track_req[2]		= IBUS_CDC2_ID;
	ibus_msg_prev_track_req[2]      = IBUS_CDC2_ID;
}


// -----------------------------------
// Sends a CD changer announce message 
// -----------------------------------

void cdc_announce(void)
{
	my_ibus_send(gEmu.cdc_id, IBUS_ALL_ID, sizeof(ibus_msg_announce), ibus_msg_announce);
}

// --------------------------------
// Sends a CD changer poll response 
// --------------------------------

void cdc_poll_response(void)
{
	my_ibus_send(gEmu.cdc_id, IBUS_ALL_ID, 0x02, ibus_msg_poll_response);
}

// ---------------------------------
// Sends "track is starting" message
// ---------------------------------

void cdc_status_track_starting(void)
{
	cdc_report_status(TX_PKT_TRACK_START);
}

// ---------------------------------
// Sends "seeking track" message
// ---------------------------------

void cdc_status_track_seeking(void)
{
	cdc_report_status(TX_PKT_TRACK_SEEK);
}

// ---------------------------------
// Sends "scanning forward" message
// ---------------------------------

void cdc_status_scan_forward(void)
{
	cdc_report_status(TX_PKT_SCAN_FWD);
}

// ---------------------------------
// Sends "scanning backward" message
// ---------------------------------

void cdc_status_scan_backward(void)
{
	cdc_report_status(TX_PKT_SCAN_BACK);
}

// ---------------------------------
// Sends "seeking disc" message
// ---------------------------------

void cdc_status_cd_seeking(void)
{
	int oTrack = gEmu.track;

	gEmu.track = -1;
	cdc_report_status(TX_PKT_CD_SEEK);

	gEmu.track = oTrack;
}


// ---------------------------------
// Sends "CD check" message
// ---------------------------------

unsigned char ibus_status_cd_check(unsigned char status, unsigned char disc, bool exists)
{
	int oTrack = gEmu.track;
	int oDisc = gEmu.disc;

	gEmu.status.magazine = status;
	gEmu.disc = disc;
	gEmu.track = 0;
	cdc_report_status(TX_PKT_CD_CHECK);

	gEmu.track = oTrack;
	gEmu.disc = oDisc;

	return status | (exists << ((disc-1)%CDC_MAGAZINE_CAPACITY));
}





static char disc_id[256];

char *disc_name(int disc)
{
	snprintf(disc_id, sizeof(disc_id), "DISC%d.M3U", disc);
	return disc_id;
}

void emu_process_pollreq(void)
{
	if(LOWDIGIT(gEmu.status.playstatus) == STOP_CODE) {
		gEmu.status.playstatus = HIDIGIT(gEmu.status.playstatus) | IDLE_CODE;
	}
	cdc_poll_response();
}

void emu_jump_to_disc(int disc)
{
	// Let's hope this is ok...
	audio_stop();
	
	gEmu.disc = disc;
	gEmu.track = 0;

	// Report disc seeking
	cdc_status_cd_seeking();

	// Issue the playback
	// ...
	playlist_create("/BMW/", disc_name(disc));

	cdc_status_track_seeking();

	// Don't even bother to check if intro playmode is on, just reset the counter anyway
	gEmu.intro_delay = gConf.iIntroTime;

	// Check if we are currently in random playmode
	if(gEmu.status.playstatus & RAND_CODE)
		playlist_shuffle(current_tick, gEmu.track);

	playlist_start(gEmu.track,0);

	gEmu.status.total_tracks = playlist_amount();

	// Turn on PLAYING, Turn OFF Idle and Paused, Intro will remain as is
	gEmu.status.playstatus = HIDIGIT(gEmu.status.playstatus) | PLAY_CODE;
}

bool hlp_file_exists(char *filename)
{
	int fd;

	fd = fopen(filename, O_RDONLY);
	if(fd >= 0) close(fd);

	return fd >= 0;
}


bool emu_is_disc_ready(int i)
{
	char fullname[32];

	snprintf(fullname, sizeof(fullname), "/BMW/%s", disc_name(i));

	return hlp_file_exists(fullname);
}

/**
 * checks if for given magazine index exists an description
 * file (/BMW/DISC1.txt for first magazine, /BMW/DISC7.txt for second magazine, ...)
 * and displays the content of this file on BMW display
 */
void display_magazine_name(void)
{
	char fullname[32];
  char description[20]="                    ";
  // emptying the previous magazine name
	strncpy(id3Ctl.magazine, description, 20);
	
	// name of magazine description is /BMW/DISC1.txt, DISC7.txt, ...
	unsigned char disc = gEmu.mag_idx*CDC_MAGAZINE_CAPACITY + 1;
	snprintf(fullname, sizeof(fullname), "/BMW/DISC%d.txt", disc);
	
	int fd = open(fullname, O_RDONLY);
	if( fd < 0 ) return;
  // reading 20 characters
  read (fd, description, sizeof(description));		
	close(fd);
	// ibus_display(description);
	strncpy(id3Ctl.magazine, description, 20);
}


// returns the index of the first disc found in the current magazine
unsigned char emu_load_magazine(void)
{
	int i;
	unsigned char status = 1;
	unsigned char ret = 0;

	int oTrack = gEmu.track;
	gEmu.track = 0;

	for(i=1; i<=CDC_MAGAZINE_CAPACITY; i++) {
		unsigned char disc = i+gEmu.mag_idx*CDC_MAGAZINE_CAPACITY;
		bool exists = emu_is_disc_ready(disc);
		if(ret == 0 && exists) ret = disc;
		status |= ibus_status_cd_check(status, disc, exists);
		sleep(HZ/10);
		ibus_status_cd_check(status, disc, exists);
	}

	gEmu.track = oTrack;

	return ret;
}

void emu_switch_magazine(bool jump)
{
	unsigned char first_disc = 0;

	first_disc = emu_load_magazine();

	if(first_disc==0) {
		gEmu.mag_idx = 0;
		first_disc = emu_load_magazine();
	}

	if(jump) emu_jump_to_disc(first_disc);
		
	display_magazine_name();	
}



void nav_init(void)
{
	navCtl.display = false;
	navCtl.index = false;
	navCtl.waitForId3 = false;
	navCtl.index_time = (unsigned long) -1;
	navCtl.display_time = (unsigned long) -1;
}

void id3_init(void)
{
	id3Ctl.scrolling = false;
	strcpy(id3Ctl.magazine, "");
	strcpy(id3Ctl.title, "");
	strcpy(id3Ctl.artist, "");
	strcpy(id3Ctl.album, "");
	strcpy(id3Ctl.text, "");
	id3Ctl.total_time = 0;
	id3Ctl.old_time = 0;
	id3Ctl.width = gConf.iDisplayWidth;
	id3Ctl.mode = ID3_DISPLAY_NOTHING;
}

void id3_disable(void)
{
	id3Ctl.scrolling = false;
	//id3Ctl.mode = ID3_DISPLAY_NOTHING;
	if(gConf.bDisplayRAD || gConf.bDisplayOBC || gConf.bDisplayMID || gConf.bDisplayIRIS)
		ibus_display("");
}

void id3_change_mode(void)
{
	id3_disable();
	id3Ctl.mode = (id3Ctl.mode+1)%ID3_NUM_MODES;
	queue_post(&id3_queue, id3Ctl.mode, NULL);
}

void id3_refresh(void)
{
	queue_post(&id3_queue, ID3_REFRESH, NULL);
}


void id3_prepare(char *header)
{
	if(header) ibus_display(header);

	id3Ctl.len = strlen(id3Ctl.text);
	id3Ctl.offset = -4*gConf.iTicksPerSecond;
	if(id3Ctl.mode != ID3_DISPLAY_NOTHING) id3Ctl.scrolling = true;
	else id3Ctl.scrolling = false;
}

void id3_display_time(void)
{
	struct mp3entry *id3 = audio_current_track();

	if(id3) 
		id3Ctl.old_time = id3->elapsed / 1000;
	id3Ctl.scrolling = true;
}

void id3_display_all(void)
{
	strcpy(id3Ctl.text, id3Ctl.title);

	if(strlen(id3Ctl.album)!=0) {
		strcat(id3Ctl.text, " - ");
		strcat(id3Ctl.text, id3Ctl.album);
	}
	if(strlen(id3Ctl.artist)!=0) {
		strcat(id3Ctl.text, " - ");
		strcat(id3Ctl.text, id3Ctl.artist);
	}

	id3_prepare(NULL);
}

void id3_display_album(void)
{	
	strcpy(id3Ctl.text, id3Ctl.album);
	id3_prepare("Album");
}

void id3_display_artist(void)
{
	strcpy(id3Ctl.text, id3Ctl.artist);
	id3_prepare("Artist");
}

void id3_display_title(void)
{
	strcpy(id3Ctl.text, id3Ctl.title);
	id3_prepare("Title");
}


static void id3_thread(void)
{
	struct event ev;

	while(1) {
		queue_wait(&id3_queue, &ev);

		switch(ev.id) {
			case ID3_REFRESH:
				if(id3Ctl.mode == ID3_DISPLAY_TIME && id3Ctl.scrolling) {
					// check to see if time has passed at least one second
					struct mp3entry *id3 = audio_current_track();

					if(!id3) return;

					int elapsed_seconds = id3->elapsed / 1000;

					if(elapsed_seconds == (int) id3Ctl.old_time) return;

					id3Ctl.old_time = elapsed_seconds;

					int elapsed_minutes = elapsed_seconds/60;
					elapsed_seconds = elapsed_seconds % 60;

					int total_seconds = id3->length / 1000;
					int total_minutes = total_seconds / 60;
					total_seconds = total_seconds % 60;


					snprintf(id3Ctl.text, id3Ctl.width+1, "%02d:%02d-%02d:%02d", elapsed_minutes, elapsed_seconds, total_minutes, total_seconds);
					ibus_display(id3Ctl.text);
				}
				else if(id3Ctl.scrolling && (id3Ctl.mode != ID3_DISPLAY_NOTHING)) {
					if((id3Ctl.offset < 0) && (id3Ctl.offset != -2*gConf.iTicksPerSecond)) {
						// Nothing to do...
						id3Ctl.offset++;
					}
					else if((id3Ctl.offset == -2*gConf.iTicksPerSecond) || 
						     (id3Ctl.offset == id3Ctl.len) ||
						     (id3Ctl.len <= id3Ctl.width)) {
						if(id3Ctl.offset == id3Ctl.len)	{ // We're through!
							id3Ctl.scrolling = false;
							if(id3Ctl.mode == ID3_DISPLAY_ALL)	
							{
								// check to see if time has passed at least one second
								struct mp3entry *id3 = audio_current_track();
			
								if(!id3)
							  {
							  	// no id3 info found. using title.
							  	// TODO: but how can we have id3Ctl.title in this case?
								  strcpy(id3Ctl.text, id3Ctl.title);
								}
								else
							  {
							  	// to get the time display actualizing 
							  	// we set scrolling to true.
							  	id3Ctl.scrolling = true;
							  	
									int elapsed_seconds = id3->elapsed / 1000;				
									if(elapsed_seconds == (int) id3Ctl.old_time) return;
									id3Ctl.old_time = elapsed_seconds;
				
									int elapsed_minutes = elapsed_seconds/60;
									elapsed_seconds = elapsed_seconds % 60;
				
									snprintf(id3Ctl.text, id3Ctl.width+1, "%02d:%02d %s", 
									  elapsed_minutes, elapsed_seconds, id3Ctl.title);
									  
									nav_display_time ();
							  }
							}
						}
						else id3Ctl.offset++;

						ibus_display(id3Ctl.text);
					}
					else {
						ibus_display(id3Ctl.text+MAX(id3Ctl.offset, 0));
						id3Ctl.offset++;
					}
//					snprintf(buf, sizeof(buf), "%d", id3Ctl.offset);
//					lcd_puts(0,0,buf);
				}
				break;
			case ID3_DISPLAY_ALL:
				id3_display_all();
				break;
			case ID3_DISPLAY_TIME:
				// elapsed and length
				id3_display_time();
				break;
			case ID3_DISPLAY_TITLE:
				id3_display_title();
				break;
			case ID3_DISPLAY_ARTIST:
				id3_display_artist();
				break;
			case ID3_DISPLAY_ALBUM:
				id3_display_album();
				break;
			case ID3_DISPLAY_NOTHING:
				ibus_display("");
				break;
		}
	}
}

void setup_index_update(bool force, int delay)
{
	char txt[64];

	if(force) navCtl.index = true;
	navCtl.index_time = current_tick + delay;
	snprintf(txt, sizeof(txt), "Setting the index update to %d\n", navCtl.index_time);
	WriteToDebug(txt, 2);
}

static void cdc_thread(void)
{
	struct event ev;
	unsigned char old_playstatus;
//	char txt[64];

	while(1)
	{
		queue_wait(&cdc_queue, &ev);
		
		old_playstatus = gEmu.status.playstatus;

		switch(ev.id) {
		case RX_PKT_RADIO_ANNOUNCE:
		case RX_PKT_RADIO_ON:
			cdc_announce();
			lcd_puts_scroll(0,1,"RADIO: ON/ANNOUNCE");
			break;

		case RX_PKT_POLL:
			emu_process_pollreq();
			lcd_puts_scroll(0,1,"RADIO: POLL CDC");
			break;

		case RX_PKT_STATUS:
WriteToDebug("STATUS: ", 2);
WriteCurrentStatus();
WriteToDebug("\n", 2);
			if(LOWDIGIT(gEmu.status.playstatus) == IDLE_CODE)
				cdc_report_special();
			else
				cdc_report_playstatus();
			lcd_puts_scroll(0,1,"RADIO: STATUS RQST");
WriteCurrentStatus();
WriteToDebug("\n", 2);
			break;

		case RX_PKT_SPECIAL:
WriteToDebug("SPECIAL: ", 2);
WriteCurrentStatus();
WriteToDebug("\n", 2);
			gEmu.status.playstatus = HIDIGIT(gEmu.status.playstatus) | IDLE_CODE;
			cdc_report_playstatus();
			cdc_report_playstatus();
			lcd_puts_scroll(0,1,"RADIO: CDC MODE");
WriteCurrentStatus();
WriteToDebug("\n", 2);
			break;

		case RX_PKT_STOP:
//			if(LOWDIGIT(gEmu.status.playstatus) == PLAY_CODE) {
				
				// We need to stop the mpeg...
				sound_set(SOUND_VOLUME, 0);
				audio_pause();
				sound_set(SOUND_VOLUME, global_settings.volume);

				struct mp3entry *id3 = audio_current_track();
				if(id3) {
					gEmu.offset = id3->offset;
				}

				if(gEmu.status.playstatus & RAND_CODE) {
					playlist_sort(NULL, true);
					playlist_get_resume_info(&gEmu.track);
					gEmu.status.playstatus &= ~RAND_CODE;
					gEmu.status.playstatus &= ~SCAN_CODE;
				}

				savePlaybackCtrl();

				id3_disable();
				lcd_puts_scroll(0,1,"RADIO: STOP");
//			}

			// Turn off PLAY status bit
			// Turn on STOP status bit
			gEmu.status.playstatus = HIDIGIT(gEmu.status.playstatus) | STOP_CODE;
			cdc_report_playstatus();
			nav_init();
			break;


		case RX_PKT_START: 
			{
				gEmu.status.playstatus = HIDIGIT(gEmu.status.playstatus) | PLAY_CODE;

				// It is better to put this here as it may be too late at the end
				cdc_report_playstatus();

//				if(LOWDIGIT(old_playstatus) == STOP_CODE || LOWDIGIT(old_playstatus) == IDLE_CODE) {

					//ibus_display("BimmerBox");
					//sleep(HZ/2);
					//ibus_display(CDC_EMU_VERSION);
					//sleep(HZ/2);
					//ibus_display("(c) F.Birra");
					//sleep(HZ/2);


					sound_set(SOUND_VOLUME, global_settings.volume);
					if(gEmu.firstTime) {
						playlist_create("/BMW/", disc_name(gEmu.disc));
						playlist_start(gEmu.track,gEmu.offset);
						gEmu.firstTime = false;
					}
					else audio_resume();
					
					gEmu.status.total_tracks = playlist_amount();
//				}
			}
			lcd_puts_scroll(0,1,"RADIO: START");
					
			break;

		case RX_PKT_FWD:

			if(gConf.bMuteOnSkip) {
				sound_set(SOUND_VOLUME, 0);
				audio_pause();
			}

			id3_disable();

			if(gConf.bSkipMagazine) {
				gEmu.mag_idx++;
				emu_switch_magazine(true);
			}
			else {
				gEmu.track+=10;
				if(gEmu.track > playlist_amount()) gEmu.track = 0;
				gEmu.skip_counter = gConf.iSkipDelay;
				cdc_status_track_seeking();
			}
			lcd_puts_scroll(0,1,"RADIO: FWD");
			break;

		case RX_PKT_REW:

			if(gConf.bMuteOnSkip) {
				sound_set(SOUND_VOLUME, 0);
				audio_pause();
			}

			id3_disable();
			if(gConf.bSkipMagazine) {
				gEmu.mag_idx = MAX(gEmu.mag_idx-1,0);
				emu_switch_magazine(true);
			}
			else {
				gEmu.track-=10;
				if(gEmu.track <= 0) gEmu.track = playlist_amount()-1;
				gEmu.skip_counter = gConf.iSkipDelay;
				cdc_status_track_seeking();
			}
			lcd_puts_scroll(0,1,"RADIO: REW");
			break;

		case RX_PKT_CHANGE_CD:

			id3_disable();

			// Get the new disc and make sure it is in the range 1..CDC_MAGAZINE_CAPACITY
			gEmu.disc = ((int) ev.data-1) % CDC_MAGAZINE_CAPACITY +1;

			// Add the offset caused by the selected virtual magazine
			emu_jump_to_disc(gEmu.disc + gEmu.mag_idx*CDC_MAGAZINE_CAPACITY);
			lcd_puts_scroll(0,1,"RADIO: CD CHANGE");

			break;

		case RX_PKT_INTRO_ON:
			id3_disable();
			gEmu.status.playstatus |= SCAN_CODE;
			gEmu.status.playstatus &= ~RAND_CODE; // This shouldn't be needed as the radio sends the appropriate message

			gEmu.intro_delay = gConf.iIntroTime;

			cdc_status_track_seeking();
			cdc_status_track_starting();
			lcd_puts_scroll(0,1,"RADIO: INTRO ON");
			break;

		case RX_PKT_INTRO_OFF:
			id3_disable();
			gEmu.status.playstatus &= ~SCAN_CODE;

			cdc_report_playstatus();

			if((old_playstatus & SCAN_CODE) && 
				(gEmu.intro_delay >= gConf.iIntroTime-gConf.iDoubleClickTime)) {
				// This was quick enough.
				
			  if(gConf.bSkipMagazine) {
			  	// in SkipMagazine mode we are
			  	// using this key for changing id3 mode
			    id3_change_mode();
			  }				
			  else
			  {
			  	// in non SkipMagazine mode this double
			  	// click means selection of next magazine
  				gEmu.mag_idx++;
	  			emu_switch_magazine(true);
	  	  }
			}
			gEmu.intro_delay = gConf.iIntroTime;
			lcd_puts_scroll(0,1,"RADIO: INTRO OFF");
			break;

		case RX_PKT_RAND_ON:
			gEmu.status.playstatus |= RAND_CODE;
			gEmu.status.playstatus &= ~SCAN_CODE;

			cdc_report_playstatus();
			cdc_status_cd_seeking();

			if(!(old_playstatus & RAND_CODE)) {
				id3_disable();
				playlist_shuffle(current_tick, gEmu.track);
				gEmu.track = 0;
				playlist_start(gEmu.track,0);
			}

			cdc_status_track_seeking();
			cdc_status_track_starting();
			lcd_puts_scroll(0,1,"RADIO: RAND ON");
			break;

		case RX_PKT_RAND_OFF: 
			gEmu.status.playstatus &= ~RAND_CODE;
			cdc_report_playstatus();

			if( old_playstatus & RAND_CODE ) {
				id3_disable();
				playlist_sort(NULL, true);
				playlist_get_resume_info(&gEmu.track);
			}
			// This is used to update the song number on the display
			cdc_status_track_starting();
			lcd_puts_scroll(0,1,"RADIO: RAND OFF");
			break;

			
			
			//sound_set(SOUND_VOLUME, 0);
			//audio_pause();
			//sound_set(SOUND_VOLUME, global_settings.volume);

		case RX_PKT_NEXT:
			// We need to know if there is another track
			if(gConf.bMuteOnSkip) {
				sound_set(SOUND_VOLUME, 0);
				audio_pause();
			}

			id3_disable();
			gEmu.track++;
			if(gEmu.track >= playlist_amount()) gEmu.track = 0;
			cdc_status_track_seeking();
			gEmu.skip_counter = gConf.iSkipDelay;
			gEmu.intro_delay = gConf.iIntroTime;
			if(gConf.bMuteHack) cdc_status_track_starting();
			//setup_index_update(false, HZ);
			lcd_puts_scroll(0,1,"RADIO: NEXT");
			break;

		case RX_PKT_PREV:
			if(gConf.bMuteOnSkip) {
				audio_pause();
			}

			// We first check to see if we should go to the start of this track instead
			//struct mp3entry *id3 = audio_current_track();


			// We need to know if we aren't at track 1 already
			id3_disable();
			if(/*id3 && id3->elapsed >= 5000 &&*/ !gEmu.first_skip) {
				gEmu.first_skip = true;
			}// Nothing to be done...
			else {					
				if(gEmu.track>0) gEmu.track--;
				else gEmu.track = playlist_amount()-1;
			}

			gEmu.skip_counter = gConf.iSkipDelay;
			gEmu.intro_delay = gConf.iIntroTime;
			//setup_index_update(false, HZ);

			cdc_status_track_seeking();
			if(gConf.bMuteHack) cdc_status_track_starting();
			lcd_puts_scroll(0,1,"RADIO: PREV");
			break;

		case RX_PKT_POLL_PHONE:
			if(gConf.bUsePhoneButtons && !gEmu.phone_installed)
				fake_phone_reply();
			break;

		case RX_PKT_MID_M_RELEASE:
			id3_change_mode();
			break;

		case RX_PKT_RT:
			if(gConf.bUsePhoneButtons && !gEmu.phone_installed) {
				// Toggle between track display and name display
				id3_change_mode();
			}
			break;

		case RX_PKT_TALK_PUSH:
			if(gConf.bUsePhoneButtons && !gEmu.phone_installed) {
				id3_disable();
				gEmu.mag_idx++;
				emu_switch_magazine(true);
			}
			break;


		case RX_PKT_PHONE_REPLY:
			gEmu.phone_installed = true;
			break;

		case RX_PKT_NAV_REVERSE_PRESS:
			if(LOWDIGIT(gEmu.status.playstatus) == PLAY_CODE) {
				if(gEmu.paused)	{
					audio_resume();
					gEmu.paused = false;
				}
				else {
					audio_pause();
					gEmu.paused = true;
				}
			}
			break;

		case RX_PKT_NAV_REFRESH_INDEX_ACK:
			if(LOWDIGIT(gEmu.status.playstatus) == IDLE_CODE) cdc_report_special();
			break;

		case RX_PKT_NAV_REFRESH_INDEX:
			// If there was some previous message regarding the CD menu,
			// the navCtl.index will be true and we need to set the time
			if(navCtl.index) {
				if(navCtl.index_time == (unsigned long) -1) 
					setup_index_update(false, HZ/5);
			}
			else {
			// otherwise we clear the navCtl.index flag and wait until we get another chance
				// navCtl.index = false;
			}

			break;

		case RX_PKT_NAV_RADIO_MENU_PRESS:	// Also know as the DISPLAY button on some cars?
		case RX_PKT_NAV_MENU_PRESS:
		case RX_PKT_NAV_MODE_PRESS:
		case RX_PKT_NAV_MENU_SET:
		case RX_PKT_NAV_MENU_TIMED:
		case RX_PKT_NAV_FM_PRESS:
		case RX_PKT_NAV_AM_PRESS:
			navCtl.display = false;
			navCtl.index = false;
			navCtl.display_time = (unsigned long) -1;
			navCtl.index_time = (unsigned long) -1;
			break;


		case EMU_PKT_TRACK_CHANGE:
			playlist_get_resume_info(&gEmu.track);
			cdc_status_track_starting();
			id3_set_info();
			if(navCtl.waitForId3) {
				setup_index_update(true, 0);
				//navCtl.waitForId3 = false;
			}
			lcd_puts_scroll(0,1,"Started a new track");

			break;


		default:
			break;
		}	
	}
}



bool match_message(unsigned char *msg, unsigned char *temp, unsigned char m_len, unsigned char t_len)
{
	// A message matches a template (without CRC) if its size is at least equal to the template and
	// all its bytes are identical
	return ( (m_len >= t_len) && (memcmp(msg, temp, t_len)==0));
}

/* This function returns a pair of values. In the high byte it returns the size of the message if recognized.
   In the lower byte it returns the message type.
   0 Means no message was recognized
*/

unsigned int message_type(unsigned char *buffer, unsigned char buffer_size)
{
	unsigned char size = 0;
	unsigned int type = 0;
	int i;

	i = 0;
	while( gTemplates[i].message != NULL ) {
//	for(i=0; i<TOTAL_MSG_TEMPLATES; i++) {
		if(match_message(buffer, gTemplates[i].message, buffer_size, gTemplates[i].size)) {
			type = gTemplates[i].type;
			size = gTemplates[i].size2;

			break;
		}
		else i++;
	}

	// If there is a recognized message increase the size to accomodate the CRC field
	if(type != 0) size++;

	return type | size;
}




void stats_init(void)
{
	cdc_stats.total_msgs = 0;
	cdc_stats.total_errors = 0;
	cdc_stats.overlap_errors = 0;
	cdc_stats.overrun_errors = 0;
	cdc_stats.framing_errors = 0;
	cdc_stats.parity_errors = 0;
	cdc_stats.overflow_errors = 0;
	cdc_stats.symbol_errors = 0;
	cdc_stats.ibus_errors = 0;
}

// Emulator initializer
void emu_init(void)
{
	gEmu.status.magic = 0x39;
	gEmu.status.msg_type = 0xFF;
	gEmu.status.playstatus = STOP_CODE;
	gEmu.status.unk1 = 0x00;
	gEmu.status.magazine = 0x00;
	gEmu.status.unk2 = 0x00;
	gEmu.status.unk3 = 0x00;

	gEmu.track = 0;
	gEmu.disc = 1;
	gEmu.offset = 0;
	gEmu.mag_idx = 0;
	gEmu.polled = false;
	gEmu.paused = false;
	gEmu.firstTime = true;

	gEmu.poll_interval = HZ/gConf.iTicksPerSecond;
	gEmu.last_time = current_tick;

	gEmu.scroll_period = HZ/gConf.iScrollSpeed;
	gEmu.last_scroll = current_tick;

	gEmu.announce_delay = gConf.iAnnouncePeriod;

	gEmu.skip_counter = 0;
	gEmu.first_skip = false;
	gEmu.phone_installed = false;

	if(gConf.bAlternateChanger) gEmu.cdc_id = IBUS_CDC2_ID;
	else						gEmu.cdc_id = IBUS_CDC1_ID;
	
	gEmu.get_ike_date = true;
	gEmu.date_delay = 10;

	idle_time = 0;

	stats_init();
	id3_init();
	nav_init();

	create_thread(cdc_thread, cdc_stack, sizeof(cdc_stack), cdc_thread_name);
	create_thread(id3_thread, id3_stack, sizeof(id3_stack), id3_thread_name);
}

void emu_fast_tick(void)
{
	static int sec_counter = 1;

	//mid_update();
	id3_refresh();

	if(--sec_counter == 0) {
		emu_tick();
		sec_counter = gConf.iTicksPerSecond;
	}

	// Finally, check if the current playing track has changed and report it to the radio
	if((LOWDIGIT(gEmu.status.playstatus) == PLAY_CODE) && audio_has_changed_track()) {
		// We need to get the information from the playlist

		// An alternative is to make everything inside the cdc_thread
		queue_post(&cdc_queue, EMU_PKT_TRACK_CHANGE, NULL);
	}
}



// This function gets called every second or so for housekeeping tasks
void emu_tick(void)
{
	char ttt[32];

	if(idle_time < gConf.iIbusIdleTime){
		snprintf(ttt,32,"idle %02d", gConf.iIbusIdleTime-idle_time);
		lcd_puts_scroll(0,1,ttt);
		idle_time++;
	}
	else if(idle_time == gConf.iIbusIdleTime) {
		sys_poweroff();
		// Perform system shutdown...
	}
	

  if(gConf.iAnnouncePeriod){
	  // Check if it is time to send an announce message
	  if(gEmu.announce_delay--==0) {
		  // if(gEmu.playstatus != EMU_PLAYING) {
			  cdc_announce();
		  // }
		  gEmu.announce_delay = gConf.iAnnouncePeriod;
	  }
	}


	// If we are in INTRO mode then we need to check if it is time to skip to the next song
	if(gEmu.status.playstatus & SCAN_CODE) {
		if(gEmu.intro_delay-- == 0) {
			// Time to skip to the next song

			playlist_get_resume_info(&gEmu.track);
			gEmu.track++;

			if(gEmu.track > playlist_amount() && playlist_amount() != 0) gEmu.track=0;

			cdc_status_track_seeking();
			audio_next();

			// Reset the intro counter
			gEmu.intro_delay = gConf.iIntroTime;
		}
	}


	// Check if it is time to process track < and > buffered requests
	switch(gEmu.skip_counter) {
		case 0:
			break;	// Do nothing, counter is not counting
		case 1:
			// Alright, it's time to skip forward/back			
			if(gConf.bMuteOnSkip) sound_set(SOUND_VOLUME, global_settings.volume);

			playlist_start(gEmu.track,0);
			// Reset the counter so that we are not counting down anymore
			gEmu.skip_counter = 0;
			gEmu.first_skip = false;
			break;

		default:
			gEmu.skip_counter--;

	}

	if(gConf.bLowCluster && gConf.bDisplayNAV) {

		if(gEmu.get_ike_date) { // The counter should be counting...

			gEmu.date_delay--;

			if(gEmu.date_delay == 1) {
				fake_mid_clock_press();
			}	
			if(gEmu.date_delay == 0) {
				// This should be the second request (the actual one since the first is for the time)
				fake_mid_clock_press();
				gEmu.date_delay = 10;
			}
		}
	}
}



// feed a radio command into the emulator
void emu_process_packet(unsigned char* mbus_msg, unsigned char msg_size, unsigned int packet)
{
//	unsigned char rcpt;

	// Check validity of received package
	if( !ibus_check_recv_message(mbus_msg, msg_size)) {

		cdc_stats.ibus_errors++;

		if(gConf.iDebugLevel > 1) {
			dump_packet(buf, sizeof(buf), mbus_msg, msg_size);
			WriteToDebug(buf, 2);
			WriteToDebug(" (CRC)\n", 2);
		}
		return;
	}


	// Check to see if the message is for our self
//	rcpt = ibus_recipient(mbus_msg, msg_size);
//	if( rcpt != IBUS_CDC_ID && rcpt != IBUS_ALL_ID && rcpt != IBUS_TELEPHONE_ID)
//		return;	


	void *param = NULL;
	
	switch(packet) {
		case RX_PKT_IKE_SET_DATE:
			fake_ike_set_date(mbus_msg+14, 10);
			gEmu.get_ike_date = false;
			return;
		case RX_PKT_IKE_SET_TIME:
			if(is_hour(mbus_msg)) {
				gEmu.get_ike_date = true;
				gEmu.date_delay=10;
			}
			return;
		case RX_PKT_CHANGE_CD:
			param = (void*) (int) mbus_msg[5];
			break;
		case RX_PKT_STATUS:
			if(LOWDIGIT(gEmu.status.playstatus) == IDLE_CODE)
				cdc_report_special();
			else
				cdc_report_playstatus();
			return;
		case RX_PKT_POLL:
			emu_process_pollreq();
			return;
		case RX_PKT_NEXT:
		case RX_PKT_PREV:
		case RX_PKT_FWD:
		case RX_PKT_REW:
		case RX_PKT_INTRO_ON:
		case RX_PKT_INTRO_OFF:
			//mid_disable(true);
			id3_disable();
			break;
	}

	queue_post(&cdc_queue, packet, param);

}


//68 12 3B 23 62 10 43 44 43 20 
bool is_cdc_nav_message(char *ptr, int len)
{
	if(len < 9) return false;

	return ptr[0] == IBUS_RADIO_ID &&
		   ptr[2] == IBUS_NAV_ID &&
//		   ptr[3] == 0x23 &&
//		   ptr[4] == 0x62 &&
		   ptr[6] == 'C' &&
		   ptr[7] == 'D' &&
		   ptr[8] == 'C' /*&&
		   ptr[9] == ' '*/;
}


bool is_cd_nav_index_message(char *ptr, int len)
{
	if(len < 14) return false;

	return ptr[0] == IBUS_RADIO_ID &&
		   ptr[2] == IBUS_NAV_ID &&
//		   ptr[3] == 0x21 &&
//		   ptr[4] == 0x60 &&
//		   ptr[5] == 0x00 &&
		   ptr[7] == 'P' &&
		   ptr[9] == ':' &&
		   ptr[10] == ' ' &&
		   ptr[11] == 'C' &&
		   ptr[12] == 'D' /*&& 
		   ptr[13] == ' '*/;
}

int collectPendingMessage(void)
{
	int len = ibus_recv(gRcvBuffer.buffer+gRcvBuffer.buf_end, 255, 1);

	if(len>0) {

		// Now this is the place to check for the special message that displays CDC on the NAV screen and turn
		// the flag on.
		if(gConf.bDisplayNAV) { 
			if(is_cdc_nav_message(gRcvBuffer.buffer+gRcvBuffer.buf_end, len)) {

				navCtl.display = true;
				navCtl.display_time = current_tick + HZ;
				WriteToDebug("Got a CDC message\n", 2);

				setup_index_update(true, 4*HZ);
//				navCtl.index = true;
				
				navCtl.waitForId3 = true;
			}
			else if(is_cd_nav_index_message(gRcvBuffer.buffer+gRcvBuffer.buf_end, len)) {
				WriteToDebug("Got a CD string on the index\n", 2);
				navCtl.index = true;
			}
		}

		// Dump the message contents to the debug output file
		dump_packet(buf, sizeof(buf), gRcvBuffer.buffer+gRcvBuffer.buf_end, len);
		WriteToDebug(buf, 1);
		// Add the timestamp
		snprintf(buf, sizeof(buf), " (%d)\n", current_tick);
		WriteToDebug(buf, 2);

		gRcvBuffer.buf_end += len;
		cdc_stats.total_msgs++;
	}
	else if( len < 0 ) {	// Error accouting
		switch(-len) {
		case RX_FRAMING:
			cdc_stats.framing_errors++;
			break;
		case RX_PARITY:
			cdc_stats.parity_errors++;
			break;
		case RX_OVERRUN:
			cdc_stats.overrun_errors++;
			break;
		case RX_SYMBOL:
			cdc_stats.symbol_errors++;
			break;
		case RX_OVERFLOW:
			cdc_stats.overflow_errors++;
			break;
		case RX_OVERLAP:
			cdc_stats.overlap_errors++;
		default:
			break;
		}
		cdc_stats.total_errors++;
	}

	return len;
}


// --------------------------------------------------------------
// This is called to know if there are any pending poll requests
// a parameter controls the message removal from the buffer.
// This is to be called inside long replies when we may be polled
// before ending the processing of the previous request.
// Poll messages are intended to be replied as soon as possible
// --------------------------------------------------------------


bool peekPollMessage(bool erase)
{
	unsigned char *ptr;
	unsigned char len = sizeof(ibus_msg_poll_req)+1;

	for(ptr=gRcvBuffer.buffer+gRcvBuffer.buf_start; ptr<gRcvBuffer.buffer+gRcvBuffer.buf_end-len; ptr++)
	{
		if(gConf.bPollsFirst) {
			if(memcmp(ptr, ibus_msg_poll_req, sizeof(ibus_msg_poll_req))==0) {
				WriteToDebug("POLL FIRST:\n", 2);
				emu_process_pollreq();
				if(erase) memset(ptr, 0, len);
				return true;
			}
		}
		if(gConf.bStatusFirst) {
			if(memcmp(ptr, ibus_msg_status_req, sizeof(ibus_msg_status_req))==0) {
				WriteToDebug("STATUS FIRST:\n", 2);
				if(LOWDIGIT(gEmu.status.playstatus) == IDLE_CODE)
					cdc_report_special();
				else
					cdc_report_playstatus();
				if(erase) memset(ptr, 0, len);
				return true;
			}
		}
	}
	return false;
}


void urgent_message_check(void)
{
	if(peekPollMessage(true)) {
		WriteToDebug("URGENT:)\n", 2);
	}
}


bool no_pending_messages(void)
{
	return gRcvBuffer.buf_start == gRcvBuffer.buf_end;
}

/* This is an attempt to write a smarter function to retrieve messages from the buffer */
/*
int get_next_message(unsigned char *msg)
{
	// Remove all pending messages from the inbox queue and put them inside the global message buffer
	while( collectPendingMessage() > 0);

	// Check if there are any poll requests pending. If so, reply to them and disguise the messages
	urgent_message_check();

	if(gRcvBuffer.buf_start < gRcvBuffer.buf_end) {
		find_fist_ibus_message(gRcvBuffer.buffer+gRcvBuffer.buf_start, gRcvBuffer.buf_end-gRcvBuffer.buf_start);

	}
	else {
		// buf_start = buf_end
		gRcvBuffer.buf_start = gRcvBuffer.buf_end = 0;
		return 0;
	}
}
*/

#ifdef SIM_BUILD
unsigned char simMsgs[256] = {
	0x80, 0x0E, 0xE7, 0x23, 0x01, 0x20, 0x03, 0x31, 0x38, 0x3A, 0x31, 0x36, 0x20, 0x20, 0x04, 0x58,
	0x80, 0x0E, 0xE7, 0x23, 0x01, 0x20, 0x03, 0x31, 0x38, 0x3A, 0x31, 0x36, 0x20, 0x20, 0x04, 0x58,
	0x50, 0x03, 0xC8, 0x01, 0x9A,
	0x80, 0x0E, 0xE7, 0x23, 0x01, 0x20, 0x03, 0x31, 0x38, 0x3A, 0x31, 0x36, 0x20, 0x20, 0x04, 0x58,
	0x68, 0x05, 0x18, 0x38, 0x00, 0x00, 0x4D,
	0x68, 0x05, 0x18, 0x38, 0x02, 0x00, 0x4F,
	0x68, 0x05, 0x18, 0x38, 0x00, 0x00, 0x4D,
	0x68, 0x06, 0x3b, 0xa5, 0x60, 0x01, 0x00, 0x91,
	0x68, 0x05, 0x18, 0x38, 0x03, 0x00, 0x4e,
	0x68, 0x06, 0x3b, 0xa5, 0x60, 0x01, 0x00, 0x91,
	0x68, 0x05, 0x18, 0x38, 0x00, 0x00, 0x4d
};

void initReceptionBuffer(void)
{
	int i;

	for(i=0; i<256; i++)
	{
		gRcvBuffer.buffer[i] = simMsgs[i];
	}
	gRcvBuffer.buf_end = 256;
}
#endif

unsigned int getNextMessage(unsigned char *msg)
{
	// Remove all pending messages from the inbox queue and put them inside the global message buffer
	while( collectPendingMessage() > 0);

	// Check if there are any poll requests pending. If so, reply to them and disguise the messages
	urgent_message_check();

	// First we need to check if the buffer has any message
	if(gRcvBuffer.buf_start < gRcvBuffer.buf_end) {
		// There are still messages in the buffer
		int s_p = message_type(gRcvBuffer.buffer+gRcvBuffer.buf_start, gRcvBuffer.buf_end-gRcvBuffer.buf_start);
		unsigned int type = s_p & 0xFF00;

		if(type != 0) {
			// There is a recognized message in the buffer
			//int len = IBusMessageSize(type)+1;
			unsigned char len = s_p & 0xFF;

			WriteToDebug("+ ", 2);
			dump_packet(buf, sizeof(buf), gRcvBuffer.buffer+gRcvBuffer.buf_start, len);
			WriteToDebug(buf, 2);

#ifdef SIM_BUILD
			lcd_puts_scroll(0,0,buf);
			sleep(10*HZ);
#endif

			snprintf(buf, sizeof(buf), " (%d)", current_tick);
			WriteToDebug(buf, 2);
			WriteToDebug("\n", 2);

			memcpy(msg, gRcvBuffer.buffer+gRcvBuffer.buf_start, len);
			gRcvBuffer.buf_start+=len;
			return s_p;
		}
		else {

			// The buffer contains an unrecognized message

			// We only skip if what is in the buffer is at least the size of the smaller message we
			// receive. Otherwise it may well be an incomplete message.
			if(gRcvBuffer.buf_end-gRcvBuffer.buf_start >= 5) 
				// we may skip until we find an interesting source id
				while(++(gRcvBuffer.buf_start) < gRcvBuffer.buf_end && 
					   (gRcvBuffer.buffer[gRcvBuffer.buf_start] != IBUS_RADIO_ID &&
						gRcvBuffer.buffer[gRcvBuffer.buf_start] != IBUS_IKE_ID &&
						gRcvBuffer.buffer[gRcvBuffer.buf_start] != IBUS_TELEPHONE_ID &&
						gRcvBuffer.buffer[gRcvBuffer.buf_start] != IBUS_NAV_ID &&
						gRcvBuffer.buffer[gRcvBuffer.buf_start] != IBUS_STEERING_WHEEL_ID &&
						gRcvBuffer.buffer[gRcvBuffer.buf_start] != IBUS_MONITOR_BUTTONS_ID));
			return 0;
		}
	}
	else {
		// buf_start = buf_end
		gRcvBuffer.buf_start = gRcvBuffer.buf_end = 0;
		return 0;
	}
}


void dump_statistics(void)
{
	snprintf(buf, sizeof(buf), "Statistics:\n");											WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Total Messages: %d\n", cdc_stats.total_msgs);			WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Total Errors:   %d\n", cdc_stats.total_errors);			WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "\n");														WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Overlap errors:  %d\n", cdc_stats.overlap_errors);		WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Overrun errors:  %d\n", cdc_stats.overrun_errors);		WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Framing errors:  %d\n", cdc_stats.framing_errors);		WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Parity errors:   %d\n", cdc_stats.parity_errors);		WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Overflow errors: %d\n", cdc_stats.overflow_errors);		WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    Symbol errors:   %d\n", cdc_stats.symbol_errors);		WriteToDebug(buf, 2);
	snprintf(buf, sizeof(buf), "    ibus errors:     %d\n", cdc_stats.ibus_errors);			WriteToDebug(buf, 2);
}

/*
		and_b(~TXMASK, &PBIORH); // float PB10/PB11 (input);

        if ((PBDR & PB10) == 0)
            gSendIRQ.collision = true;
		break;
*/

#define PB10 0x0400
#define PB11 0x0800

#ifdef _USE_SERIAL_MOD
#define TXBIT	PB11
#else
#define TXBIT	PB10	// Use PB10 to make RX/TX on the same pin
#endif

#define TXMASK	(TXBIT>>8)


void emu_main(void)
{
    int button;
    unsigned int s_p;
	unsigned char msg_size;
    unsigned char ibus_msg[IBUS_MAX_SIZE];

	// This is really the very first thing to do... as some init may depend on parameters
	loadSetting();
#if defined(ARCHOS_PLAYER)
	lcd_double_height(false);
#endif

	// Load the user settings
	lcd_clear_display();
	lcd_puts(0,0,"Settings");
	lcd_puts(0,1,"Loaded");
	sleep(HZ/2);
#ifdef SIM_BUILD
	sleep(2*HZ);
#endif

	// Adjust messages to match CD changer id
	if(gConf.bAlternateChanger) cdc_adjust_id();

	// Display version information
	lcd_clear_display();
	lcd_puts(0,0,"BimmerBox");
	lcd_puts(0,1,CDC_EMU_VERSION);
	sleep(2*HZ);
	

	if(gConf.iDebugLevel == 1)	lcd_puts(0,0,"BMW DEBUG=1");
	else if(gConf.iDebugLevel == 2) lcd_puts(0,0,"BMW DEBUG=2");
	else				lcd_puts(0,0,"BMW MODE");

    ibus_init(); // init the I-Bus layer
    emu_init(); // init emulator

	yield();

	debug_file = fopen("/out.txt", O_WRONLY|O_CREAT|O_APPEND);
	if(debug_file <0) lcd_puts(0,1,"DBG FAILED!");

	WriteToDebug("Starting BimmerBox Version: ", 2);
	WriteToDebug(CDC_EMU_VERSION, 2);

//	int i;
//	for(i=0; i<100; i++) {
//		snprintf(buf, sizeof(buf), "Starting IBUS CD Changer Emulator %d... (tick = %d)\n", current_tick, 10-i);
//		WriteToDebug(buf, 1);
//	}


	lcd_puts_scroll(0,1,"Loading playback information");
	// Greetings everyone...

	// Recover the saved playback information
	loadPlaybackCtrl();

	lcd_puts(0,1,"Done!");

#ifndef DUMMY_BUILD
	// Now it's time to load the changer magazine and report the loaded CDs
	emu_switch_magazine(false);
#endif

	// STEFAN ONLY
	fake_mid_clock_press();
	request_date_from_ike();

#ifndef DUMMY_BUILD
	// first announce message at startup
	cdc_announce();
#else 
	sleep(HZ);
#endif


	// STEFAN ONLY
	fake_mid_clock_press();
	//fake_ike_set_date();

#ifndef DUMMY_BUILD
	// Report seek to first CD
	cdc_status_cd_seeking();

	// Report seek to first track
	cdc_status_track_seeking();

	// Report playstatus
	cdc_report_playstatus();

	// Poll the telephone...
	if(gConf.bUsePhoneButtons) poll_telephone();
#endif

	lcd_puts_scroll(0,1,"Main Loop...");
//	lcd_puts_scroll(0,1,gConf.sGreetings);


	// Clean up the button even queue
	while(button_get(false));

#ifdef SIM_BUILD
	initReceptionBuffer();
#endif

	// Main loop
    do
    {
		button = button_get(false);

		if( button == SYS_USB_CONNECTED) {
			usb_screen();
			system_reboot();
		};

		if( button == BUTTON_RIGHT) {
			gConf.iDebugLevel = (gConf.iDebugLevel+1)%3;
			if(gConf.iDebugLevel == 1) lcd_puts(0,0,"BMW DEBUG=1");
			else if(gConf.iDebugLevel == 2) lcd_puts(0,0,"BMW DEBUG=2");
			else lcd_puts(0,0,"BMW MODE");
		}
		if( button == BUTTON_LEFT) {
			id3_change_mode();
		}

		s_p = getNextMessage(ibus_msg);
	
		msg_size = s_p & 0xFF;

		if(s_p > 0) {		
			dump_packet(buf, sizeof(buf), ibus_msg, msg_size);
			lcd_clear_display();
            lcd_puts_scroll(0,1,buf);
			emu_process_packet(ibus_msg, msg_size, s_p & 0xFF00);
		}

		if(current_tick - gEmu.last_time >= gEmu.poll_interval) {
			emu_fast_tick();
			gEmu.last_time += gEmu.poll_interval;
		}

		if(current_tick - gEmu.last_scroll >= gEmu.scroll_period) {
			id3_refresh();
			gEmu.last_scroll += gEmu.scroll_period;
		}

		if(gConf.bDisplayNAV) {

			if(current_tick > navCtl.display_time) {
				navCtl.display_time = (unsigned long) -1;
				if(navCtl.display) {
					nav_display_info();
				}
			}

			if(current_tick > navCtl.index_time) {
				if(navCtl.index) {
					nav_display_index();
				}
				navCtl.index_time = (unsigned long) -1;
				navCtl.index = false;
				navCtl.waitForId3 = false;
			}

		}
	} while (button != BUTTON_PLAY);

	dump_statistics();
	fclose(debug_file);
}
