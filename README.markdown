Hyperparser - Lua HTTP parser (based on [http-parser](http://github.com/ry/http-parser))

It runs on *BSD and Linux. Windows was not tested.

### Requirements

- [Premake](http://industriousone.com/premake) - used for Makefile generation

### Compilation

Use [premake](http://industriousone.com/premake) to generate appropriate build files. E.g run `premake4 gmake` to generate a Makefile. Then execute `make config=release32` or `make config=release64` to compile.

### Usage:

The library follows behavior from http-parser C library.

	require "hyperparser"
	
	local parser = hyperparser.new("request") -- or hyperparser.new("response")
	local req = "GET /index/?key=val&key2=val2 HTTP/1.1\r\nHost: www.example.com\r\nContent-Length: 9\r\n\r\n"
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
	
	parser:execute(settings, req, #req+1)

