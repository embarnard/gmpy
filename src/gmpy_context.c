/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * gmpy_context.c                                                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Python interface to the GMP or MPIR, MPFR, and MPC multiple precision   *
 * libraries.                                                              *
 *                                                                         *
 * Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,               *
 *           2008, 2009 Alex Martelli                                      *
 *                                                                         *
 * Copyright 2008, 2009, 2010, 2011, 2012, 2013 Case Van Horsen            *
 *                                                                         *
 * This file is part of GMPY2.                                             *
 *                                                                         *
 * GMPY2 is free software: you can redistribute it and/or modify it under  *
 * the terms of the GNU Lesser General Public License as published by the  *
 * Free Software Foundation, either version 3 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * GMPY2 is distributed in the hope that it will be useful, but WITHOUT    *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or   *
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public    *
 * License for more details.                                               *
 *                                                                         *
 * You should have received a copy of the GNU Lesser General Public        *
 * License along with GMPY2; if not, see <http://www.gnu.org/licenses/>    *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* A GMPyContextObject contains an instance of the C struct gmpy_context
 * and a PyObject* used to reference the enclosing instance when used as a
 * context manager in Python.
 *
 * gmpy2 uses a global pointer "context" to refer to the active
 * GMPyContextObject.
 *
 * WARNING: The context manager is not thread-safe. This may be fixed in a
 *          future version.
 */

/* Create and delete Context objects. */

static PyObject *
GMPyContext_new(void)
{
    GMPyContextObject *result;

    if ((result = PyObject_New(GMPyContextObject, &GMPyContext_Type))) {
        result->ctx.mpfr_prec = DBL_MANT_DIG;
        result->ctx.mpfr_round = MPFR_RNDN;
        result->ctx.emax = MPFR_EMAX_DEFAULT;
        result->ctx.emin = MPFR_EMIN_DEFAULT;
        result->ctx.subnormalize = 0;
        result->ctx.underflow = 0;
        result->ctx.overflow = 0;
        result->ctx.inexact = 0;
        result->ctx.invalid = 0;
        result->ctx.erange = 0;
        result->ctx.divzero = 0;
        result->ctx.trap_underflow = 0;
        result->ctx.trap_overflow = 0;
        result->ctx.trap_inexact = 0;
        result->ctx.trap_invalid = 0;
        result->ctx.trap_erange = 0;
        result->ctx.trap_divzero = 0;
        result->ctx.trap_expbound = 0;
        result->ctx.real_prec = -1;
        result->ctx.imag_prec = -1;
        result->ctx.real_round = -1;
        result->ctx.imag_round = -1;
        result->ctx.allow_complex = 0;
        result->ctx.readonly = 0;

#ifndef WITHOUT_THREADS
        result->tstate = NULL;
#endif

    }
    return (PyObject*)result;
};

static void
GMPyContext_dealloc(GMPyContextObject *self)
{
    PyObject_Del(self);
};

/* Support for global and thread local contexts. */

/* Doc-string, alternate definitions below. */

PyDoc_STRVAR(doc_set_context,
"set_context(context)\n\n"
"Activate a context object controlling MPFR and MPC arithmetic.\n");

#ifdef WITHOUT_THREADS

/* Return a borrowed reference to current context. */

static GMPyContextObject *
GMPyContext_current(void)
{
    return module_context;
}

static PyObject *
GMPyContext_set_context(PyObject *self, PyObject *other)
{
    if (!GMPyContext_Check(other)) {
        VALUE_ERROR("set_context() requires a context argument");
        return NULL;
    }

    Py_DECREF((PyObject*)module_context);
    if (((GMPyContextObject*)other)->ctx.readonly) {
        module_context = (GMPyContextObject*)GMPyContext_context_copy(other, NULL);
    }
    else {
        Py_INCREF((PyObject*)other);
        module_context = (GMPyContextObject*)other;
    }
    mpfr_set_emin(module_context->ctx.emin);
    mpfr_set_emax(module_context->ctx.emax);
    Py_RETURN_NONE;
}

#else

/* Begin support for thread local contexts. */

/* Get the context from the thread state dictionary. */
static GMPyContextObject *
current_context_from_dict(void)
{
    PyObject *dict;
    PyObject *tl_context;
    PyThreadState *tstate;

    dict = PyThreadState_GetDict();
    if (dict == NULL) {
        RUNTIME_ERROR("cannot get thread state");
        return NULL;
    }

#ifdef PY3
    tl_context = PyDict_GetItemWithError(dict, tls_context_key);
#else
    tl_context = PyDict_GetItem(dict, tls_context_key);
#endif
    if (!tl_context) {
#ifdef PY3
        if (PyErr_Occurred()) {
            return NULL;
        }
#endif

        /* Set up a new thread local context. */
        tl_context = GMPyContext_new();
        if (tl_context == NULL) {
            return NULL;
        }

        if (PyDict_SetItem(dict, tls_context_key, tl_context) < 0) {
            Py_DECREF(tl_context);
            return NULL;
        }
        Py_DECREF(tl_context);
    }

    /* Cache the context of the current thread, assuming that it
     * will be accessed several times before a thread switch. */
    tstate = PyThreadState_GET();
    if (tstate) {
        cached_context = (GMPyContextObject*)tl_context;
        cached_context->tstate = tstate;
    }

    /* Borrowed reference with refcount==1 */
    return (GMPyContextObject*)tl_context;
}

/* Return borrowed reference to thread local context. */
static GMPyContextObject *
GMPyContext_current(void)
{
    PyThreadState *tstate;

    tstate = PyThreadState_GET();
    if (cached_context && cached_context->tstate == tstate) {
        return (GMPyContextObject*)cached_context;
    }

    return current_context_from_dict();
}

