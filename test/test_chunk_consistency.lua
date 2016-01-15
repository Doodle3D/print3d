-- TODO: replace with actual test library? (busted, lunatest, ...)
-- TODO: start and stop own print server
package.cpath = package.cpath .. ';build/Debug/frontends/lua/?.so;../build/Debug/frontends/lua/?.so'

local p3d = require("print3d")

local FATAL_EXIT_CODE = 2
local rv,msg
local tests_total, tests_success, tests_failed = 0, 0, 0

----- PREPARATION -----

local printer = p3d.getPrinter("tty.usbmodem1a21")
if not printer then
	print("Error: could not instantiate printer object")
	os.exit(FATAL_EXIT_CODE)
end

rv,msg = printer:setLocalLogLevel('quiet')
--rv,msg = printer:setLocalLogLevel('bulk')
if not rv then
	print("Error: could not set log level (" .. msg .. ")")
	os.exit(FATAL_EXIT_CODE)
end


----- TESTING -----

local function assessResult(rv, msg, expected, testName)
	tests_total = tests_total + 1
	if rv == expected and type(rv) == type(expected) then
		tests_success = tests_success + 1

		io.write("+ " .. testName .. ": ok")
		if (msg) then print(" (msg: " .. msg .. ")") else print(" (no msg)") end

		return true
	else
		tests_failed = tests_failed + 1
		print("- " .. testName .. ": failed (" .. msg .. ")")
		return false
	end
end


-- Note: this also resets from last run (if the server works correctly that is)
rv,msg = printer:clearGcode()
assessResult(rv, msg, true, "clear_gcode")

rv,msg = printer:appendGcode()
assessResult(rv, msg, nil, "append_no_gcode")

rv,msg = printer:appendGcode("M104 S01")
assessResult(rv, msg, true, "append_no_metadata")

rv,msg = printer:appendGcode("M104 S02", { seq_number = 0, seq_total = 3 })
assessResult(rv, msg, true, "append_0_of_3_missing")

rv,msg = printer:appendGcode("M104 S03", { seq_number = 1, seq_total = 3, source = "there" })
assessResult(rv, msg, true, "append_1_of_3_src1")

rv,msg = printer:appendGcode("M104 S04", { seq_number = -1, seq_total = 3, source = "there" })
assessResult(rv, msg, false, "append_missing_of_3_src1")

rv,msg = printer:appendGcode("M104 S05", { seq_number = 2, seq_total = -1, source = "there" })
assessResult(rv, msg, false, "append_2_of_missing_src1")

rv,msg = printer:appendGcode("M104 S06", { seq_number = 2, seq_total = 3, source = "here" })
assessResult(rv, msg, false, "append_2_of_3_src2")

rv,msg = printer:appendGcode("M104 S07", { seq_number = 3, seq_total = 3, source = "there" })
assessResult(rv, msg, false, "append_3_of_3_src1")

rv,msg = printer:clearGcode()
assessResult(rv, msg, true, "clear_gcode")

rv,msg = printer:appendGcode("M105 S01", { seq_number = 0, seq_total = 5 })
assessResult(rv, msg, true, "append_0_of_5_missing")


----- SUMMARY -----

print("\nTotal tests: " .. tests_total .. ", successful: " .. tests_success .. ", failed: " .. tests_failed)
os.exit(0)
