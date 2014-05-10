/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * gmpy2_macros.h                                                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Python interface to the GMP or MPIR, MPFR, and MPC multiple precision   *
 * libraries.                                                              *
 *                                                                         *
 * Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007,               *
 *           2008, 2009 Alex Martelli                                      *
 *                                                                         *
 * Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2014 Case Van Horsen      *
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

/* This file contains a collection of macros that can be used to reduce
 * repetitive code. As new macros are written to support of refactoring,
 * they should be placed here.
 */

/* NAME is used as part of the GMPy function name. It usually uses an upper-
 *      case first character.
 * FUNC is the component of the actual name used by MPFR and MPC.
 *
 * GMPY_MPFR_MPC_UNIOP(NAME, FUNC) creates the following functions:
 *     GMPy_Real_NAME(x, context)
 *     GMPy_Complex_NAME(x, context)
 *     GMPy_Number_NAME(x, context)
 *     GMPy_Context_NAME(self, other)
 *     - called with METH_O
 *
 * GMPY_MPFR_MPC_UNIOP_TEMPLATE(NAME, FUNC) creates the following functions:
 *     GMPy_Number_NAME(x, context)
 *     - assumes GMPy_Real_NAME & GMPy_Complex_NAME exist
 *     GMPy_Context_NAME(self, other)
 *     - called with METH_O
 * 
 * GMPY_MPFR_MPC_BIOP_TEMPLATE(NAME, FUNC) creates the following functions:
 *     GMPy_Number_NAME(x, y, context)
 *     - assumes GMPy_Real_NAME & GMPy_Complex_NAME exist
 *     GMPy_Context_NAME(self, args)
 *     - called with METH_VARARGS
 * 
 * GMPY_MPFR_MPC_TRIOP_TEMPLATE(NAME, FUNC) creates the following functions:
 *     GMPy_Number_NAME(x, y, Z, context)
 *     - assumes GMPy_Real_NAME & GMPy_Complex_NAME exist
 *     GMPy_Context_NAME(self, args)
 *     - called with METH_VARARGS
 * 
 * GMPY_MPFR_MPC_UNIOP_TEMPLATE(NAME, FUNC) creates the following functions:
 *     GMPy_Number_NAME(x, context)
 *     - assumes GMPy_Real_NAME & GMPy_Complex_NAME exist
 *     GMPy_Context_NAME(self, other)
 *     - called with METH_O
 * 
 * GMPY_MPFR_UNIOP(NAME, FUNC) creates the following functions:
 *     GMPy_Real_NAME(x, context)
 *     GMPy_Number_NAME(x, context)
 *     GMPy_Context_NAME(self, other)
 *     - called with METH_O
 *
 * GMPY_MPFR_UNIOP_TEMPLATE(NAME, FUNC) creates the following functions:
 *     GMPy_Number_NAME(x, context)
 *     - assumes GMPy_Real_NAME exists
 *     GMPy_Context_NAME(self, other)
 *     - called with METH_O
 * 
 * GMPY_MPFR_BINOP(NAME, FUNC) creates the following functions:
 *     GMPy_Real_NAME(x, y, context)
 *     GMPy_Number_NAME(x, y, context)
 *     GMPy_Context_NAME(self, args)
 *     - called with METH_VARARGS
 *
 */

