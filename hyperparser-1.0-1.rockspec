package = "hyperparser"
version = "1.0-1"

source = {
  url = "https://github.com/armatys/hyperparser"
}

description = {
  summary = "Socket utilities",
  detailed = [[
    Lua bindings to http parser library.
  ]],
  homepage = "https://github.com/armatys/hyperparser",
  license = "MIT/X11"
}

dependencies = {
  "lua >= 5.1"
}

supported_platforms = { "macosx", "freebsd", "linux" }

build = {
  type = "builtin",
  modules = {
    hyperparser = {
      sources = {
        "src/http_parser.c",
        "src/main.c",
      }
    }
  }
}
