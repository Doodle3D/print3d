package.cpath = package.cpath .. ';build/Debug/frontends/lua/?.so;../build/Debug/frontends/lua/?.so'

local p3d = require("print3d")

local printer = p3d.getPrinter("tty.usbmodem1a21")

local rv,msg


rv,msg = printer:getId()
if rv then
	print("printer:getId returned '" .. rv .. "'")
else
	print("printer:getId returned false/nil");
end


rv,msg = printer:getState()
if rv then
	print("printer:getState returned '" .. rv .. "'")
else
	print("printer:getState returned false/nil");
end


rv,msg = printer:getTemperatures()
if rv then
	if type(rv) == 'table' then
		print("printer:getTemperatures() returned the following table:\n")
		for k,v in pairs(rv) do
			print("\t[" .. k .. "] => '" .. v .. "'")
		end
	else
		print("result of printer:getTemperatures(): " .. rv)
	end
else
	print("printer:getTemperatures() returned an error (" .. msg .. ")")
end


rv,msg = printer:heatup(42)
if rv then
	print("requested printer:heatup(42) successfully")
else
	print("printer:heatup(42) returned false/nil");
end


rv,msg = printer:appendGcode("M104 S11")
if rv then
	print("requested printer:appendGcode successfully")
else
	print("printer:appendGcode returned false/nil");
end
rv,msg = printer:startPrint()
if rv then
	print("requested printer:startPrint successfully")
else
	print("printer:startPrint returned false/nil");
end


local v1,v2,v3 = printer:getProgress()
if v1 then
	local percent = v1 / v3 * 100
	print("printer:getProgress returned curr/buff/total: " .. v1 .. "/" .. v2 .. "/" .. v3 .. " (" .. percent .. "%)")
else
	print("printer:getProgress returned false/nil (" .. v2 .. ")");
end


rv,msg = printer:stopPrint()
if rv then print("printer:stopPrint() returned true")
else print("printer:stopPrint() returned false/nil");
end

rv,msg = printer:stopPrint("xyzzy")
if rv then print("printer:stopPrint(\"xyzzy\") returned true")
else print("printer:stopPrint(\"xyzzy\") returned false/nil");
end
