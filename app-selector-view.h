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

#ifndef __DEF_APP_SELECT_VIEW_H_
#define __DEF_APP_SELECT_VIEW_H_

#include <Elementary.h>
#include "app-selector.h"

void load_info_popup(struct appdata *ad, char *str);
void update_app_list(struct appdata *ad);
void load_app_select_popup(struct appdata *ad);
void clear_list_info(struct appdata *ad);
#endif				/* __DEF_APP_SELECT_VIEW_H__ */
