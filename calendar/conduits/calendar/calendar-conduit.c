/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Calendar Conduit
 *
 * Copyright (C) 1998 Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Eskil Heyn Olsen <deity@eskil.dk> 
 *          JP Rosevear <jpr@ximian.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <cal-client/cal-client-types.h>
#include <cal-client/cal-client.h>
#include <cal-util/timeutil.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <libical/src/libical/icaltypes.h>
#include <e-pilot-util.h>

#define CAL_CONFIG_LOAD 1
#define CAL_CONFIG_DESTROY 1
#include <calendar-conduit-config.h>
#undef CAL_CONFIG_LOAD
#undef CAL_CONFIG_DESTROY

#include <calendar-conduit.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

#define CONDUIT_VERSION "0.1.5"
#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "ecalconduit"

#define DEBUG_CALCONDUIT 1
/* #undef DEBUG_CALCONDUIT */

#ifdef DEBUG_CALCONDUIT
#define LOG(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, e)
#else
#define LOG(e...)
#endif 

#define WARN(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, e)
#define INFO(e...) g_log (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, e)

/* Debug routines */
static char *
print_local (ECalLocalRecord *local)
{
	static char buff[ 4096 ];

	if (local == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	if (local->appt && local->appt->description) {
		g_snprintf (buff, 4096, "[%ld %ld '%s' '%s']",
			    mktime (&local->appt->begin),
			    mktime (&local->appt->end),
			    local->appt->description ?
			    local->appt->description : "",
			    local->appt->note ?
			    local->appt->note : "");
		return buff;
	}

	return "";
}

static char *print_remote (GnomePilotRecord *remote)
{
	static char buff[ 4096 ];
	struct Appointment appt;

	if (remote == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	memset (&appt, 0, sizeof (struct Appointment));
	unpack_Appointment (&appt, remote->record, remote->length);

	g_snprintf (buff, 4096, "[%ld %ld '%s' '%s']",
		    mktime (&appt.begin),
		    mktime (&appt.end),
		    appt.description ?
		    appt.description : "",
		    appt.note ?
		    appt.note : "");

	return buff;
}

/* Context Routines */
static ECalConduitContext *
e_calendar_context_new (guint32 pilot_id) 
{
	ECalConduitContext *ctxt = g_new0 (ECalConduitContext, 1);

	calconduit_load_configuration (&ctxt->cfg, pilot_id);

	return ctxt;
}

static void
e_calendar_context_foreach_change (gpointer key, gpointer value, gpointer data) 
{
	g_free (key);
}

static void
e_calendar_context_destroy (ECalConduitContext *ctxt)
{
	g_return_if_fail (ctxt != NULL);

	if (ctxt->cfg != NULL)
		calconduit_destroy_configuration (&ctxt->cfg);

	if (ctxt->client != NULL)
		gtk_object_unref (GTK_OBJECT (ctxt->client));

	if (ctxt->calendar_file)
		g_free (ctxt->calendar_file);

	if (ctxt->uids)
		cal_obj_uid_list_free (ctxt->uids);

	if (ctxt->changed_hash)
		g_hash_table_foreach (ctxt->changed_hash, e_calendar_context_foreach_change, NULL);

	if (ctxt->changed)
		cal_client_change_list_free (ctxt->changed);
	
	if (ctxt->map)
		e_pilot_map_destroy (ctxt->map);

	g_free (ctxt);
}

/* Calendar Server routines */
static void
start_calendar_server_cb (CalClient *cal_client,
			  CalClientOpenStatus status,
			  gpointer data)
{
	ECalConduitContext *ctxt;

	ctxt = data;

	LOG ("  entering start_calendar_server_cb\n");

	if (status == CAL_CLIENT_OPEN_SUCCESS) {
		ctxt->calendar_open_success = TRUE;
		LOG ("    success\n");
	} else
		LOG ("    open of calendar failed\n");

	gtk_main_quit (); /* end the sub event loop */
}

static int
start_calendar_server (ECalConduitContext *ctxt)
{
	
	g_return_val_if_fail (ctxt != NULL, -2);

	ctxt->client = cal_client_new ();

	/* FIX ME */
	ctxt->calendar_file = g_concat_dir_and_file (g_get_home_dir (),
			       "evolution/local/Calendar/calendar.ics");

	gtk_signal_connect (GTK_OBJECT (ctxt->client), "cal_opened",
			    start_calendar_server_cb, ctxt);

	LOG ("    calling cal_client_open_calendar\n");
	if (!cal_client_open_calendar (ctxt->client, ctxt->calendar_file, FALSE))
		return -1;

	/* run a sub event loop to turn cal-client's async load
	   notification into a synchronous call */
	gtk_main ();

	if (ctxt->calendar_open_success)
		return 0;

	return -1;
}

/* Utility routines */
static icaltimezone *
get_timezone (CalClient *client, const char *tzid) 
{
	icaltimezone *timezone = NULL;

	timezone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (timezone == NULL)
		 cal_client_get_timezone (client, tzid, &timezone);
	
	return timezone;
}

static icaltimezone *
get_default_timezone (void)
{
	Bonobo_ConfigDatabase db;
	icaltimezone *timezone = NULL;
	char *location;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return NULL;
 	}

	CORBA_exception_free (&ev);

	location = bonobo_config_get_string (db, "/Calendar/Display/Timezone", NULL);
	if (location == NULL)
		goto cleanup;
	
	timezone = icaltimezone_get_builtin_timezone (location);
	g_free (location);

 cleanup:
	bonobo_object_release_unref (db, NULL);

	return timezone;	
}


static char *
map_name (ECalConduitContext *ctxt) 
{
	char *filename;
	
	filename = g_strdup_printf ("%s/evolution/local/Calendar/pilot-map-calendar-%d.xml", g_get_home_dir (), ctxt->cfg->pilot_id);

	return filename;
}

static icalrecurrencetype_weekday
get_ical_day (int day) 
{
	switch (day) {
	case 0:
		return ICAL_SUNDAY_WEEKDAY;
	case 1:
		return ICAL_MONDAY_WEEKDAY;
	case 2:
		return ICAL_TUESDAY_WEEKDAY;
	case 3:
		return ICAL_WEDNESDAY_WEEKDAY;
	case 4:
		return ICAL_THURSDAY_WEEKDAY;
	case 5:
		return ICAL_FRIDAY_WEEKDAY;
	case 6:
		return ICAL_SATURDAY_WEEKDAY;
	}

	return ICAL_NO_WEEKDAY;
}

static int
get_pilot_day (icalrecurrencetype_weekday wd) 
{
	switch (wd) {
	case ICAL_SUNDAY_WEEKDAY:
		return 0;
	case ICAL_MONDAY_WEEKDAY:
		return 1;
	case ICAL_TUESDAY_WEEKDAY:
		return 2;
	case ICAL_WEDNESDAY_WEEKDAY:
		return 3;
	case ICAL_THURSDAY_WEEKDAY:
		return 4;
	case ICAL_FRIDAY_WEEKDAY:
		return 5;
	case ICAL_SATURDAY_WEEKDAY:
		return 6;
	default:
		return -1;
	}
}

static gboolean
is_empty_time (struct tm time) 
{
	if (time.tm_sec || time.tm_min || time.tm_hour 
	    || time.tm_mday || time.tm_mon || time.tm_year) 
		return FALSE;
	
	return TRUE;
}

static short
nth_weekday (int pos, icalrecurrencetype_weekday weekday)
{
	g_assert (pos > 0 && pos <= 5);

	return (pos << 3) | (int) weekday;
}

static GList *
next_changed_item (ECalConduitContext *ctxt, GList *changes) 
{
	CalClientChange *ccc;
	GList *l;
	
	for (l = changes; l != NULL; l = l->next) {
		const char *uid;

		ccc = l->data;
		
		cal_component_get_uid (ccc->comp, &uid);
		if (g_hash_table_lookup (ctxt->changed_hash, uid))
			return l;
	}
	
	return NULL;
}

static void
compute_status (ECalConduitContext *ctxt, ECalLocalRecord *local, const char *uid)
{
	CalClientChange *ccc;

	local->local.archived = FALSE;
	local->local.secret = FALSE;

	ccc = g_hash_table_lookup (ctxt->changed_hash, uid);
	
	if (ccc == NULL) {
		local->local.attr = GnomePilotRecordNothing;
		return;
	}
	
	switch (ccc->type) {
	case CAL_CLIENT_CHANGE_ADDED:
		local->local.attr = GnomePilotRecordNew;
		break;
		
	case CAL_CLIENT_CHANGE_MODIFIED:
		local->local.attr = GnomePilotRecordModified;
		break;
		
	case CAL_CLIENT_CHANGE_DELETED:
		local->local.attr = GnomePilotRecordDeleted;
		break;
	}
}

static GnomePilotRecord
local_record_to_pilot_record (ECalLocalRecord *local,
			      ECalConduitContext *ctxt)
{
	GnomePilotRecord p;
	
	g_assert (local->comp != NULL);
	g_assert (local->appt != NULL );
	
	p.ID = local->local.ID;
	p.category = 0;
	p.attr = local->local.attr;
	p.archived = local->local.archived;
	p.secret = local->local.secret;

	/* Generate pilot record structure */
	p.record = g_new0 (char, 0xffff);
	p.length = pack_Appointment (local->appt, p.record, 0xffff);

	return p;	
}

/*
 * converts a CalComponent object to a ECalLocalRecord
 */
static void
local_record_from_comp (ECalLocalRecord *local, CalComponent *comp, ECalConduitContext *ctxt) 
{
	const char *uid;
	CalComponentText summary;
	GSList *d_list = NULL;
	CalComponentText *description;
	CalComponentDateTime dt;
	time_t dt_time;
	CalComponentClassification classif;
	int i;
	
	g_return_if_fail (local != NULL);
	g_return_if_fail (comp != NULL);

	local->comp = comp;
	gtk_object_ref (GTK_OBJECT (comp));
	
	cal_component_get_uid (local->comp, &uid);
	local->local.ID = e_pilot_map_lookup_pid (ctxt->map, uid);
	compute_status (ctxt, local, uid);

	local->appt = g_new0 (struct Appointment, 1);

	/* STOP: don't replace these with g_strdup, since free_Appointment
	   uses free to deallocate */
	cal_component_get_summary (comp, &summary);
	if (summary.value) 
		local->appt->description = e_pilot_utf8_to_pchar (summary.value);

	cal_component_get_description_list (comp, &d_list);
	if (d_list) {
		description = (CalComponentText *) d_list->data;
		if (description && description->value)
			local->appt->note = e_pilot_utf8_to_pchar (description->value);
		else
			local->appt->note = NULL;
	} else {
		local->appt->note = NULL;
	}

	cal_component_get_dtstart (comp, &dt);	
	if (dt.value) {
		dt_time = icaltime_as_timet_with_zone (*dt.value, get_timezone (ctxt->client, dt.tzid));
		
		local->appt->begin = *localtime (&dt_time);
	}

	cal_component_get_dtend (comp, &dt);	
	if (dt.value && time_add_day (dt_time, 1) != icaltime_as_timet_with_zone (*dt.value, get_timezone (ctxt->client, dt.tzid))) {
		dt_time = icaltime_as_timet_with_zone (*dt.value, get_timezone (ctxt->client, dt.tzid));
		
		local->appt->end = *localtime (&dt_time);
		local->appt->event = 0;
	} else {
		local->appt->event = 1;
	}

	/* Recurrence Rules */
	local->appt->repeatType = repeatNone;
	
	if (cal_component_has_rrules (comp)) {
		GSList *list;
		struct icalrecurrencetype *recur;
		
		cal_component_get_rrule_list (comp, &list);
		recur = list->data;
		
		switch (recur->freq) {
		case ICAL_DAILY_RECURRENCE:
			local->appt->repeatType = repeatDaily;
			break;
		case ICAL_WEEKLY_RECURRENCE:
			local->appt->repeatType = repeatWeekly;
			for (i = 0; i <= 7 && recur->by_day[i] != ICAL_RECURRENCE_ARRAY_MAX; i++) {
				icalrecurrencetype_weekday wd;

				wd = icalrecurrencetype_day_day_of_week (recur->by_day[i]);
				local->appt->repeatDays[get_pilot_day (wd)] = 1;
			}
			
			break;
		case ICAL_MONTHLY_RECURRENCE:
			if (recur->by_month_day[0] != ICAL_RECURRENCE_ARRAY_MAX) {
				local->appt->repeatType = repeatMonthlyByDate;
				break;
			}
			
			/* FIX ME Not going to work with -ve by_day */
			local->appt->repeatType = repeatMonthlyByDay;
			switch (icalrecurrencetype_day_position (recur->by_day[0])) {
			case 1:
				local->appt->repeatDay = dom1stSun;
				break;
			case 2:
				local->appt->repeatDay = dom2ndSun;
				break;
			case 3:
				local->appt->repeatDay = dom3rdSun;
				break;
			case 4:
				local->appt->repeatDay = dom4thSun;
				break;
			case 5:
				local->appt->repeatDay = domLastSun;
				break;
			}
			local->appt->repeatDay += get_pilot_day (icalrecurrencetype_day_day_of_week (recur->by_day[0]));
			break;
		case ICAL_YEARLY_RECURRENCE:
			local->appt->repeatType = repeatYearly;
			break;
		default:
			break;
		}

		if (local->appt->repeatType != repeatNone) {
			local->appt->repeatFrequency = recur->interval;
		}
		
		if (icaltime_is_null_time (recur->until)) {
			local->appt->repeatForever = 1;
		} else {
			local->appt->repeatForever = 0;
			dt_time = icaltime_as_timet (recur->until);
			local->appt->repeatEnd = *localtime (&dt_time);
		}
		
		cal_component_free_recur_list (list);
	}

	cal_component_get_classification (comp, &classif);

	if (classif == CAL_COMPONENT_CLASS_PRIVATE)
		local->local.secret = 1;
	else
		local->local.secret = 0;

	local->local.archived = 0;  
}

static void 
local_record_from_uid (ECalLocalRecord *local,
		       const char *uid,
		       ECalConduitContext *ctxt)
{
	CalComponent *comp;
	CalClientGetStatus status;

	g_assert(local!=NULL);

	status = cal_client_get_object (ctxt->client, uid, &comp);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		local_record_from_comp (local, comp, ctxt);
	} else if (status == CAL_CLIENT_GET_NOT_FOUND) {
		comp = cal_component_new ();
		cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);
		cal_component_set_uid (comp, uid);
		local_record_from_comp (local, comp, ctxt);
	} else {
		INFO ("Object did not exist");
	}	
}

