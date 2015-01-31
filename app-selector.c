/*
 * Copyright 2012  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://floralicense.org/license

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <appcore-efl.h>
#include <Ecore_X.h>
#include <utilX.h>
#include <aul.h>
#include <appsvc.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <Ecore.h>
#include <Ecore_Input.h>
#include <efl_assist.h>

#include <vconf.h>

#include "app-selector.h"
#include "app-selector-view.h"
#include "app-selector-util.h"

#define WIN_PROP_NAME "APP_SELECTOR"

#ifndef EXPORT_API
#define EXPORT_API __attribute__ ((visibility("default")))
#endif

extern int appsvc_request_transient_app(bundle *b, Ecore_X_Window callee_id, appsvc_host_res_fn cbfunc, void *data);
extern int app_send_cmd_with_noreply(int pid, int cmd, bundle *kb);
extern void popup_update_by_lang_changed_sig(struct appdata *ad);

bool enable_pause_terminate = false;
Ecore_Timer *exit_timer;

int is_reset = 0;

Ecore_Event_Handler *ecore_x_handler;


static void __win_del(void *data, Evas_Object * obj, void *event)
{
	elm_exit();
}

static Eina_Bool __window_focus_out_event_cb(void *data, int type, void *event)
{
	_D("selector popup get focus out event. popup will be terminated");

	elm_exit();

	return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool __exit_timer_cb(void *data)
{
	_D("selected app is not launched successfully. abnormal case. selector popup will be terminated by force.");

	elm_exit();

	return ECORE_CALLBACK_CANCEL;
}

void _tasks_related_launching_app(void *data)
{
	struct appdata *ad = (struct appdata *)data;

	/* handler for case that 32bit window is launched from popup  (no pause event) */
	ecore_x_window_client_manage(ad->xwin);
	ecore_x_handler = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_FOCUS_OUT, __window_focus_out_event_cb, NULL);

	/* app terminate timer for case that abnormal case like selected app launch fail */
	exit_timer = ecore_timer_add(3, __exit_timer_cb, NULL);

	enable_pause_terminate = TRUE;
}

bool _is_caller_window_floating_state(void *data)
{
	struct appdata *ad = (struct appdata *)data;
	int ret;

	ret = ecore_x_e_illume_window_state_get(ad->caller_xwin);
	if(ret == ECORE_X_ILLUME_WINDOW_STATE_FLOATING) {
		return true;
	}

	return false;
}

static Evas_Object *__create_win(const char *name)
{
	Evas_Object *eo;
	int w;
	int h;
	Evas_Object *bg;

	eo = elm_win_add(NULL, name, ELM_WIN_BASIC);
	if (eo) {
		elm_win_title_set(eo, name);
		elm_win_borderless_set(eo, EINA_TRUE);

		elm_win_alpha_set(eo, EINA_TRUE);

		evas_object_smart_callback_add(eo, "delete,request",
					       __win_del, NULL);
		ecore_x_window_size_get(ecore_x_window_root_first_get(),
					&w, &h);
		evas_object_resize(eo, w, h);

		ea_screen_reader_support_set(eo, EINA_TRUE);

		bg = elm_bg_add(eo);
		evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		elm_win_resize_object_add(eo, bg);
		evas_object_show(bg);
	}

	return eo;
}

static Evas_Object *__create_layout_main(Evas_Object * parent)
{
	Evas_Object *layout;

	if (!parent)
		return NULL;

	layout = elm_layout_add(parent);
	if (!layout)
		return NULL;

	elm_layout_theme_set(layout, "layout", "application", "default");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND,
					 EVAS_HINT_EXPAND);

	edje_object_signal_emit(_EDJ(layout), "elm,state,show,content", "elm");

	evas_object_show(layout);

	return layout;
}