/* Set the thread local context to a new context, decrement old reference */
static PyObject *
GMPyContext_set_context(PyObject *self, PyObject *other)
{
    PyObject *dict;
    PyThreadState *tstate;

    if (!GMPyContext_Check(other)) {
        VALUE_ERROR("set_context() requires a context argument");
        return NULL;
    }

    dict = PyThreadState_GetDict();
    if (dict == NULL) {
        RUNTIME_ERROR("cannot get thread state");
        return NULL;
    }

    /* Make a copy if it is readonly. */
    if (((GMPyContextObject*)other)->ctx.readonly) {
        other = GMPyContext_context_copy(other, NULL);
        if (!other) {
            return NULL;
        }
    }
    else {
        Py_INCREF(other);
    }

    if (PyDict_SetItem(dict, tls_context_key, other) < 0) {
        Py_DECREF(other);
        return NULL;
    }

    mpfr_set_emin(((GMPyContextObject*)other)->ctx.emin);
    mpfr_set_emax(((GMPyContextObject*)other)->ctx.emax);

    /* Cache the context of the current thread, assuming that it
     * will be accessed several times before a thread switch. */
    cached_context = NULL;
    tstate = PyThreadState_GET();
    if (tstate) {
        cached_context = (GMPyContextObject*)other;
        cached_context->tstate = tstate;
    }

    Py_DECREF(other);
    Py_RETURN_NONE;
}
#endif

PyDoc_STRVAR(doc_context_ieee,
"ieee(bitwidth) -> context\n\n"
"Return a new context corresponding to a standard IEEE floating point\n"
"format. The currently supported precisions are 32, 64, and 128 bits.");

static PyObject *
GMPyContext_ieee(PyObject *self, PyObject *other)
{
    long bitwidth;
    GMPyContextObject *result;

    bitwidth = PyIntOrLong_AsLong(other);
    if (bitwidth == -1 && PyErr_Occurred()) {
        TYPE_ERROR("ieee() requires 'int' argument");
        return NULL;
    }

    if (bitwidth == 32) {
        result = (GMPyContextObject*)GMPyContext_new();
        if (result) {
            result->ctx.subnormalize = 1;
            result->ctx.mpfr_prec = 24;
            result->ctx.emax = 128;
            result->ctx.emin = -148;
        }
        return (PyObject*)result;
    }
    else if (bitwidth == 64) {
        result = (GMPyContextObject*)GMPyContext_new();
        if (result) {
            result->ctx.subnormalize = 1;
            result->ctx.mpfr_prec = 53;
            result->ctx.emax = 1024;
            result->ctx.emin = -1073;
        }
        return (PyObject*)result;
    }
    else if (bitwidth == 128) {
        result = (GMPyContextObject*)GMPyContext_new();
        if (result) {
            result->ctx.subnormalize = 1;
            result->ctx.mpfr_prec = 113;
            result->ctx.emax = 16384;
            result->ctx.emin = -16493;
        }
        return (PyObject*)result;
    }
    else {
        VALUE_ERROR("bitwidth must be 32, 64, or 128");
        return NULL;
    }
}

/* Create and delete ContextManager objects. */

static PyObject *
GMPyContextManager_new(void)
{
    return (PyObject*)PyObject_New(GMPyContextManagerObject,
                                   &GMPyContextManager_Type);
};

static void
GMPyContextManager_dealloc(GMPyContextManagerObject *self)
{
    Py_DECREF(self->new_context);
    Py_DECREF(self->old_context);
    PyObject_Del(self);
};

/* Helper function to convert to convert a rounding mode to a string. */

static PyObject *
_round_to_name(int val)
{
    if (val == MPFR_RNDN) return Py2or3String_FromString("RoundToNearest");
    if (val == MPFR_RNDZ) return Py2or3String_FromString("RoundToZero");
    if (val == MPFR_RNDU) return Py2or3String_FromString("RoundUp");
    if (val == MPFR_RNDD) return Py2or3String_FromString("RoundDown");
    if (val == MPFR_RNDA) return Py2or3String_FromString("RoundAwayZero");
    if (val == GMPY_DEFAULT) return Py2or3String_FromString("Default");
    return NULL;
};

static PyObject *
GMPyContext_repr(GMPyContextObject *self)
{
    PyObject *format;
    PyObject *tuple;
    PyObject *result = NULL;
    int i = 0;

    tuple = PyTuple_New(23);
    if (!tuple) return NULL;

    format = Py2or3String_FromString(
            "context(precision=%s, real_prec=%s, imag_prec=%s,\n"
            "        round=%s, real_round=%s, imag_round=%s,\n"
            "        emax=%s, emin=%s,\n"
            "        subnormalize=%s,\n"
            "        trap_underflow=%s, underflow=%s,\n"
            "        trap_overflow=%s, overflow=%s,\n"
            "        trap_inexact=%s, inexact=%s,\n"
            "        trap_invalid=%s, invalid=%s,\n"
            "        trap_erange=%s, erange=%s,\n"
            "        trap_divzero=%s, divzero=%s,\n"
            "        trap_expbound=%s,\n"
            "        allow_complex=%s)"
            );
    if (!format) {
        Py_DECREF(tuple);
        return NULL;
    }

    PyTuple_SET_ITEM(tuple, i++, PyIntOrLong_FromLong(self->ctx.mpfr_prec));
    if (self->ctx.real_prec == GMPY_DEFAULT)
        PyTuple_SET_ITEM(tuple, i++, Py2or3String_FromString("Default"));
    else
        PyTuple_SET_ITEM(tuple, i++, PyIntOrLong_FromLong(self->ctx.real_prec));
    if (self->ctx.imag_prec == GMPY_DEFAULT)
        PyTuple_SET_ITEM(tuple, i++, Py2or3String_FromString("Default"));
    else
        PyTuple_SET_ITEM(tuple, i++, PyIntOrLong_FromLong(self->ctx.imag_prec));
    PyTuple_SET_ITEM(tuple, i++, _round_to_name(self->ctx.mpfr_round));
    PyTuple_SET_ITEM(tuple, i++, _round_to_name(self->ctx.real_round));
    PyTuple_SET_ITEM(tuple, i++, _round_to_name(self->ctx.imag_round));
    PyTuple_SET_ITEM(tuple, i++, PyIntOrLong_FromLong(self->ctx.emax));
    PyTuple_SET_ITEM(tuple, i++, PyIntOrLong_FromLong(self->ctx.emin));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.subnormalize));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.trap_underflow));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.underflow));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.trap_overflow));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.overflow));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.trap_inexact));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.inexact));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.trap_invalid));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.invalid));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.trap_erange));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.erange));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.trap_divzero));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.divzero));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.trap_expbound));
    PyTuple_SET_ITEM(tuple, i++, PyBool_FromLong(self->ctx.allow_complex));

    if (!PyErr_Occurred())
        result = Py2or3String_Format(format, tuple);
    else
        SYSTEM_ERROR("internal error in GMPyContext_repr");

    Py_DECREF(format);
    Py_DECREF(tuple);
    return result;
};