static CalComponent *
comp_from_remote_record (GnomePilotConduitSyncAbs *conduit,
			 GnomePilotRecord *remote,
			 CalComponent *in_comp,
			 icaltimezone *timezone)
{
	CalComponent *comp;
	struct Appointment appt;
	struct icaltimetype now = icaltime_from_timet_with_zone (time (NULL), FALSE, timezone), it;
	struct icalrecurrencetype recur;
	int pos, i;
	CalComponentText summary = {NULL, NULL};
	CalComponentDateTime dt = {NULL, icaltimezone_get_tzid (timezone)};
	char *txt;
	
	g_return_val_if_fail (remote != NULL, NULL);

	memset (&appt, 0, sizeof (struct Appointment));
	unpack_Appointment (&appt, remote->record, remote->length);

	if (in_comp == NULL) {
		comp = cal_component_new ();
		cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);
		cal_component_set_created (comp, &now);
	} else {
		comp = cal_component_clone (in_comp);
	}

	cal_component_set_last_modified (comp, &now);

	summary.value = txt = e_pilot_utf8_from_pchar (appt.description);
	cal_component_set_summary (comp, &summary);
	free (txt);

	/* The iCal description field */
	if (!appt.note) {
		cal_component_set_comment_list (comp, NULL);
	} else {
		GSList l;
		CalComponentText text;

		text.value = txt = e_pilot_utf8_from_pchar (appt.note);
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		cal_component_set_description_list (comp, &l);
		free (txt);
	} 

	if (!is_empty_time (appt.begin)) {
		it = icaltime_from_timet_with_zone (mktime (&appt.begin), FALSE, timezone);
		dt.value = &it;
		cal_component_set_dtstart (comp, &dt);
	}

	if (appt.event) {
		time_t t = mktime (&appt.begin);
		
		t = time_day_end (t);
		it = icaltime_from_timet_with_zone (t, FALSE, timezone);
		dt.value = &it;
		cal_component_set_dtend (comp, &dt);
	} else if (!is_empty_time (appt.end)) {
		it = icaltime_from_timet_with_zone (mktime (&appt.end), FALSE, timezone);
		dt.value = &it;
		cal_component_set_dtend (comp, &dt);
	}

	/* Recurrence information */
  	icalrecurrencetype_clear (&recur);

	switch (appt.repeatType) {
	case repeatNone:
		recur.freq = ICAL_NO_RECURRENCE;
		break;

	case repeatDaily:
		recur.freq = ICAL_DAILY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		break;

	case repeatWeekly:
		recur.freq = ICAL_WEEKLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;

		pos = 0;
		for (i = 0; i < 7; i++) {
			if (appt.repeatDays[i])
				recur.by_day[pos++] = get_ical_day (i);
		}
		
		break;

	case repeatMonthlyByDay:
		recur.freq = ICAL_MONTHLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		recur.by_day[0] = nth_weekday (appt.repeatDay / 5, get_ical_day (appt.repeatDay % 5 - 1));
		break;
		
	case repeatMonthlyByDate:
		recur.freq = ICAL_MONTHLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		recur.by_month_day[0] = appt.begin.tm_mday;
		break;

	case repeatYearly:
		recur.freq = ICAL_YEARLY_RECURRENCE;
		recur.interval = appt.repeatFrequency;
		break;
		
	default:
		g_assert_not_reached ();
	}

	if (recur.freq != ICAL_NO_RECURRENCE) {
		GSList *list = NULL;
		
		/* recurrence start of week */
		recur.week_start = get_ical_day (appt.repeatWeekstart);

		if (!appt.repeatForever) {
			time_t t = mktime (&appt.repeatEnd);
			t = time_add_day (t, 1);
			recur.until = icaltime_from_timet_with_zone (t, FALSE, timezone);
		}

		list = g_slist_append (list, &recur);
		cal_component_set_rrule_list (comp, list);
		g_slist_free (list);
	} else {
		cal_component_set_rrule_list (comp, NULL);		
	}

	cal_component_set_transparency (comp, CAL_COMPONENT_TRANSP_NONE);

	if (remote->attr & dlpRecAttrSecret)
		cal_component_set_classification (comp, CAL_COMPONENT_CLASS_PRIVATE);
	else
		cal_component_set_classification (comp, CAL_COMPONENT_CLASS_PUBLIC);

	cal_component_commit_sequence (comp);
	
	free_Appointment (&appt);

	return comp;
}

