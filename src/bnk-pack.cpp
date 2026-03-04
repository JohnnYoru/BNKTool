#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstring>

namespace fs = std::filesystem;

struct RawSection { std::string tag; std::vector<uint8_t> body; };

template<typename T> void write_le(std::vector<uint8_t>& b, T v) {
    uint8_t t[sizeof(T)]; std::memcpy(t, &v, sizeof(T));
    b.insert(b.end(), t, t + sizeof(T));
}

void pack_bnk(const fs::path& orig_bnk, const fs::path& wem_dir, const fs::path& out_file) {
    std::ifstream f(orig_bnk, std::ios::binary);
    if (!f) return;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    std::vector<RawSection> sections;
    std::vector<uint32_t> ids;

    size_t i = 0;
    while (i + 8 <= raw.size()) {
        char tag[5] = {0}; std::memcpy(tag, raw.data() + i, 4);
        uint32_t sz = *(uint32_t*)(raw.data() + i + 4);
        if (std::string(tag) == "DIDX") {
            for(uint32_t e=0; e < sz/12; e++)
                ids.push_back(*(uint32_t*)(raw.data() + i + 8 + (e*12)));
        } else if (std::string(tag) != "DATA") {
            RawSection s; s.tag = tag;
            s.body.assign(raw.begin() + i + 8, raw.begin() + i + 8 + sz);
            sections.push_back(s);
        }
        i += 8 + sz;
    }

    std::vector<uint8_t> data_body, didx_body;
    for (size_t idx = 0; idx < ids.size(); idx++) {
        uint32_t id = ids[idx];
        fs::path wem_path = wem_dir / "bank_streams" / (std::to_string(id) + ".wem");

        std::ifstream wf(wem_path, std::ios::binary);
        if (!wf) throw std::runtime_error("Missing WEM: " + wem_path.string());

        std::vector<uint8_t> wdata((std::istreambuf_iterator<char>(wf)), std::istreambuf_iterator<char>());

        uint32_t current_off = (uint32_t)data_body.size();
        write_le(didx_body, id);
        write_le(didx_body, current_off);
        write_le(didx_body, (uint32_t)wdata.size());

        data_body.insert(data_body.end(), wdata.begin(), wdata.end());

        if (idx < ids.size() - 1 && data_body.size() % 16) {
            size_t pad = 16 - (data_body.size() % 16);
            for(size_t p=0; p<pad; p++) data_body.push_back(0);
        }
    }

    std::ofstream out(out_file, std::ios::binary);
    auto write_sec = [&](const std::string& tag, const std::vector<uint8_t>& b) {
        out.write(tag.c_str(), 4);
        uint32_t s = (uint32_t)b.size();
        out.write((char*)&s, 4);
        out.write((char*)b.data(), s);
    };

    for(auto& s : sections) if(s.tag == "BKHD") write_sec(s.tag, s.body);
    write_sec("DIDX", didx_body);
    write_sec("DATA", data_body);
    for(auto& s : sections) if(s.tag != "BKHD") write_sec(s.tag, s.body);
}

int main() {
    try {
        fs::path in_bnk = "BNK-Input";
        fs::path in_wem = "WEM";
        fs::path out_dir = "BNK-Output";
        fs::create_directories(out_dir);

        if (!fs::exists(in_wem)) return 0;

        for (const auto& entry : fs::directory_iterator(in_wem)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                fs::path original = in_bnk / (name + ".bnk");
                if (fs::exists(original)) {
                    pack_bnk(original, entry.path(), out_dir / (name + ".bnk"));
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
