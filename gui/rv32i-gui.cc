#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iterator>
#include <cassert>
#include <array>
#include <deque>
#include <algorithm>
#include <map>
#include <stdexcept>

#ifdef GUI
  #include "imgui.h"
  #include "imgui_impl_sdl2.h"
  #include "imgui_impl_opengl3.h"
  #include <SDL2/SDL.h>
  #include <GL/glew.h>
#endif

// CPU core
struct CPU {
    struct Snapshot {
        uint32_t pc;
        std::array<uint32_t, 32> x;
        uint64_t cycles;
    };

    uint32_t pc = 0;
    std::array<uint32_t, 32> x{};
    uint64_t cycles = 0;
    std::vector<uint8_t> mem;
    std::array<bool, 32> reg_changed{};
    std::vector<uint32_t> mem_changed;
    size_t bin_size = 0;

    struct Trace {
        uint64_t cyc;
        uint32_t pc, ins;
        std::string txt;
    };
    std::deque<Trace> trace;
    size_t trace_max = 10000;

    std::deque<Snapshot> history;
    size_t history_max = 512;

    explicit CPU(size_t mem_sz = 1 << 20) : mem(mem_sz) {}

    void clear_changes() {
        reg_changed.fill(false);
    }

    void reset() {
        pc = cycles = 0;
        x.fill(0);
        clear_changes();
        mem_changed.clear();
        history.clear();
        trace.clear();
    }

    bool load_bin(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        std::vector<uint8_t> bin((std::istreambuf_iterator<char>(f)), {});
        if (bin.size() > mem.size()) return false;
        std::copy(bin.begin(), bin.end(), mem.begin());
        bin_size = bin.size() & ~3u;
        reset();
        return true;
    }

    bool save_bin(const std::string &path) {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f.write(reinterpret_cast<const char*>(mem.data()), bin_size);
        return true;
    }

    static uint32_t sx(uint32_t v, int bits) {
        uint32_t m = 1u << (bits - 1);
        return (v ^ m) - m;
    }

    uint32_t fetch32(uint32_t addr) const {
        assert(addr + 3 < mem.size());
        return mem[addr] | (mem[addr + 1] << 8) | (mem[addr + 2] << 16) | (mem[addr + 3] << 24);
    }

    uint32_t fetch() const {
        return fetch32(pc);
    }

    void store32(uint32_t a, uint32_t v) {
        assert(a + 3 < mem.size());
        mem[a] = (uint8_t)v;
        mem[a + 1] = (uint8_t)(v >> 8);
        mem[a + 2] = (uint8_t)(v >> 16);
        mem[a + 3] = (uint8_t)(v >> 24);
        for (int i = 0; i < 4; ++i) mem_changed.push_back(a + i);
    }

