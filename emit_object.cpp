
#include <memory>
#include <optional>
#include <string>
#include <system_error>

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

namespace fractl {

namespace {

llvm::TargetMachine* hostMachine(std::string& err) {
    static std::unique_ptr<llvm::TargetMachine> tm;
    if (tm) return tm.get();

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    std::string triple = llvm::sys::getDefaultTargetTriple();
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) return nullptr;

    llvm::TargetOptions opts;
#if LLVM_VERSION_MAJOR >= 21
    tm.reset(target->createTargetMachine(llvm::Triple(triple), "generic", "", opts,
                                         llvm::Reloc::PIC_, std::nullopt,
                                         llvm::CodeGenOptLevel::Default));
#else
    tm.reset(target->createTargetMachine(triple, "generic", "", opts,
                                         llvm::Reloc::PIC_, std::nullopt,
                                         llvm::CodeGenOptLevel::Default));
#endif
    if (!tm) err = "could not create a TargetMachine for " + triple;
    return tm.get();
}

}

bool configureModuleForHost(llvm::Module& m, std::string& err) {
    llvm::TargetMachine* tm = hostMachine(err);
    if (!tm) return false;
#if LLVM_VERSION_MAJOR >= 21
    m.setTargetTriple(tm->getTargetTriple());
#else
    m.setTargetTriple(tm->getTargetTriple().str());
#endif
    m.setDataLayout(tm->createDataLayout());
    return true;
}

bool emitObjectFile(llvm::Module& m, const std::string& path, std::string& err) {
    llvm::TargetMachine* tm = hostMachine(err);
    if (!tm) return false;

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) { err = "cannot write '" + path + "': " + ec.message(); return false; }

    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        err = "target cannot emit an object file";
        return false;
    }
    pm.run(m);
    dest.flush();
    return true;
}

}