static PyObject *
GMPyContextManager_repr(GMPyContextManagerObject *self)
{
    return Py_BuildValue("s", "<gmpy2.ContextManagerObject>");
}

PyDoc_STRVAR(doc_get_context,
"get_context() -> gmpy2 context\n\n"
"Return a reference to the current context.");

static PyObject *
GMPyContext_get_context(PyObject *self, PyObject *args)
{
    GMPyContextObject *context;

    CURRENT_CONTEXT(context);
    Py_XINCREF((PyObject*)context);
    return (PyObject*)context;
}

PyDoc_STRVAR(doc_context_copy,
"context.copy() -> gmpy2 context\n\n"
"Return a copy of a context.");

static PyObject *
GMPyContext_context_copy(PyObject *self, PyObject *other)
{
    GMPyContextObject *result;

    result = (GMPyContextObject*)GMPyContext_new();
    result->ctx = ((GMPyContextObject*)self)->ctx;
    /* If a copy is made from a readonly template, it should no longer be
     * considered readonly. */
    result->ctx.readonly = 0;
    return (PyObject*)result;
}

PyDoc_STRVAR(doc_local_context,
"local_context([context[,keywords]]) -> context manager\n\n"
"Create a context manager object that will restore the current context\n"
"when the 'with ...' block terminates. The temporary context for the\n"
"'with ...' block is based on the current context if no context is\n"
"specified. Keyword arguments are supported and will modify the\n"
"temporary new context.");