    std::string disasm(uint32_t ins) const {
        uint32_t opc = ins & 0x7F, rd = (ins >> 7) & 0x1F, f3 = (ins >> 12) & 7, rs1 = (ins >> 15) & 0x1F, rs2 = (ins >> 20) & 0x1F, f7 = ins >> 25;
        auto imm_i = [&]() { return sx(ins >> 20, 12); };
        auto imm_u = [&]() { return ins & 0xFFFFF000u; };
        auto imm_s = [&]() { return sx(((ins >> 7) & 0x1F) | ((ins >> 20) & 0xFE0), 12); };
        auto imm_b = [&]() { uint32_t v = ((ins >> 7) & 0x1E) | ((ins >> 20) & 0x7E0) | ((ins << 4) & 0x800) | ((ins >> 19) & 0x1000); return sx(v, 13); };
        auto imm_j = [&]() { uint32_t v = ((ins >> 21) & 0x3FF) << 1 | ((ins >> 20) & 1) << 11 | ((ins >> 12) & 0xFF) << 12 | ((ins >> 31) << 20); return sx(v, 21); };

        std::ostringstream os;
        os << std::dec;
        switch (opc) {
            case 0x37: os << "lui x" << rd << ", 0x" << std::hex << imm_u(); break;
            case 0x17: os << "auipc x" << rd << ", 0x" << std::hex << imm_u(); break;
            case 0x6F: os << "jal x" << rd << ", " << imm_j(); break;
            case 0x67: os << "jalr x" << rd << ", x" << rs1 << ", " << imm_i(); break;
            case 0x63: { const char* m[] = {"beq","bne","","","blt","bge","bltu","bgeu"}; os << m[f3] << " x" << rs1 << ", x" << rs2 << ", " << imm_b(); } break;
            case 0x03: { const char* m[] = {"lb","lh","lw","","lbu","lhu"}; os << m[f3] << " x" << rd << ", " << imm_i() << "(x" << rs1 << ")"; } break;
            case 0x23: { const char* m[] = {"sb","sh","sw"}; os << m[f3] << " x" << rs2 << ", " << imm_s() << "(x" << rs1 << ")"; } break;
            case 0x13: { if (f3 == 1) os << "slli"; else if (f3 == 5) os << ((ins >> 30) ? "srai" : "srli"); else { const char* m[] = {"addi","","slti","sltiu","xori","","ori","andi"}; os << m[f3]; } os << " x" << rd << ", x" << rs1 << ", " << ((f3 == 1 || f3 == 5) ? (imm_i() & 31) : imm_i()); } break;
            case 0x33: { if (f3 == 0 && f7) os << "sub"; else if (f3 == 5 && f7) os << "sra"; else { const char* m[] = {"add","sll","slt","sltu","xor","srl","or","and"}; os << m[f3]; } os << " x" << rd << ", x" << rs1 << ", x" << rs2; } break;
            case 0x0F: os << "fence"; break;
            case 0x73: os << "ecall"; break;
            default: os << "illegal"; break;
        }
        return os.str();
    }

