/* Copyright (c) 2010-2012 Mateusz Armatys
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
#include <stdlib.h>
#include "http_parser.h"

static const char* ParserUDataName = "hyperparser.parser";
static const char* HyperParserCallbacksName = "__hyperparser_callbacks";

typedef struct parser_context {
  lua_State* L;
  struct http_parser_settings* settings;
} parser_context;

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
  lua_getfield(L, LUA_ENVIRONINDEX, HyperParserCallbacksName);
}

static int l_parsertostring(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  if (p->type == 0)
    lua_pushfstring(L, "<hyperparser request>");
  else
    lua_pushfstring(L, "<hyperparser response>");
  return 1;
}

static void l_push_callback(lua_State* L, http_parser* p, const char* type) {
  pushCallbacks(L);
  lua_pushlightuserdata(L, p);
  lua_gettable(L, -2);
  lua_getfield(L, -1, type);
}

static int l_http_data_cb(http_parser* p, const char *at, size_t length, const char* type) {
  parser_context* ctx = (parser_context*) p->data;
  lua_State* L = ctx->L;
  l_push_callback(L, p, type);

  if (! lua_isnoneornil(L, -1)) {
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushlstring(L, at, length);
    lua_call(L, 1, 0);
  }

  return 0;
}

static int l_http_cb(http_parser* p, const char* type) {
  parser_context* ctx = (parser_context*) p->data;
  lua_State* L = ctx->L;
  l_push_callback(L, p, type);
  
  if (! lua_isnoneornil(L, -1)) {
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_call(L, 0, 0);
  }
  
  return 0;
}

static int l_msgbegin(http_parser* p) {
  return l_http_cb(p, "msgbegin");
}

static int l_statuscomplete(http_parser* p) {
  return l_http_cb(p, "statuscomplete");
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

static int l_create(lua_State* L, int type) {
  luaL_checktype(L, 1, LUA_TTABLE); // settings table

  // Create http_parser
  http_parser* p = lua_newuserdata(L, sizeof(http_parser));
  http_parser_init(p, type);
  luaL_getmetatable(L, ParserUDataName);
  lua_setmetatable(L, -2);

  // store settings in HyperParserCallbacksName[*p]
  pushCallbacks(L);
  lua_pushlightuserdata(L, p); // push key
  lua_pushvalue(L, 1); // push settings table - value
  lua_settable(L, -3); // set the field of HyperParserCallbacksName table
  lua_pop(L, 1); // pop the callbacks table
  
  // Parse settings callbacks
  struct http_parser_settings* settings = (struct http_parser_settings*) malloc(sizeof(struct http_parser_settings));
  settings->on_message_begin = NULL;
  settings->on_status_complete = NULL;
  settings->on_url = NULL;
  settings->on_header_field = NULL;
  settings->on_header_value = NULL;
  settings->on_headers_complete = NULL;
  settings->on_body = NULL;
  settings->on_message_complete = NULL;

  lua_pushnil(L); // the first key
  while(lua_next(L, 1) != 0) {
    lua_pop(L, 1); /* pops value */
    const char* key = lua_tostring(L, -1);
    
    if (strncmp(key, "msgbegin", 9) == 0)
      settings->on_message_begin = l_msgbegin;
    else if (strncmp(key, "url", 4) == 0)
      settings->on_url = l_url;
    else if (strncmp(key, "headerfield", 12) == 0)
      settings->on_header_field = l_headerfield;
    else if (strncmp(key, "headervalue", 12) == 0)
      settings->on_header_value = l_headervalue;
    else if (strncmp(key, "headerscomplete", 16) == 0)
      settings->on_headers_complete = l_headerscomplete;
    else if (strncmp(key, "body", 5) == 0)
      settings->on_body = l_body;
    else if (strncmp(key, "msgcomplete", 12) == 0)
      settings->on_message_complete = l_msgcomplete;
    else if (strncmp(key, "statuscomplete", 15) == 0)
      settings->on_status_complete = l_statuscomplete;
    else
      return luaL_error(L, "Callback '%s' is not available (misspelled name?)", key);
  }

  parser_context* ctx = (parser_context*) malloc(sizeof(parser_context));
  ctx->L = L;
  ctx->settings = settings;
  p->data = ctx;

  lua_pushvalue(L, 2); // push the http_parser userdata
  return 1;
}

static int l_create_request(lua_State* L) {
  return l_create(L, HTTP_REQUEST);
}

static int l_create_response(lua_State* L) {
  return l_create(L, HTTP_RESPONSE);
}

