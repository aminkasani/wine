/*
 * Copyright 2022 Gabriel Ivăncescu for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#include <limits.h>

#include "jscript.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

typedef struct {
    jsdisp_t dispex;
    DWORD size;

    /* align the buffer */
    union {
        double _align_;
        BYTE buf[1];
    };
} ArrayBufferInstance;

typedef struct {
    jsdisp_t dispex;

    jsdisp_t *buffer;
    DWORD offset;
    DWORD size;
} DataViewInstance;

static inline ArrayBufferInstance *arraybuf_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, ArrayBufferInstance, dispex);
}

static inline DataViewInstance *dataview_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, DataViewInstance, dispex);
}

static inline ArrayBufferInstance *arraybuf_this(jsval_t vthis)
{
    jsdisp_t *jsdisp = is_object_instance(vthis) ? to_jsdisp(get_object(vthis)) : NULL;
    return (jsdisp && is_class(jsdisp, JSCLASS_ARRAYBUFFER)) ? arraybuf_from_jsdisp(jsdisp) : NULL;
}

static HRESULT ArrayBuffer_get_byteLength(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_number(arraybuf_from_jsdisp(jsthis)->size);
    return S_OK;
}

static HRESULT ArrayBuffer_slice(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FIXME("not implemented\n");

    return E_NOTIMPL;
}

static const builtin_prop_t ArrayBuffer_props[] = {
    {L"byteLength",            NULL, 0,                    ArrayBuffer_get_byteLength},
    {L"slice",                 ArrayBuffer_slice,          PROPF_METHOD|2},
};

static const builtin_info_t ArrayBuffer_info = {
    JSCLASS_ARRAYBUFFER,
    NULL,
    ARRAY_SIZE(ArrayBuffer_props),
    ArrayBuffer_props,
    NULL,
    NULL
};

static const builtin_prop_t ArrayBufferInst_props[] = {
    {L"byteLength",            NULL, 0,                    ArrayBuffer_get_byteLength},
};

static const builtin_info_t ArrayBufferInst_info = {
    JSCLASS_ARRAYBUFFER,
    NULL,
    ARRAY_SIZE(ArrayBufferInst_props),
    ArrayBufferInst_props,
    NULL,
    NULL
};

static HRESULT create_arraybuf(script_ctx_t *ctx, DWORD size, jsdisp_t **ret)
{
    ArrayBufferInstance *arraybuf;
    HRESULT hres;

    if(!(arraybuf = heap_alloc_zero(FIELD_OFFSET(ArrayBufferInstance, buf[size]))))
        return E_OUTOFMEMORY;

    hres = init_dispex_from_constr(&arraybuf->dispex, ctx, &ArrayBufferInst_info, ctx->arraybuf_constr);
    if(FAILED(hres)) {
        heap_free(arraybuf);
        return hres;
    }

    arraybuf->size = size;

    *ret = &arraybuf->dispex;
    return S_OK;
}

static HRESULT ArrayBufferConstr_isView(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FIXME("not implemented\n");

    return E_NOTIMPL;
}

static HRESULT ArrayBufferConstr_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DWORD size = 0;
    jsdisp_t *obj;
    HRESULT hres;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        if(argc) {
            double n;
            hres = to_integer(ctx, argv[0], &n);
            if(FAILED(hres))
                return hres;
            if(n < 0.0)
                return JS_E_INVALID_LENGTH;
            if(n > (UINT_MAX - FIELD_OFFSET(ArrayBufferInstance, buf[0])))
                return E_OUTOFMEMORY;
            size = n;
        }

        if(r) {
            hres = create_arraybuf(ctx, size, &obj);
            if(FAILED(hres))
                return hres;
            *r = jsval_obj(obj);
        }
        break;
    }
    default:
        FIXME("unimplemented flags: %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static const builtin_prop_t ArrayBufferConstr_props[] = {
    {L"isView",                ArrayBufferConstr_isView,   PROPF_METHOD|1},
};

static const builtin_info_t ArrayBufferConstr_info = {
    JSCLASS_FUNCTION,
    Function_value,
    ARRAY_SIZE(ArrayBufferConstr_props),
    ArrayBufferConstr_props,
    NULL,
    NULL
};