#define GMPY_MPFR_MPC_UNIOP(NAME, FUNC) \
static PyObject * \
GMPy_Real_##NAME(PyObject *x, CTXT_Object *context) \
{ \
    MPFR_Object *result = NULL, *tempx = NULL; \
    CHECK_CONTEXT(context); \
    result = GMPy_MPFR_New(0, context); \
    tempx = GMPy_MPFR_From_Real(x, 1, context); \
    if (!result || !tempx) { \
        Py_XDECREF((PyObject*)result); \
        Py_XDECREF((PyObject*)tempx); \
        return NULL; \
    } \
    mpfr_clear_flags(); \
    result->rc = mpfr_##FUNC(result->f, tempx->f, GET_MPFR_ROUND(context)); \
    Py_DECREF((PyObject*)tempx); \
    GMPY_MPFR_CLEANUP(result, context, #FUNC "()"); \
    return (PyObject*)result; \
} \
static PyObject * \
GMPy_Complex_##NAME(PyObject *x, CTXT_Object *context) \
{ \
    MPC_Object *result = NULL, *tempx = NULL; \
    CHECK_CONTEXT(context); \
    result = GMPy_MPC_New(0, 0, context); \
    tempx = GMPy_MPC_From_Complex(x, 1, 1, context); \
    if (!result || !tempx) { \
        Py_XDECREF((PyObject*)result); \
        Py_XDECREF((PyObject*)tempx); \
        return NULL; \
    } \
    result->rc = mpc_##FUNC(result->c, tempx->c, GET_MPC_ROUND(context)); \
    Py_DECREF((PyObject*)tempx); \
    GMPY_MPC_CLEANUP(result, context, #FUNC "()"); \
    return (PyObject*)result; \
} \
static PyObject * \
GMPy_Number_##NAME(PyObject *x, CTXT_Object *context) \
{ \
    if (IS_REAL(x)) \
        return GMPy_Real_##NAME(x, context); \
    if (IS_COMPLEX(x)) \
        return GMPy_Complex_##NAME(x, context); \
    TYPE_ERROR(#FUNC"() argument type not supported"); \
    return NULL; \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *other) \
{ \
    CTXT_Object *context = NULL; \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(other, context); \
}

#define GMPY_MPFR_MPC_UNIOP_TEMPLATE(NAME, FUNC) \
static PyObject * \
GMPy_Number_##NAME(PyObject *x, CTXT_Object *context) \
{ \
    if (IS_REAL(x)) \
        return GMPy_Real_##NAME(x, context); \
    if (IS_COMPLEX(x)) \
        return GMPy_Complex_##NAME(x, context); \
    TYPE_ERROR(#FUNC"() argument type not supported"); \
    return NULL; \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *other) \
{ \
    CTXT_Object *context = NULL; \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(other, context); \
}

#define GMPY_MPFR_MPC_BIOP_TEMPLATE(NAME, FUNC) \
static PyObject * \
GMPy_Number_##NAME(PyObject *x, PyObject * y, CTXT_Object *context) \
{ \
    if (IS_REAL(x) && IS_REAL(y)) \
        return GMPy_Real_##NAME(x, y, context); \
    if (IS_COMPLEX(x) && IS_COMPLEX(y)) \
        return GMPy_Complex_##NAME(x, y, context); \
    TYPE_ERROR(#FUNC"() argument type not supported"); \
    return NULL; \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *args) \
{ \
    CTXT_Object *context = NULL; \
    if (PyTuple_GET_SIZE(args) != 2) { \
        TYPE_ERROR(#FUNC"() requires 2 arguments"); \
        return NULL; \
    } \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(PyTuple_GET_ITEM(args, 0), PyTuple_GET_ITEM(args, 1), context); \
}

#define GMPY_MPFR_MPC_TRIOP(NAME, FUNC) \
static PyObject * \
GMPy_Real_##NAME(PyObject *x, PyObject *y, PyObject *z, CTXT_Object *context) \
{ \
    MPFR_Object *result = NULL, *tempx = NULL, *tempy = NULL, *tempz = NULL; \
    CHECK_CONTEXT(context); \
    result = GMPy_MPFR_New(0, context); \
    tempx = GMPy_MPFR_From_Real(x, 1, context); \
    tempy = GMPy_MPFR_From_Real(y, 1, context); \
    tempz = GMPy_MPFR_From_Real(z, 1, context); \
    if (!result || !tempx || !tempy || !tempz) { \
        Py_XDECREF((PyObject*)tempx); \
        Py_XDECREF((PyObject*)tempy); \
        Py_XDECREF((PyObject*)tempz); \
        Py_XDECREF((PyObject*)result); \
        return NULL; \
    } \
    mpfr_clear_flags(); \
    result->rc = mpfr_##FUNC(result->f, tempx->f, tempy->f, tempz->f, GET_MPFR_ROUND(context)); \
    Py_DECREF((PyObject*)tempx); \
    Py_DECREF((PyObject*)tempy); \
    Py_DECREF((PyObject*)tempz); \
    GMPY_MPFR_CLEANUP(result, context, #FUNC"()"); \
    return (PyObject*)result; \
} \
static PyObject * \
GMPy_Complex_##NAME(PyObject *x, PyObject *y, PyObject *z, CTXT_Object *context) \
{ \
    MPC_Object *result = NULL, *tempx = NULL, *tempy = NULL, *tempz = NULL; \
    CHECK_CONTEXT(context); \
    result = GMPy_MPC_New(0, 0, context); \
    tempx = GMPy_MPC_From_Complex(x, 1, 1, context); \
    tempy = GMPy_MPC_From_Complex(y, 1, 1, context); \
    tempz = GMPy_MPC_From_Complex(z, 1, 1, context); \
    if (!result || !tempx || !tempy || !tempz) { \
        Py_XDECREF((PyObject*)tempx); \
        Py_XDECREF((PyObject*)tempy); \
        Py_XDECREF((PyObject*)tempz); \
        Py_XDECREF((PyObject*)result); \
        return NULL; \
    } \
    result->rc = mpc_fma(result->c, tempx->c, tempy->c, tempz->c, GET_MPC_ROUND(context)); \
    Py_DECREF((PyObject*)tempx); \
    Py_DECREF((PyObject*)tempy); \
    Py_DECREF((PyObject*)tempz); \
    GMPY_MPC_CLEANUP(result, context, #FUNC"()"); \
    return (PyObject*)result; \
} \
static PyObject * \
GMPy_Number_##NAME(PyObject *x, PyObject *y, PyObject *z, CTXT_Object *context) \
{ \
    if (IS_REAL(x) && IS_REAL(y) && IS_REAL(z)) \
        return GMPy_Real_##NAME(x, y, z, context); \
    if (IS_COMPLEX(x) && IS_COMPLEX(y) && IS_COMPLEX(z)) \
        return GMPy_Complex_##NAME(x, y, z, context); \
    TYPE_ERROR(#FUNC"() argument type not supported"); \
    return NULL; \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *args) \
{ \
    CTXT_Object *context = NULL; \
    if (PyTuple_GET_SIZE(args) != 3) { \
        TYPE_ERROR(#FUNC"() requires 3 arguments"); \
        return NULL; \
    } \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(PyTuple_GET_ITEM(args, 0), PyTuple_GET_ITEM(args, 1), \
                              PyTuple_GET_ITEM(args, 2), context); \
}

#define GMPY_MPFR_UNIOP(NAME, FUNC) \
static PyObject * \
GMPy_Real_##NAME(PyObject *x, CTXT_Object *context) \
{ \
    MPFR_Object *result = NULL, *tempx = NULL; \
    CHECK_CONTEXT(context); \
    result = GMPy_MPFR_New(0, context); \
    tempx = GMPy_MPFR_From_Real(x, 1, context); \
    if (!result || !tempx) { \
        Py_XDECREF((PyObject*)result); \
        Py_XDECREF((PyObject*)tempx); \
        return NULL; \
    } \
    mpfr_clear_flags(); \
    result->rc = mpfr_##FUNC(result->f, tempx->f, GET_MPFR_ROUND(context)); \
    Py_DECREF((PyObject*)tempx); \
    GMPY_MPFR_CLEANUP(result, context, #FUNC "()"); \
    return (PyObject*)result; \
} \
static PyObject * \
GMPy_Number_##NAME(PyObject *x, CTXT_Object *context) \
{ \
    if (IS_REAL(x)) \
        return GMPy_Real_##NAME(x, context); \
    TYPE_ERROR(#FUNC"() argument type not supported"); \
    return NULL; \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *other) \
{ \
    CTXT_Object *context = NULL; \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(other, context); \
}

#define GMPY_MPFR_UNIOP_TEMPLATE(NAME, FUNC) \
static PyObject * \
GMPy_Number_##NAME(PyObject *x, CTXT_Object *context) \
{ \
    if (IS_REAL(x)) \
        return GMPy_Real_##NAME(x, context); \
    TYPE_ERROR(#FUNC"() argument type not supported"); \
    return NULL; \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *other) \
{ \
    CTXT_Object *context = NULL; \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(other, context); \
}

#define GMPY_MPFR_BINOP(NAME, FUNC) \
static PyObject * \
GMPy_Real_##NAME(PyObject *x, PyObject *y, CTXT_Object *context) \
{ \
    MPFR_Object *result = NULL, *tempx = NULL, *tempy = NULL; \
    CHECK_CONTEXT(context); \
    tempx = GMPy_MPFR_From_Real(x, 1, context); \
    tempy = GMPy_MPFR_From_Real(y, 1, context); \
    result = GMPy_MPFR_New(0, context); \
    if (!result || !tempx || !tempy) { \
        Py_XDECREF((PyObject*)tempx); \
        Py_XDECREF((PyObject*)tempy); \
        Py_XDECREF((PyObject*)result); \
        return NULL; \
    } \
    mpfr_clear_flags(); \
    result->rc = mpfr_##FUNC(result->f, tempx->f, tempy->f, GET_MPFR_ROUND(context)); \
    Py_DECREF((PyObject*)tempx); \
    Py_DECREF((PyObject*)tempy); \
    GMPY_MPFR_CLEANUP(result, context, #FUNC"()"); \
    return (PyObject*)result; \
} \
static PyObject * \
GMPy_Number_##NAME(PyObject *x, PyObject *y, CTXT_Object *context) \
{ \
    if (IS_REAL(x) && IS_REAL(y)) \
        return GMPy_Real_##NAME(x, y, context); \
    TYPE_ERROR(#FUNC"() argument type not supported"); \
    return NULL; \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *args) \
{ \
    CTXT_Object *context = NULL; \
    if (PyTuple_GET_SIZE(args) != 2) { \
        TYPE_ERROR(#FUNC"() requires 2 arguments"); \
        return NULL; \
    } \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(PyTuple_GET_ITEM(args, 0), PyTuple_GET_ITEM(args, 1), context); \
} \

/* GMPY_MPFR_CONST(NAME, FUNCT) is the template for creating constants. For
 * compatibility with gmpy 2.0.x, the functions that create constants accept an
 * optional precision.
 */

#define GMPY_MPFR_CONST(NAME, FUNC) \
static PyObject * \
GMPy_Function_##NAME(PyObject *self, PyObject *args, PyObject *keywds) \
{ \
    MPFR_Object *result = NULL; \
    mpfr_prec_t bits = 0; \
    static char *kwlist[] = {"precision", NULL}; \
    CTXT_Object *context = NULL; \
    CHECK_CONTEXT(context); \
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|l", kwlist, &bits)) return NULL; \
    if ((result = GMPy_MPFR_New(bits, context))) { \
        mpfr_clear_flags(); \
        result->rc = mpfr_##FUNC(result->f, GET_MPFR_ROUND(context)); \
        GMPY_MPFR_CLEANUP(result, context, #FUNC"()") \
    } \
    return (PyObject*)result; \
}\

/* GMPY_MPFR_NOOP(NAME, FUNC) is used in creating constants.
 */

#define GMPY_MPFR_NOOP(NAME, FUNC) \
static PyObject * \
GMPy_Real_##NAME(CTXT_Object *context) \
{ \
    MPFR_Object *result = NULL; \
    CHECK_CONTEXT(context); \
    if (!(result = GMPy_MPFR_New(0, context))) { \
        return NULL; \
    } \
    mpfr_clear_flags(); \
    result->rc = mpfr_##FUNC(result->f, GET_MPFR_ROUND(context)); \
    GMPY_MPFR_CLEANUP(result, context, #FUNC"()"); \
    return (PyObject*)result; \
} \
static PyObject * \
GMPy_Number_##NAME(CTXT_Object *context) \
{ \
    return GMPy_Real_##NAME(context); \
} \
static PyObject * \
GMPy_Context_##NAME(PyObject *self, PyObject *args) \
{ \
    CTXT_Object *context = NULL; \
    if (PyTuple_GET_SIZE(args) != 0) { \
        TYPE_ERROR(#FUNC"() requires 0 arguments"); \
        return NULL; \
    } \
    if (self && CTXT_Check(self)) { \
        context = (CTXT_Object*)self; \
    } \
    else { \
        CHECK_CONTEXT(context); \
    } \
    return GMPy_Number_##NAME(context); \
} \



/* The following legacy macros should be removed in the future. */

#define MPFR_MONOP(NAME) \
static PyObject * \
Py##NAME(MPFR_Object *x) \
{ \
    MPFR_Object *r; \
    CTXT_Object *context = NULL; \
    CHECK_CONTEXT_SET_EXPONENT(context); \
    if (!(r = GMPy_MPFR_New(0, context))) \
        return NULL; \
    if (MPFR_Check(x)) { \
        r->rc = NAME(r->f, x->f, context->ctx.mpfr_round); \
    } \
    else { \
        mpfr_set(r->f, x->f, context->ctx.mpfr_round); \
        r->round_mode = x->round_mode; \
        r->rc = x->rc; \
        mpfr_clear_flags(); \
        mpfr_check_range(r->f, r->rc, r->round_mode); \
        r->rc = NAME(r->f, r->f, context->ctx.mpfr_round); \
        MERGE_FLAGS; \
        CHECK_FLAGS(#NAME "()"); \
    } \
  done: \
    return (PyObject *) r; \
}


#define MPFR_UNIOP_NOROUND(NAME) \
static PyObject * \
Pympfr_##NAME(PyObject* self, PyObject *other) \
{ \
    MPFR_Object *result; \
    CTXT_Object *context = NULL; \
    CHECK_CONTEXT_SET_EXPONENT(context); \
    PARSE_ONE_MPFR_OTHER(#NAME "() requires 'mpfr' argument"); \
    if (!(result = GMPy_MPFR_New(0, context))) goto done; \
    mpfr_clear_flags(); \
    result->rc = mpfr_##NAME(result->f, MPFR(self)); \
    MPFR_CLEANUP_SELF(#NAME "()"); \
}

#define MPFR_UNIOP(NAME) \
static PyObject * \
Pympfr_##NAME(PyObject* self, PyObject *other) \
{ \
    MPFR_Object *result; \
    CTXT_Object *context = NULL; \
    CHECK_CONTEXT_SET_EXPONENT(context); \
    PARSE_ONE_MPFR_OTHER(#NAME "() requires 'mpfr' argument"); \
    if (!(result = GMPy_MPFR_New(0, context))) goto done; \
    mpfr_clear_flags(); \
    result->rc = mpfr_##NAME(result->f, MPFR(self), context->ctx.mpfr_round); \
    MPFR_CLEANUP_SELF(#NAME "()"); \
}

