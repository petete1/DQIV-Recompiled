// Crash reporting: native faults and uncaught exceptions print a symbolized
// stack to the journal and show an error before the process ends.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#pragma comment(lib,"dbghelp.lib")
namespace {
void trace(CONTEXT* context=nullptr) {
    auto process=GetCurrentProcess();
    // Symbols beside the executable first (copied or packaged builds).
    char directory[MAX_PATH]{};
    GetModuleFileNameA(nullptr,directory,MAX_PATH);
    if(auto* slash=std::strrchr(directory,'\\'))*slash=0;
    SymInitialize(process,directory,TRUE);
    void* frames[48];unsigned count=0;
    if(context) {
        STACKFRAME64 frame{};
        frame.AddrPC={context->Eip,0,AddrModeFlat};
        frame.AddrFrame={context->Ebp,0,AddrModeFlat};
        frame.AddrStack={context->Esp,0,AddrModeFlat};
        frames[count++]=reinterpret_cast<void*>(context->Eip);
        while(count<48 && StackWalk64(IMAGE_FILE_MACHINE_I386,process,GetCurrentThread(),&frame,context,nullptr,
            SymFunctionTableAccess64,SymGetModuleBase64,nullptr) && frame.AddrPC.Offset)
            frames[count++]=reinterpret_cast<void*>(frame.AddrPC.Offset);
    } else count=CaptureStackBackTrace(0,48,frames,nullptr);
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+1024]{};
    auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;
    for(unsigned i=0;i<count;++i) {
        DWORD64 offset=0;
        if(SymFromAddr(process,reinterpret_cast<DWORD64>(frames[i]),&offset,symbol))
            std::fprintf(stderr,"  %s + 0x%llx\n",symbol->Name,offset);
        else std::fprintf(stderr,"  %p\n",frames[i]);
    }
    SymCleanup(process);std::fflush(stderr);
}
// The DS divides in software: x / 0 gives x and x % 0 gives 0 (the
// runtime's _s32_div_f/_u32_div_f), and the game relies on it (a colour fade
// of zero frames). On x86 the division traps: the faulting div/idiv is
// finished the DS way and execution goes on.
bool divideLikeTheDs(CONTEXT* context) {
    const auto* code=reinterpret_cast<const unsigned char*>(context->Eip);
    unsigned length=0;bool word=false;
    for(;;++length) {
        const unsigned char prefix=code[length];
        if(prefix==0x66) {word=true;continue;}
        if(prefix==0x2e||prefix==0x36||prefix==0x3e||prefix==0x26||prefix==0x64||prefix==0x65||prefix==0xf0||prefix==0xf2||prefix==0xf3)continue;
        break;
    }
    const unsigned char opcode=code[length++];
    if(opcode!=0xf6&&opcode!=0xf7)return false;
    const unsigned char modrm=code[length++];
    const unsigned reg=(modrm>>3)&7,mod=modrm>>6,rm=modrm&7;
    if(reg!=6&&reg!=7)return false; // div, idiv
    if(mod!=3) {
        if(rm==4) {const unsigned char sib=code[length++];if(mod==0&&(sib&7)==5)length+=4;}
        if(mod==0&&rm==5)length+=4;
        if(mod==1)length+=1;
        if(mod==2)length+=4;
    }
    if(opcode==0xf6)context->Eax=(context->Eax&0xffff0000u)|(context->Eax&0xffu); // AL = AL, AH = 0
    else if(word)context->Edx&=0xffff0000u;                                        // AX = AX, DX = 0
    else context->Edx=0;                                                            // EAX = EAX, EDX = 0
    context->Eip+=length;
    return true;
}
int installCrashHandler() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1,[](EXCEPTION_POINTERS* failure)->LONG {
        const auto code=failure->ExceptionRecord->ExceptionCode;
        if(code==EXCEPTION_INT_DIVIDE_BY_ZERO&&divideLikeTheDs(failure->ContextRecord)) {
            static void* reported[16]{};
            void* where=failure->ExceptionRecord->ExceptionAddress;
            for(auto*& slot:reported) {
                if(slot==where)break;
                if(!slot) {
                    slot=where;
                    std::fprintf(stderr,"Division by zero at %p finished as on the DS\n",where);
                    CONTEXT copy=*failure->ContextRecord; // the stack walk changes the context it gets
                    trace(&copy);
                    break;
                }
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if(code!=EXCEPTION_ACCESS_VIOLATION && code!=EXCEPTION_INT_DIVIDE_BY_ZERO &&
            code!=EXCEPTION_INT_OVERFLOW && code!=EXCEPTION_ILLEGAL_INSTRUCTION)
            return EXCEPTION_CONTINUE_SEARCH;
        std::fprintf(stderr,"Native exception 0x%08lx at %p, address %p\n",failure->ExceptionRecord->ExceptionCode,
            failure->ExceptionRecord->ExceptionAddress,reinterpret_cast<void*>(failure->ExceptionRecord->ExceptionInformation[1]));
        trace(failure->ContextRecord);
        MessageBoxA(nullptr,"Dragon Quest IV a rencontre une erreur et doit se fermer.\nLe detail est dans journal.txt, a cote de la sauvegarde.",
            "Dragon Quest IV",MB_OK|MB_ICONERROR);
        TerminateProcess(GetCurrentProcess(),4);return EXCEPTION_CONTINUE_SEARCH;
    });
    std::set_terminate([] {
        try {if(auto failure=std::current_exception()) std::rethrow_exception(failure);}
        catch(const std::exception& error) {std::fprintf(stderr,"Fatal native initialization: %s\n",error.what());}
        catch(...) {std::fprintf(stderr,"Fatal native initialization: unknown exception\n");}
        trace();TerminateProcess(GetCurrentProcess(),3);
    });
    return 0;
}
}
#pragma section(".CRT$XIB",read)
extern "C" __declspec(allocate(".CRT$XIB")) int (__cdecl* const pcCrashHandlerInitializer)()=installCrashHandler;
#pragma comment(linker,"/include:_pcCrashHandlerInitializer")
#endif

