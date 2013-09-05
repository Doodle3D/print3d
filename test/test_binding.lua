package.cpath = package.cpath .. ';build/Debug/frontends/lua/?.so;../build/Debug/frontends/lua/?.so'

local p3d = require("print3d")
local exv = 1

local rv,msg = p3d.getTemperature("xyz")

if rv then
	print("result of p3d.getTemperature(\"xyz\"): " .. rv)
else
	print("p3d.getTemperature() returned an error (" .. msg .. ")")
end

local rv,msg = p3d.setTemperatureCheckInterval("xyz", 42)

if rv then
	print("result of p3d.setTemperatureCheckInterval(\"xyz\", 42): " .. rv)
else
	print("p3d.setTemperatureCheckInterval(\"xyz\", 42) returned false");
end
