#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define IS_TTY() (_isatty(_fileno(stdin)))
#else
#include <unistd.h>
#include <limits.h>
#define IS_TTY() (isatty(STDIN_FILENO))
#endif

namespace fs = std::filesystem;

enum class Out { FULL, QUIET, NONE };
static Out g_out = Out::FULL;

static void emit(Out level, const std::string& msg) {
    if (g_out == Out::NONE) return;
    if (g_out == Out::QUIET && level == Out::FULL) return;
    std::cout << msg;
    std::cout.flush();
}
#define LOG(msg)  emit(Out::FULL,  std::string(msg))
#define INFO(msg) emit(Out::QUIET, std::string(msg))

struct WemEntry {
    uint32_t id, offset, size, padding;
    std::vector<uint8_t> data;
};

struct RawSection {
    std::string tag;
    std::vector<uint8_t> body;
};

struct BnkFile {
    std::string             name;
    std::vector<WemEntry>   wems;
    std::vector<RawSection> other_sections;
};

using DataTxt = std::map<uint32_t, std::string>;

static fs::path get_exe_dir() {
    #ifdef _WIN32
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path).parent_path();
    #else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count > 0) {
        return fs::path(std::string(result, count)).parent_path();
    }
    return fs::current_path();
    #endif
}

static std::string dec_id(uint32_t v) { return std::to_string(v); }

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
    if (!f) throw std::runtime_error("Cannot write: " + p.string());
    f.write(reinterpret_cast<const char*>(data), sz);
}

template<typename T> static T read_le(const uint8_t* p)
{ T v{}; std::memcpy(&v, p, sizeof(T)); return v; }

template<typename T> static void write_le(std::vector<uint8_t>& buf, T v) {
    uint8_t t[sizeof(T)]; std::memcpy(t, &v, sizeof(T));
    buf.insert(buf.end(), t, t + sizeof(T));
}

static bool match_tag(const uint8_t* b, size_t off, const char* tag) {
    return b[off]==(uint8_t)tag[0] && b[off+1]==(uint8_t)tag[1] &&
    b[off+2]==(uint8_t)tag[2] && b[off+3]==(uint8_t)tag[3];
}

static BnkFile parse_bnk(const fs::path& path) {
    BnkFile bnk;
    bnk.name = path.stem().string();
    auto raw = read_file(path);
    const uint8_t* b = raw.data();
    size_t n = raw.size();
    if (n < 8) throw std::runtime_error("File too small");

    size_t i = 0; bool didx = false;
    while (i + 8 <= n) {
        uint32_t sec = read_le<uint32_t>(b + i + 4);
        if (i + 8 + sec > n) { i++; continue; }

        if (match_tag(b, i, "DIDX") && !didx) {
            didx = true;
            uint32_t ne = sec / 12;
            size_t ds = i + 8 + sec;
            if (ds + 8 > n || !match_tag(b, ds, "DATA"))
                throw std::runtime_error("DATA not found after DIDX");
            const uint8_t* db = b + ds + 8;
            size_t eo = i + 8;
            for (uint32_t e = 0; e < ne; e++, eo += 12) {
                WemEntry w;
                w.id     = read_le<uint32_t>(b + eo);
                w.offset = read_le<uint32_t>(b + eo + 4);
                w.size   = read_le<uint32_t>(b + eo + 8);
                w.padding = (!(e == ne - 1) && w.size % 16) ? (16 - w.size % 16) : 0;
                w.data.assign(db + w.offset, db + w.offset + w.size);
                bnk.wems.push_back(std::move(w));
            }
            i = ds + 8 + read_le<uint32_t>(b + ds + 4);
        }
        else if (match_tag(b, i, "DATA")) { i += 8 + sec; }
        else if (match_tag(b,i,"BKHD") || match_tag(b,i,"HIRC") ||
            match_tag(b,i,"ENVS") || match_tag(b,i,"FXPR") ||
            match_tag(b,i,"STID")) {
            RawSection s;
        s.tag = std::string(reinterpret_cast<const char*>(b + i), 4);
        s.body.assign(b + i + 8, b + i + 8 + sec);
        bnk.other_sections.push_back(std::move(s));
        i += 8 + sec;
            } else { i++; }
    }
    return bnk;
}

static void do_extract(const BnkFile& bnk, const fs::path& out_root) {
    fs::path bank_dir = out_root / bnk.name;
    fs::path stream_dir = bank_dir / "bank_streams";
    fs::create_directories(stream_dir);

    std::ostringstream sinfo, data_txt;
    for (size_t i = 0; i < bnk.wems.size(); i++) {
        const auto& w = bnk.wems[i];
        write_file(stream_dir / (dec_id(w.id) + ".wem"), w.data.data(), w.data.size());

        sinfo << "Section#: " << i << "\n\n"
        << "Sound File Id: " << hex8(w.id) << "\n"
        << "File Offset:   " << hex8(w.offset) << "\n"
        << "File length:   " << hex8(w.size) << "\n";
        data_txt << dec_id(w.id) << "=#\n";
    }
    sinfo << "Section#: " << bnk.wems.size() << "\n";

    std::ofstream f1(stream_dir / "stream_info.txt"); f1 << sinfo.str();
    std::ofstream f2(bank_dir / "data.txt"); f2 << data_txt.str();
}

static DataTxt parse_data_txt(const fs::path& path) {
    std::ifstream f(path);
    if (!f) return {};
    DataTxt r; std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('='), hsh = line.find('#');
        if (eq == std::string::npos || hsh == std::string::npos) continue;
        uint32_t id = (uint32_t)std::stoul(line.substr(0, eq));
        r[id] = line.substr(eq+1, hsh-eq-1);
    }
    return r;
}