static PyObject *
GMPyContext_local_context(PyObject *self, PyObject *args, PyObject *kwargs)
{
    GMPyContextManagerObject *result;
    PyObject *local_args = args;
    int arg_context = 0;
    GMPyContextObject *context, *temp;

    CURRENT_CONTEXT(context);

    static char *kwlist[] = {
        "precision", "real_prec", "imag_prec", "round",
        "real_round", "imag_round", "emax", "emin", "subnormalize",
        "trap_underflow", "trap_overflow", "trap_inexact",
        "trap_invalid", "trap_erange", "trap_divzero",
        "trap_expbound", "allow_complex", NULL };

    if (PyTuple_GET_SIZE(args) == 1 && GMPyContext_Check(PyTuple_GET_ITEM(args, 0))) {
        arg_context = 1;
        if (!(local_args = PyTuple_New(0)))
            return NULL;
    }
    else if (PyTuple_GET_SIZE(args)) {
        VALUE_ERROR("local_context() only supports [context[,keyword]] arguments");
        return NULL;
    }

    if (!(result = (GMPyContextManagerObject*)GMPyContextManager_new()))
        return NULL;

    if (arg_context) {
        temp = (GMPyContextObject*)PyTuple_GET_ITEM(args, 0);
        if (temp->ctx.readonly) {
            result->new_context = (GMPyContextObject*)GMPyContext_context_copy( \
                                                (PyObject*)temp, NULL);
        }
        else {
            result->new_context = (GMPyContextObject*)PyTuple_GET_ITEM(args, 0);
            Py_INCREF((PyObject*)(result->new_context));
        }
    }
    else {
        result->new_context = context;
        Py_INCREF((PyObject*)(result->new_context));
    }

    result->old_context = (GMPyContextObject*)GMPyContext_context_copy( \
                                                (PyObject*)context, NULL);
    if (!(result->old_context)) {
        Py_DECREF((PyObject*)result);
        return NULL;
    }

    if (!(PyArg_ParseTupleAndKeywords(local_args, kwargs,
            "|llliiilliiiiiiiii", kwlist,
            &result->new_context->ctx.mpfr_prec,
            &result->new_context->ctx.real_prec,
            &result->new_context->ctx.imag_prec,
            &result->new_context->ctx.mpfr_round,
            &result->new_context->ctx.real_round,
            &result->new_context->ctx.imag_round,
            &result->new_context->ctx.emax,
            &result->new_context->ctx.emin,
            &result->new_context->ctx.subnormalize,
            &result->new_context->ctx.trap_underflow,
            &result->new_context->ctx.trap_overflow,
            &result->new_context->ctx.trap_inexact,
            &result->new_context->ctx.trap_invalid,
            &result->new_context->ctx.trap_erange,
            &result->new_context->ctx.trap_divzero,
            &result->new_context->ctx.trap_expbound,
            &result->new_context->ctx.allow_complex))) {
        VALUE_ERROR("invalid keyword arguments in local_context()");
        goto error;
    }

    /* Sanity check for values. */
    if (result->new_context->ctx.mpfr_prec < MPFR_PREC_MIN ||
        result->new_context->ctx.mpfr_prec > MPFR_PREC_MAX) {
        VALUE_ERROR("invalid value for precision");
        goto error;
    }

    if (!(result->new_context->ctx.real_prec == GMPY_DEFAULT ||
        (result->new_context->ctx.real_prec >= MPFR_PREC_MIN &&
        result->new_context->ctx.real_prec <= MPFR_PREC_MAX))) {
        VALUE_ERROR("invalid value for real_prec");
        goto error;
    }
    if (!(result->new_context->ctx.imag_prec == GMPY_DEFAULT ||
        (result->new_context->ctx.imag_prec >= MPFR_PREC_MIN &&
        result->new_context->ctx.imag_prec <= MPFR_PREC_MAX))) {
        VALUE_ERROR("invalid value for imag_prec");
        goto error;
    }

    if (!(result->new_context->ctx.mpfr_round == MPFR_RNDN ||
        result->new_context->ctx.mpfr_round == MPFR_RNDZ ||
        result->new_context->ctx.mpfr_round == MPFR_RNDU ||
        result->new_context->ctx.mpfr_round == MPFR_RNDD ||
        result->new_context->ctx.mpfr_round == MPFR_RNDA)) {
        VALUE_ERROR("invalid value for round");
        goto error;
    }

    if (result->new_context->ctx.mpfr_round == MPFR_RNDA) {
        /* Since RNDA is not supported for MPC, set the MPC rounding modes
         * to MPFR_RNDN.
         */
        result->new_context->ctx.real_round = MPFR_RNDN;
        result->new_context->ctx.imag_round = MPFR_RNDN;
    }
    if (!(result->new_context->ctx.real_round == MPFR_RNDN ||
        result->new_context->ctx.real_round == MPFR_RNDZ ||
        result->new_context->ctx.real_round == MPFR_RNDU ||
        result->new_context->ctx.real_round == MPFR_RNDD ||
        result->new_context->ctx.real_round == GMPY_DEFAULT)) {
        VALUE_ERROR("invalid value for real_round");
        goto error;
    }
    if (!(result->new_context->ctx.imag_round == MPFR_RNDN ||
        result->new_context->ctx.imag_round == MPFR_RNDZ ||
        result->new_context->ctx.imag_round == MPFR_RNDU ||
        result->new_context->ctx.imag_round == MPFR_RNDD ||
        result->new_context->ctx.imag_round == GMPY_DEFAULT)) {
        VALUE_ERROR("invalid value for imag_round");
        goto error;
    }

    if (!(result->new_context->ctx.emin < 0 && result->new_context->ctx.emax > 0)) {
        VALUE_ERROR("invalid values for emin and/or emax");
        goto error;
    }

    if (mpfr_set_emin(result->new_context->ctx.emin)) {
        VALUE_ERROR("invalid value for emin");
        goto error;
    }
    if (mpfr_set_emax(result->new_context->ctx.emax)) {
        VALUE_ERROR("invalid value for emax");
        goto error;
    }

    if (arg_context) {
        Py_DECREF(local_args);
    }
    return (PyObject*)result;

  error:
    if (arg_context) {
        Py_DECREF(local_args);
    }
    Py_DECREF((PyObject*)result);
    return NULL;
}

PyDoc_STRVAR(doc_context,
"context() -> context manager\n\n"
"Return a new context for controlling MPFR and MPC arithmetic. To load\n"
"the new context, use set_context(). Options can only be specified as\n"
"keyword arguments. \n\n"
"    precision:      precision, in bits, of an MPFR result\n"
"    real_prec:      precision, in bits, of Re(MPC)\n"
"                      -1 implies use mpfr_prec\n"
"    imag_prec:      precision, in bits, of Im(MPC)\n"
"                      -1 implies use real_prec\n"
"    round:          rounding mode for MPFR\n"
"    real_round:     rounding mode for Re(MPC)\n"
"                      -1 implies use mpfr_round\n"
"    imag_round:     rounding mode for Im(MPC)\n"
"                      -1 implies use real_round\n"
"    e_max:          maximum allowed exponent\n"
"    e_min:          minimum allowed exponent\n"
"    subnormalize:   if True, subnormalized results can be returned\n"
"    trap_underflow: if True, raise exception for underflow\n"
"                    if False, set underflow flag\n"
"    trap_overflow:  if True, raise exception for overflow\n"
"                    if False, set overflow flag and return Inf or -Inf\n"
"    trap_inexact:   if True, raise exception for inexact result\n"
"                    if False, set inexact flag\n"
"    trap_invalid:   if True, raise exception for invalid operation\n"
"                    if False, set invalid flag and return NaN\n"
"    trap_erange:    if True, raise exception for range error\n"
"                    if False, set erange flag\n"
"    trap_divzero:   if True, raise exception for division by zero\n"
"                    if False, set divzero flag and return Inf or -Inf\n"
"    trap_expbound:  if True, raise exception when mpfr/mpc exponent\n"
"                        no longer valid in current context\n"
"                    if False, mpfr/mpc with exponent out-of-bounds\n"
"                        will be coerced to either 0 or Infinity\n"
"    allow_complex:  if True, allow mpfr functions to return mpc\n"
"                    if False, mpfr functions cannot return an mpc\n");

