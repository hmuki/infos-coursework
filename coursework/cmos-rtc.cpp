/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (1)
 */

/*
 * STUDENT NUMBER: s1894401
 */
#include <infos/drivers/timer/rtc.h>
#include <infos/util/lock.h>
#include <arch/x86/pio.h>

using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::arch::x86;
using namespace infos::util;

class CMOSRTC : public RTC {
public:
	static const DeviceClass CMOSRTCDeviceClass;
	
	private:
	int const CMOS_ADDRESS = 0x70;
	int const CMOS_DATA = 0x71;
	
	uint8_t get_RTC_register(int reg) 
	{
		__outb(CMOS_ADDRESS, reg);
		return __inb(CMOS_DATA);
	}
	
	bool get_update_in_progress_flag() 
	{
		// bit 7 from status register A is non-zero if
		// an update is in progress
      		return (get_RTC_register(0x0A) & 0x80);
	}
	
	bool timepoints_are_equal(RTCTimePoint tp1, RTCTimePoint tp2)
	{	
		// all fields of both timepoints must be the same if both timepoints are equal
		return (tp1.seconds == tp2.seconds) && (tp1.minutes == tp2.minutes) && (tp1.hours == tp2.hours) && (tp1.day_of_month == tp2.day_of_month) && (tp1.month == tp2.month) && (tp1.year == tp2.year);
	}
	
	void get_current_time(RTCTimePoint& tp) 
	{
		// read values from various each register
		// pertaining to each field
		tp.seconds = get_RTC_register(0x00);
		tp.minutes = get_RTC_register(0x02);
	        tp.hours = get_RTC_register(0x04);
	        tp.day_of_month = get_RTC_register(0x07);
	        tp.month = get_RTC_register(0x08);
	        tp.year = get_RTC_register(0x09);
	}
	
	const DeviceClass& device_class() const override
	{
		return CMOSRTCDeviceClass;
	}

	
	 // Interrogates the RTC to read the current date & time.
	 // @param tp Populates the tp structure with the current data & time, as
	 // given by the CMOS RTC device.
	 
	void read_timepoint(RTCTimePoint& tp) override
	{
		RTCTimePoint prev_tp;
	        unsigned short registerB;      
 
      	// This uses the "read registers until you get the same values twice in a row" technique
      	// to avoid getting dodgy/inconsistent values due to RTC updates
 
	      while (get_update_in_progress_flag());  // Make sure an update isn't in progress
	      get_current_time(tp);
 		
	      	do {
		    prev_tp = tp;
		    while (get_update_in_progress_flag());  // Make sure an update isn't in progress
		    get_current_time(tp);
	      	} while (!timepoints_are_equal(prev_tp, tp));
 		
      		registerB = get_RTC_register(0x0B);
 
	      // Convert BCD to binary values if necessary
	 
	      if (!(registerB & 0x04)) {
		    tp.seconds = (tp.seconds & 0x0F) + ((tp.seconds / 16) * 10);
		    tp.minutes = (tp.minutes & 0x0F) + ((tp.minutes / 16) * 10);
		    tp.hours = ( (tp.hours & 0x0F) + (((tp.hours & 0x70) / 16) * 10) ) | (tp.hours & 0x80);
		    tp.day_of_month = (tp.day_of_month & 0x0F) + ((tp.day_of_month / 16) * 10);
		    tp.month = (tp.month & 0x0F) + ((tp.month / 16) * 10);
		    tp.year = (tp.year & 0x0F) + ((tp.year / 16) * 10);
	      }
 
	       // Convert 12 hour clock to 24 hour clock if necessary
	       // (i.e. if we are in 12-hour mode and the PM bit is set)
	 
	      if (!(registerB & 0x02) && (tp.hours & 0x80)) {
		    tp.hours = ((tp.hours & 0x7F) + 12) % 24;
	      }
	}
};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
