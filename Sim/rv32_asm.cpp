// rv32_asm_V5.cpp
// Features: Zero-copy parsing, Data-driven ISA, Two-pass resolution.
// Supported: R, I, S, B, U, J types + Pseudo-instructions (nop, mv).
// g++ -std=c++17 rv32_asm.cpp -o assembler : in termial 
// .\assembler.exe test.s

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <variant>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <cstdint>

namespace rv32 {

using Address = uint32_t;
using InstructionCode = uint32_t;

enum class InstrType { R_TYPE, I_TYPE, S_TYPE, B_TYPE, U_TYPE, J_TYPE, PSEUDO };

struct InstructionDef {
    InstrType type;
    uint32_t opcode;
    uint32_t funct3;
    uint32_t funct7;
};

struct Token {
    enum Kind { Label, Mnemonic, Register, Immediate, Comma, LParen, RParen, Directive, EndOfLine };
    Kind kind;
    std::string_view text; // points into original source string
    size_t lineNum;
};

// ============================================================================
// 1. ISA DATABASE
// ============================================================================
class ISA {
public:
    static std::optional<InstructionDef> getDef(std::string_view mnemonic_sv) {
        static const std::unordered_map<std::string, InstructionDef> table = {
            // R-Type
            {"add",  {InstrType::R_TYPE, 0x33, 0x0, 0x00}},
            {"sub",  {InstrType::R_TYPE, 0x33, 0x0, 0x20}},
            {"xor",  {InstrType::R_TYPE, 0x33, 0x4, 0x00}},
            {"or",   {InstrType::R_TYPE, 0x33, 0x6, 0x00}},
            {"and",  {InstrType::R_TYPE, 0x33, 0x7, 0x00}},
            {"sll",  {InstrType::R_TYPE, 0x33, 0x1, 0x00}},
            {"srl",  {InstrType::R_TYPE, 0x33, 0x5, 0x00}},
            {"sra",  {InstrType::R_TYPE, 0x33, 0x5, 0x20}},
            {"slt",  {InstrType::R_TYPE, 0x33, 0x2, 0x00}},
            {"sltu", {InstrType::R_TYPE, 0x33, 0x3, 0x00}},

            // I-Type
            {"addi", {InstrType::I_TYPE, 0x13, 0x0, 0x00}},
            {"xori", {InstrType::I_TYPE, 0x13, 0x4, 0x00}},
            {"ori",  {InstrType::I_TYPE, 0x13, 0x6, 0x00}},
            {"andi", {InstrType::I_TYPE, 0x13, 0x7, 0x00}},
            {"slli", {InstrType::I_TYPE, 0x13, 0x1, 0x00}},
            {"srli", {InstrType::I_TYPE, 0x13, 0x5, 0x00}},
            {"srai", {InstrType::I_TYPE, 0x13, 0x5, 0x20}},
            {"slti", {InstrType::I_TYPE, 0x13, 0x2, 0x00}},
            {"sltiu",{InstrType::I_TYPE, 0x13, 0x3, 0x00}},
            {"lb",   {InstrType::I_TYPE, 0x03, 0x0, 0x00}},
            {"lh",   {InstrType::I_TYPE, 0x03, 0x1, 0x00}},
            {"lw",   {InstrType::I_TYPE, 0x03, 0x2, 0x00}},
            {"lbu",  {InstrType::I_TYPE, 0x03, 0x4, 0x00}},
            {"lhu",  {InstrType::I_TYPE, 0x03, 0x5, 0x00}},
            {"jalr", {InstrType::I_TYPE, 0x67, 0x0, 0x00}},

            // S-Type
            {"sb",   {InstrType::S_TYPE, 0x23, 0x0, 0x00}},
            {"sh",   {InstrType::S_TYPE, 0x23, 0x1, 0x00}},
            {"sw",   {InstrType::S_TYPE, 0x23, 0x2, 0x00}},

            // B-Type
            {"beq",  {InstrType::B_TYPE, 0x63, 0x0, 0x00}},
            {"bne",  {InstrType::B_TYPE, 0x63, 0x1, 0x00}},
            {"blt",  {InstrType::B_TYPE, 0x63, 0x4, 0x00}},
            {"bge",  {InstrType::B_TYPE, 0x63, 0x5, 0x00}},
            {"bltu", {InstrType::B_TYPE, 0x63, 0x6, 0x00}},
            {"bgeu", {InstrType::B_TYPE, 0x63, 0x7, 0x00}},

            // U-Type
            {"lui",  {InstrType::U_TYPE, 0x37, 0x0, 0x00}},
            {"auipc",{InstrType::U_TYPE, 0x17, 0x0, 0x00}},

            // J-Type
            {"jal",  {InstrType::J_TYPE, 0x6F, 0x0, 0x00}},

            // Pseudo-Instructions
            {"nop",  {InstrType::PSEUDO, 0x13, 0x0, 0x00}}, // addi x0, x0, 0
            {"mv",   {InstrType::PSEUDO, 0x13, 0x0, 0x00}}, // addi rd, rs, 0
            {"not",  {InstrType::PSEUDO, 0x13, 0x4, 0x00}}, // xori rd, rs, -1
        };

        std::string key(mnemonic_sv);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        auto it = table.find(key);
        if (it != table.end()) return it->second;
        return std::nullopt;
    }

