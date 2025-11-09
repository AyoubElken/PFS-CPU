# RISC-V 32-bit CPU – PFS 

This repo contains the implementation of a 32-bit RISC-V CPU in Verilog.

---

## Project Overview

In this project, we are designing a **RISC-V CPU based on the RV32I instruction set**. The CPU is designed using a modular approach, allowing each part to be developed and tested independently before integration. 

The main objectives of this project are:

- Understand the architecture of a RISC-V CPU.
- Design and implement core modules such as the ALU, Register File, and Control Unit.
- Implement instruction fetch, decode, execution, memory access, and write-back stages.
- Test all modules thoroughly using Verilog testbenches.
- Integrate the modules into a complete CPU system.
- Implement a pipelined architecture with hazard detection.
- Run example programs to demonstrate CPU functionality.

---

## Project Skeleton

# RISC-V 32-bit CPU Project

## Project Structure

- PFS-CPU/
  - README.md
  - LICENSE
  - Dashboard ([Static Dashboard - self mode](https://pfs25v13.netlify.app/))
  - .gitignore
  - Docs/
  - rtl/
    - CPU/
      - riscv_cpu.v — Top CPU module
      - pc.v — Program Counter
      - alu.v — ALU
      - register_file.v — Register file
      - control_unit.v — Control Unit
    - Memory/
      - instruction_memory.v
      - data_memory.v
  - Tsb_Bench/
    - cpu_tb.v — Top CPU testbench
    - alu_tb.v — ALU testbench
    - memory_tb.v — Memory testbench
  - sim/
    - run_sim.sh — Simulation script
  - tools/ — Helper scripts
  - tests/ — Optional test programs for CPU sim (.py...)



---

## Team Roles and Responsibilities

| Team Member | Role | Responsibilities |
|------------|------|----------------|
| Person A | Project Architect & Control Unit | Responsible for designing the overall CPU architecture, defining module interfaces, and implementing the control unit and hazard detection. |
| P B | ALU & Branch Unit | Develops the arithmetic and logic unit, branch comparison logic, and all operations required for instruction execution. |
| P C | Instruction Fetch & Decode | Handles program counter updates, instruction memory, instruction decoding, and immediate generation. |
| P D | Register File & Data Memory | Implements the register file, load/store instructions, and memory access logic. |
| P E | Verification & Testing | Writes testbenches for all modules, runs simulations, and ensures that all parts work correctly together. |
| P F | Toolchain & Infrastructure | Sets up the Verilog development environment, manages version control, and ensures that all simulations and builds run smoothly. |

---

## 6-Week Timeline

[Static Dashboard - self mode](https://pfs25v13.netlify.app/)

### Week 1 – Foundations and Planning
**Members:** Person A, Person B  
- Set up Verilog simulator and RISC-V toolchain.  
- Define module interfaces and plan control signals.  
- Research RV32I instruction set and operation codes.  
- Create skeleton testbenches for all modules.  
- Deliverables: Environment ready, system block diagram, instruction set documentation.

### Week 2 – Core Module Implementation
**Members:** Person A, Person D  
- Implement the ALU and test it with multiple operations.  
- Build the Register File and test read/write operations.  
- Develop the Instruction Fetch module including the program counter.  
- Begin design of the Control Unit.  
- Deliverables: Tested core modules, initial Control Unit design.

### Week 3 – Instruction Decode and Memory
**Members:** Person C  
- Implement the instruction decoder.  
- Complete the branch unit for jump and conditional branch instructions.  
- Develop Data Memory module and load/store functionality.  
- Prepare integration plan for modules.  
- Deliverables: Fully implemented modules ready for integration.

### Week 4 – Integration of Modules
**Members:** Person A, Person D  
- Connect all modules to form a single-cycle CPU.  
- Run basic instruction tests to ensure correct operation.  
- Set up automated build scripts and simulation pipelines.  
- Deliverables: Integrated CPU with initial verification tests.

### Week 5 – Pipelining and Hazard Handling
**Members:** Person B  
- Implement a 5-stage pipeline: IF, ID, EX, MEM, WB.  
- Add hazard detection and forwarding units to avoid pipeline stalls.  
- Perform full testbench simulations including pipeline scenarios.  
- Deliverables: Pipelined CPU with hazard detection, test coverage report.

### Week 6 – Final Verification and Demonstration
**Members:** All members  
- Perform final integration of all modules.  
- Run example programs to demonstrate CPU functionality.  
- Document the CPU design and testing process.  
- Prepare final presentation and demonstration.  
- Deliverables: Fully functional CPU, verified instruction execution, project documentation.


---

## CPU Architecture

Our CPU follows a **Harvard architecture** with separate instruction and data memory. The CPU is modular and includes the following main components:

1. **Instruction Fetch (IF)**: Handles program counter, instruction memory, and optional branch prediction.  
2. **Instruction Decode (ID)**: Reads registers, generates control signals, and computes immediate values.  
3. **Execution (EX)**: Performs ALU operations, branch comparisons, and address calculations.  
4. **Memory Access (MEM)**: Handles data memory read and write operations for load/store instructions.  
5. **Write-Back (WB)**: Writes results from ALU or memory back to the register file.  
6. **Control Unit**: Generates control signals for all modules, manages hazards, and ensures correct data flow in the CPU.  

Each module is implemented independently and tested using dedicated Verilog testbenches before integration.

---

## Testing and Verification

Testing is a key part of this project. Each module is tested with a set of predefined test cases:

- ALU: Tests for arithmetic, logic, and shift operations.
- Register File: Tests read and write operations.
- Instruction Fetch: Tests program counter updates and instruction memory reads.
- Control Unit: Verifies correct generation of control signals.
- Pipeline: Tests data forwarding and hazard detection.

Integration tests verify that the CPU executes a sequence of RISC-V instructions correctly.

---

## Notes

- Team member responsibilities are adjustable according to individual strengths.  
- All modules are version-controlled using Git to track progress.  
- Weekly deliverables must be completed before the end of each week to ensure project success and completion before the exams period nchallah.  