static int l_execute(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  size_t len = 0;
  const char *data = luaL_checklstring(L, 2, &len);
  parser_context* ctx = (parser_context*) p->data;

  size_t nparsed = http_parser_execute(p, ctx->settings, data, len);
  lua_pushnumber(L, nparsed);
  
  return 1;
}

/*========== Hyperparser Attributes ================*/
static int l_upgrade_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  if (p->upgrade)
    lua_pushboolean(L, 1);
  else
    lua_pushboolean(L, 0);

  return 1;
}

static int l_statuscode_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  lua_pushinteger(L, p->status_code);
  return 1;
}

static int l_httpmajor_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  lua_pushinteger(L, p->http_major);
  return 1;
}

static int l_method_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  lua_pushstring(L, getHttpMethodEnum(p->method));
  return 1;
}

static int l_httpminor_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  lua_pushinteger(L, p->http_minor);
  return 1;
}

static int l_contentlength_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  lua_pushinteger(L, p->content_length);
  return 1;
}

static int l_nread_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  lua_pushinteger(L, p->nread);
  return 1;
}

static int l_shouldkeepalive_a(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  int shouldKeepAlive = http_should_keep_alive(p);
  lua_pushboolean(L, shouldKeepAlive);
  return 1;
}

static int l_parsergc(lua_State* L) {
  http_parser* p = (http_parser*)luaL_checkudata(L, 1, ParserUDataName);
  parser_context* ctx = (parser_context*) p->data;
  free(ctx->settings);
  free(ctx);
  pushCallbacks(L);
  lua_pushlightuserdata(L, p);
  lua_pushnil(L);
  lua_settable(L, -3);
  return 0;
}

#define hasField(u, field) ((u)->field_set && (1 << (field)))

inline int checkField(lua_State *L, const char* url, const struct http_parser_url *u, enum http_parser_url_fields field) {
  if (hasField(u, field)) {
    const uint16_t offset = u->field_data[field].off;
    const uint16_t len = u->field_data[field].len;
    const char* s = url + offset;
    if (len > 0) {
      lua_pushlstring(L, s, len);
      return 1;
    }
  }
  return 0;
}

static int l_parse_url(lua_State* L) {
  size_t len = 0;
  const char* buf = luaL_checklstring(L, 1, &len);
  int isconnect = 0;
  if (lua_isboolean(L, 2)) {
    isconnect = lua_toboolean(L, 2);
  }

  lua_settop(L, 0);

  struct http_parser_url u;
  http_parser_parse_url(buf, len, isconnect, &u);
  lua_newtable(L);

  if (hasField(&u, UF_PORT) && u.port) {
    lua_pushnumber(L, u.port);
    lua_setfield(L, 1, "port");
  }

  if (checkField(L, buf, &u, UF_SCHEMA)) {
    lua_setfield(L, 1, "schema");
  }

  if (checkField(L, buf, &u, UF_HOST)) {
    lua_setfield(L, 1, "host");
  }

  if (checkField(L, buf, &u, UF_PATH)) {
    lua_setfield(L, 1, "path");
  }

  if (checkField(L, buf, &u, UF_QUERY)) {
    lua_setfield(L, 1, "query");
  }

  if (checkField(L, buf, &u, UF_FRAGMENT)) {
    lua_setfield(L, 1, "fragment");
  }

  if (checkField(L, buf, &u, UF_USERINFO)) {
    lua_setfield(L, 1, "userinfo");
  }

  return 1;
}

static const struct luaL_Reg hyperlib [] = {
  {"request", l_create_request},
  {"response", l_create_response},
  {"parseurl", l_parse_url},
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
  {"shouldkeepalive", l_shouldkeepalive_a},
  {NULL, NULL}
};

/**
 * Register functions to lua_State.
 */
LUALIB_API int luaopen_hyperparser(lua_State* L) {
  lua_newtable(L);
  lua_newtable(L);
  lua_setfield(L, -2, HyperParserCallbacksName);
  lua_replace(L, LUA_ENVIRONINDEX);
  
  /* Metatable for http parser */
  luaL_newmetatable(L, ParserUDataName);

  lua_pushstring(L, "__mode");
  lua_pushstring(L, "k");
  lua_settable(L, -3);
  
  lua_pushstring(L, "__gc");
  lua_pushcfunction(L, l_parsergc);
  lua_settable(L, -3);
  
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_register(L, NULL, hyperlib_m);

  /* Main functions */
  lua_newtable(L);
  luaL_register(L, NULL, hyperlib);
  
  return 1;
}
