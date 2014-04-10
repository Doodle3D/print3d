-- TODO: replace with actual test library? (busted, lunatest, ...)
package.cpath = package.cpath .. ';build/Debug/frontends/lua/?.so;../build/Debug/frontends/lua/?.so'

local p3d = require("print3d")

local printer = p3d.getPrinter("tty.usbmodem1a21")
local rv,msg

local function assessResult(rv, msg, expected, testName)
	if rv == expected then
		print(testName .. ": ok")
		return true
	else
		print(testName .. ": failed (" .. msg .. ")")
		return false
	end
end


rv,msg = printer:appendGcode()
assessResult(rv, msg, nil, "append_no_gcode")

rv,msg = printer:appendGcode("M104 S11")
assessResult(rv, msg, true, "append_no_metadata")

rv,msg = printer:appendGcode("M104 S11", { seq_number  = 0, seq_total = 3 })
assessResult(rv, msg, true, "append_0_of_3_no_src")

--rv,msg = printer:appendGcode("M104 S11", { seq_number  = 0, seq_total = 3, source = "there" })
--assessResult(rv, msg, true, "append_0_of_3_src1")
