/* Copyright (c) 2010 Mateusz Armatys
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include "http_parser.h"

static int getRequestTypeFlags(lua_State* L, const char* name) {
    if (strncmp(name, "request", 8) == 0) return HTTP_REQUEST;
    if (strncmp(name, "response", 9) == 0) return HTTP_RESPONSE;
    
    return luaL_error(L, "Invalid flag.");
}

static const char* getHttpMethodEnum(int method) {
    switch(method) {
        case HTTP_DELETE: return "delete";
        case HTTP_GET: return "get";
        case HTTP_HEAD: return "head";
        case HTTP_POST: return "post";
        case HTTP_PUT: return "put";
        case HTTP_CONNECT: return "connect";
        case HTTP_OPTIONS: return "options";
        case HTTP_TRACE: return "trace";
        case HTTP_COPY: return "copy";
        case HTTP_LOCK: return "lock";
        case HTTP_MKCOL: return "mkcol";
        case HTTP_MOVE: return "move";
        case HTTP_PROPFIND: return "propfind";
        case HTTP_PROPPATCH: return "proppatch";
        case HTTP_UNLOCK: return "unlock";
        default: return "undefined";
    }
}

static void pushCallbacks(lua_State* L) {
    lua_getfield(L, LUA_ENVIRONINDEX, "__callbacks");
}

static int l_parsertostring(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    if (p->type == 0)
        lua_pushfstring(L, "<hyperparser request>");
    else
        lua_pushfstring(L, "<hyperparser response>");
    return 1;
}

static int l_create(lua_State* L) {
    const char* type_str = luaL_checkstring(L, 1);
    int type = getRequestTypeFlags(L, type_str);
    
    http_parser* p = lua_newuserdata(L, sizeof(http_parser));
    http_parser_init(p, type);
    p->data = L; /* Associate Lua state with parser */
    
    luaL_getmetatable(L, "hyperparser.parser");
    lua_setmetatable(L, -2);
    
    return 1;
}

static void l_push_callback(lua_State* L, http_parser* p, const char* type) {
    pushCallbacks(L);
    lua_pushlightuserdata(L, p);
    lua_gettable(L, -2);
    lua_getfield(L, -1, type);
}

static int l_http_data_cb(http_parser* p, const char *at, size_t length, const char* type) {
    lua_State* L = (lua_State*)p->data;
    l_push_callback(L, p, type);
    
    if (! lua_isnoneornil(L, -1)) {
        luaL_checktype(L, -1, LUA_TFUNCTION);
        lua_pushlstring(L, at, length);
        int ret = lua_pcall(L, 1, 0, 0);
        if (ret != 0) {
            printf("Error in http_data_cb.\n");
        }
    }
    
    
    return 0;
}

static int l_http_cb (http_parser* p, const char* type) {
    lua_State* L = (lua_State*)p->data;
    l_push_callback(L, p, type);
    
    if (! lua_isnoneornil(L, -1)) {
        luaL_checktype(L, -1, LUA_TFUNCTION);
        int ret = lua_pcall(L, 0, 0, 0);
        if (ret != 0) {
            printf("Error in http_cb (%d).\n", ret);
        }
    }
    
    return 0;
}

static int l_msgbegin(http_parser* p) {
    return l_http_cb(p, "msgbegin");
}

static int l_headerscomplete(http_parser* p) {
    return l_http_cb(p, "headerscomplete");
}

static int l_msgcomplete(http_parser* p) {
    return l_http_cb(p, "msgcomplete");
}

static int l_url(http_parser* p, const char *at, size_t length) {
    return l_http_data_cb(p, at, length, "url");
}

static int l_headerfield(http_parser* p, const char *at, size_t length) {
    return l_http_data_cb(p, at, length, "headerfield");
}

static int l_headervalue(http_parser* p, const char *at, size_t length) {
    return l_http_data_cb(p, at, length, "headervalue");
}

static int l_body(http_parser* p, const char *at, size_t length) {
    return l_http_data_cb(p, at, length, "body");
}