    void step() {
        mem_changed.clear();
        if (history.size() == history_max) history.pop_front();
        history.push_back({pc, x, cycles});

        uint32_t ins = fetch();
        uint32_t opc = ins & 0x7F, rd = (ins >> 7) & 0x1F, f3 = (ins >> 12) & 7, rs1 = (ins >> 15) & 0x1F, rs2 = (ins >> 20) & 0x1F, f7 = ins >> 25;
        auto imm_i = [&]() { return sx(ins >> 20, 12); };
        auto imm_u = [&]() { return ins & 0xFFFFF000u; };
        auto imm_s = [&]() { return sx(((ins >> 7) & 0x1F) | ((ins >> 20) & 0xFE0), 12); };
        auto imm_b = [&]() { uint32_t v = ((ins >> 7) & 0x1E) | ((ins >> 20) & 0x7E0) | ((ins << 4) & 0x800) | ((ins >> 19) & 0x1000); return sx(v, 13); };
        auto imm_j = [&]() { uint32_t v = ((ins >> 21) & 0x3FF) << 1 | ((ins >> 20) & 1) << 11 | ((ins >> 12) & 0xFF) << 12 | ((ins >> 31) << 20); return sx(v, 21); };

        std::array<uint32_t, 32> prev = x;

        switch (opc) {
            case 0x37: x[rd] = imm_u(); pc += 4; break;
            case 0x17: x[rd] = pc + imm_u(); pc += 4; break;
            case 0x6F: { uint32_t t = pc + 4; pc += imm_j(); if (rd) x[rd] = t; } break;
            case 0x67: { uint32_t t = pc + 4; pc = (x[rs1] + imm_i()) & ~1u; if (rd) x[rd] = t; } break;
            case 0x63: { bool take = false; switch (f3) { case 0: take = x[rs1] == x[rs2]; break; case 1: take = x[rs1] != x[rs2]; break; case 4: take = (int32_t)x[rs1] < (int32_t)x[rs2]; break; case 5: take = (int32_t)x[rs1] >= (int32_t)x[rs2]; break; case 6: take = x[rs1] < x[rs2]; break; case 7: take = x[rs1] >= x[rs2]; break; } pc += take ? imm_b() : 4; } break;
            case 0x03: { uint32_t a = x[rs1] + imm_i(); switch (f3) { case 0: x[rd] = (int8_t)mem[a]; break; case 1: x[rd] = (int16_t)(mem[a] | (mem[a + 1] << 8)); break; case 2: x[rd] = fetch32(a); break; case 4: x[rd] = mem[a]; break; case 5: x[rd] = mem[a] | (mem[a + 1] << 8); break; } pc += 4; } break;
            case 0x23: { uint32_t a = x[rs1] + imm_s(); switch (f3) { case 0: mem[a] = (uint8_t)x[rs2]; mem_changed.push_back(a); break; case 1: mem[a] = (uint8_t)x[rs2]; mem[a + 1] = (uint8_t)(x[rs2] >> 8); mem_changed.push_back(a); mem_changed.push_back(a + 1); break; case 2: store32(a, x[rs2]); break; } pc += 4; } break;
            case 0x13: { uint32_t imm = imm_i(); switch (f3) { case 0: x[rd] = x[rs1] + imm; break; case 2: x[rd] = (int32_t)x[rs1] < (int32_t)imm; break; case 3: x[rd] = x[rs1] < imm; break; case 4: x[rd] = x[rs1] ^ imm; break; case 6: x[rd] = x[rs1] | imm; break; case 7: x[rd] = x[rs1] & imm; break; case 1: x[rd] = x[rs1] << (imm & 0x1F); break; case 5: x[rd] = (imm >> 10) ? (int32_t)x[rs1] >> (imm & 0x1F) : x[rs1] >> (imm & 0x1F); break; } pc += 4; } break;
            case 0x33: { switch (f3) { case 0: x[rd] = f7 ? x[rs1] - x[rs2] : x[rs1] + x[rs2]; break; case 1: x[rd] = x[rs1] << (x[rs2] & 0x1F); break; case 2: x[rd] = (int32_t)x[rs1] < (int32_t)x[rs2]; break; case 3: x[rd] = x[rs1] < x[rs2]; break; case 4: x[rd] = x[rs1] ^ x[rs2]; break; case 5: x[rd] = f7 ? (int32_t)x[rs1] >> (x[rs2] & 0x1F) : x[rs1] >> (x[rs2] & 0x1F); break; case 6: x[rd] = x[rs1] | x[rs2]; break; case 7: x[rd] = x[rs1] & x[rs2]; break; } pc += 4; } break;
            case 0x0F: pc += 4; break;
            case 0x73: std::cout << "ECALL @ cycle " << cycles << "\n"; std::exit(0);
            default: std::cerr << "Illegal opcode 0x" << std::hex << opc << "\n"; std::exit(1);
        }

        x[0] = 0;
        for (int i = 0; i < 32; ++i) reg_changed[i] = (x[i] != prev[i]);

        if (trace.size() == trace_max) trace.pop_front();
        trace.push_back({cycles, history.back().pc, ins, disasm(ins)});

        ++cycles;
    }

    bool step_back() {
        if (history.size() < 1) return false;
        Snapshot snap = history.back();
        history.pop_back();
        pc = snap.pc;
        x = snap.x;
        cycles = snap.cycles;
        clear_changes();
        mem_changed.clear();
        if (!trace.empty()) trace.pop_back();
        return true;
    }
};