static PyObject *
GMPyContext_context(PyObject *self, PyObject *args, PyObject *kwargs)
{
    GMPyContextObject *result;

    static char *kwlist[] = {
        "precision", "real_prec", "imag_prec", "round",
        "real_round", "imag_round", "emax", "emin", "subnormalize",
        "trap_underflow", "trap_overflow", "trap_inexact",
        "trap_invalid", "trap_erange", "trap_divzero", "trap_expbound",
        "allow_complex", NULL };

    if (PyTuple_GET_SIZE(args)) {
        VALUE_ERROR("context() only supports keyword arguments");
        return NULL;
    }

    if (!(result = (GMPyContextObject*)GMPyContext_new()))
        return NULL;

    if (!(PyArg_ParseTupleAndKeywords(args, kwargs,
            "|llliiilliiiiiiiii", kwlist,
            &result->ctx.mpfr_prec,
            &result->ctx.real_prec,
            &result->ctx.imag_prec,
            &result->ctx.mpfr_round,
            &result->ctx.real_round,
            &result->ctx.imag_round,
            &result->ctx.emax,
            &result->ctx.emin,
            &result->ctx.subnormalize,
            &result->ctx.trap_underflow,
            &result->ctx.trap_overflow,
            &result->ctx.trap_inexact,
            &result->ctx.trap_invalid,
            &result->ctx.trap_erange,
            &result->ctx.trap_divzero,
            &result->ctx.trap_expbound,
            &result->ctx.allow_complex))) {
        VALUE_ERROR("invalid keyword arguments in context()");
        return NULL;
    }

    /* Sanity check for values. */
    if (result->ctx.mpfr_prec < MPFR_PREC_MIN ||
        result->ctx.mpfr_prec > MPFR_PREC_MAX) {
        Py_DECREF((PyObject*)result);
        VALUE_ERROR("invalid value for precision");
        return NULL;
    }

    if (!(result->ctx.real_prec == GMPY_DEFAULT ||
        (result->ctx.real_prec >= MPFR_PREC_MIN &&
        result->ctx.real_prec <= MPFR_PREC_MAX))) {
        Py_DECREF((PyObject*)result);
        VALUE_ERROR("invalid value for real_prec");
        return NULL;
    }
    if (!(result->ctx.imag_prec == GMPY_DEFAULT ||
        (result->ctx.imag_prec >= MPFR_PREC_MIN &&
        result->ctx.imag_prec <= MPFR_PREC_MAX))) {
        Py_DECREF((PyObject*)result);
        VALUE_ERROR("invalid value for imag_prec");
        return NULL;
    }

    if (!(result->ctx.mpfr_round == MPFR_RNDN ||
        result->ctx.mpfr_round == MPFR_RNDZ ||
        result->ctx.mpfr_round == MPFR_RNDU ||
        result->ctx.mpfr_round == MPFR_RNDD ||
        result->ctx.mpfr_round == MPFR_RNDA)) {
        Py_DECREF((PyObject*)result);
        VALUE_ERROR("invalid value for round");
        return NULL;
    }

    if (result->ctx.mpfr_round == MPFR_RNDA) {
        /* Since RNDA is not supported for MPC, set the MPC rounding modes
           to MPFR_RNDN. */
        result->ctx.real_round = MPFR_RNDN;
        result->ctx.imag_round = MPFR_RNDN;
    }
    if (!(result->ctx.real_round == MPFR_RNDN ||
        result->ctx.real_round == MPFR_RNDZ ||
        result->ctx.real_round == MPFR_RNDU ||
        result->ctx.real_round == MPFR_RNDD ||
        result->ctx.real_round == GMPY_DEFAULT)) {
        Py_DECREF((PyObject*)result);
        VALUE_ERROR("invalid value for real_round");
        return NULL;
    }
    if (!(result->ctx.imag_round == MPFR_RNDN ||
        result->ctx.imag_round == MPFR_RNDZ ||
        result->ctx.imag_round == MPFR_RNDU ||
        result->ctx.imag_round == MPFR_RNDD ||
        result->ctx.imag_round == GMPY_DEFAULT)) {
        Py_DECREF((PyObject*)result);
        VALUE_ERROR("invalid value for imag_round");
        return NULL;
    }

    if (!(result->ctx.emin < 0 && result->ctx.emax > 0)) {
        VALUE_ERROR("invalid values for emin and/or emax");
        Py_DECREF((PyObject*)result);
        return NULL;
    }

    if (mpfr_set_emin(result->ctx.emin)) {
        VALUE_ERROR("invalid value for emin");
        Py_DECREF((PyObject*)result);
        return NULL;
    }
    if (mpfr_set_emax(result->ctx.emax)) {
        VALUE_ERROR("invalid value for emax");
        Py_DECREF((PyObject*)result);
        return NULL;
    }

    result->ctx.underflow = 0;
    result->ctx.overflow = 0;
    result->ctx.inexact = 0;
    result->ctx.invalid = 0;
    result->ctx.erange = 0;
    result->ctx.divzero = 0;
    return (PyObject*)result;
}

static PyObject *
GMPyContextManager_enter(PyObject *self, PyObject *args)
{
    PyObject *temp;

    temp = GMPyContext_set_context(NULL,
                    (PyObject*)((GMPyContextManagerObject*)self)->new_context);
    if (!temp)
        return NULL;
    Py_DECREF(temp);

    mpfr_set_emin(((GMPyContextManagerObject*)self)->new_context->ctx.emin);
    mpfr_set_emax(((GMPyContextManagerObject*)self)->new_context->ctx.emax);

    Py_INCREF((PyObject*)(((GMPyContextManagerObject*)self)->new_context));
    return (PyObject*)(((GMPyContextManagerObject*)self)->new_context);
}

