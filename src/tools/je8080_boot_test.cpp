#include "jeLib/je8086.h"
#include "jeLib/romloader.h"
#include "jeLib/sysexRemoteControl.h"
#include "synthLib/midiTypes.h"
#include "../dsp/je8080_rack_image.h"
#include <cstdio>
#include <string>
#include <cmath>
#include <vector>
#include <cstdint>
int main(int argc, char** argv) {
    synthLib::RomLoader::addSearchPath(argv[1]);
    jeLib::Rom rom;
    for (auto& r : jeLib::RomLoader::findROMs())
        if (r.isValid() && r.getDeviceType() == jeLib::DeviceType::Rack) rom = r;
    if (!rom.isValid()) { puts("no rack rom"); return 1; }
    const auto img = je8080::rack_boot_image(rom.getData());
    if (img.empty()) { puts("not a 917504-byte rack image"); return 1; }
    std::string ram = std::string(argv[1]) + "/ram_dump.bin";
    long steps = argc > 2 ? atol(argv[2]) : 20000000;
    auto* je = new jeLib::Je8086(img, ram);
    if (je->hasDoneFactoryReset()) { puts("factory reset done, recreating"); delete je; je = new jeLib::Je8086(img, ram); }
    jeLib::SysexRemoteControl remote;
    std::string last;
    remote.evLcdDdDataChanged.addListener([&](const std::array<char, 40>& c) {
        char l[41]{0}; for (int i = 0; i < 40; i++) l[i] = c[i] >= ' ' ? c[i] : ' ';
        std::string s(l); if (s != last) { last = s; printf("LCD [%s] [%s]\n", s.substr(0,16).c_str(), s.substr(20,16).c_str()); fflush(stdout);} });
    double peak = 0; long samples = 0; bool noteOn = false;
    std::vector<int16_t> wav;
    for (long i = 0; i < steps; i++) {
        je->step();
        auto& buf = je->getSampleBuffer();
        for (auto& s : buf) { double v = std::fabs((double)s.first) + std::fabs((double)s.second); if (noteOn && v > peak) peak = v;
            if (noteOn) { wav.push_back((int16_t)std::max(-32768, std::min(32767, s.first >> 8))); wav.push_back((int16_t)std::max(-32768, std::min(32767, s.second >> 8))); } }
        samples += buf.size(); if (!buf.empty()) je->clearSampleBuffer();
        std::vector<synthLib::SMidiEvent> out; je->readMidiOut(out); for (auto& e : out) remote.receive(e);
        if (!noteOn && samples > 88200 * 8) {
            for (int ch = 0; ch < 16; ch++) { synthLib::SMidiEvent ev(synthLib::MidiEventSource::Host, (uint8_t)(0x90 | ch), 69, 100); je->addMidiEvent(ev); }
            noteOn = true; printf("note on at %ld samples\n", samples); fflush(stdout);
        }
        if (noteOn && samples > 88200 * 11) break;
    }
    printf("samples=%ld (%.1fs) peak_after_note=%g\n", samples, samples / 88200.0, peak);
    if (argc > 3) {   /* optional: write what followed the note as a 16-bit stereo wav */
        FILE* f = fopen(argv[3], "wb");
        if (f) {
            const uint32_t rate = 88200, bytes = (uint32_t)(wav.size() * 2);
            auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
            auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
            fwrite("RIFF", 1, 4, f); u32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
            u32(16); u16(1); u16(2); u32(rate); u32(rate * 4); u16(4); u16(16);
            fwrite("data", 1, 4, f); u32(bytes); fwrite(wav.data(), 2, wav.size(), f); fclose(f);
            printf("wrote %s\n", argv[3]);
        }
    }
}
