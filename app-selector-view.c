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

#include <aul.h>
#include <appsvc.h>
#include <efl_assist.h>
#include <pkgmgr-info.h>

#include "app-selector.h"
#include "app-selector-view.h"

#define PATH_LEN	256
#define NAME_LEN	256
#define MAX_MIME_STR_SIZE 256

extern int aul_forward_app(const char* pkgname, bundle *kb);

static Evas_Object *app_genlist;
static Elm_Genlist_Item_Class app_gic;

extern void _tasks_related_launching_app(void *data);
extern void _unregister_key_handler(void *data);
extern bool _is_caller_window_floating_state(void *data);
extern void _transient_for_unset_app(void *data);

static void __launch_selected_app(void *data)
{
	int ret=0;

	if (!data)
		return;

	struct _select_app_info *info = (struct _select_app_info *)data;

	_unregister_key_handler(info->ad);

#ifdef END_POPUP_AFTER_LAUNCH_RUNNING_APP
	bool running = false;
	ret = aul_app_is_running(info->app_id);
	if(ret) {
		_SD("app(%s)_is_running. app-selector will be destroyed immediately after app launch", info->app_id);
		running = true;
	}
#endif

	ret = aul_forward_app(info->app_id, info->ad->kb);
	if(ret < 0) {
		_SE("app(%s) launch error", info->app_id);
	} else {
		_SD("app(%s) launch ok", info->app_id);
		bundle_add(info->ad->kb, "__APP_SVC_START_INFO__", info->app_id);
	}


	if(ret == info->ad->caller_pid)
	{
		/* Issue case : A app --(transient)--> popup --> launch A app */
		_D("launch app is same as caller. unset transient");
		_transient_for_unset_app(info->ad);
	}

#ifdef END_POPUP_AFTER_LAUNCH_RUNNING_APP
	if(running)
		elm_exit();
#endif

	if(_is_caller_window_floating_state(info->ad)) {
		_D("caller window is floating mode. app-selector will be destroyed immediately after app launch");
		elm_exit();
	}
}

static void __clear_app_list(void *data)
{
	struct appdata *ad = data;
	Eina_List *l;
	struct _select_app_info *info;

	if (ad->app_list) {
		EINA_LIST_FOREACH(ad->app_list, l, info) {
			if (info != NULL) {
				free(info->pkg_name);
				free(info->app_name);
				free(info->app_id);
				free(info->app_path);
				free(info->app_icon_path);
				free(info);
				info = NULL;
			}
		}

		eina_list_free(ad->app_list);
		ad->app_list = NULL;
	}

	if (app_genlist) {
		elm_genlist_clear(app_genlist);
	}
}

static int __iterate(const char* pkg_name, void *data)
{
	struct appdata *ad = data;
	struct _select_app_info *info;
	int ret = 0;
	pkgmgrinfo_appinfo_h handle = NULL;
	char *str = NULL;

	ret = pkgmgrinfo_appinfo_get_appinfo(pkg_name, &handle);
	if (ret != PMINFO_R_OK) {
		return -1;
	}

	info = calloc(1, sizeof(struct _select_app_info));
	if (info == NULL) {
		_E("out of memory");
		if (pkgmgrinfo_appinfo_destroy_appinfo(handle) != PMINFO_R_OK)
			_D("destroy appinfo handle error");

		return -1;
	}

	ret = pkgmgrinfo_appinfo_get_pkgname(handle, &str);
	if ((ret == PMINFO_R_OK) && (str)) {
		info->pkg_name = strdup(str);
	}
	str = NULL;

	ret = pkgmgrinfo_appinfo_get_label(handle, &str);
	if ((ret == PMINFO_R_OK) && (str)) {
		info->app_name = strdup(str);
	}
	str = NULL;

	ret = pkgmgrinfo_appinfo_get_appid(handle, &str);
	if ((ret == PMINFO_R_OK) && (str)) {
		info->app_id = strdup(str);
	}
	str = NULL;

	ret = pkgmgrinfo_appinfo_get_exec(handle, &str);
	if ((ret == PMINFO_R_OK) && (str)) {
		info->app_path = strdup(str);
	}
	str = NULL;

	ret = pkgmgrinfo_appinfo_get_icon(handle, &str);
	if ((ret == PMINFO_R_OK) && (str)) {
		info->app_icon_path = strdup(str);
	}
	str = NULL;

	_SD("PKGNAME : %s, APPNAME : %s, ICONPATH : %s\n", info->pkg_name, info->app_name,
		       info->app_icon_path);

	info->ad = data;

	ad->app_list = eina_list_append(ad->app_list, info);

	ret = pkgmgrinfo_appinfo_destroy_appinfo(handle);
	if (ret != PMINFO_R_OK) {
		_D("destroy appinfo handle error(%d)", ret);
		return -1;
	}

	return 0;
}


static int __app_list_get_with_control(void *data)
{
	struct appdata *ad = data;
	bundle *kb;
	int i=0;

	for(i=0;i<ad->extra_list_cnt;i++) {
		_D("extra list(%d) : %s", i , ad->extra_list[i]);
		__iterate(ad->extra_list[i],data);
	}

	kb = bundle_create();
	if(kb == NULL) {
		_E("bundle creation fail\n");
		return -1;
	}

	if(ad->window_id){
		bundle_add(kb, "__APP_SVC_K_WIN_ID__", ad->window_id);
	}

	if(ad->control_op){
		appsvc_set_operation(kb, ad->control_op);
	}

	if(ad->control_uri){
		appsvc_set_uri(kb, ad->control_uri);
	}

	if(ad->control_mime){
		appsvc_set_mime(kb, ad->control_mime);
	}

	if( appsvc_get_list(kb, __iterate, ad) != 0 ){
		bundle_free(kb);
		return -1;
	}

	bundle_free(kb);

	return 0;
}

