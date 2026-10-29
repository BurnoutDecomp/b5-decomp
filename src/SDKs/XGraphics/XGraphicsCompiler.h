#pragma once

#include "types.hpp"
#include <setjmp.h>

// Declaration shape comes from the original XDK xgraphics.lib CodeView types.
// Its Arena routines match ARTIST82C21EC0/82C21F88/82C26AC8, including the
// Compiler callbacks at5A0/5A4. The Xbox jmp_buf supplies the native alignment
// and storage; no opaque padding is substituted for it.
namespace XGRAPHICS
{
class Arena;
class HwLimits;
class LabelInfo;
class SSMHandle;
class LinkageInfo;
class ILProgram;
class CFG;
struct CompAState;
struct _SS_SHADER_STORE;
struct _SSM_REGISTRY;
enum _E_SSM_RETURNCODE;

class Compiler
{
public:
    struct MemStat
    {
        char* name;
        int phase;
        int shader;
        int app;
        bool active;
    };

    struct PerfCounter
    {
        u64 total_cycles;
        long count;
        char* name;
        bool active;
    };

    typedef void* (*MallocCallback)(void*, unsigned int);
    typedef _E_SSM_RETURNCODE (*FreeCallback)(void*, void*);

    _SS_SHADER_STORE* pShaderStore;
    jmp_buf jump_buffer;
    HwLimits* hardware;
    int error_code;
    bool first_time_for_output;
    void* compiler_debug_file;
    int inst_ct;
    bool in_vertex_shader;
    _SSM_REGISTRY* registry;
    int block_ct;
    bool konsts_are_packed;
    bool in_ssa_form;
    bool symbol_table_invalid;
    bool phi_nodes_folded;
    bool phi_nodes_replaced;
    LabelInfo* first_label;
    int iNumEPosRegs;
    int iNumMPassRegs;
    int iNumTempRegs;
    int iNumFixedRegs;
    int iNumLoopConstSets;
    int iNumLoopInputSets;
    int iNumLoopOutputSets;
    int iNumVRegs;
    FreeCallback free;
    MallocCallback malloc;
    void* handle;
    Arena* aps[10];
    SSMHandle* external_stream;
    int debug_order;
    char* rslog_buffer;
    int rslog_cnt;
    int num_rslog_lines;
    int next_temp;
    int next_loop_const_set;
    int next_loop_input_set;
    int next_loop_output_set;
    int next_global_num;
    int next_value_number;
    int next_call_block;
    CompAState* current_abstract_state;
    u32 code_size_in_words[20];
    char* code_buffer[20];
    int num_temps[20];
    int num_interps[20];
    int last_color_export[20];
    int first_instruction_is_fetch[20];
    LinkageInfo* required_linkage;
    bool enable_z_export;
    bool enable_sprite_export;
    bool enable_kill_export;
    bool enable_edgeflag_export;
    bool enable_multipass_export;
    bool enable_parameter_generation;
    bool enable_screen_xy_faceness;
    int parameter_generation_position;
    int current_shader_index;
    char* base_name;
    char* ext_base_name[20];
    char* dbg_file_name;
    bool debug_print_il;
    bool debug_print_disassembly;
    bool out_dbg;
    bool out_SP;
    bool out_fast;
    bool out_quiet;
    u32 opt_flags;
    PerfCounter perf_counters[16];
    MemStat mem_stats[9];
    int num_il_insts;
    int max_coalesce_loops;
    int max_val_num_kills;
    int max_copy_folds;
    int max_balance_conversions;
    int slackness_threshold;
    ILProgram* pILP;
    CFG* cfg;
};

CGS_STATIC_ASSERT((sizeof(jmp_buf) == 0x540), "XDK Compiler jump buffer");
CGS_STATIC_ASSERT((offsetof(Compiler, jump_buffer) == 0x10), "XDK aligned jump buffer");
CGS_STATIC_ASSERT((offsetof(Compiler, hardware) == 0x550), "XDK Compiler hardware limits");
CGS_STATIC_ASSERT((offsetof(Compiler, free) == 0x59C), "XDK Compiler free callback");
CGS_STATIC_ASSERT((offsetof(Compiler, malloc) == 0x5A0), "ARTIST Arena allocation callback");
CGS_STATIC_ASSERT((offsetof(Compiler, handle) == 0x5A4), "ARTIST Arena allocation context");
CGS_STATIC_ASSERT((offsetof(Compiler, aps) == 0x5A8), "XDK Compiler arenas");
CGS_STATIC_ASSERT((offsetof(Compiler, rslog_buffer) == 0x5D8), "ARTIST RSLog buffer");
CGS_STATIC_ASSERT((offsetof(Compiler, rslog_cnt) == 0x5DC), "ARTIST RSLog cursor");
CGS_STATIC_ASSERT((offsetof(Compiler, num_rslog_lines) == 0x5E0), "ARTIST RSLog line count");
CGS_STATIC_ASSERT((sizeof(Compiler::PerfCounter) == 0x18), "XDK profiling counter");
CGS_STATIC_ASSERT((sizeof(Compiler::MemStat) == 0x14), "XDK arena metrics");
CGS_STATIC_ASSERT((offsetof(Compiler, perf_counters) == 0x860), "XDK profiling counters");
CGS_STATIC_ASSERT((offsetof(Compiler, mem_stats) == 0x9E0), "XDK arena metrics array");
CGS_STATIC_ASSERT((offsetof(Compiler, cfg) == 0xAB0), "XDK Compiler CFG");
CGS_STATIC_ASSERT((sizeof(Compiler) == 0xAC0), "XDK Compiler storage");
}
