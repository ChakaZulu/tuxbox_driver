/*
 * $Id: avia_av_ksyms.c,v 1.5 2004/07/03 01:18:36 carjay Exp $
 *
 * AViA 500/600 core driver (dbox-II-project)
 *
 * Homepage: http://www.tuxbox.org
 *
 * Copyright (C) 2000-2001 Felix "tmbinc" Domke (tmbinc@gmx.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include "avia_av.h"

EXPORT_SYMBOL(avia_av_is500);
EXPORT_SYMBOL(avia_av_read);
EXPORT_SYMBOL(avia_av_write);
EXPORT_SYMBOL(avia_av_dram_memcpy32);
EXPORT_SYMBOL(avia_av_cmd);
EXPORT_SYMBOL(avia_av_set_audio_attenuation);
EXPORT_SYMBOL(avia_av_set_stc);
EXPORT_SYMBOL(avia_av_standby);
EXPORT_SYMBOL(avia_av_get_sample_rate);
EXPORT_SYMBOL(avia_av_set_sample_rate);
EXPORT_SYMBOL(avia_av_bypass_mode_set);
EXPORT_SYMBOL(avia_av_pid_set);
EXPORT_SYMBOL(avia_av_play_state_set_audio);
EXPORT_SYMBOL(avia_av_play_state_set_video);
EXPORT_SYMBOL(avia_av_stream_type_set);
EXPORT_SYMBOL(avia_av_sync_mode_set);
EXPORT_SYMBOL(avia_av_audio_pts_to_stc);
EXPORT_SYMBOL(avia_av_register_video_event_handler);
EXPORT_SYMBOL(avia_av_unregister_video_event_handler);
EXPORT_SYMBOL(avia_av_get_video_size);
EXPORT_SYMBOL(avia_av_new_audio_config);
EXPORT_SYMBOL(avia_av_set_video_system);
