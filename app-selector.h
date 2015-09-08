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

#ifndef __APP_COMMON_H__
#define __APP_COMMON_H__

#include <Elementary.h>
#include <bundle.h>
#include <dlog.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>


#undef LOG_TAG
#define LOG_TAG "APP-SELECTOR"

#define _E(fmt, arg...) LOGE("[%s,%d] "fmt,__FUNCTION__,__LINE__,##arg)
#define _D(fmt, arg...) LOGD("[%s,%d] "fmt,__FUNCTION__,__LINE__,##arg)

#define _SE(fmt, arg...) SECURE_LOGE(fmt,##arg)
#define _SD(fmt, arg...) SECURE_LOGD(fmt,##arg)


#if !defined(PACKAGE)
#  define PACKAGE "app-selector"
#endif

#if !defined(LOCALEDIR)
#  define LOCALEDIR "/usr/apps/org.tizen.app-selector/res/locale"
#endif

#define SYSSTRING "sys_string"

#define ICON_PATH PREFIX"/share/icon"

#define NAME_BUF_LEN (128)
#define _EDJ(x)	elm_layout_edje_get(x)

#if !defined(_)
#define _(msgid) gettext(msgid)
#endif

#if !defined(N_)
#define N_(msgid) gettext_noop(msgid)
#endif

struct appdata {
	Evas_Object *win;
	Evas_Object *layout;

	Ecore_Event_Handler *key_up;
	Ecore_Event_Handler *rotate;

	Evas_Object *popup;

	Eina_List *app_list;

	Display *dpy;
	Ecore_X_Window xwin;

	bundle *kb;
	int caller_pid;
	Ecore_X_Window caller_xwin;

	char *window_id;
	char *control_op;
	char *control_mime;
	char *control_uri;
	char **extra_list;
	int extra_list_cnt;
	/* add more variables here */
};

struct _select_app_info {
	char *pkg_name;
	char *app_name;
	char *app_id;
	char *app_path;
	char *app_icon_path;
	struct appdata *ad;
	Elm_Object_Item *it;
};

#endif				/* __APP_COMMON_H__ */