static void
update_comp (GnomePilotConduitSyncAbs *conduit, CalComponent *comp,
	     ECalConduitContext *ctxt) 
{
	gboolean success;

	g_return_if_fail (conduit != NULL);
	g_return_if_fail (comp != NULL);

	success = cal_client_update_object (ctxt->client, comp);

	if (!success)
		WARN (_("Error while communicating with calendar server"));
}

static void
check_for_slow_setting (GnomePilotConduit *c, ECalConduitContext *ctxt)
{
	int count, map_count;

	count = g_list_length (ctxt->uids);
	map_count = g_hash_table_size (ctxt->map->pid_map);
	
	/* If there are no objects or objects but no log */
	if (map_count == 0) {
		GnomePilotConduitStandard *conduit;
		LOG ("    doing slow sync\n");
		conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
		gnome_pilot_conduit_standard_set_slow (conduit, TRUE);
	} else {
		LOG ("    doing fast sync\n");
	}
}

/* Pilot syncing callbacks */
static gint
pre_sync (GnomePilotConduit *conduit,
	  GnomePilotDBInfo *dbi,
	  ECalConduitContext *ctxt)
{
	GnomePilotConduitSyncAbs *abs_conduit;
	GList *l;
	int len;
	unsigned char *buf;
	char *filename, *change_id;
	gint num_records, add_records = 0, mod_records = 0, del_records = 0;

	abs_conduit = GNOME_PILOT_CONDUIT_SYNC_ABS (conduit);

	LOG ("---------------------------------------------------------\n");
	LOG ("pre_sync: Calendar Conduit v.%s", CONDUIT_VERSION);

	ctxt->client = NULL;
	
	if (start_calendar_server (ctxt) != 0) {
		WARN(_("Could not start wombat server"));
		gnome_pilot_conduit_error (conduit, _("Could not start wombat"));
		return -1;
	}

	/* Get the timezone */
	ctxt->timezone = get_default_timezone ();
	if (ctxt->timezone == NULL)
		return -1;
	LOG ("  Using timezone: %s", icaltimezone_get_tzid (ctxt->timezone));
	
	/* Load the uid <--> pilot id mapping */
	filename = map_name (ctxt);
	e_pilot_map_read (filename, &ctxt->map);
	g_free (filename);

	/* Get the local database */
	ctxt->uids = cal_client_get_uids (ctxt->client, CALOBJ_TYPE_EVENT);

	/* Find the added, modified and deleted items */
	change_id = g_strdup_printf ("pilot-sync-evolution-calendar-%d", ctxt->cfg->pilot_id);
	ctxt->changed = cal_client_get_changes (ctxt->client, CALOBJ_TYPE_EVENT, change_id);
	ctxt->changed_hash = g_hash_table_new (g_str_hash, g_str_equal);

	for (l = ctxt->changed; l != NULL; l = l->next) {
		CalClientChange *ccc = l->data;
		const char *uid;
		
		cal_component_get_uid (ccc->comp, &uid);
		if (!e_pilot_map_uid_is_archived (ctxt->map, uid)) {
			
			g_hash_table_insert (ctxt->changed_hash, g_strdup (uid), ccc);

			switch (ccc->type) {
			case CAL_CLIENT_CHANGE_ADDED:
				add_records++;
				break;
			case CAL_CLIENT_CHANGE_MODIFIED:
				mod_records++;
				break;
			case CAL_CLIENT_CHANGE_DELETED:
				del_records++;
				break;
			}
		}
	}

	/* Set the count information */
	num_records = cal_client_get_n_objects (ctxt->client, CALOBJ_TYPE_TODO);
	gnome_pilot_conduit_sync_abs_set_num_local_records(abs_conduit, num_records);
	gnome_pilot_conduit_sync_abs_set_num_new_local_records (abs_conduit, add_records);
	gnome_pilot_conduit_sync_abs_set_num_updated_local_records (abs_conduit, mod_records);
	gnome_pilot_conduit_sync_abs_set_num_deleted_local_records(abs_conduit, del_records);

	gtk_object_set_data (GTK_OBJECT (conduit), "dbinfo", dbi);

	buf = (unsigned char*)g_malloc (0xffff);
	len = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
			      (unsigned char *)buf, 0xffff);
	
	if (len < 0) {
		WARN (_("Could not read pilot's Calendar application block"));
		WARN ("dlp_ReadAppBlock(...) = %d", len);
		gnome_pilot_conduit_error (conduit,
					   _("Could not read pilot's Calendar application block"));
		return -1;
	}
	unpack_AppointmentAppInfo (&(ctxt->ai), buf, len);
	g_free (buf);

	check_for_slow_setting (conduit, ctxt);

	return 0;
}

