local hyperparser = require "hyperparser"
  
local settings = {
  msgcomplete = function()
    print("Message completed.")
  end,
   
  headerfield = function(a)
    io.write(a)
  end,
  
  headervalue = function(a)
    io.write(" -> " .. a .. "\n")
  end
}
local parser = hyperparser.request(settings) -- or hyperparser.response(settings)
local req = "GET /index/?key=val&key2=val2 HTTP/1.1\r\nHost: www.example.com\r\nContent-Length: 12\r\n\r\nHello world!"

parser:execute(req)