static inline DataViewInstance *dataview_this(jsval_t vthis)
{
    jsdisp_t *jsdisp = is_object_instance(vthis) ? to_jsdisp(get_object(vthis)) : NULL;
    return (jsdisp && is_class(jsdisp, JSCLASS_DATAVIEW)) ? dataview_from_jsdisp(jsdisp) : NULL;
}

static HRESULT DataView_get_buffer(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DataViewInstance *view;

    TRACE("\n");

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(r) *r = jsval_obj(jsdisp_addref(view->buffer));
    return S_OK;
}

static HRESULT DataView_get_byteLength(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DataViewInstance *view;

    TRACE("\n");

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(r) *r = jsval_number(view->size);
    return S_OK;
}

static HRESULT DataView_get_byteOffset(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DataViewInstance *view;

    TRACE("\n");

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(r) *r = jsval_number(view->offset);
    return S_OK;
}

static inline void copy_type_data(void *dst, const void *src, unsigned type_size, BOOL little_endian)
{
#ifdef WORDS_BIGENDIAN
    BOOL swap = little_endian;
#else
    BOOL swap = !little_endian;
#endif
    const BYTE *in = src;
    BYTE *out = dst;
    unsigned i;

    if(swap)
        for(i = 0; i < type_size; i++)
            out[i] = in[type_size - i - 1];
    else
        memcpy(out, in, type_size);
}

static HRESULT get_data(script_ctx_t *ctx, jsval_t vthis, unsigned argc, jsval_t *argv, unsigned type_size, void *ret)
{
    BOOL little_endian = FALSE;
    DataViewInstance *view;
    HRESULT hres;
    DWORD offset;
    BYTE *data;
    double n;

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(!argc || is_undefined(argv[0]))
        return JS_E_DATAVIEW_NO_ARGUMENT;

    hres = to_integer(ctx, argv[0], &n);
    if(FAILED(hres))
        return hres;

    if(n < 0.0 || n >= view->size)
        return JS_E_DATAVIEW_INVALID_ACCESS;

    offset = n;
    if(view->size - offset < type_size)
        return JS_E_DATAVIEW_INVALID_ACCESS;
    data = &arraybuf_from_jsdisp(view->buffer)->buf[view->offset + offset];

    if(type_size == 1) {
        *(BYTE*)ret = data[0];
        return S_OK;
    }

    if(argc > 1) {
        hres = to_boolean(argv[1], &little_endian);
        if(FAILED(hres))
            return hres;
    }

    copy_type_data(ret, data, type_size, little_endian);
    return S_OK;
}

static HRESULT set_data(script_ctx_t *ctx, jsval_t vthis, unsigned argc, jsval_t *argv, unsigned type_size, const void *val)
{
    BOOL little_endian = FALSE;
    DataViewInstance *view;
    HRESULT hres;
    DWORD offset;
    BYTE *data;
    double n;

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(is_undefined(argv[0]) || is_undefined(argv[1]))
        return JS_E_DATAVIEW_NO_ARGUMENT;

    hres = to_integer(ctx, argv[0], &n);
    if(FAILED(hres))
        return hres;

    if(n < 0.0 || n >= view->size)
        return JS_E_DATAVIEW_INVALID_ACCESS;

    offset = n;
    if(view->size - offset < type_size)
        return JS_E_DATAVIEW_INVALID_ACCESS;
    data = &arraybuf_from_jsdisp(view->buffer)->buf[view->offset + offset];

    if(type_size == 1) {
        data[0] = *(const BYTE*)val;
        return S_OK;
    }

    if(argc > 2) {
        hres = to_boolean(argv[2], &little_endian);
        if(FAILED(hres))
            return hres;
    }

    copy_type_data(data, val, type_size, little_endian);
    return S_OK;
}