static int __get_window_property(Display *dpy, Window win, Atom atom,
					  Atom type, unsigned int *val,
					  unsigned int len)
{
	unsigned char *prop_ret;
	Atom type_ret;
	unsigned long bytes_after;
	unsigned long  num_ret;
	int format_ret;
	unsigned int i;
	int num;

	prop_ret = NULL;
	if (XGetWindowProperty(dpy, win, atom, 0, 0x7fffffff, False,
			       type, &type_ret, &format_ret, &num_ret,
			       &bytes_after, &prop_ret) != Success)
		return -1;

	if (type_ret != type || format_ret != 32)
		num = -1;
	else if (num_ret == 0 || !prop_ret)
		num = 0;
	else {
		if (num_ret < len)
			len = num_ret;
		for (i = 0; i < len; i++) {
			val[i] = ((unsigned long *)prop_ret)[i];
		}
		num = len;
	}

	if (prop_ret)
		XFree(prop_ret);

	return num;
}

#ifdef ROTATE_USING_X_CLIENT
static int __x_rotation_get(Display *dpy, Window win)
{
	Window active_win;
	Window root_win;
	int rotation = -1;
	int ret;

	Atom atom_active_win;
	Atom atom_win_rotate_angle;

	root_win = XDefaultRootWindow(dpy);

	atom_active_win = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	ret = __get_window_property(dpy, root_win, atom_active_win,
					     XA_WINDOW,
					     (unsigned int *)&active_win, 1);

	if (ret < 0)
		return ret;

	atom_win_rotate_angle =
		XInternAtom(dpy, "_E_ILLUME_ROTATE_WINDOW_ANGLE", False);
	ret = __get_window_property(dpy, active_win,
					  atom_win_rotate_angle, XA_CARDINAL,
					  (unsigned int *)&rotation, 1);

	if (ret != -1)
		return rotation;

	return -1;
}

static int __as_rotate(Display *dpy, Window xwin, Evas_Object *win, void *data)
{
	int rotation;
	struct appdata *ad = data;

	ecore_x_icccm_hints_set(xwin, 0, 0, 0, 0, 0, 0, 0);

	rotation = __x_rotation_get(dpy, xwin);

	if (rotation == -1) {
		rotation = 0;
	}

	if (rotation >= 0 && ad->rotate != NULL)
		elm_win_rotation_with_resize_set(win, rotation);

	return 0;
}

static Eina_Bool __rotate(void *data, int type, void *event)
{
	struct appdata *ad = data;

	Ecore_X_Event_Client_Message *ev = event;

	if (!event)
		return ECORE_CALLBACK_RENEW;

	if (ev->message_type == ECORE_X_ATOM_E_ILLUME_ROTATE_ROOT_ANGLE)
		__as_rotate(ad->dpy, ad->xwin, ad->win, ad);

	return 0;
}
#endif

static Eina_Bool s_key_registered;

static
Eina_Bool __key_press_cb(void *data, int type, void *event)
{
	Evas_Event_Key_Down *ev = event;

	if (!s_key_registered) {
		_E("Key is not registered");
		return ECORE_CALLBACK_RENEW;
	}

	if (!ev) {
		_E("Invalid event object");
		return ECORE_CALLBACK_RENEW;
	}

	_D("key press - %s", ev->keyname);

	if ((strcmp(ev->keyname, KEY_END) == 0) ||
		(strcmp(ev->keyname, "Escape") == 0)) {
		elm_exit();
	}

	return ECORE_CALLBACK_RENEW;
}

static void __register_key_handler(void *data)
{
	struct appdata *ad = (struct appdata *)data;

	ad->key_up = ecore_event_handler_add(ECORE_EVENT_KEY_UP,
										__key_press_cb, ad);
	if (!ad->key_up) {
		_E("Failed to register a key up event handler");
	}

	_D("Key handler is registered");
	s_key_registered = EINA_TRUE;
}

void _unregister_key_handler(void *data)
{
	struct appdata *ad = (struct appdata *)data;

	if (ad->key_up) {
		ecore_event_handler_del(ad->key_up);
		ad->key_up = NULL;
	}

	_D("Unregister key handler is invoked");
	s_key_registered = EINA_FALSE;
}