static gint
post_sync (GnomePilotConduit *conduit,
	   GnomePilotDBInfo *dbi,
	   ECalConduitContext *ctxt)
{
	GList *changed;
	gchar *filename, *change_id;
	
	LOG ("post_sync: Calendar Conduit v.%s", CONDUIT_VERSION);
	LOG ("---------------------------------------------------------\n");

	filename = map_name (ctxt);
	e_pilot_map_write (filename, ctxt->map);
	g_free (filename);
	
	/* FIX ME ugly hack - our changes musn't count, this does introduce
	 * a race condition if anyone changes a record elsewhere during sycnc
         */
	change_id = g_strdup_printf ("pilot-sync-evolution-calendar-%d", ctxt->cfg->pilot_id);
	changed = cal_client_get_changes (ctxt->client, CALOBJ_TYPE_EVENT, change_id);
	cal_client_change_list_free (changed);

	return 0;
}

static gint
set_pilot_id (GnomePilotConduitSyncAbs *conduit,
	      ECalLocalRecord *local,
	      guint32 ID,
	      ECalConduitContext *ctxt)
{
	const char *uid;

	LOG ("set_pilot_id: setting to %d\n", ID);
	
	cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, ID, uid, FALSE);

        return 0;
}

static gint
set_status_cleared (GnomePilotConduitSyncAbs *conduit,
		    ECalLocalRecord *local,
		    ECalConduitContext *ctxt)
{
	const char *uid;
	
	LOG ("set_status_cleared: clearing status\n");
	
	cal_component_get_uid (local->comp, &uid);
	g_hash_table_remove (ctxt->changed_hash, uid);
	
        return 0;
}