#ifndef _WIN32
#include <SDL3/SDL_messagebox.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <execinfo.h>
#include <link.h>
#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>
namespace {
void trace(void* pc=nullptr) {
    void* frames[48];
    int count=backtrace(frames,48);
    if(pc) {std::fprintf(stderr,"  at %p\n",pc);}
    std::fflush(stderr);
    backtrace_symbols_fd(frames,count,fileno(stderr));
}
// The DS divides in software: x / 0 gives x and x % 0 gives 0 (the
// runtime's _s32_div_f/_u32_div_f), and the game relies on it (a colour fade
// of zero frames). On x86 the division traps: the faulting div/idiv is
// finished the DS way and execution goes on.
#ifdef __x86_64__
constexpr int programCounter=REG_RIP,accumulator=REG_RAX,dataRegister=REG_RDX;
#else
constexpr int programCounter=REG_EIP,accumulator=REG_EAX,dataRegister=REG_EDX;
#endif
bool divideLikeTheDs(greg_t* registers) {
    const auto* code=reinterpret_cast<const unsigned char*>(registers[programCounter]);
    unsigned length=0;bool word=false,quad=false;
    for(;;++length) {
        const unsigned char prefix=code[length];
        if(prefix==0x66) {word=true;continue;}
#ifdef __x86_64__
        if((prefix&0xf0)==0x40) {quad=(prefix&8)!=0;continue;} // REX
#endif
        if(prefix==0x2e||prefix==0x36||prefix==0x3e||prefix==0x26||prefix==0x64||prefix==0x65||prefix==0xf0||prefix==0xf2||prefix==0xf3)continue;
        break;
    }
    const unsigned char opcode=code[length++];
    if(opcode!=0xf6&&opcode!=0xf7)return false;
    const unsigned char modrm=code[length++];
    const unsigned reg=(modrm>>3)&7,mod=modrm>>6,rm=modrm&7;
    if(reg!=6&&reg!=7)return false; // div, idiv
    if(mod!=3) {
        if(rm==4) {const unsigned char sib=code[length++];if(mod==0&&(sib&7)==5)length+=4;}
        if(mod==0&&rm==5)length+=4;
        if(mod==1)length+=1;
        if(mod==2)length+=4;
    }
    auto& eax=reinterpret_cast<unsigned&>(registers[accumulator]);
    auto& edx=reinterpret_cast<unsigned&>(registers[dataRegister]);
    if(opcode==0xf6)eax=(eax&0xffff0000u)|(eax&0xffu); // AL = AL, AH = 0
    else if(word&&!quad)edx&=0xffff0000u;              // AX = AX, DX = 0
    else if(quad)registers[dataRegister]=0;             // RAX = RAX, RDX = 0
    else registers[accumulator]=eax,registers[dataRegister]=0; // EAX = EAX, EDX = 0 (upper halves cleared)
    registers[programCounter]+=length;
    return true;
}
void showFatalError() {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Dragon Quest IV",
        "Dragon Quest IV a rencontre une erreur et doit se fermer.\nLe detail est dans journal.txt, a cote de la sauvegarde.",nullptr);
}
void onFault(int signal,siginfo_t* info,void* context) {
    auto* registers=static_cast<ucontext_t*>(context)->uc_mcontext.gregs;
    // Division by zero; INT_MIN / -1 traps the same way and ends as the DS's.
    if(signal==SIGFPE&&info->si_code==FPE_INTDIV&&divideLikeTheDs(registers)) {
        static void* reported[16]{};
        void* where=info->si_addr;
        for(auto*& slot:reported) {
            if(slot==where)break;
            if(!slot) {
                slot=where;
                std::fprintf(stderr,"Division by zero at %p finished as on the DS\n",where);
                trace(where);
                break;
            }
        }
        return;
    }
    std::fprintf(stderr,"Native signal %d at %p, address %p\n",signal,reinterpret_cast<void*>(registers[programCounter]),info->si_addr);
    trace(reinterpret_cast<void*>(registers[programCounter]));
    showFatalError();
    _exit(4);
}
// The DS keeps read-only data in RAM and the game writes to some of it
// (battle effect parameters, for one): the executable's constant data stays writable.
void makeConstantDataWritable() {
    dl_iterate_phdr([](dl_phdr_info* info,std::size_t,void*) {
        const auto page=std::uintptr_t(sysconf(_SC_PAGESIZE));
        for(int i=0;i<info->dlpi_phnum;++i) {
            const auto& header=info->dlpi_phdr[i];
            if(header.p_type!=PT_LOAD||(header.p_flags&(PF_W|PF_X))||!(header.p_flags&PF_R))continue;
            const auto begin=(info->dlpi_addr+header.p_vaddr)&~(page-1);
            const auto end=(info->dlpi_addr+header.p_vaddr+header.p_memsz+page-1)&~(page-1);
            if(mprotect(reinterpret_cast<void*>(begin),end-begin,PROT_READ|PROT_WRITE)!=0)
                std::fprintf(stderr,"Cannot make constant data writable at %p\n",reinterpret_cast<void*>(begin));
        }
        return 1; // the first object is the executable
    },nullptr);
}
// Before every other initializer, as Windows' .CRT$XIB.
__attribute__((constructor(101))) void installCrashHandler() {
    makeConstantDataWritable();
    // Faults are reported even when the stack has overflowed.
    static char alternate[64*1024];
    stack_t stack{};stack.ss_sp=alternate;stack.ss_size=sizeof alternate;
    sigaltstack(&stack,nullptr);
    struct sigaction action{};
    action.sa_sigaction=onFault;action.sa_flags=SA_SIGINFO|SA_ONSTACK|SA_NODEFER;
    sigemptyset(&action.sa_mask);
    for(int signal:{SIGFPE,SIGSEGV,SIGBUS,SIGILL})sigaction(signal,&action,nullptr);
    std::set_terminate([] {
        try {if(auto failure=std::current_exception()) std::rethrow_exception(failure);}
        catch(const std::exception& error) {std::fprintf(stderr,"Fatal native initialization: %s\n",error.what());}
        catch(...) {std::fprintf(stderr,"Fatal native initialization: unknown exception\n");}
        trace();_exit(3);
    });
}
}
#endif