static int l_execute(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    luaL_checktype(L, 2, LUA_TTABLE);
    size_t len = 0;
    const char *data = luaL_checklstring(L, 3, &len);
    
    pushCallbacks(L);
    lua_pushlightuserdata(L, p);
    lua_pushvalue(L, 2);
    /* Sets a new table like: __callbacks[*p] = {}
       In this table I will put callbacks for parser p. */
    lua_settable(L, -3);
    
    struct http_parser_settings settings;
    settings.on_message_begin = NULL;
    settings.on_url = NULL;
    settings.on_header_field = NULL;
    settings.on_header_value = NULL;
    settings.on_headers_complete = NULL;
    settings.on_body = NULL;
    settings.on_message_complete = NULL;
    
    lua_pushnil(L);
    while(lua_next(L, 2) != 0) {
        lua_pop(L, 1); /* pops value */
        const char* key = lua_tostring(L, -1);
        
        if (strncmp(key, "msgbegin", 9) == 0)
            settings.on_message_begin = l_msgbegin;
        else if (strncmp(key, "url", 4) == 0)
            settings.on_url = l_url;
        else if (strncmp(key, "headerfield", 12) == 0)
            settings.on_header_field = l_headerfield;
        else if (strncmp(key, "headervalue", 12) == 0)
            settings.on_header_value = l_headervalue;
        else if (strncmp(key, "headerscomplete", 16) == 0)
            settings.on_headers_complete = l_headerscomplete;
        else if (strncmp(key, "body", 5) == 0)
            settings.on_body = l_body;
        else if (strncmp(key, "msgcomplete", 12) == 0)
            settings.on_message_complete = l_msgcomplete;
        else
            return luaL_error(L, "Callback '%s' is not available (misspelled name?)", key);
    }
    
    ssize_t nparsed = http_parser_execute(p, &settings, data, len);
    lua_pushnumber(L, nparsed);
    
    return 1;
}

/*========== Hyperparser Attributes ================*/
static int l_upgrade_a(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    if (p->upgrade)
        lua_pushboolean(L, 1);
    else
        lua_pushboolean(L, 0);
    
    return 1;
}

static int l_statuscode_a(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    lua_pushinteger(L, p->status_code);
    
    return 1;
}

static int l_httpmajor_a(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    lua_pushinteger(L, p->http_major);
    
    return 1;
}

static int l_method_a(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    lua_pushstring(L, getHttpMethodEnum(p->method));
    
    return 1;
}

static int l_httpminor_a(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    lua_pushinteger(L, p->http_minor);
    
    return 1;
}

static int l_contentlength_a(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    lua_pushinteger(L, p->content_length);
    
    return 1;
}

static int l_nread_a(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    lua_pushinteger(L, p->nread);
    
    return 1;
}

static int l_parsergc(lua_State* L) {
    http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
    
    pushCallbacks(L);
    lua_pushlightuserdata(L, p);
    lua_pushnil(L);
    lua_settable(L, -3);
    
    return 0;
}

static const struct luaL_Reg hyperlib [] = {
    {"new", l_create},
    {NULL, NULL}
};

/* (Meta)methods for hyperparser object */
static const struct luaL_Reg hyperlib_m [] = {
    {"__tostring", l_parsertostring},
    {"execute", l_execute},
    {"isupgrade", l_upgrade_a},
    {"statuscode", l_statuscode_a},
    {"method", l_method_a},
    {"httpmajor", l_httpmajor_a},
    {"httpminor", l_httpminor_a},
    {"contentlength", l_contentlength_a},
    {"nread", l_nread_a},
    {NULL, NULL}
};


/**
 * Register functions to lua_State.
 */
LUALIB_API int luaopen_hyperparser(lua_State* L) {
    lua_newtable(L);
    lua_newtable(L);
    lua_setfield(L, -2, "__callbacks");
    lua_replace(L, LUA_ENVIRONINDEX);
    
    luaL_newmetatable(L, "hyperparser.parser");
    
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, l_parsergc);
    lua_settable(L, -3);
    
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, hyperlib_m);

    luaL_register(L, "hyperparser", hyperlib);
    
    return 1;
}