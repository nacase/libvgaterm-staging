/*
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include <Python.h>
#include <pygobject.h>
#include <pygtk/pygtk.h>
#include "../vgaterm/vgaterm.h"

extern void pyvgaterm_register_classes(PyObject * d);
/*
extern void pyvgaterm_add_constants(PyObject *module, const gchar *strip_prefix);
*/
extern PyMethodDef pyvgaterm_functions[];
extern DL_EXPORT(void) initvgaterm(void);
extern PyTypeObject PyVGATerm_Type;

DL_EXPORT(void)
initvgaterm(void)
{
	PyObject *m, *d;

	init_pygobject();
	init_pygtk();

	m = Py_InitModule("vgaterm", pyvgaterm_functions);
	d = PyModule_GetDict(m);

	pyvgaterm_register_classes(d);
	/* pyvgaterm_add_constants(m, "VGATERM_"); */

	if (PyErr_Occurred()) {
		Py_FatalError("can't initialise module vgaterm");
	}
}
