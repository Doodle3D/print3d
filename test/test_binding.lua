package.cpath = package.cpath .. ';build/Debug/frontends/lua/?.so;../build/Debug/frontends/lua/?.so'

local p3d = require("print3d")
local exv = 1

local rv,msg

local printer = p3d.getPrinter("xyz")
rv,msg = printer:getTemperature()

if rv then
	if type(rv) == 'table' then
		print("printer:getTemperature() returned the following table:\n")
		for k,v in pairs(rv) do
			print("\t[" .. k .. "] => '" .. v .. "'")
		end
	else
		print("result of printer:getTemperature(): " .. rv)
	end
else
	print("printer:getTemperature() returned an error (" .. msg .. ")")
end


rv,msg = printer:heatup(42)

if rv then
	print("requested printer:heatup(42) successfully")
else
	print("printer:heatup(42) returned false/nil");
end


v1,v2 = printer:getProgress()

if v1 then
	local percent = v1 / v2 * 100
	print("printer:getProgress returned curr/total: " .. v1 .. "/" .. v2 .. "(" .. percent .. "%)")
else
	print("printer:getProgress returned false/nil");
end
