/* 
 * Copyright (c) 2010 Mateusz Armatys
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include "http_parser.h"

static int getRequestTypeFlags(lua_State* L, const char* name) {
	if (strncmp(name, "request", 4) == 0) return HTTP_REQUEST;
	if (strncmp(name, "response", 4) == 0) return HTTP_RESPONSE;
	
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
	lua_remove(L, lua_gettop(L)-1);
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
	p->data = L; //Associate Lua state with parser
	
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

static int l_path(http_parser* p, const char *at, size_t length) {
	return l_http_data_cb(p, at, length, "path");
}

static int l_querystring(http_parser* p, const char *at, size_t length) {
	return l_http_data_cb(p, at, length, "querystring");
}

static int l_url(http_parser* p, const char *at, size_t length) {
	return l_http_data_cb(p, at, length, "url");
}

static int l_fragment(http_parser* p, const char *at, size_t length) {
	return l_http_data_cb(p, at, length, "fragment");
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
	const char *data = luaL_checkstring(L, 3);
	size_t len = 0;
	
	if (!lua_isnoneornil(L, 4)) {
		len = luaL_checknumber(L, 4);
	}
	
	pushCallbacks(L);
	lua_pushlightuserdata(L, p);
	lua_pushvalue(L, 2);
	/* Sets a new table like: hyperparser.__callbacks[*p] = {}
	   In this table I will hold callbacks for parser p. */
	lua_settable(L, -3);
	
	struct http_parser_settings settings;
	settings.on_message_begin = NULL;
	settings.on_path = NULL;
	settings.on_query_string = NULL;
	settings.on_url = NULL;
	settings.on_fragment = NULL;
	settings.on_header_field = NULL;
	settings.on_header_value = NULL;
	settings.on_headers_complete = NULL;
	settings.on_body = NULL;
	settings.on_message_complete = NULL;
	
	lua_pushnil(L);
	while(lua_next(L, 2) != 0) {
		lua_pop(L, 1); //pops value
		const char* key = lua_tostring(L, -1);
		
		if (strncmp(key, "msgbegin", 9) == 0)
			settings.on_message_begin = l_msgbegin;
		else if (strncmp(key, "path", 5) == 0)
			settings.on_path = l_path;
		else if (strncmp(key, "querystring", 12) == 0)
			settings.on_query_string = l_querystring;
		else if (strncmp(key, "url", 4) == 0)
			settings.on_url = l_url;
		else if (strncmp(key, "fragment", 9) == 0)
			settings.on_fragment = l_fragment;
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
			return luaL_error(L, "Callback '%s' is not available (mispelled name?)", key);
	}
    
	ssize_t nparsed = http_parser_execute(p, settings, data, len);
	lua_pushnumber(L, nparsed);
	
	return 1;
}

//========== Hyperparser Attributes ================
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

static int l_bodyread_a(lua_State* L) {
	http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
	lua_pushinteger(L, p->body_read);
	
	return 1;
}

static int l_pushstrparam(lua_State* L, const char* s, ssize_t len) {
	lua_pushlstring(L, s, len);
	
	return 1;
}

static int l_headerfield_a(lua_State* L) {
	http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
	return l_pushstrparam(L, p->header_field_mark, p->header_field_size);
}

static int l_headervalue_a(lua_State* L) {
	http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
	return l_pushstrparam(L, p->header_value_mark, p->header_value_size);
}

static int l_querystring_a(lua_State* L) {
	http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
	return l_pushstrparam(L, p->query_string_mark, p->query_string_size);
}

static int l_path_a(lua_State* L) {
	http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
	return l_pushstrparam(L, p->path_mark, p->path_size);
}

static int l_url_a(lua_State* L) {
	http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
	return l_pushstrparam(L, p->url_mark, p->url_size);
}

static int l_fragment_a(lua_State* L) {
	http_parser* p = (http_parser*)luaL_checkudata(L, 1, "hyperparser.parser");
	return l_pushstrparam(L, p->fragment_mark, p->fragment_size);
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
	{"bodyread", l_bodyread_a},
	{"nread", l_nread_a},
	{"headerfield", l_headerfield_a},
	{"headervalue", l_headervalue_a},
	{"querystring", l_querystring_a},
	{"path", l_path_a},
	{"url", l_url_a},
	{"fragment", l_fragment_a},
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
	
	/*lua_newtable(L);
	lua_pushstring(L, "kv");
	lua_setfield(L, -2, "__mode");
	lua_setmetatable(L, -2);*/
	
	return 1;
}