static void
__selected_cb(void *data, Evas_Object *obj, void *event_info)
{
	Elm_Object_Item *item = (Elm_Object_Item *) event_info;
	if (item) {
		_E("selected item will be launched");
		_tasks_related_launching_app(data);
		__launch_selected_app((void*)elm_object_item_data_get(item));
	} else {
		_E("selected item data is null");
	}	
}

static char *__list_text_get(void *data, Evas_Object * obj,
			   const char *part)
{
	struct _select_app_info *info = (struct _select_app_info *)data;

	if(!info) {
		_E("app info is null");
		return NULL;
	}

	if (!strcmp(part, "elm.text")) {
		return elm_entry_utf8_to_markup(info->app_name);
	}
	return NULL;
}

static Evas_Object *
__list_content_get(void *data, Evas_Object *obj, const char *part)
{
	struct _select_app_info *info = (struct _select_app_info *)data;
	char buf[PATH_LEN] = { 0, };

	Evas_Object *icon = NULL;

	if (!strcmp(part, "elm.icon")) {
		snprintf(buf, (size_t) PATH_LEN, "%s", info->app_icon_path);

		_SD("icon buf : %s", buf);

		icon = elm_image_add(obj);
		elm_image_file_set(icon, buf, NULL);
		evas_object_size_hint_aspect_set(icon, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
		//evas_object_size_hint_min_set(icon, 50 * elm_config_scale_get(), 50 * elm_config_scale_get());
		//evas_object_size_hint_max_set(icon, 50 * elm_config_scale_get(), 50 * elm_config_scale_get());
		evas_object_show(icon);

		return icon;
	}
	return NULL;
}

static int __app_genlist_item_append(struct appdata *ad)
{
	struct _select_app_info *info;
	Eina_List *l;
	int count = 0;

	if(!ad->app_list) {
		return 0;
	}

	app_gic.item_style = "1text";
	app_gic.func.text_get = __list_text_get;
	app_gic.func.content_get = __list_content_get;
	app_gic.func.state_get = NULL;
	app_gic.func.del = NULL;

	EINA_LIST_FOREACH(ad->app_list, l, info) {
		count++;
		info->it =
		    elm_genlist_item_append(app_genlist, &app_gic,
		    	(void *)info, NULL, ELM_GENLIST_ITEM_NONE, __selected_cb, ad);
	}

	return count;
}

static void _popup_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	_D("ea callback back for popup");
	evas_object_del(obj);
	elm_exit();
}

static void __load_app_list(struct appdata *ad)
{
	Evas_Object *popup, *layout;
	int cnt;

	ea_theme_changeable_ui_enabled_set(EINA_TRUE);

	popup = elm_popup_add(ad->win);
	evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	ea_object_event_callback_add(popup, EA_CALLBACK_BACK, _popup_back_cb, NULL);
	ad->popup = popup;

	layout = elm_layout_add(popup);
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	Evas_Object *genlist;
	genlist = elm_genlist_add(popup);
	elm_object_style_set(genlist, "popup");
	elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);
	elm_genlist_homogeneous_set(genlist, EINA_TRUE);

	app_genlist = genlist;

	cnt = __app_genlist_item_append(ad);

	if(cnt > 4) {
		elm_layout_file_set(layout, EDJ_NAME, "app-selector/popup/layout/full");
	} else {
		elm_layout_file_set(layout, EDJ_NAME, "app-selector/popup/layout/default");
	}

	elm_object_part_content_set(layout, "elm.swallow.content", genlist);
	elm_object_content_set(popup, layout);
	evas_object_show(genlist);

	evas_object_show(popup);
}

static Eina_Bool __unload_info_popup(void *data)
{
	elm_exit();

	return ECORE_CALLBACK_CANCEL;
}

void load_info_popup(struct appdata *ad, char *str)
{
	Evas_Object *eo = NULL;
	eo = elm_popup_add(ad->win);
	evas_object_size_hint_weight_set(eo, EVAS_HINT_EXPAND,
					 EVAS_HINT_EXPAND);
	elm_object_text_set(eo, str);

	ecore_timer_add(2.0, __unload_info_popup, eo);
	evas_object_show(eo);
}

void update_app_list(struct appdata *ad)
{
	int ret = -1;

	__clear_app_list(ad);

	ret = __app_list_get_with_control(ad);
	if (ret == -1) {
		_E("app list get failed\n");
		return;
	}

	if (eina_list_count(ad->app_list) < 1) {
		_E("no matched list");
		return;
	} else {
		__app_genlist_item_append(ad);
	}

	return;
}

void load_app_select_popup(struct appdata *ad)
{
	int ret = -1;

	ret = __app_list_get_with_control(ad);
	if (ret == -1) {
		_E("app list get fail\n");
		return;
	}
	else
		__load_app_list(ad);
}

void clear_list_info(struct appdata *ad)
{
	_D("clear_list_info");

	__clear_app_list(ad);

	if (app_genlist) {
		elm_genlist_clear(app_genlist);
		evas_object_del(app_genlist);
		app_genlist = NULL;
	}
}

void popup_update_by_lang_changed_sig(struct appdata *ad)
{
	_D("update app list by lang changed");
	update_app_list(ad);
}