static gint
for_each (GnomePilotConduitSyncAbs *conduit,
	  ECalLocalRecord **local,
	  ECalConduitContext *ctxt)
{
	static GList *uids, *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG ("beginning for_each");

		uids = ctxt->uids;
		count = 0;
		
		if (uids != NULL) {
			LOG ("iterating over %d records", g_list_length (uids));

			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_uid (*local, uids->data, ctxt);

			iterator = uids;
		} else {
			LOG ("no events");
			(*local) = NULL;
			return 0;
		}
	} else {
		count++;
		if (g_list_next (iterator)) {
			iterator = g_list_next (iterator);

			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_uid (*local, iterator->data, ctxt);
		} else {
			LOG ("for_each ending");

			/* Tell the pilot the iteration is over */
			*local = NULL;

			return 0;
		}
	}

	return 0;
}

static gint
for_each_modified (GnomePilotConduitSyncAbs *conduit,
		   ECalLocalRecord **local,
		   ECalConduitContext *ctxt)
{
	static GList *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG ("beginning for_each_modified: beginning\n");
		
		iterator = ctxt->changed;
		
		count = 0;
	
		LOG ("iterating over %d records", g_hash_table_size (ctxt->changed_hash));
		
		iterator = next_changed_item (ctxt, iterator);
		if (iterator != NULL) {
			CalClientChange *ccc = iterator->data;
		
			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_comp (*local, ccc->comp, ctxt);
		} else {
			LOG ("no events");

			*local = NULL;
		}
	} else {
		count++;
		iterator = g_list_next (iterator);
		if (iterator && (iterator = next_changed_item (ctxt, iterator))) {
			CalClientChange *ccc = iterator->data;
			
			*local = g_new0 (ECalLocalRecord, 1);
			local_record_from_comp (*local, ccc->comp, ctxt);
		} else {
			LOG ("for_each_modified ending");

			/* Signal the iteration is over */
			*local = NULL;
		}
	}

	return 0;
}