static void do_pack(const BnkFile& bnk, const fs::path& bank_extracted_dir, const fs::path& output_path) {
    BnkFile p = bnk;
    DataTxt manifest = parse_data_txt(bank_extracted_dir / "data.txt");

    std::vector<uint8_t> data_body;
    for (size_t i = 0; i < p.wems.size(); i++) {
        auto& w = p.wems[i];
        auto it = manifest.find(w.id);
        fs::path wp;

        if (it != manifest.end() && !it->second.empty()) wp = bank_extracted_dir / it->second;
        else wp = bank_extracted_dir / "bank_streams" / (dec_id(w.id) + ".wem");

        if (fs::exists(wp)) {
            w.data = read_file(wp);
            w.size = (uint32_t)w.data.size();
        }

        w.offset = (uint32_t)data_body.size();
        data_body.insert(data_body.end(), w.data.begin(), w.data.end());
        w.padding = (!(i == p.wems.size() - 1) && w.size % 16) ? (16 - w.size % 16) : 0;
        if (w.padding) data_body.insert(data_body.end(), w.padding, 0x00);
    }

    std::vector<uint8_t> didx_body;
    for (const auto& w : p.wems) {
        write_le<uint32_t>(didx_body, w.id);
        write_le<uint32_t>(didx_body, w.offset);
        write_le<uint32_t>(didx_body, w.size);
    }

    std::vector<uint8_t> out;
    auto write_sec = [&](const char* tag, const std::vector<uint8_t>& b) {
        out.insert(out.end(), tag, tag + 4);
        write_le<uint32_t>(out, (uint32_t)b.size());
        out.insert(out.end(), b.begin(), b.end());
    };

    for (const auto& s : p.other_sections) if (s.tag == "BKHD") write_sec("BKHD", s.body);
    if (!didx_body.empty()) { write_sec("DIDX", didx_body); write_sec("DATA", data_body); }
    for (const auto& s : p.other_sections) if (s.tag != "BKHD") write_sec(s.tag.c_str(), s.body);

    fs::create_directories(output_path.parent_path());
    write_file(output_path, out.data(), out.size());
}

static void print_usage() {
    std::cout << "BNKTool - Usage:\n"
    << "  -bi <dir>  BNK Input folder\n"
    << "  -bo <dir>  BNK Output folder\n"
    << "  -wo <dir>  Extracted WEM folder (output)\n"
    << "  -wi <dir>  WEM Input folder (for repacking)\n"
    << "  -e         Extract only\n"
    << "  -p         Pack only\n"
    << "  -s         Skip confirmation prompt\n"
    << "  -no        Quiet mode\n";
}

int main(int argc, char* argv[]) {
    fs::path exe_dir = get_exe_dir();

    struct Config {
        fs::path bnk_in;
        fs::path bnk_out;
        fs::path wem_dir;
        fs::path wem_in;
        std::string mode = "pipeline";
        bool skip_wait = false;
    } conf;

    conf.bnk_in  = exe_dir / "BNK-Input";
    conf.bnk_out = exe_dir / "BNK-Output";
    conf.wem_dir = exe_dir / "WEM";
    conf.wem_in  = exe_dir / "WEM";

    for (int i = 1; i < argc; i++) {
        std::string s = argv[i];
        if (s == "-bi" && i + 1 < argc) conf.bnk_in = fs::path(argv[++i]);
        else if (s == "-bo" && i + 1 < argc) conf.bnk_out = fs::path(argv[++i]);
        else if (s == "-wo" && i + 1 < argc) conf.wem_dir = fs::path(argv[++i]);
        else if (s == "-wi" && i + 1 < argc) conf.wem_in = fs::path(argv[++i]);
        else if (s == "-e")  conf.mode = "extract";
        else if (s == "-p")  conf.mode = "pack";
        else if (s == "-s")  conf.skip_wait = true;
        else if (s == "-no") g_out = Out::NONE;
        else if (s == "-h" || s == "--help") { print_usage(); return 0; }
    }

    fs::create_directories(conf.bnk_in);
    fs::create_directories(conf.bnk_out);
    fs::create_directories(conf.wem_dir);
    fs::create_directories(conf.wem_in);

    try {
        if (conf.mode == "extract" || conf.mode == "pipeline") {
            for (const auto& entry : fs::directory_iterator(conf.bnk_in)) {
                if (entry.path().extension() == ".bnk") {
                    INFO("Extracting: " + entry.path().filename().string() + "\n");
                    BnkFile bnk = parse_bnk(entry.path());
                    do_extract(bnk, conf.wem_dir);
                }
            }
        }

        if (conf.mode == "pipeline" && !conf.skip_wait) {
            INFO("\nPress ENTER to repack...\n");
            std::cin.get();
        }

        if (conf.mode == "pack" || conf.mode == "pipeline") {
            for (const auto& entry : fs::directory_iterator(conf.wem_in)) {
                if (!entry.is_directory()) continue;
                std::string name = entry.path().filename().string();
                fs::path original_bnk = conf.bnk_in / (name + ".bnk");
                if (fs::exists(original_bnk)) {
                    INFO("Packing: " + name + ".bnk\n");
                    BnkFile bnk = parse_bnk(original_bnk);
                    do_pack(bnk, entry.path(), conf.bnk_out / (name + ".bnk"));
                }
            }
        }
    } catch (const std::exception& e) {
        if (g_out != Out::NONE) std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
