-- Coverage logger for Splatterhouse main CPU.
-- Usage: mame splatter -rompath rom -debug -debugger none -autoboot_script tools/mame/trace_cov.lua ...

local cpu = manager.machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local f = io.open("trace/coverage_pcs.txt", "w")
local seen = {}
local count = 0

emu.register_periodic(function()
  local pc = cpu.state["PC"].value
  if not seen[pc] then
    seen[pc] = true
    f:write(string.format("%04X\n", pc))
    count = count + 1
  end
end)

emu.register_stop(function()
  f:write(string.format("; unique=%d\n", count))
  f:close()
end)