    static std::optional<uint8_t> getRegister(std::string_view reg_sv) {
        static const std::unordered_map<std::string, uint8_t> regs = {
            {"x0", 0}, {"zero", 0}, {"x1", 1}, {"ra", 1}, {"x2", 2}, {"sp", 2},
            {"x3", 3}, {"gp", 3},   {"x4", 4}, {"tp", 4}, {"x5", 5}, {"t0", 5},
            {"x6", 6}, {"t1", 6},   {"x7", 7}, {"t2", 7}, {"x8", 8}, {"s0", 8}, {"fp", 8},
            {"x9", 9}, {"s1", 9}, {"x10", 10}, {"a0", 10}, {"x11", 11}, {"a1", 11},
            {"x12", 12}, {"a2", 12}, {"x13", 13}, {"a3", 13}, {"x14", 14}, {"a4", 14},
            {"x15", 15}, {"a5", 15}, {"x16", 16}, {"a6", 16}, {"x17", 17}, {"a7", 17},
            {"x18", 18}, {"s2", 18}, {"x19", 19}, {"s3", 19}, {"x20", 20}, {"s4", 20},
            {"x21", 21}, {"s5", 21}, {"x22", 22}, {"s6", 22}, {"x23", 23}, {"s7", 23},
            {"x24", 24}, {"s8", 24}, {"x25", 25}, {"s9", 25}, {"x26", 26}, {"s10", 26},
            {"x27", 27}, {"s11", 27}, {"x28", 28}, {"t3", 28}, {"x29", 29}, {"t4", 29},
            {"x30", 30}, {"t5", 30}, {"x31", 31}, {"t6", 31}
        };

        std::string key(reg_sv);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        auto it = regs.find(key);
        if (it != regs.end()) return it->second;
        return std::nullopt;
    }
};

// ============================================================================
// 2. LEXER
// ============================================================================
class Lexer {
    std::string_view src;
    size_t cursor = 0;
    size_t line = 1;

public:
    Lexer(std::string_view source) : src(source) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (cursor < src.size()) {
            char c = src[cursor];

            if (c == '#') { // Comment
                while (cursor < src.size() && src[cursor] != '\n') ++cursor;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (c == '\n') ++line;
                ++cursor;
                continue;
            }
            if (c == ',') { tokens.push_back({Token::Comma, ",", line}); ++cursor; continue; }
            if (c == '(') { tokens.push_back({Token::LParen, "(", line}); ++cursor; continue; }
            if (c == ')') { tokens.push_back({Token::RParen, ")", line}); ++cursor; continue; }

            if (c == '.') { // Directive
                size_t start = cursor++;
                while (cursor < src.size() && (std::isalnum(static_cast<unsigned char>(src[cursor])) || src[cursor]=='_')) ++cursor;
                tokens.push_back({Token::Directive, src.substr(start, cursor - start), line});
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') { // Words
                size_t start = cursor;
                while (cursor < src.size() && (std::isalnum(static_cast<unsigned char>(src[cursor])) || src[cursor] == '_')) ++cursor;
                if (cursor < src.size() && src[cursor] == ':') { // Label
                    tokens.push_back({Token::Label, src.substr(start, cursor - start), line});
                    ++cursor; 
                    continue;
                }
                std::string_view word = src.substr(start, cursor - start);
                if (ISA::getRegister(word)) tokens.push_back({Token::Register, word, line});
                else tokens.push_back({Token::Mnemonic, word, line});
                continue;
            }

            if (c == '+' || c == '-' || std::isdigit(static_cast<unsigned char>(c))) { // Immediate
                size_t start = cursor;
                if (src[cursor] == '+' || src[cursor] == '-') ++cursor;
                if (cursor + 1 < src.size() && src[cursor] == '0' && (src[cursor+1] == 'x' || src[cursor+1] == 'X')) {
                    cursor += 2;
                    while (cursor < src.size() && std::isxdigit(static_cast<unsigned char>(src[cursor]))) ++cursor;
                } else {
                    while (cursor < src.size() && std::isdigit(static_cast<unsigned char>(src[cursor]))) ++cursor;
                }
                tokens.push_back({Token::Immediate, src.substr(start, cursor - start), line});
                continue;
            }
            throw std::runtime_error("Unexpected character '" + std::string(1, c) + "' at line " + std::to_string(line));
        }
        return tokens;
    }
};