static PyObject *
GMPyContextManager_exit(PyObject *self, PyObject *args)
{
    PyObject *temp;

    temp = GMPyContext_set_context(NULL,
                    (PyObject*)((GMPyContextManagerObject*)self)->old_context);
    if (!temp)
        return NULL;
    Py_DECREF(temp);

    mpfr_set_emin(((GMPyContextManagerObject*)self)->old_context->ctx.emin);
    mpfr_set_emax(((GMPyContextManagerObject*)self)->old_context->ctx.emax);
    Py_RETURN_NONE;
}

static PyObject *
GMPyContext_enter(PyObject *self, PyObject *args)
{
    PyObject *temp;
    PyObject *result;

    result = GMPyContext_context_copy(self, NULL);
    if (!result)
        return NULL;

    temp = GMPyContext_set_context(NULL, result);
    if (!temp)
        return NULL;
    Py_DECREF(temp);

    mpfr_set_emin(((GMPyContextObject*)result)->ctx.emin);
    mpfr_set_emax(((GMPyContextObject*)result)->ctx.emax);
    return result;
}

static PyObject *
GMPyContext_exit(PyObject *self, PyObject *args)
{
    PyObject *temp;

    temp = GMPyContext_set_context(NULL, self);
    if (!temp)
        return NULL;
    Py_DECREF(temp);

    mpfr_set_emin(((GMPyContextObject*)self)->ctx.emin);
    mpfr_set_emax(((GMPyContextObject*)self)->ctx.emax);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(doc_context_clear_flags,
"clear_flags()\n\n"
"Clear all MPFR exception flags.");
static PyObject *
GMPyContext_clear_flags(PyObject *self, PyObject *args)
{
    ((GMPyContextObject*)self)->ctx.underflow = 0;
    ((GMPyContextObject*)self)->ctx.overflow = 0;
    ((GMPyContextObject*)self)->ctx.inexact = 0;
    ((GMPyContextObject*)self)->ctx.invalid = 0;
    ((GMPyContextObject*)self)->ctx.erange = 0;
    ((GMPyContextObject*)self)->ctx.divzero = 0;
    Py_RETURN_NONE;
}

/* Define the get/set functions. */

#define GETSET_BOOLEAN(NAME) \
static PyObject * \
GMPyContext_get_##NAME(GMPyContextObject *self, void *closure) \
{ \
    return PyBool_FromLong(self->ctx.NAME); \
}; \
static int \
GMPyContext_set_##NAME(GMPyContextObject *self, PyObject *value, void *closure) \
{ \
    if (self->ctx.readonly) { \
        VALUE_ERROR("can not modify a readonly context"); \
        return -1; \
    } \
    if (!(PyBool_Check(value))) { \
        TYPE_ERROR(#NAME " must be True or False"); \
        return -1; \
    } \
    self->ctx.NAME = (value == Py_True) ? 1 : 0; \
    return 0; \
}

/* The _EX version doesn't check if the context is already readonly. This
 * allows the readonly state to be temporarily cleared. */

#define GETSET_BOOLEAN_EX(NAME) \
static PyObject * \
GMPyContext_get_##NAME(GMPyContextObject *self, void *closure) \
{ \
    return PyBool_FromLong(self->ctx.NAME); \
}; \
static int \
GMPyContext_set_##NAME(GMPyContextObject *self, PyObject *value, void *closure) \
{ \
    if (!(PyBool_Check(value))) { \
        TYPE_ERROR(#NAME " must be True or False"); \
        return -1; \
    } \
    self->ctx.NAME = (value == Py_True) ? 1 : 0; \
    return 0; \
}

GETSET_BOOLEAN(subnormalize);
GETSET_BOOLEAN(underflow);
GETSET_BOOLEAN(overflow);
GETSET_BOOLEAN(inexact);
GETSET_BOOLEAN(invalid);
GETSET_BOOLEAN(erange);
GETSET_BOOLEAN(divzero);
GETSET_BOOLEAN(trap_underflow);
GETSET_BOOLEAN(trap_overflow);
GETSET_BOOLEAN(trap_inexact);
GETSET_BOOLEAN(trap_invalid);
GETSET_BOOLEAN(trap_erange);
GETSET_BOOLEAN(trap_divzero);
GETSET_BOOLEAN(trap_expbound);
GETSET_BOOLEAN(allow_complex)
GETSET_BOOLEAN_EX(readonly)

static PyObject *
GMPyContext_get_precision(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromSsize_t((Py_ssize_t)(self->ctx.mpfr_prec));
}

static int
GMPyContext_set_precision(GMPyContextObject *self, PyObject *value, void *closure)
{
    Py_ssize_t temp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("precision must be Python integer");
        return -1;
    }
    temp = PyIntOrLong_AsSsize_t(value);
    if (temp < MPFR_PREC_MIN || temp > MPFR_PREC_MAX || PyErr_Occurred()) {
        VALUE_ERROR("invalid value for precision");
        return -1;
    }
    self->ctx.mpfr_prec = (mpfr_prec_t)temp;
    return 0;
}

static PyObject *
GMPyContext_get_real_prec(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromSsize_t((Py_ssize_t)(GET_REAL_PREC(self)));
}

static int
GMPyContext_set_real_prec(GMPyContextObject *self, PyObject *value, void *closure)
{
    Py_ssize_t temp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("real_prec must be Python integer");
        return -1;
    }
    temp = PyIntOrLong_AsSsize_t(value);
    if (temp == -1) {
        if (PyErr_Occurred()) {
            VALUE_ERROR("invalid value for real_prec");
            return -1;
        }
    }
    else if (temp < MPFR_PREC_MIN || temp > MPFR_PREC_MAX) {
        VALUE_ERROR("invalid value for real_prec");
        return -1;
    }
    self->ctx.real_prec = (mpfr_prec_t)temp;
    return 0;
}

static PyObject *
GMPyContext_get_imag_prec(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromSsize_t((Py_ssize_t)(GET_IMAG_PREC(self)));
}

static int
GMPyContext_set_imag_prec(GMPyContextObject *self, PyObject *value, void *closure)
{
    Py_ssize_t temp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("imag_prec must be Python integer");
        return -1;
    }
    temp = PyIntOrLong_AsSsize_t(value);
    if (temp == -1) {
        if (PyErr_Occurred()) {
            VALUE_ERROR("invalid value for imag_prec");
            return -1;
        }
    }
    else if (temp < MPFR_PREC_MIN || temp > MPFR_PREC_MAX) {
        VALUE_ERROR("invalid value for imag_prec");
        return -1;
    }
    self->ctx.imag_prec = (mpfr_prec_t)temp;
    return 0;
}

