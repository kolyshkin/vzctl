/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef _SCRIPT_H_
#define _SCRIPT_H_

int read_script(const char *fname, char *include, char **buf);
int run_script(const char *f, char *argv[], char *envp[], int quiet);
int run_pre_script(int veid, char *script);
int mk_reboot_script();
int mk_quota_link();

#endif /* _SCRIPT_H_ */