static HRESULT DataView_getFloat32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    float v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getFloat64(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    double v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getInt8(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT8 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getInt16(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT16 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getInt32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getUint8(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    UINT8 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getUint16(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    UINT16 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getUint32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    UINT32 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_setFloat32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    double n;
    float v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_number(ctx, argv[1], &n);
    if(FAILED(hres))
        return hres;
    v = n;  /* FIXME: don't assume rounding mode is round-to-nearest ties-to-even */

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setFloat64(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    double v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_number(ctx, argv[1], &v);
    if(FAILED(hres))
        return hres;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setInt8(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 n;
    INT8 v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_int32(ctx, argv[1], &n);
    if(FAILED(hres))
        return hres;
    v = n;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setInt16(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 n;
    INT16 v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_int32(ctx, argv[1], &n);
    if(FAILED(hres))
        return hres;
    v = n;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setInt32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_int32(ctx, argv[1], &v);
    if(FAILED(hres))
        return hres;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static const builtin_prop_t DataView_props[] = {
    {L"getFloat32",            DataView_getFloat32,        PROPF_METHOD|1},
    {L"getFloat64",            DataView_getFloat64,        PROPF_METHOD|1},
    {L"getInt16",              DataView_getInt16,          PROPF_METHOD|1},
    {L"getInt32",              DataView_getInt32,          PROPF_METHOD|1},
    {L"getInt8",               DataView_getInt8,           PROPF_METHOD|1},
    {L"getUint16",             DataView_getUint16,         PROPF_METHOD|1},
    {L"getUint32",             DataView_getUint32,         PROPF_METHOD|1},
    {L"getUint8",              DataView_getUint8,          PROPF_METHOD|1},
    {L"setFloat32",            DataView_setFloat32,        PROPF_METHOD|1},
    {L"setFloat64",            DataView_setFloat64,        PROPF_METHOD|1},
    {L"setInt16",              DataView_setInt16,          PROPF_METHOD|1},
    {L"setInt32",              DataView_setInt32,          PROPF_METHOD|1},
    {L"setInt8",               DataView_setInt8,           PROPF_METHOD|1},
    {L"setUint16",             DataView_setInt16,          PROPF_METHOD|1},
    {L"setUint32",             DataView_setInt32,          PROPF_METHOD|1},
    {L"setUint8",              DataView_setInt8,           PROPF_METHOD|1},
};

static void DataView_destructor(jsdisp_t *dispex)
{
    DataViewInstance *view = dataview_from_jsdisp(dispex);
    if(view->buffer)
        jsdisp_release(view->buffer);
    heap_free(view);
}

static HRESULT DataView_gc_traverse(jsdisp_t *dispex, void *arg)
{
    DataViewInstance *view = dataview_from_jsdisp(dispex);
    return gc_process_linked_obj(dispex, view->buffer, (void**)&view->buffer, arg);
}

static const builtin_info_t DataView_info = {
    JSCLASS_DATAVIEW,
    NULL,
    ARRAY_SIZE(DataView_props),
    DataView_props,
    DataView_destructor,
    NULL,
    NULL,
    NULL,
    NULL,
    DataView_gc_traverse
};

static const builtin_info_t DataViewInst_info = {
    JSCLASS_DATAVIEW,
    NULL,
    0,
    NULL,
    DataView_destructor,
    NULL,
    NULL,
    NULL,
    NULL,
    DataView_gc_traverse
};

static HRESULT DataViewConstr_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    ArrayBufferInstance *arraybuf;
    DataViewInstance *view;
    DWORD offset = 0, size;
    HRESULT hres;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        if(!argc || !(arraybuf = arraybuf_this(argv[0])))
            return JS_E_DATAVIEW_NO_ARGUMENT;
        size = arraybuf->size;

        if(argc > 1) {
            double offs, len, maxsize = size;
            hres = to_integer(ctx, argv[1], &offs);
            if(FAILED(hres))
                return hres;
            if(offs < 0.0 || offs > maxsize)
                return JS_E_DATAVIEW_INVALID_OFFSET;
            offset = offs;

            if(argc > 2 && !is_undefined(argv[2])) {
                hres = to_integer(ctx, argv[2], &len);
                if(FAILED(hres))
                    return hres;
                if(len < 0.0 || offs+len > maxsize)
                    return JS_E_DATAVIEW_INVALID_OFFSET;
                size = len;
            }else
                size -= offset;
        }

        if(!r)
            return S_OK;

        if(!(view = heap_alloc_zero(sizeof(DataViewInstance))))
            return E_OUTOFMEMORY;

        hres = init_dispex_from_constr(&view->dispex, ctx, &DataViewInst_info, ctx->dataview_constr);
        if(FAILED(hres)) {
            heap_free(view);
            return hres;
        }

        view->buffer = jsdisp_addref(&arraybuf->dispex);
        view->offset = offset;
        view->size = size;

        *r = jsval_obj(&view->dispex);
        break;
    }
    default:
        FIXME("unimplemented flags: %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static const builtin_info_t DataViewConstr_info = {
    JSCLASS_FUNCTION,
    Function_value,
    0,
    NULL,
    NULL,
    NULL
};

HRESULT init_arraybuf_constructors(script_ctx_t *ctx)
{
    static const struct {
        const WCHAR *name;
        builtin_invoke_t get;
    } DataView_getters[] = {
        { L"buffer",        DataView_get_buffer },
        { L"byteLength",    DataView_get_byteLength },
        { L"byteOffset",    DataView_get_byteOffset },
    };
    ArrayBufferInstance *arraybuf;
    DataViewInstance *view;
    property_desc_t desc;
    HRESULT hres;
    unsigned i;

    if(ctx->version < SCRIPTLANGUAGEVERSION_ES5)
        return S_OK;

    if(!(arraybuf = heap_alloc_zero(FIELD_OFFSET(ArrayBufferInstance, buf[0]))))
        return E_OUTOFMEMORY;

    hres = init_dispex(&arraybuf->dispex, ctx, &ArrayBuffer_info, ctx->object_prototype);
    if(FAILED(hres)) {
        heap_free(arraybuf);
        return hres;
    }

    hres = create_builtin_constructor(ctx, ArrayBufferConstr_value, L"ArrayBuffer", &ArrayBufferConstr_info,
                                      PROPF_CONSTR|1, &arraybuf->dispex, &ctx->arraybuf_constr);
    jsdisp_release(&arraybuf->dispex);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_define_data_property(ctx->global, L"ArrayBuffer", PROPF_CONFIGURABLE | PROPF_WRITABLE,
                                       jsval_obj(ctx->arraybuf_constr));
    if(FAILED(hres))
        return hres;

    if(!(view = heap_alloc_zero(sizeof(DataViewInstance))))
        return E_OUTOFMEMORY;

    hres = create_arraybuf(ctx, 0, &view->buffer);
    if(FAILED(hres)) {
        heap_free(view);
        return hres;
    }

    hres = init_dispex(&view->dispex, ctx, &DataView_info, ctx->object_prototype);
    if(FAILED(hres)) {
        jsdisp_release(view->buffer);
        heap_free(view);
        return hres;
    }

    desc.flags = PROPF_CONFIGURABLE;
    desc.mask  = PROPF_CONFIGURABLE | PROPF_ENUMERABLE;
    desc.explicit_getter = desc.explicit_setter = TRUE;
    desc.explicit_value = FALSE;
    desc.setter = NULL;

    for(i = 0; i < ARRAY_SIZE(DataView_getters); i++) {
        hres = create_builtin_function(ctx, DataView_getters[i].get, NULL, NULL, PROPF_METHOD, NULL, &desc.getter);
        if(SUCCEEDED(hres)) {
            hres = jsdisp_define_property(&view->dispex, DataView_getters[i].name, &desc);
            jsdisp_release(desc.getter);
        }
        if(FAILED(hres)) {
            jsdisp_release(&view->dispex);
            return hres;
        }
    }

    hres = create_builtin_constructor(ctx, DataViewConstr_value, L"DataView", &DataViewConstr_info,
                                      PROPF_CONSTR|1, &view->dispex, &ctx->dataview_constr);
    jsdisp_release(&view->dispex);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_define_data_property(ctx->global, L"DataView", PROPF_CONFIGURABLE | PROPF_WRITABLE,
                                       jsval_obj(ctx->dataview_constr));

    return hres;
}