static gint
compare (GnomePilotConduitSyncAbs *conduit,
	 ECalLocalRecord *local,
	 GnomePilotRecord *remote,
	 ECalConduitContext *ctxt)
{
	/* used by the quick compare */
	GnomePilotRecord local_pilot;
	int retval = 0;

	LOG ("compare: local=%s remote=%s...\n",
		print_local (local), print_remote (remote));

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

	local_pilot = local_record_to_pilot_record (local, ctxt);

	if (remote->length != local_pilot.length
	    || memcmp (local_pilot.record, remote->record, remote->length))
		retval = 1;

	if (retval == 0)
		LOG ("    equal");
	else
		LOG ("    not equal");
	
	return retval;
}

static gint
add_record (GnomePilotConduitSyncAbs *conduit,
	    GnomePilotRecord *remote,
	    ECalConduitContext *ctxt)
{
	CalComponent *comp;
	const char *uid;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG ("add_record: adding %s to desktop\n", print_remote (remote));

	comp = comp_from_remote_record (conduit, remote, NULL, ctxt->timezone);
	update_comp (conduit, comp, ctxt);

	cal_component_get_uid (comp, &uid);

	e_pilot_map_insert (ctxt->map, remote->ID, uid, FALSE);

	return retval;
}

static gint
replace_record (GnomePilotConduitSyncAbs *conduit,
		ECalLocalRecord *local,
		GnomePilotRecord *remote,
		ECalConduitContext *ctxt)
{
	CalComponent *new_comp;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG ("replace_record: replace %s with %s\n",
	     print_local (local), print_remote (remote));

	new_comp = comp_from_remote_record (conduit, remote, local->comp, ctxt->timezone);
	gtk_object_unref (GTK_OBJECT (local->comp));
	local->comp = new_comp;
	update_comp (conduit, local->comp, ctxt);

	return retval;
}

