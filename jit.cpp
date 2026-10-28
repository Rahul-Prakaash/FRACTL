#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/Mangling.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>

#include "runtime.h"

namespace fractl {

bool runModuleJIT(std::unique_ptr<llvm::LLVMContext> ctx, std::unique_ptr<llvm::Module> mod,
                  std::string& err, double& compileMs, double& runMs) {
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();
    using namespace llvm;
    using namespace llvm::orc;
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    auto jtmb = JITTargetMachineBuilder::detectHost();
    if (!jtmb) { err = "JIT: " + toString(jtmb.takeError()); return false; }
    jtmb->setCodeGenOptLevel(CodeGenOptLevel::None);
    auto jit = LLJITBuilder().setJITTargetMachineBuilder(std::move(*jtmb)).create();
    if (!jit) { err = "JIT: " + toString(jit.takeError()); return false; }
    LLJIT& J = **jit;
    mod->setDataLayout(J.getDataLayout());

    MangleAndInterner mangle(J.getExecutionSession(), J.getDataLayout());
    SymbolMap syms;
    auto add = [&](const char* name, std::uintptr_t addr) {
        syms[mangle(name)] = ExecutorSymbolDef(ExecutorAddr(static_cast<std::uint64_t>(addr)),
                                               JITSymbolFlags::Exported | JITSymbolFlags::Callable);
    };
#define FRACTL_RT(fn) add(#fn, reinterpret_cast<std::uintptr_t>(&fn))
    FRACTL_RT(turtle_init);   FRACTL_RT(turtle_forward); FRACTL_RT(turtle_move);
    FRACTL_RT(turtle_turn);   FRACTL_RT(turtle_push);    FRACTL_RT(turtle_pop);
    FRACTL_RT(turtle_finish);
    FRACTL_RT(turtle3_init);  FRACTL_RT(turtle3_begin);  FRACTL_RT(turtle3_forward);
    FRACTL_RT(turtle3_blade); FRACTL_RT(turtle3_move);   FRACTL_RT(turtle3_yaw);
    FRACTL_RT(turtle3_pitch); FRACTL_RT(turtle3_roll);   FRACTL_RT(turtle3_push);
    FRACTL_RT(turtle3_pop);   FRACTL_RT(turtle3_thin);   FRACTL_RT(turtle3_color);
    FRACTL_RT(turtle3_end);   FRACTL_RT(turtle3_finish);
    FRACTL_RT(fractl_seed);   FRACTL_RT(fractl_rand);
#undef FRACTL_RT

    if (auto e = J.getMainJITDylib().define(absoluteSymbols(std::move(syms)))) {
        err = "JIT: " + toString(std::move(e));
        return false;
    }
    if (auto e = J.addIRModule(ThreadSafeModule(std::move(mod), ThreadSafeContext(std::move(ctx))))) {
        err = "JIT: " + toString(std::move(e));
        return false;
    }
    auto sym = J.lookup("fractl_main");
    if (!sym) { err = "JIT: " + toString(sym.takeError()); return false; }
    auto t1 = Clock::now();
    sym->toPtr<void (*)()>()();
    auto t2 = Clock::now();
    compileMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    runMs     = std::chrono::duration<double, std::milli>(t2 - t1).count();
    return true;
}

}