static PyObject *
GMPyContext_get_round(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromLong((long)(self->ctx.mpfr_round));
}

static int
GMPyContext_set_round(GMPyContextObject *self, PyObject *value, void *closure)
{
    long temp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("round mode must be Python integer");
        return -1;
    }
    temp = PyIntOrLong_AsLong(value);
    if (temp == -1 && PyErr_Occurred()) {
        VALUE_ERROR("invalid value for round mode");
        return -1;
    }
    if (temp == MPFR_RNDN)
        self->ctx.mpfr_round = MPFR_RNDN;
    else if (temp == MPFR_RNDZ)
        self->ctx.mpfr_round = MPFR_RNDZ;
    else if (temp == MPFR_RNDU)
        self->ctx.mpfr_round = MPFR_RNDU;
    else if (temp == MPFR_RNDD)
        self->ctx.mpfr_round = MPFR_RNDD;
    else if (temp == MPFR_RNDA) {
        self->ctx.mpfr_round = MPFR_RNDA;
        /* Since RNDA is not supported for MPC, set the MPC rounding modes
           to MPFR_RNDN. */
        self->ctx.real_round = MPFR_RNDN;
        self->ctx.imag_round = MPFR_RNDN;
    }
    else {
        VALUE_ERROR("invalid value for round mode");
        return -1;
    }
    return 0;
}

static PyObject *
GMPyContext_get_real_round(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromLong((long)GET_REAL_ROUND(self));
}

static int
GMPyContext_set_real_round(GMPyContextObject *self, PyObject *value, void *closure)
{
    long temp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("round mode must be Python integer");
        return -1;
    }
    temp = PyIntOrLong_AsLong(value);
    if (temp == -1 && PyErr_Occurred()) {
        VALUE_ERROR("invalid value for round mode");
        return -1;
    }
    if (temp == GMPY_DEFAULT || temp == MPFR_RNDN || temp == MPFR_RNDZ ||
        temp == MPFR_RNDU || temp == MPFR_RNDD) {
        self->ctx.real_round = (int)temp;
    }
    else {
        VALUE_ERROR("invalid value for round mode");
        return -1;
    }
    return 0;
}

static PyObject *
GMPyContext_get_imag_round(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromLong((long)GET_IMAG_ROUND(self));
}

static int
GMPyContext_set_imag_round(GMPyContextObject *self, PyObject *value, void *closure)
{
    long temp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("round mode must be Python integer");
        return -1;
    }
    temp = PyIntOrLong_AsLong(value);
    if (temp == -1 && PyErr_Occurred()) {
        VALUE_ERROR("invalid value for round mode");
        return -1;
    }
    if (temp == GMPY_DEFAULT || temp == MPFR_RNDN || temp == MPFR_RNDZ ||
        temp == MPFR_RNDU || temp == MPFR_RNDD) {
        self->ctx.imag_round = (int)temp;
    }
    else {
        VALUE_ERROR("invalid value for round mode");
        return -1;
    }
    return 0;
}

static PyObject *
GMPyContext_get_emin(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromLong(self->ctx.emin);
}

static int
GMPyContext_set_emin(GMPyContextObject *self, PyObject *value, void *closure)
{
    long exp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("emin must be Python integer");
        return -1;
    }
    exp = PyIntOrLong_AsLong(value);
    if (exp == -1 && PyErr_Occurred()) {
        VALUE_ERROR("requested minimum exponent is invalid");
        return -1;
    }
    if (mpfr_set_emin((mpfr_prec_t)exp)) {
        VALUE_ERROR("requested minimum exponent is invalid");
        return -1;
    }
    self->ctx.emin = exp;
    mpfr_set_emin(exp);
    return 0;
}

static PyObject *
GMPyContext_get_emax(GMPyContextObject *self, void *closure)
{
    return PyIntOrLong_FromLong(self->ctx.emax);
}

static int
GMPyContext_set_emax(GMPyContextObject *self, PyObject *value, void *closure)
{
    long exp;

    if (self->ctx.readonly) {
        VALUE_ERROR("can not modify a readonly context");
        return -1;
    }
    if (!(PyIntOrLong_Check(value))) {
        TYPE_ERROR("emax must be Python integer");
        return -1;
    }
    exp = PyIntOrLong_AsLong(value);
    if (exp == -1 && PyErr_Occurred()) {
        VALUE_ERROR("requested maximum exponent is invalid");
        return -1;
    }
    if (mpfr_set_emax((mpfr_prec_t)exp)) {
        VALUE_ERROR("requested maximum exponent is invalid");
        return -1;
    }
    self->ctx.emax = exp;
    mpfr_set_emax(exp);
    return 0;
}