static const char *REG_NAMES[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

static std::map<std::string, int> reg_map = {
    {"x0", 0}, {"zero", 0},
    {"x1", 1}, {"ra", 1},
    {"x2", 2}, {"sp", 2},
    {"x3", 3}, {"gp", 3},
    {"x4", 4}, {"tp", 4},
    {"x5", 5}, {"t0", 5},
    {"x6", 6}, {"t1", 6},
    {"x7", 7}, {"t2", 7},
    {"x8", 8}, {"s0", 8},
    {"x9", 9}, {"s1", 9},
    {"x10", 10}, {"a0", 10},
    {"x11", 11}, {"a1", 11},
    {"x12", 12}, {"a2", 12},
    {"x13", 13}, {"a3", 13},
    {"x14", 14}, {"a4", 14},
    {"x15", 15}, {"a5", 15},
    {"x16", 16}, {"a6", 16},
    {"x17", 17}, {"a7", 17},
    {"x18", 18}, {"s2", 18},
    {"x19", 19}, {"s3", 19},
    {"x20", 20}, {"s4", 20},
    {"x21", 21}, {"s5", 21},
    {"x22", 22}, {"s6", 22},
    {"x23", 23}, {"s7", 23},
    {"x24", 24}, {"s8", 24},
    {"x25", 25}, {"s9", 25},
    {"x26", 26}, {"s10", 26},
    {"x27", 27}, {"s11", 27},
    {"x28", 28}, {"t3", 28},
    {"x29", 29}, {"t4", 29},
    {"x30", 30}, {"t5", 30},
    {"x31", 31}, {"t6", 31}
};

static int get_reg(const std::string& r) {
    std::string reg = r;
    if (!reg.empty() && reg.back() == ',') reg.pop_back();
    auto it = reg_map.find(reg);
    if (it == reg_map.end()) throw std::runtime_error(std::string("Invalid register: ") + r);
    return it->second;
}

static int32_t parse_imm(const std::string& s) {
    std::string imm_str = s;
    if (!imm_str.empty() && imm_str.back() == ',') imm_str.pop_back();
    char* end;
    int32_t imm;
    bool is_hex_suffix = !imm_str.empty() && (imm_str.back() == 'h' || imm_str.back() == 'H');
    if (is_hex_suffix) {
        imm_str.pop_back();
        imm = strtol(imm_str.c_str(), &end, 16);
    } else {
        imm = strtol(imm_str.c_str(), &end, 0);
    }
    if (*end != '\0') throw std::runtime_error(std::string("Invalid immediate: ") + s);
    return imm;
}

static int32_t parse_offset(const std::string& s, const std::map<std::string, uint32_t>& labels, uint32_t current_addr) {
    try {
        return parse_imm(s);
    } catch (...) {
        auto it = labels.find(s);
        if (it == labels.end()) throw std::runtime_error(std::string("Undefined label: ") + s);
        return static_cast<int32_t>(it->second) - static_cast<int32_t>(current_addr);
    }
}

static std::pair<int32_t, int> parse_mem_op(const std::string& op_str) {
    size_t paren_pos = op_str.find('(');
    if (paren_pos == std::string::npos) throw std::runtime_error(std::string("Invalid memory operand: ") + op_str);
    std::string imm_part = op_str.substr(0, paren_pos);
    std::string reg_part = op_str.substr(paren_pos + 1);
    if (!reg_part.empty() && reg_part.back() == ')') reg_part.pop_back();
    int32_t imm = imm_part.empty() ? 0 : parse_imm(imm_part);
    int rs1 = get_reg(reg_part);
    return {imm, rs1};
}

static std::vector<uint8_t> assemble(const std::string& asm_text) {
    std::vector<std::string> lines;
    std::istringstream iss(asm_text);
    std::string line;
    while (std::getline(iss, line)) {
        size_t comment_pos = line.find(';');
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (!line.empty() and line[0] != '\0') {
          lines.push_back(line);
          std::cout << "[" << line << "]" << std::endl;
        }
    }

    std::map<std::string, uint32_t> labels;
    uint32_t addr = 0;
    for (const auto& l : lines) {
        if (!l.empty() && l.back() == ':') {
            std::string label = l.substr(0, l.size() - 1);
            labels[label] = addr;
            continue;
        }
        addr += 4;
    }

    std::vector<uint8_t> bin;
    addr = 0;
    for (const auto& l : lines) {
        if (!l.empty() && l.back() == ':') continue;

        std::istringstream is(l);
        std::cout << l << std::endl;
        std::string op;
        is >> op;
        std::transform(op.begin(), op.end(), op.begin(), ::tolower);

        std::string operand_line;
        std::getline(is, operand_line);

        std::vector<std::string> operands;
        std::istringstream ops(operand_line);
        std::string opd;
        while (std::getline(ops, opd, ',')) {
            opd.erase(0, opd.find_first_not_of(" \t"));
            opd.erase(opd.find_last_not_of(" \t") + 1);
            if (!opd.empty()) operands.push_back(opd);
        }

        uint32_t ins = 0;
        if (op == "lui") {
            int rd = get_reg(operands[0]);
            uint32_t imm = parse_imm(operands[1]);
            ins = (imm << 12) | (rd << 7) | 0x37;
        } else if (op == "auipc") {
            int rd = get_reg(operands[0]);
            uint32_t imm = parse_imm(operands[1]);
            ins = (imm << 12) | (rd << 7) | 0x17;
        } else if (op == "jal") {
            int rd = get_reg(operands[0]);
            int32_t off = parse_offset(operands[1], labels, addr);
            uint32_t imm20 = (off >> 20) & 1;
            uint32_t imm10_1 = (off >> 1) & 0x3FF;
            uint32_t imm11 = (off >> 11) & 1;
            uint32_t imm19_12 = (off >> 12) & 0xFF;
            ins = (imm20 << 31) | (imm10_1 << 21) | (imm11 << 20) | (imm19_12 << 12) | (rd << 7) | 0x6F;
        } else if (op == "jalr") {
            int rd = get_reg(operands[0]);
            auto [imm, rs1] = parse_mem_op(operands[1]);
            ins = ((imm & 0xFFF) << 20) | (rs1 << 15) | (0 << 12) | (rd << 7) | 0x67;
        } else if (op == "beq" || op == "bne" || op == "blt" || op == "bge" || op == "bltu" || op == "bgeu") {
            int rs1 = get_reg(operands[0]);
            int rs2 = get_reg(operands[1]);
            int32_t off = parse_offset(operands[2], labels, addr);
            uint32_t f3 = (op == "beq" ? 0 : op == "bne" ? 1 : op == "blt" ? 4 : op == "bge" ? 5 : op == "bltu" ? 6 : 7);
            uint32_t imm12 = (off >> 12) & 1;
            uint32_t imm10_5 = (off >> 5) & 0x3F;
            uint32_t imm4_1 = (off >> 1) & 0xF;
            uint32_t imm11 = (off >> 11) & 1;
            ins = (imm12 << 31) | (imm10_5 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (imm4_1 << 8) | (imm11 << 7) | 0x63;
        } else if (op == "lb" || op == "lh" || op == "lw" || op == "lbu" || op == "lhu") {
            int rd = get_reg(operands[0]);
            auto [imm, rs1] = parse_mem_op(operands[1]);
            uint32_t f3 = (op == "lb" ? 0 : op == "lh" ? 1 : op == "lw" ? 2 : op == "lbu" ? 4 : 5);
            ins = ((imm & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x03;
        } else if (op == "sb" || op == "sh" || op == "sw") {
            int rs2 = get_reg(operands[0]);
            auto [imm, rs1] = parse_mem_op(operands[1]);
            uint32_t f3 = (op == "sb" ? 0 : op == "sh" ? 1 : 2);
            ins = (((imm >> 5) & 0x7F) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((imm & 0x1F) << 7) | 0x23;
        } else if (op == "addi" || op == "slti" || op == "sltiu" || op == "xori" || op == "ori" || op == "andi" || op == "slli" || op == "srli" || op == "srai") {
            int rd = get_reg(operands[0]);
            int rs1 = get_reg(operands[1]);
            int32_t imm = parse_imm(operands[2]);
            uint32_t f3 = (op == "addi" ? 0 : op == "slti" ? 2 : op == "sltiu" ? 3 : op == "xori" ? 4 : op == "ori" ? 6 : op == "andi" ? 7 : op == "slli" ? 1 : op == "srli" ? 5 : 5);
            uint32_t shamt = imm & 0x1F;
            uint32_t sr_f7 = (op == "srai" ? 0x20 : 0);
            if (op == "slli" || op == "srli" || op == "srai") imm = shamt | (sr_f7 << 5);
            ins = ((imm & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x13;
        } else if (op == "add" || op == "sub" || op == "sll" || op == "slt" || op == "sltu" || op == "xor" || op == "srl" || op == "sra" || op == "or" || op == "and") {
            int rd = get_reg(operands[0]);
            int rs1 = get_reg(operands[1]);
            int rs2 = get_reg(operands[2]);
            uint32_t f3 = (op == "add" ? 0 : op == "sub" ? 0 : op == "sll" ? 1 : op == "slt" ? 2 : op == "sltu" ? 3 : op == "xor" ? 4 : op == "srl" ? 5 : op == "sra" ? 5 : op == "or" ? 6 : 7);
            uint32_t f7 = (op == "sub" || op == "sra" ? 0x20 : 0);
            ins = (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x33;
        } else if (op == "fence") {
            ins = 0x0F;
        } else if (op == "ecall") {
            ins = 0x73;
        } else {
            //throw std::runtime_error(std::string("Unsupported instruction: ") + op);
        }

        bin.push_back(static_cast<uint8_t>(ins));
        bin.push_back(static_cast<uint8_t>(ins >> 8));
        bin.push_back(static_cast<uint8_t>(ins >> 16));
        bin.push_back(static_cast<uint8_t>(ins >> 24));
        std::cout << ins << std::endl;
        addr += 4;
    }
    return bin;
}

struct InputTextCallback_UserData
{
    std::string* Str;
    ImGuiInputTextCallback ChainCallback;
    void* ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
    if (data->Flags & ImGuiInputTextFlags_CallbackResize)
    {
        std::string* str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen + 1);
        data->Buf = (char*)str->c_str();
    }
    else if (user_data->ChainCallback)
    {
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

#ifdef GUI
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* win = SDL_CreateWindow("RV32I Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_ctx = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, gl_ctx);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(win, gl_ctx);
    ImGui_ImplOpenGL3_Init();

    CPU cpu;
    if (argc > 1) cpu.load_bin(argv[1]);

    bool run = true, auto_run = false;
    char bin_path[256] = "program.bin";
    const int STEPS_PER_FRAME = 500;
    static bool edit_mode = false;
    static std::string asm_code = "";
    static std::string assemble_error = "";

    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) run = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == SDL_GetWindowID(win)) run = false;
        }
        if (auto_run) for (int i = 0; i < STEPS_PER_FRAME; ++i) cpu.step();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Control panel
        ImGui::Begin("Control");
        if (ImGui::Button("Step")) cpu.step();
        ImGui::SameLine();
        if (ImGui::Button("Step Back")) cpu.step_back();
        ImGui::SameLine();
        if (ImGui::Button(auto_run ? "Pause" : "Run")) auto_run = !auto_run;
        ImGui::Separator();
        ImGui::InputText("Binary", bin_path, sizeof(bin_path));
        ImGui::SameLine();
        if (ImGui::Button("Load")) cpu.load_bin(bin_path);
        ImGui::SameLine();
        if (ImGui::Button("Save")) cpu.save_bin(bin_path);
        ImGui::Text("Cycle: %llu", (unsigned long long)cpu.cycles);
        ImGui::Text("PC: 0x%08X", cpu.pc);
        uint32_t ins = cpu.fetch();
        ImGui::Text("Ins: 0x%08X  %s", ins, cpu.disasm(ins).c_str());
        ImGui::End();

        // Registers view
        ImGui::Begin("Registers");
        if (ImGui::BeginTable("regtab", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            for (int i = 0; i < 32; ++i) {
                ImGui::TableNextColumn();
                if (cpu.reg_changed[i]) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
                ImGui::Text("%s (x%d): 0x%08X / %d", REG_NAMES[i], i, cpu.x[i], (int32_t)cpu.x[i]);
                if (cpu.reg_changed[i]) ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }
        ImGui::End();

        // Memory viewer
        ImGui::Begin("Memory");
        static uint32_t mem_base = 0;
        ImGui::InputScalar("Base", ImGuiDataType_U32, &mem_base, nullptr, nullptr, "0x%08X", ImGuiInputTextFlags_CharsHexadecimal);
        mem_base &= ~0xFu;
        if (mem_base + 16 * 16 > cpu.mem.size()) mem_base = cpu.mem.size() - 16 * 16;
        if (ImGui::BeginTable("mem", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Hex", 0, 0.0f);
            ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            uint32_t addr = mem_base;
            for (int r = 0; r < 16; ++r, addr += 16) {
                ImGui::TableNextRow();
                bool row_is_pc = (addr <= cpu.pc && cpu.pc < addr + 16);
                bool row_has_write = std::any_of(cpu.mem_changed.begin(), cpu.mem_changed.end(), [&](uint32_t w) { return addr <= w && w < addr + 16; });
                if (row_is_pc) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(60, 60, 120, 255));
                else if (row_has_write) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(120, 60, 60, 255));

                ImGui::TableNextColumn();
                ImGui::Text("0x%08X", addr);

                ImGui::TableNextColumn();
                for (int i = 0; i < 16; ++i) {
                    uint32_t ma = addr + i;
                    if (ma >= cpu.mem.size()) { ImGui::Text("  "); ImGui::SameLine(0.0f, 0.0f); continue; }
                    bool is_ins = (ma >= cpu.pc && ma < cpu.pc + 4);
                    bool is_changed = std::find(cpu.mem_changed.begin(), cpu.mem_changed.end(), ma) != cpu.mem_changed.end();
                    ImVec4 col = ImVec4(1, 1, 1, 1);
                    if (is_ins && is_changed) col = ImVec4(1, 0.5, 0, 1);
                    else if (is_ins) col = ImVec4(1, 1, 0, 1);
                    else if (is_changed) col = ImVec4(1, 0, 0, 1);
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::PushID(ma);
                    ImGui::Text("%02X", cpu.mem[ma]);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        uint32_t wa = ma & ~3u;
                        if (wa + 3 < cpu.mem.size()) {
                            uint32_t word_ins = cpu.fetch32(wa);
                            std::string d = cpu.disasm(word_ins);
                            if (d != "illegal") {
                                ImGui::Text("Instruction at 0x%08X: %s", wa, d.c_str());
                            } else {
                                ImGui::Text("Word: 0x%08X / %d", word_ins, static_cast<int32_t>(word_ins));
                            }
                        }
                        ImGui::Text("Byte: 0x%02X / %d", cpu.mem[ma], static_cast<int8_t>(cpu.mem[ma]));
                        ImGui::EndTooltip();
                    }
                    static uint32_t edit_addr = 0xFFFFFFFF;
                    static char edit_buf[3] = "";
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        edit_addr = ma;
                        snprintf(edit_buf, sizeof(edit_buf), "%02X", cpu.mem[ma]);
                        ImGui::OpenPopup("Edit Byte");
                    }
                    if (ImGui::BeginPopupModal("Edit Byte", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        ImGui::InputText("##editval", edit_buf, sizeof(edit_buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
                        if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                            uint32_t v = strtoul(edit_buf, nullptr, 16);
                            cpu.mem[edit_addr] = v & 0xFF;
                            cpu.mem_changed.push_back(edit_addr);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.0f, 0.0f);
                }

                ImGui::TableNextColumn();
                for (int i = 0; i < 16; ++i) {
                    uint32_t ma = addr + i;
                    if (ma >= cpu.mem.size()) { ImGui::Text("."); ImGui::SameLine(0.0f, 0.0f); continue; }
                    bool is_ins = (ma >= cpu.pc && ma < cpu.pc + 4);
                    bool is_changed = std::find(cpu.mem_changed.begin(), cpu.mem_changed.end(), ma) != cpu.mem_changed.end();
                    ImVec4 col = ImVec4(1, 1, 1, 1);
                    if (is_ins && is_changed) col = ImVec4(1, 0.5, 0, 1);
                    else if (is_ins) col = ImVec4(1, 1, 0, 1);
                    else if (is_changed) col = ImVec4(1, 0, 0, 1);
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::Text("%c", (cpu.mem[ma] >= 32 && cpu.mem[ma] < 127) ? cpu.mem[ma] : '.');
                    ImGui::PopStyleColor();
                    ImGui::SameLine(0.0f, 0.0f);
                }
            }
            ImGui::EndTable();
        }
        ImGui::End();

        // Disassembly viewer
        ImGui::Begin("Disassembly");
        if (ImGui::Button(edit_mode ? "Assemble" : "Edit")) {
            if (edit_mode) {
                assemble_error = "";
                try {
                    auto bin = assemble(asm_code);
                    std::copy(bin.begin(), bin.end(), cpu.mem.begin());
                    cpu.bin_size = bin.size();
                    cpu.reset();
                } catch (const std::exception& ex) {
                    assemble_error = ex.what();
                }
                edit_mode = false;
            } else {
                asm_code = "";
                for (uint32_t a = 0; a < cpu.bin_size; a += 4) {
                    asm_code += cpu.disasm(cpu.fetch32(a)) + "\n";
                }
                edit_mode = true;
            }
        }
        if (!assemble_error.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", assemble_error.c_str());
        }
        if (edit_mode) {
            InputTextCallback_UserData cb_user_data;
            cb_user_data.Str = &asm_code;
            cb_user_data.ChainCallback = NULL;
            cb_user_data.ChainCallbackUserData = NULL;
            ImGui::InputTextMultiline("##asm_edit", (char*)asm_code.data(), asm_code.capacity() + 1, ImVec2(0, 0), ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize, InputTextCallback, &cb_user_data);
        } else {
            if (ImGui::BeginChild("disasm_scroll")) {
                for (uint32_t addr = 0; addr < cpu.bin_size; addr += 4) {
                    std::string line = cpu.disasm(cpu.fetch32(addr));
                    bool is_current = (addr == cpu.pc);
                    if (is_current) {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
                    }
                    ImGui::Text("0x%08X  %s", addr, line.c_str());
                    if (is_current) {
                        ImGui::PopStyleColor();
                        ImGui::SetScrollHereY(0.5f);
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        // History view
        ImGui::Begin("History");
        if (ImGui::BeginTable("hist", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Cycle");
            ImGui::TableSetupColumn("PC");
            ImGui::TableSetupColumn("Disasm");
            for (auto &t : cpu.trace) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%llu", (unsigned long long)t.cyc);
                ImGui::TableNextColumn();
                ImGui::Text("0x%08X", t.pc);
                ImGui::TableNextColumn();
                ImGui::Text("%s", t.txt.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
#else
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " program.bin\n";
        return 1;
    }
    CPU cpu;
    if (!cpu.load_bin(argv[1])) {
        std::cerr << "failed to load " << argv[1] << "\n";
        return 1;
    }
    while (true) {
        uint32_t ins = cpu.fetch();
        cpu.step();
        std::cout << "[cycle " << std::dec << cpu.cycles << "] pc=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.pc << " ins=0x" << std::setw(8) << ins << " " << cpu.disasm(ins) << "\n";
        for (int i = 0; i < 32; ++i) {
            if (cpu.reg_changed[i]) std::cout << "*";
            std::cout << REG_NAMES[i] << "=" << std::hex << cpu.x[i] << ((i % 8 == 7) ? "\n" : " ");
        }
    }
}
#endif