// ============================================================================
// 3. ASSEMBLER ENGINE
// ============================================================================
class Assembler {
    std::vector<Token> tokens;
    std::unordered_map<std::string, Address> symbolTable; 
    std::vector<InstructionCode> binaryOutput;
    Address currentPC = 0;

    static uint32_t pack(uint32_t val, int offset, int bits) {
        if (bits == 32) return (val << offset);
        uint32_t mask = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
        return (val & mask) << offset;
    }

    static int32_t parseImmediate(std::string_view sv) {
        std::string s(sv);
        size_t idx = 0;
        long long val = std::stoll(s, &idx, 0);
        return static_cast<int32_t>(val);
    }

public:
    Assembler(std::vector<Token> t) : tokens(std::move(t)) {}

    // --- PASS 1: SYMBOL RESOLUTION ---
    void pass1() {
        currentPC = 0;
        for (size_t i = 0; i < tokens.size(); ++i) {
            const auto& tk = tokens[i];
            if (tk.kind == Token::Label) {
                std::string name(tk.text);
                if (symbolTable.count(name)) throw std::runtime_error("Duplicate label: " + name);
                symbolTable.emplace(std::move(name), currentPC);
            } else if (tk.kind == Token::Mnemonic) {
                currentPC += 4;
                // Skip operands
                while (i + 1 < tokens.size() && tokens[i+1].kind != Token::Mnemonic &&
                       tokens[i+1].kind != Token::Label && tokens[i+1].kind != Token::Directive) { ++i; }
            } else if (tk.kind == Token::Directive && tk.text == ".org") {
                if (i + 1 < tokens.size() && tokens[i+1].kind == Token::Immediate) {
                    currentPC = static_cast<Address>(parseImmediate(tokens[i+1].text));
                    ++i;
                }
            }
        }
    }