static gint
delete_record (GnomePilotConduitSyncAbs *conduit,
	       ECalLocalRecord *local,
	       ECalConduitContext *ctxt)
{
	const char *uid;

	g_return_val_if_fail (local != NULL, -1);
	g_assert (local->comp != NULL);

	cal_component_get_uid (local->comp, &uid);

	LOG ("delete_record: deleting %s\n", uid);

	e_pilot_map_remove_by_uid (ctxt->map, uid);
	cal_client_remove_object (ctxt->client, uid);
	
        return 0;
}

static gint
archive_record (GnomePilotConduitSyncAbs *conduit,
		ECalLocalRecord *local,
		gboolean archive,
		ECalConduitContext *ctxt)
{
	const char *uid;
	int retval = 0;
	
	g_return_val_if_fail (local != NULL, -1);

	LOG ("archive_record: %s\n", archive ? "yes" : "no");

	cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, local->local.ID, uid, archive);
	
        return retval;
}

static gint
match (GnomePilotConduitSyncAbs *conduit,
       GnomePilotRecord *remote,
       ECalLocalRecord **local,
       ECalConduitContext *ctxt)
{
	const char *uid;
	
	LOG ("match: looking for local copy of %s\n",
	     print_remote (remote));	
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	*local = NULL;
	uid = e_pilot_map_lookup_uid (ctxt->map, remote->ID);
	
	if (!uid)
		return 0;

	LOG ("  matched\n");
	
	*local = g_new0 (ECalLocalRecord, 1);
	local_record_from_uid (*local, uid, ctxt);
	
	return 0;
}

