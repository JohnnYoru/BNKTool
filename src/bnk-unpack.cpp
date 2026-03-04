#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

template<typename T> T read_le(const uint8_t* p) {
    T v{}; std::memcpy(&v, p, sizeof(T)); return v;
}

static std::string hex8(uint32_t v) {
    std::ostringstream o;
    o << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << v;
    return o.str();
}

static std::vector<uint8_t> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + p.string());
    f.seekg(0, std::ios::end);
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

static void write_file(const fs::path& p, const void* data, size_t sz) {
    std::ofstream f(p, std::ios::binary);
    if (f) f.write(reinterpret_cast<const char*>(data), sz);
}

void extract_bnk(const fs::path& bnk_path, const fs::path& out_root) {
    auto raw = read_file(bnk_path);
    const uint8_t* b = raw.data();
    size_t n = raw.size();
    std::string bnk_name = bnk_path.stem().string();

    fs::path bank_dir = out_root / bnk_name;
    fs::path stream_dir = bank_dir / "bank_streams";
    fs::create_directories(stream_dir);

    std::ostringstream sinfo, data_txt;
    size_t i = 0;
    while (i + 8 <= n) {
        uint32_t sec_sz = read_le<uint32_t>(b + i + 4);
        if (std::memcmp(b + i, "DIDX", 4) == 0) {
            uint32_t num_entries = sec_sz / 12;
            size_t data_idx = 0;
            for(size_t search = 0; search + 8 <= n; search++) {
                if(std::memcmp(b + search, "DATA", 4) == 0) {
                    data_idx = search + 8;
                    break;
                }
            }

            if (data_idx != 0) {
                for (uint32_t e = 0; e < num_entries; e++) {
                    size_t entry_off = i + 8 + (e * 12);
                    uint32_t id = read_le<uint32_t>(b + entry_off);
                    uint32_t off = read_le<uint32_t>(b + entry_off + 4);
                    uint32_t sz = read_le<uint32_t>(b + entry_off + 8);

                    write_file(stream_dir / (std::to_string(id) + ".wem"), b + data_idx + off, sz);

                    sinfo << "Section#: " << e << "\n\n"
                    << "Sound File Id: " << hex8(id) << "\n"
                    << "File Offset:   " << hex8(off) << "\n"
                    << "File length:   " << hex8(sz) << "\n";
                    data_txt << id << "=#\n";
                }
                sinfo << "Section#: " << num_entries << "\n";

                std::ofstream f1(stream_dir / "stream_info.txt"); f1 << sinfo.str();
                std::ofstream f2(bank_dir / "data.txt"); f2 << data_txt.str();
            }
            break;
        }
        i += 8 + sec_sz;
    }
}

int main() {
    try {
        fs::path in_dir = "BNK-Input";
        fs::path out_dir = "WEM";
        fs::create_directories(in_dir);
        if (!fs::exists(in_dir)) return 0;

        for (const auto& entry : fs::directory_iterator(in_dir)) {
            if (entry.path().extension() == ".bnk") {
                extract_bnk(entry.path(), out_dir);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
