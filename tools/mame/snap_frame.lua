-- Save snapshots at comma-separated frame counts in SH_SNAP_FRAMES.
local targets = {}
for value in string.gmatch(os.getenv("SH_SNAP_FRAMES") or "180", "[^,]+") do
  targets[tonumber(value)] = true
end
local frames = 0
local dump_dir = os.getenv("SH_DUMP_DIR")
local coin_frame = tonumber(os.getenv("SH_COIN_FRAME"))
local start_frame = tonumber(os.getenv("SH_START_FRAME"))
local input_hold = tonumber(os.getenv("SH_INPUT_HOLD")) or 4
local right_frame = tonumber(os.getenv("SH_RIGHT_FRAME"))
local right_hold = tonumber(os.getenv("SH_RIGHT_HOLD")) or 60
local coin_field = manager.machine.ioport.ports[":COIN"]:field(0x10)
local start_field = manager.machine.ioport.ports[":CONTROL0"]:field(0x80)
local right_field = manager.machine.ioport.ports[":CONTROL0"]:field(0x01)

local function update_pulse(field, at, hold, name)
  if not at then return end
  if frames == at then
    field:set_value(1)
    print(name .. " down after frame " .. frames)
  elseif frames == at + hold then
    field:clear_value()
    print(name .. " up after frame " .. frames)
  end
end

local function dump_video(name, base, size)
  local path = string.format("%s/frame_%05d_%s.bin", dump_dir, frames, name)
  local file = assert(io.open(path, "wb"))
  local space = manager.machine.devices[":c117"].spaces["program"]
  local bytes = {}
  for offset = 0, size - 1 do
    bytes[#bytes + 1] = string.char(space:read_u8(base + offset))
    if #bytes == 256 then
      file:write(table.concat(bytes))
      bytes = {}
    end
  end
  if #bytes > 0 then file:write(table.concat(bytes)) end
  file:close()
end

emu.register_frame_done(function()
  frames = frames + 1
  update_pulse(coin_field, coin_frame, input_hold, "coin1")
  update_pulse(start_field, start_frame, input_hold, "start1")
  update_pulse(right_field, right_frame, right_hold, "right1")
  if targets[frames] then
    print("snapshot frame " .. frames)
    local main = manager.machine.devices[":maincpu"].spaces["program"]
    local slot = main:read_u8(0x39)
    local player = 0x011d + slot * 0x40
    print(string.format("state frame %d mode=%02X health=%d hearts=%d lives=%d 147A=%02X 147E=%02X",
      frames, main:read_u8(0x2c), main:read_u8(player + 0x1c),
      main:read_u8(0x0fa2), main:read_u8(player + 0x32), main:read_u8(0x147a),
      main:read_u8(0x147e)))
    manager.machine.video:snapshot()
    if dump_dir then
      dump_video("vram", 0x2f0000, 0x8000)
      dump_video("palette", 0x2e0000, 0x8000)
      dump_video("sprites", 0x2fc800, 0x800)
      dump_video("control", 0x2fd000, 0x20)
    end
  end
end)