static void __grab_key(void *data)
{
	struct appdata *ad = (struct appdata *)data;

	_D("Grab home / back key.");

	utilx_grab_key(ad->dpy, ad->xwin, KEY_HOME, SHARED_GRAB);

	utilx_grab_key(ad->dpy, ad->xwin, KEY_BACK, TOP_POSITION_GRAB);

	__register_key_handler(ad);
}

static void __ungrab_key(void *data)
{
	struct appdata *ad = (struct appdata *)data;

	_unregister_key_handler(ad);

	_D("Ungrab home / back key.");

	utilx_ungrab_key(ad->dpy, ad->xwin, KEY_HOME);

	utilx_ungrab_key(ad->dpy, ad->xwin, KEY_BACK);
}

static int __lang_changed(void *dummy, void *data)
{
	char* lang = NULL;

	lang = vconf_get_str(VCONFKEY_LANGSET);
	if(lang) {
		LOGD("lang : %s", lang);
		elm_language_set((const char*)lang);
		free(lang);
	} else {
		LOGW("language get error");
	}

	popup_update_by_lang_changed_sig(data);

	return 0;
}

static Eina_Bool __window_destory_event_cb(void *data, int type, void *event)
{
	struct appdata *ad = data;
	Ecore_X_Event_Window_Hide *ev;

	if((!event) || (!data)) {
		_E("input param of transient cb is null");
		return ECORE_CALLBACK_CANCEL;
	}

	ev = event;

	if(ev->win == ad->caller_xwin) {
		_D("caller window(%d) is destroyed. app-selector will be terminated after window lower", ad->caller_xwin);
		elm_win_lower(ad->win);
		elm_exit();
	} else {
		_D("__window_destory_event_cb : %d", ev->win);
	}

	return ECORE_CALLBACK_RENEW;
}

static int __aul_sub_app_cb(void *data)
{
	struct appdata *ad = data;

	_D("aul sub app cb. caller request terminate event");
	elm_win_lower(ad->win);
	elm_exit();

	return 0;
}

/* A app --> Popup --> B app (B app transient for App) : B app will be located between A app and Popup */
static void __transient_for_set_app(void *data)
{
	struct appdata *ad = data;
	int num = 0,i = 0,pid = 0;
	Ecore_X_Window *wins;
	Ecore_X_Window childwin;
	Ecore_X_Window_Attributes attrs;

	if(ad->caller_pid <= 0) {
		_E("get CALLER PID ERROR for transient app");
		return;
	} else {
		_SD("CALLER PID(%d)", ad->caller_pid);
	}

	if (is_reset == 1) {
		if(ecore_x_icccm_transient_for_get(ad->xwin)){
			ecore_x_icccm_transient_for_unset(ad->xwin);
		}
	}


	/* Below logic is for covering mini app(floating window) case */
	wins = ecore_x_window_children_get(ecore_x_window_root_first_get(), &num);
	Ecore_X_Atom E_ILLUME_USER_CREATED_WINDOW = ecore_x_atom_get("_E_USER_CREATED_WINDOW");

	if (wins) {
		/* Searching top window */
		for (i = 0; i < num; i++) {
			int ret;
			ret = ecore_x_window_prop_window_get(wins[i], E_ILLUME_USER_CREATED_WINDOW, &childwin, 1);
			if((ret < 1) || (!childwin))
				continue;

			ecore_x_window_attributes_get(childwin, &attrs);
			if ( (attrs.visible == 0) || (attrs.viewable == 0) )
				continue;

			//char *wname = NULL, *wclass = NULL,*wmname= NULL;
			//ecore_x_icccm_name_class_get(childwin, &wname, &wclass);
			//ecore_x_netwm_name_get(childwin, &wmname);
			ecore_x_netwm_pid_get(childwin, &pid);
			if(ad->caller_pid == pid) {
				_D("caller xid : %d", childwin);

				ad->caller_xwin = childwin;

				aul_set_subapp(__aul_sub_app_cb, data);
				ecore_x_icccm_transient_for_set(ad->xwin, childwin);
				ecore_x_window_client_manage(childwin);
				ecore_event_handler_add(ECORE_X_EVENT_WINDOW_DESTROY, __window_destory_event_cb, ad);
				free(wins);
				return;
			}
		}
		free(wins);
	} else {
		_E("x window children get error");
	}
}