#define ADD_GETSET(NAME) \
    {#NAME, \
        (getter)GMPyContext_get_##NAME, \
        (setter)GMPyContext_set_##NAME, NULL, NULL}

static PyGetSetDef GMPyContext_getseters[] = {
    ADD_GETSET(precision),
    ADD_GETSET(real_prec),
    ADD_GETSET(imag_prec),
    ADD_GETSET(round),
    ADD_GETSET(real_round),
    ADD_GETSET(imag_round),
    ADD_GETSET(emax),
    ADD_GETSET(emin),
    ADD_GETSET(subnormalize),
    ADD_GETSET(underflow),
    ADD_GETSET(overflow),
    ADD_GETSET(inexact),
    ADD_GETSET(invalid),
    ADD_GETSET(erange),
    ADD_GETSET(divzero),
    ADD_GETSET(trap_underflow),
    ADD_GETSET(trap_overflow),
    ADD_GETSET(trap_inexact),
    ADD_GETSET(trap_invalid),
    ADD_GETSET(trap_erange),
    ADD_GETSET(trap_divzero),
    ADD_GETSET(trap_expbound),
    ADD_GETSET(allow_complex),
    ADD_GETSET(readonly),
    {NULL}
};

static PyMethodDef GMPyContext_methods[] =
{
    { "add", Pympany_add, METH_VARARGS, doc_context_add },
    { "clear_flags", GMPyContext_clear_flags, METH_NOARGS, doc_context_clear_flags },
    { "copy", GMPyContext_context_copy, METH_NOARGS, doc_context_copy },
    { "div", Pympany_div, METH_VARARGS, doc_context_div },
    { "div_mod", Pympany_divmod, METH_VARARGS, doc_context_divmod },
    { "floor_div", Pympany_floordiv, METH_VARARGS, doc_context_floordiv },
    { "mod", Pympany_mod, METH_VARARGS, doc_context_mod },
    { "mul", Pympany_mul, METH_VARARGS, doc_context_mul },
    { "sub", Pympany_sub, METH_VARARGS, doc_context_sub },
    { "__enter__", GMPyContext_enter, METH_NOARGS, NULL },
    { "__exit__", GMPyContext_exit, METH_VARARGS, NULL },
    { NULL, NULL, 1 }
};

static PyTypeObject GMPyContext_Type =
{
#ifdef PY3
    PyVarObject_HEAD_INIT(0, 0)
#else
    PyObject_HEAD_INIT(0)
        0,                                  /* ob_size          */
#endif
    "gmpy2 context",                        /* tp_name          */
    sizeof(GMPyContextObject),              /* tp_basicsize     */
        0,                                  /* tp_itemsize      */
    (destructor) GMPyContext_dealloc,       /* tp_dealloc       */
        0,                                  /* tp_print         */
        0,                                  /* tp_getattr       */
        0,                                  /* tp_setattr       */
        0,                                  /* tp_reserved      */
    (reprfunc) GMPyContext_repr,            /* tp_repr          */
        0,                                  /* tp_as_number     */
        0,                                  /* tp_as_sequence   */
        0,                                  /* tp_as_mapping    */
        0,                                  /* tp_hash          */
        0,                                  /* tp_call          */
        0,                                  /* tp_str           */
        0,                                  /* tp_getattro      */
        0,                                  /* tp_setattro      */
        0,                                  /* tp_as_buffer     */
    Py_TPFLAGS_DEFAULT,                     /* tp_flags         */
    "GMPY2 Context Object",                 /* tp_doc           */
        0,                                  /* tp_traverse      */
        0,                                  /* tp_clear         */
        0,                                  /* tp_richcompare   */
        0,                                  /* tp_weaklistoffset*/
        0,                                  /* tp_iter          */
        0,                                  /* tp_iternext      */
    GMPyContext_methods,                    /* tp_methods       */
        0,                                  /* tp_members       */
    GMPyContext_getseters,                  /* tp_getset        */
};

static PyMethodDef GMPyContextManager_methods[] =
{
    { "__enter__", GMPyContextManager_enter, METH_NOARGS, NULL },
    { "__exit__", GMPyContextManager_exit, METH_VARARGS, NULL },
    { NULL, NULL, 1 }
};

static PyTypeObject GMPyContextManager_Type =
{
#ifdef PY3
    PyVarObject_HEAD_INIT(0, 0)
#else
    PyObject_HEAD_INIT(0)
        0,                                   /* ob_size          */
#endif
    "gmpy2 context",                         /* tp_name          */
    sizeof(GMPyContextManagerObject),        /* tp_basicsize     */
        0,                                   /* tp_itemsize      */
    (destructor) GMPyContextManager_dealloc, /* tp_dealloc       */
        0,                                   /* tp_print         */
        0,                                   /* tp_getattr       */
        0,                                   /* tp_setattr       */
        0,                                   /* tp_reserved      */
    (reprfunc) GMPyContextManager_repr,      /* tp_repr          */
        0,                                   /* tp_as_number     */
        0,                                   /* tp_as_sequence   */
        0,                                   /* tp_as_mapping    */
        0,                                   /* tp_hash          */
        0,                                   /* tp_call          */
        0,                                   /* tp_str           */
        0,                                   /* tp_getattro      */
        0,                                   /* tp_setattro      */
        0,                                   /* tp_as_buffer     */
    Py_TPFLAGS_DEFAULT,                      /* tp_flags         */
    "GMPY2 Context manager",                 /* tp_doc           */
        0,                                   /* tp_traverse      */
        0,                                   /* tp_clear         */
        0,                                   /* tp_richcompare   */
        0,                                   /* tp_weaklistoffset*/
        0,                                   /* tp_iter          */
        0,                                   /* tp_iternext      */
    GMPyContextManager_methods,              /* tp_methods       */
        0,                                   /* tp_members       */
        0,                                   /* tp_getset        */
};
