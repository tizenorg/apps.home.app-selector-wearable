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

#include <Ecore.h>
#include <Ecore_X.h>

#include <sys/shm.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XShm.h>

#include "app-selector.h"
#include "app-selector-util.h"


static pid_t __get_win_pid(Display *d, Window win);
static int __tm_get_win_with_pid(Display *d, Window * win, pid_t pid);
static Window __find_top_win(Window win);


static pid_t __get_win_pid(Display *d, Window win)
{
	int ret;
	pid_t pid;

	Atom a_pid;
	Atom a_type;
	int format;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char *prop_ret;

	if (!d)
		return -1;

	a_pid = XInternAtom(d, "_NET_WM_PID", True);
	if (a_pid == 0)
		return -1;

	ret = XGetWindowProperty(d, win, a_pid, 0, 1, False, XA_CARDINAL,
				 &a_type, &format, &nitems, &bytes_after,
				 &prop_ret);

	if (ret != 0 || prop_ret == NULL)
		return -1;

	pid = *(unsigned long *)prop_ret;

	XFree(prop_ret);
	return pid;
}

static int __tm_get_win_with_pid(Display *d, Window * win, pid_t pid)
{
	int ret;
	pid_t p;
	unsigned int n;
	Window root, parent, *child;

	p = __get_win_pid(d, *win);
	if (p == pid)
		return 1;

	ret = XQueryTree(d, *win, &root, &parent, &child, &n);
	if (ret) {
		int i;
		int found = 0;

		for (i = 0; i < n; i++) {
			found = __tm_get_win_with_pid(d, &child[i], pid);
			if (found) {
				*win = child[i];
				break;
			}
		}

		XFree(child);
		if (found)
			return 1;
	}
	return 0;
}

static Window __find_top_win(Window win)
{
	Window root, parent, *childw, return_win;
	unsigned int nchild;
	if (!XQueryTree(ecore_x_display_get(),
	    win, &root, &parent, &childw, &nchild)) {
/*              DBG_PRT("XQueryTree failed.\n");*/
		return win;
	}
	if (parent == root) {
/*              DBG_PRT("toplevel window: 0x%x \n", (unsigned int)win);*/
		return_win = win;
		XFree(childw);
		return return_win;
	} else {
/*              DBG_PRT("window: 0x%x \n",(unsigned int)win);*/
		XFree(childw);
		return __find_top_win(parent);
	}
}

int set_transient(Ecore_X_Display *display, Ecore_X_Window win,
		  Ecore_X_Window prev_win)
{
	int ret = 0;

	ret = XSetTransientForHint(display, win, prev_win);

	return ret;
}