void _transient_for_unset_app(void *data)
{
	struct appdata *ad = data;

	/* transient for unset */
	if(ecore_x_icccm_transient_for_get(ad->xwin)){
		_D("unset transient");
		ecore_x_icccm_transient_for_unset(ad->xwin);
	}
}

static int __app_create(void *data)
{
	struct appdata *ad = (struct appdata *)data;
	Evas_Object *win;
	Evas_Object *ly;
	int r;

	/* create window */
	win = __create_win(PACKAGE);
	if (win == NULL)
		return -1;
	ad->win = win;

	/* Base Layout */
	ly = __create_layout_main(ad->win);
	if (!ly)
		return -1;

	ad->layout = ly;

	edje_object_signal_emit(_EDJ(ly), "elm,bg,show,transparent", "elm");

	ad->dpy = ecore_x_display_get();
	ad->xwin = elm_win_xwindow_get(win);
#ifdef ROTATE_USING_X_CLIENT
	ad->rotate = ecore_event_handler_add(ECORE_X_EVENT_CLIENT_MESSAGE,
							 __rotate, (void *)ad);
	__as_rotate(ad->dpy, ad->xwin, win, ad);
#else
	ecore_x_icccm_name_class_set(ad->xwin, WIN_PROP_NAME, WIN_PROP_NAME);
	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int*)(&rots), 4);
	} else {
		_E("win rotation no supported");
	}
#endif

	evas_object_show(win);

	/* init internationalization */
	r = appcore_set_i18n(PACKAGE, LOCALEDIR);
	if (r)
		return -1;

	__grab_key(ad);

	appcore_set_event_callback(APPCORE_EVENT_LANG_CHANGE, __lang_changed, ad);

	return 0;
}

static int __app_init_with_bundle(void *data)
{
	struct appdata *ad = data;

	load_app_select_popup(ad);

	return 0;
}

static int __app_terminate(void *data)
{
	struct appdata *ad = data;
	const char *val_noti = NULL;
	const char *val_launch = NULL;
	int ret;

	_D("app_terminate");

	if(exit_timer) {
		ecore_timer_del(exit_timer);
		exit_timer = NULL;
	}

	if(ecore_x_handler) {
		ecore_event_handler_del(ecore_x_handler);
		ecore_x_handler = NULL;
	}

	if(ad->rotate) {
		ecore_event_handler_del(ad->rotate);
		ad->rotate = NULL;
	}

	_transient_for_unset_app(data);

	val_noti = bundle_get_val(ad->kb, "__APP_SVC_CALLER_NOTI__");
	val_launch = bundle_get_val(ad->kb, "__APP_SVC_START_INFO__");

	/* For native C++ app */
	if(val_noti) {
		/* Cancel case. START_INFO is set when app is launched successful */
		if(aul_app_is_running(val_noti))
		{
			if(!val_launch) {
				bundle_add(ad->kb, "__APP_SVC_START_INFO__", "c");
			}

			ret = aul_launch_app(val_noti, ad->kb);
			if(ret < AUL_R_OK) {
				_E("noti for natie app is failed(%d)", ret);
			}
		}
	/* For C app */
	} else {
		/* Cancel case. START_INFO is set when app is launched successful */
		if(!val_launch) {
			do {
				bundle *kb;
				int ret_val;
				const char *pid = NULL;
				char callee_pid[20]={0,};

				pid = bundle_get_val(ad->kb, AUL_K_CALLER_PID);
				if(pid == NULL) {
					_E("get CALLER PID ERROR");
					break;
				} else {
					_D("CALLER PID(%s)", pid);
				}

				kb = bundle_create();
				if(kb == NULL) {
					_E("bundle create error");
					break;
				}
				bundle_add(kb, AUL_K_SEND_RESULT, "1");
				bundle_add(kb, AUL_K_CALLER_PID, pid);

				snprintf(callee_pid,20,"%d",getpid());
				bundle_add(kb, AUL_K_CALLEE_PID, (const char*)callee_pid);

				ret_val = app_send_cmd_with_noreply(-2, 7, kb); /* 7 is APP_CANCEL */
				if(ret_val != AUL_R_OK) {
					_E("app_send_cmd error(%d)", ret_val);
				}

				bundle_free(kb);
			} while (0);
		}
	}

	clear_list_info(ad);

	if (ad->layout) {
		evas_object_del(ad->layout);
		ad->layout = NULL;
	}

	if (ad->popup) {
		evas_object_del(ad->popup);
		ad->popup = NULL;
	}

	if (ad->win) {
		evas_object_del(ad->win);
		ad->win = NULL;
	}

	if (ad->kb) {
		bundle_free(ad->kb);
		ad->kb = NULL;
	}

	__ungrab_key(ad);

	return 0;
}