    // --- PASS 2: BINARY GENERATION ---
    void pass2() {
        currentPC = 0;
        binaryOutput.clear();

        for (size_t i = 0; i < tokens.size(); ++i) {
            const auto& tk = tokens[i];
            if (tk.kind == Token::Label) continue;
            if (tk.kind == Token::Directive) {
                if (tk.text == ".org") {
                    if (i + 1 < tokens.size() && tokens[i+1].kind == Token::Immediate) {
                        currentPC = static_cast<Address>(parseImmediate(tokens[i+1].text));
                        ++i; 
                    }
                }
                continue;
            }
            if (tk.kind != Token::Mnemonic) continue;

            auto defOpt = ISA::getDef(tk.text);
            if (!defOpt) throw std::runtime_error("Unknown instruction: " + std::string(tk.text));
            InstructionDef def = *defOpt;
            uint32_t instr = 0;

            // Safe token consumer
            auto next = [&](size_t &idx) -> const Token& {
                if (++idx >= tokens.size()) throw std::runtime_error("Unexpected end of tokens");
                return tokens[idx];
            };
            size_t idx = i; 

            // --- ENCODING LOGIC ---
            if (def.type == InstrType::PSEUDO) {
                // Handling Pseudo-Instructions
                if (tk.text == "nop" || tk.text == "NOP") {
                    // nop -> addi x0, x0, 0
                    instr = 0x00000013; 
                    i = idx; 
                }
                else if (tk.text == "mv" || tk.text == "MV") {
                    // mv rd, rs -> addi rd, rs, 0
                    const Token& t1 = next(idx); // rd
                    uint8_t rd = ISA::getRegister(t1.text).value();
                    next(idx); // comma
                    const Token& t3 = next(idx); // rs
                    uint8_t rs1 = ISA::getRegister(t3.text).value();
                    
                    // Encode as ADDI (Op: 0x13, F3: 0, Imm: 0)
                    instr = pack(0x13, 0, 7) | pack(rd, 7, 5) | pack(0, 12, 3) | pack(rs1, 15, 5) | pack(0, 20, 12);
                    i = idx;
                }
                else if (tk.text == "not" || tk.text == "NOT") {
                    // not rd, rs -> xori rd, rs, -1
                    const Token& t1 = next(idx);
                    uint8_t rd = ISA::getRegister(t1.text).value();
                    next(idx);
                    const Token& t3 = next(idx);
                    uint8_t rs1 = ISA::getRegister(t3.text).value();
                    
                    // Encode as XORI (Op: 0x13, F3: 4, Imm: -1)
                    instr = pack(0x13, 0, 7) | pack(rd, 7, 5) | pack(4, 12, 3) | pack(rs1, 15, 5) | pack(0xFFF, 20, 12);
                    i = idx;
                }
            }
            else if (def.type == InstrType::R_TYPE) {
                uint8_t rd  = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                uint8_t rs1 = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                uint8_t rs2 = ISA::getRegister(next(idx).text).value();
                instr = pack(def.opcode, 0, 7) | pack(rd, 7, 5) | pack(def.funct3, 12, 3) | pack(rs1, 15, 5) | pack(rs2, 20, 5) | pack(def.funct7, 25, 7);
                i = idx;
            }
            else if (def.type == InstrType::I_TYPE) {
                uint8_t rd = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                if (tk.text == "lw" || tk.text == "lb" || tk.text == "lh" || tk.text == "lbu" || tk.text == "lhu") {
                    // lw rd, off(rs1)
                    int32_t imm = parseImmediate(next(idx).text);
                    next(idx); // (
                    uint8_t rs1 = ISA::getRegister(next(idx).text).value();
                    next(idx); // )
                    instr = pack(def.opcode, 0, 7) | pack(rd, 7, 5) | pack(def.funct3, 12, 3) | pack(rs1, 15, 5) | pack(static_cast<uint32_t>(imm) & 0xFFF, 20, 12);
                } else {
                    // addi rd, rs1, imm
                    uint8_t rs1 = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                    int32_t imm = parseImmediate(next(idx).text);
                    instr = pack(def.opcode, 0, 7) | pack(rd, 7, 5) | pack(def.funct3, 12, 3) | pack(rs1, 15, 5) | pack(static_cast<uint32_t>(imm) & 0xFFF, 20, 12);
                }
                i = idx;
            }
            else if (def.type == InstrType::S_TYPE) {
                // sw rs2, off(rs1)
                uint8_t rs2 = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                int32_t imm = parseImmediate(next(idx).text);
                next(idx); // (
                uint8_t rs1 = ISA::getRegister(next(idx).text).value();
                next(idx); // )
                
                uint32_t imm_low = static_cast<uint32_t>(imm) & 0x1F;
                uint32_t imm_high = (static_cast<uint32_t>(imm) >> 5) & 0x7F;
                instr = pack(def.opcode, 0, 7) | pack(imm_low, 7, 5) | pack(def.funct3, 12, 3) | pack(rs1, 15, 5) | pack(rs2, 20, 5) | pack(imm_high, 25, 7);
                i = idx;
            }
            else if (def.type == InstrType::B_TYPE) {
                // beq rs1, rs2, label
                uint8_t rs1 = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                uint8_t rs2 = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                std::string labelName(next(idx).text);

                if (symbolTable.find(labelName) == symbolTable.end()) throw std::runtime_error("Undefined label: " + labelName);
                int32_t offset = static_cast<int32_t>(symbolTable[labelName] - currentPC);
                if (offset % 2 != 0) throw std::runtime_error("Branch offset must be even");
                
                uint32_t imm_s = static_cast<uint32_t>(offset >> 1) & 0xFFF;
                uint32_t imm_12   = (imm_s >> 11) & 0x1;
                uint32_t imm_10_5 = (imm_s >> 5) & 0x3F;
                uint32_t imm_4_1  = (imm_s >> 1) & 0xF;
                uint32_t imm_11   = (imm_s >> 10) & 0x1;

                instr = pack(def.opcode, 0, 7) | pack(imm_11, 7, 1) | pack(imm_4_1, 8, 4) | pack(def.funct3, 12, 3) 
                      | pack(rs1, 15, 5) | pack(rs2, 20, 5) | pack(imm_10_5, 25, 6) | pack(imm_12, 31, 1);
                i = idx;
            }
            else if (def.type == InstrType::U_TYPE) {
                uint8_t rd = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                int32_t imm = parseImmediate(next(idx).text);
                instr = pack(def.opcode, 0, 7) | pack(rd, 7, 5) | pack(static_cast<uint32_t>(imm) & 0xFFFFF, 12, 20);
                i = idx;
            }
            else if (def.type == InstrType::J_TYPE) {
                 // jal rd, label
                 uint8_t rd = ISA::getRegister(next(idx).text).value(); next(idx); // ,
                 std::string labelName(next(idx).text);
                 
                 if (symbolTable.find(labelName) == symbolTable.end()) throw std::runtime_error("Undefined label: " + labelName);
                 int32_t offset = static_cast<int32_t>(symbolTable[labelName] - currentPC);
                 if (offset % 2 != 0) throw std::runtime_error("Jump offset must be even");

                 uint32_t imm_s = static_cast<uint32_t>(offset >> 1) & 0xFFFFF; // 20 bits
                 uint32_t imm_20 = (imm_s >> 19) & 0x1;
                 uint32_t imm_10_1 = imm_s & 0x3FF;
                 uint32_t imm_11 = (imm_s >> 10) & 0x1;
                 uint32_t imm_19_12 = (imm_s >> 11) & 0xFF;

                 instr = pack(def.opcode, 0, 7) | pack(rd, 7, 5) | pack(imm_19_12, 12, 8) 
                       | pack(imm_11, 20, 1) | pack(imm_10_1, 21, 10) | pack(imm_20, 31, 1);
                 i = idx;
            }
            
            binaryOutput.push_back(instr);
            currentPC += 4;
        }
    }

