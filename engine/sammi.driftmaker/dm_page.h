/* DRIFTMAKER: the T1 page, a hurdy-gurdy in line art with the page's four values. Copyright (C) 2026 Sammi.
 * GPL-3.0-or-later. */
#ifndef DM_PAGE_H
#define DM_PAGE_H

struct opl_self;
void dm_page_init(struct opl_self *self);
void dm_page_t1(void);

extern volatile int dm_group, dm_touch, dm_touch_age, dm_preset_now;
extern volatile signed char dm_notes[6];
extern const signed char DM_PAGE_PARAM[4][4];
int dm_param99(int group, int enc, const short *k);

#endif
