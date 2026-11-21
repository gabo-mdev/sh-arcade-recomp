#include "machine/machine.hpp"
#include "machine/romset.hpp"

#include <cstdio>
#include <unordered_set>

int main(int argc, char** argv) {
  const int frames = argc > 1 ? std::atoi(argv[1]) : 120;
  auto roms = sh::RomSet::load("rom");
  sh::MachineConfig cfg;
  cfg.headless = true;
  sh::Machine m(cfg);
  m.load(roms);
  m.reset();

  std::printf("FFFE=%02X%02X E000=%02X PC=%04X\n", m.c117().read(sh::C117::Main, 0xFFFE),
              m.c117().read(sh::C117::Main, 0xFFFF), m.c117().read(sh::C117::Main, 0xE000),
              m.maincpu().pc);

  for (int i = 0; i < 10; ++i) {
    const sh::u16 pc = m.maincpu().pc;
    m.maincpu().step();
    std::printf("step %d: %04X -> %04X A=%02X B=%02X\n", i, pc, m.maincpu().pc, m.maincpu().a,
                m.maincpu().b);
  }

  std::unordered_set<sh::u16> pcs;
  for (int f = 0; f < frames; ++f) {
    m.run_frame();
    pcs.insert(m.maincpu().pc);
    if (f < 40 || f % 50 == 0)
      std::printf("frame %d: main=%04X sub=%04X X=%04X subres=%u\n", f, m.maincpu().pc,
                  m.subcpu().pc, m.maincpu().x, m.c117().subres_bit());
  }

  size_t nonzero = 0;
  for (auto b : m.video().vram())
    if (b)
      ++nonzero;

  std::printf(
      "after %d frames: main=%04X sub=%04X unique=%zu vram_nz=%zu subres=%u cycles=%llu\n", frames,
      m.maincpu().pc, m.subcpu().pc, pcs.size(), nonzero, m.c117().subres_bit(),
      static_cast<unsigned long long>(m.maincpu().cycles_total));
  return 0;
}