    void exportHex(const std::string& filename) {
        std::ofstream out(filename);
        if (!out) throw std::runtime_error("Could not open output file " + filename);
        out << std::hex << std::setfill('0');
        for (auto word : binaryOutput) {
            out << std::setw(8) << (word & 0xFFFFFFFFu) << "\n";
        }
        std::cout << "[Info] Hex file written to " << filename << "\n";
    }
};

} // namespace rv32

// ---------------- DRIVER ----------------
std::string readFile(const char* filename) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (!in) throw std::runtime_error("Could not open file");
    std::string contents;
    in.seekg(0, std::ios::end);
    std::streampos len = in.tellg();
    if (len > 0) {
        contents.resize(static_cast<size_t>(len));
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
    } else {
        in.clear(); in.seekg(0);
        contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    return contents;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: rv32_asm <input.s>\n";
        return 1;
    }
    try {
        std::string source = readFile(argv[1]);
        rv32::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        rv32::Assembler asmCore(std::move(tokens));
        std::cout << "Pass 1: Symbol Resolution...\n";
        asmCore.pass1();
        std::cout << "Pass 2: Binary Generation...\n";
        asmCore.pass2();

        std::string outFile = std::string(argv[1]) + ".hex";
        asmCore.exportHex(outFile);

        std::cout << "Assembly Complete.\n";
    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << "\n";
        return 1;
    }
    return 0;
}