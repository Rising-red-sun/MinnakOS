#ifndef RTC_H
#define RTC_H

/* Writes "HH:MM" into out (at least 6 bytes). */
void rtc_time(char *out);
int  rtc_hour(void);
int  rtc_minute(void);

#endif