static int __app_pause(void *data)
{
	_D("app_pause");

	if(enable_pause_terminate) {
		_D("pause_terminate is true. app will be terminated.");
		elm_exit();
	}

	return 0;
}

static int __app_resume(void *data)
{
	_D("app_resume");

	return 0;
}

static int __app_reset(bundle *b, void *data)
{
	struct appdata *ad = data;
	char *str = NULL;
	int ret = 0;

	if (ad->kb) {
		bundle_free(ad->kb);
		ad->kb = NULL;
	}

	ad->kb = bundle_dup(b);

	ad->window_id = (char *)bundle_get_val(ad->kb, "__APP_SVC_K_WIN_ID__");
	_D("window id is %s", ad->window_id);

	str = (char *)bundle_get_val(ad->kb, AUL_K_CALLER_PID);
	_D("caller pid is %s", str);

	if (str)
		ad->caller_pid = atoi(str);
	else
		ad->caller_pid = -1;

	ad->control_op = (char *)appsvc_get_operation(ad->kb);
	ad->control_uri = (char *)appsvc_get_uri(ad->kb);
	ad->control_mime = (char *)appsvc_get_mime(ad->kb);

	_D("control_op is %s", ad->control_op);
	_D("control_uri is %s", ad->control_uri);
	_D("control_mime is %s", ad->control_mime);

	ret = bundle_get_type(ad->kb, APP_SVC_K_SELECTOR_EXTRA_LIST);

	ad->extra_list = NULL;
	ad->extra_list_cnt = 0;

	if(ret != BUNDLE_TYPE_NONE) {
		if(ret & BUNDLE_TYPE_ARRAY) {
			ad->extra_list = (char**)appsvc_get_data_array(b, APP_SVC_K_SELECTOR_EXTRA_LIST , &(ad->extra_list_cnt));
			_D("extra list cnt : %d", ad->extra_list_cnt);
		} else {
			str = (char*)appsvc_get_data(b, APP_SVC_K_SELECTOR_EXTRA_LIST );
			if(str) {
				ad->extra_list = (char**)&str;
				ad->extra_list_cnt = 1;
			}
		}
	}

	if (!ad->control_op) {
		load_info_popup(ad, dgettext("sys_string", "IDS_COM_BODY_NO_APPLICATIONS_CAN_PERFORM_THIS_ACTION"));
		return 0;
	}

	__transient_for_set_app(ad);

	/*
	   If AUL_K_ARGV0 is not NULL, the situation is launching(fork & exec).
	   else the situation is being received a reset event(old relaunch evet)
	 */
	if (is_reset == 0) {
		__app_init_with_bundle(data);
		is_reset = 1;
		evas_object_show(ad->win);
	} else {
		update_app_list(data);	/*(reset event) */
		elm_win_activate(ad->win);
	}

	return 0;
}

EXPORT_API int main(int argc, char *argv[])
{
	struct appdata ad;
	struct appcore_ops ops = {
		.create = __app_create,
		.terminate = __app_terminate,
		.pause = __app_pause,
		.resume = __app_resume,
		.reset = __app_reset,
	};

	memset(&ad, 0x0, sizeof(struct appdata));
	ops.data = &ad;

	return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
}