static gint
free_match (GnomePilotConduitSyncAbs *conduit,
	    ECalLocalRecord *local,
	    ECalConduitContext *ctxt)
{
	LOG ("free_match: freeing\n");

	g_return_val_if_fail (local != NULL, -1);

	gtk_object_unref (GTK_OBJECT (local->comp));
	g_free (local);

	return 0;
}

static gint
prepare (GnomePilotConduitSyncAbs *conduit,
	 ECalLocalRecord *local,
	 GnomePilotRecord *remote,
	 ECalConduitContext *ctxt)
{
	LOG ("prepare: encoding local %s\n", print_local (local));

	*remote = local_record_to_pilot_record (local, ctxt);

	return 0;
}

static ORBit_MessageValidationResult
accept_all_cookies (CORBA_unsigned_long request_id,
		    CORBA_Principal *principal,
		    CORBA_char *operation)
{
	/* allow ALL cookies */
	return ORBIT_MESSAGE_ALLOW_ALL;
}


GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilot_id)
{
	GtkObject *retval;
	ECalConduitContext *ctxt;

	LOG ("in calendar's conduit_get_gpilot_conduit\n");

	/* we need to find wombat with oaf, so make sure oaf
	   is initialized here.  once the desktop is converted
	   to oaf and gpilotd is built with oaf, this can go away */
	if (!oaf_is_initialized ()) {
		char *argv[ 1 ] = {"hi"};
		oaf_init (1, argv);

		if (bonobo_init (CORBA_OBJECT_NIL,
				 CORBA_OBJECT_NIL,
				 CORBA_OBJECT_NIL) == FALSE)
			g_error (_("Could not initialize Bonobo"));

		ORBit_set_request_validation_handler (accept_all_cookies);
	}

	retval = gnome_pilot_conduit_sync_abs_new ("DatebookDB", 0x64617465);
	g_assert (retval != NULL);

	ctxt = e_calendar_context_new (pilot_id);
	gtk_object_set_data (GTK_OBJECT (retval), "calconduit_context", ctxt);

	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, ctxt);
	gtk_signal_connect (retval, "post_sync", (GtkSignalFunc) post_sync, ctxt);

  	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, ctxt);
  	gtk_signal_connect (retval, "set_status_cleared", (GtkSignalFunc) set_status_cleared, ctxt);

  	gtk_signal_connect (retval, "for_each", (GtkSignalFunc) for_each, ctxt);
  	gtk_signal_connect (retval, "for_each_modified", (GtkSignalFunc) for_each_modified, ctxt);
  	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, ctxt);

  	gtk_signal_connect (retval, "add_record", (GtkSignalFunc) add_record, ctxt);
  	gtk_signal_connect (retval, "replace_record", (GtkSignalFunc) replace_record, ctxt);
  	gtk_signal_connect (retval, "delete_record", (GtkSignalFunc) delete_record, ctxt);
  	gtk_signal_connect (retval, "archive_record", (GtkSignalFunc) archive_record, ctxt);

  	gtk_signal_connect (retval, "match", (GtkSignalFunc) match, ctxt);
  	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, ctxt);

  	gtk_signal_connect (retval, "prepare", (GtkSignalFunc) prepare, ctxt);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{ 
	GtkObject *obj = GTK_OBJECT (conduit);
	ECalConduitContext *ctxt;

	ctxt = gtk_object_get_data (obj, "calconduit_context");
	e_calendar_context_destroy (ctxt);

	gtk_object_destroy (obj);
}
