-- Instruction trace for main CPU (PC + regs + cycles).
local cpu = manager.machine.devices[":maincpu"]
local f = io.open("trace/mame_main.log", "w")

emu.register_periodic(function()
  local s = cpu.state
  f:write(string.format(
    "PC=%04X A=%02X B=%02X X=%04X Y=%04X U=%04X S=%04X CC=%02X total=%d\n",
    s["PC"].value, s["A"].value, s["B"].value, s["X"].value, s["Y"].value,
    s["U"].value, s["S"].value, s["CC"].value, s["TOTALCYCLES"] and s["TOTALCYCLES"].value or 0
  ))
end)

emu.register_stop(function()
  f:close()
end)
