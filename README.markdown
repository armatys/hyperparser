Hyperparser - Lua HTTP parser (based on [http-parser](https://github.com/joyent/http-parser))

It runs on *BSD and Linux. Windows was not tested.

### Compilation

Use luarocks to compile and install:

    $ [sudo] luarocks make

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
	
	parser:execute(settings, req)

