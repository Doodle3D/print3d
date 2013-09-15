package.cpath = package.cpath .. ';build/Debug/frontends/lua/?.so;../build/Debug/frontends/lua/?.so'

local p3d = require("print3d")
local exv = 1

local printer = p3d.getPrinter("xyz")
local rv,msg = printer:getTemperature()

if rv then
	print("result of printer:getTemperature(): " .. rv)
else
	print("printer:getTemperature() returned an error (" .. msg .. ")")
end

--local rv,msg = printer:setTemperatureCheckInterval(42)
--
--if rv then
--	print("result of printer:setTemperatureCheckInterval(42): " .. rv)
--else
--	print("printer:setTemperatureCheckInterval(42) returned false");
--end
