
#include "codegen.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <system_error>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include "lower.hpp"
#include "rewriter.hpp"

namespace fractl {

bool configureModuleForHost(llvm::Module& m, std::string& err);
bool emitObjectFile(llvm::Module& m, const std::string& path, std::string& err);
bool runModuleJIT(std::unique_ptr<llvm::LLVMContext> ctx, std::unique_ptr<llvm::Module> mod,
                  std::string& err, double& compileMs, double& runMs);

struct Backend::Impl {
    std::unique_ptr<llvm::LLVMContext> ctx;
    std::unique_ptr<llvm::Module>      mod;
};

Backend::Backend(const std::string& moduleName) : impl_(new Impl) {
    impl_->ctx = std::make_unique<llvm::LLVMContext>();
    impl_->mod = std::make_unique<llvm::Module>(moduleName, *impl_->ctx);
}

Backend::~Backend() = default;

namespace {

std::string symbolName(char c) {
    if (std::isalnum(static_cast<unsigned char>(c))) return std::string(1, c);
    char buf[8];
    std::snprintf(buf, sizeof buf, "x%02X", static_cast<unsigned char>(c));
    return buf;
}

std::string joinColors(const std::vector<std::string>& cs) {
    std::string s;
    for (const auto& c : cs) s += (s.empty() ? "" : ",") + c;
    return s;
}

struct Emitter {
    llvm::Module&      m;
    llvm::LLVMContext& ctx;
    bool               is3d;
    llvm::Type*        voidTy;
    llvm::Type*        dblTy;
    llvm::Type*        i32Ty;
    llvm::Type*        i64Ty;
    llvm::PointerType* ptrTy;
    std::map<std::string, llvm::FunctionCallee> rt;

    Emitter(llvm::Module& mod, bool threeD)
        : m(mod), ctx(mod.getContext()), is3d(threeD),
          voidTy(llvm::Type::getVoidTy(ctx)), dblTy(llvm::Type::getDoubleTy(ctx)),
          i32Ty(llvm::Type::getInt32Ty(ctx)), i64Ty(llvm::Type::getInt64Ty(ctx)),
          ptrTy(llvm::PointerType::getUnqual(ctx)) {}

    llvm::FunctionCallee fn(const std::string& name, llvm::Type* ret,
                            llvm::ArrayRef<llvm::Type*> params) {
        auto it = rt.find(name);
        if (it != rt.end()) return it->second;
        auto f = m.getOrInsertFunction(name, llvm::FunctionType::get(ret, params, false));
        rt.emplace(name, f);
        return f;
    }
    llvm::FunctionCallee v0(const std::string& n) { return fn(n, voidTy, {}); }
    llvm::FunctionCallee v1(const std::string& n) { return fn(n, voidTy, {dblTy}); }
    llvm::Constant*      num(double v) { return llvm::ConstantFP::get(dblTy, v); }

    void declareRuntime(bool stochastic) {
        if (is3d) {
            fn("turtle3_init", voidTy, {ptrTy});
            fn("turtle3_begin", voidTy, {dblTy, dblTy, dblTy, dblTy, dblTy, dblTy, dblTy, ptrTy, ptrTy});
            for (const char* n : {"turtle3_forward", "turtle3_blade", "turtle3_move", "turtle3_yaw",
                                  "turtle3_pitch", "turtle3_roll", "turtle3_thin"}) v1(n);
            for (const char* n : {"turtle3_push", "turtle3_pop", "turtle3_color", "turtle3_end",
                                  "turtle3_finish"}) v0(n);
        } else {
            fn("turtle_init", voidTy, {dblTy, dblTy, dblTy, dblTy, dblTy, ptrTy});
            for (const char* n : {"turtle_forward", "turtle_move", "turtle_turn"}) v1(n);
            for (const char* n : {"turtle_push", "turtle_pop", "turtle_finish"}) v0(n);
        }
        if (stochastic) {
            fn("fractl_seed", voidTy, {i64Ty});
            fn("fractl_rand", dblTy, {i32Ty});
        }
    }

    void op(llvm::IRBuilder<>& b, const TurtleCmd& c) {
        if (!is3d) {
            switch (c.op) {
                case Op::Forward:   b.CreateCall(v1("turtle_forward"), {num(c.arg)});  return;
                case Op::Move:      b.CreateCall(v1("turtle_move"),    {num(c.arg)});  return;
                case Op::TurnLeft:  b.CreateCall(v1("turtle_turn"),    {num(+c.arg)}); return;
                case Op::TurnRight: b.CreateCall(v1("turtle_turn"),    {num(-c.arg)}); return;
                case Op::Push:      b.CreateCall(v0("turtle_push"));                   return;
                case Op::Pop:       b.CreateCall(v0("turtle_pop"));                    return;
                default:            return;
            }
        }
        switch (c.op) {
            case Op::Forward:   b.CreateCall(v1("turtle3_forward"), {num(c.arg)});  return;
            case Op::Blade:     b.CreateCall(v1("turtle3_blade"),   {num(c.arg)});  return;
            case Op::Move:      b.CreateCall(v1("turtle3_move"),    {num(c.arg)});  return;
            case Op::TurnLeft:  b.CreateCall(v1("turtle3_yaw"),     {num(+c.arg)}); return;
            case Op::TurnRight: b.CreateCall(v1("turtle3_yaw"),     {num(-c.arg)}); return;
            case Op::PitchDown: b.CreateCall(v1("turtle3_pitch"),   {num(+c.arg)}); return;
            case Op::PitchUp:   b.CreateCall(v1("turtle3_pitch"),   {num(-c.arg)}); return;
            case Op::RollLeft:  b.CreateCall(v1("turtle3_roll"),    {num(+c.arg)}); return;
            case Op::RollRight: b.CreateCall(v1("turtle3_roll"),    {num(-c.arg)}); return;
            case Op::Thin:      b.CreateCall(v1("turtle3_thin"),    {num(c.arg)});  return;
            case Op::Push:      b.CreateCall(v0("turtle3_push"));                   return;
            case Op::Pop:       b.CreateCall(v0("turtle3_pop"));                    return;
            case Op::NextColor: b.CreateCall(v0("turtle3_color"));                  return;
        }
    }
};

struct RecursiveFractal {
    Emitter&                        e;
    const FractalDecl&              d;
    std::map<char, llvm::Function*> fns;
    llvm::Function*                 axiomFn = nullptr;

    RecursiveFractal(Emitter& em, const FractalDecl& decl) : e(em), d(decl) {
        using namespace llvm;
        for (const auto& kv : d.rules)
            fns[kv.first] = Function::Create(FunctionType::get(e.voidTy, {e.i32Ty}, false),
                                             Function::InternalLinkage,
                                             d.name + "." + symbolName(kv.first), e.m);
        for (const auto& kv : d.rules) defineSymbol(kv.first, kv.second);
    }

    void body(llvm::IRBuilder<>& b, const std::string& s, llvm::Value* depth) {
        TurtleCmd cmd{Op::Forward, 0.0};
        for (char c : s) {
            auto it = fns.find(c);
            if (it != fns.end())             b.CreateCall(it->second, {depth});
            else if (terminalCmd(d, c, cmd)) e.op(b, cmd);
        }
    }

    void defineSymbol(char c, const RuleSet& rs) {
        using namespace llvm;
        Function*    f     = fns[c];
        Value*       depth = f->getArg(0);
        depth->setName("d");
        LLVMContext& ctx   = e.ctx;
        BasicBlock*  entry = BasicBlock::Create(ctx, "entry", f);
        BasicBlock*  leaf  = BasicBlock::Create(ctx, "leaf", f);
        BasicBlock*  grow  = BasicBlock::Create(ctx, "grow", f);
        IRBuilder<>  b(entry);
        b.CreateCondBr(b.CreateICmpEQ(depth, ConstantInt::get(e.i32Ty, 0), "done"), leaf, grow);

        b.SetInsertPoint(leaf);
        TurtleCmd cmd{Op::Forward, 0.0};
        if (terminalCmd(d, c, cmd)) e.op(b, cmd);
        b.CreateRetVoid();

        b.SetInsertPoint(grow);
        Value* next = b.CreateSub(depth, ConstantInt::get(e.i32Ty, 1), "next");
        if (rs.size() == 1) {
            body(b, rs.front().text, next);
            b.CreateRetVoid();
            return;
        }
        Value* level = b.CreateSub(ConstantInt::get(e.i32Ty, d.iterations), depth, "level");
        Value* r     = b.CreateCall(e.fn("fractl_rand", e.dblTy, {e.i32Ty}), {level}, "r");
        Value* x     = b.CreateFMul(r, e.num(totalWeight(rs)), "x");
        double cum = 0.0;
        for (size_t i = 0; i < rs.size(); ++i) {
            BasicBlock* prod = BasicBlock::Create(ctx, "p" + std::to_string(i), f);
            if (i + 1 < rs.size()) {
                cum += rs[i].weight;
                BasicBlock* nextTest = BasicBlock::Create(ctx, "t" + std::to_string(i + 1), f);
                b.CreateCondBr(b.CreateFCmpOLT(x, e.num(cum)), prod, nextTest);
                b.SetInsertPoint(nextTest);
            } else {
                b.CreateBr(prod);
            }
            IRBuilder<> pb(prod);
            body(pb, rs[i].text, next);
            pb.CreateRetVoid();
        }
    }

    llvm::Function* axiom() {
        using namespace llvm;
        if (axiomFn) return axiomFn;
        axiomFn = Function::Create(FunctionType::get(e.voidTy, false), Function::InternalLinkage,
                                   d.name + ".axiom", e.m);
        IRBuilder<> b(BasicBlock::Create(e.ctx, "entry", axiomFn));
        body(b, d.axiom, ConstantInt::get(e.i32Ty, d.iterations));
        b.CreateRetVoid();
        return axiomFn;
    }
};

}

bool Backend::codegen(const SourceFile& file, const std::vector<Instance>& instances,
                      const CodegenOptions& opts, std::string& err) {
    using namespace llvm;
    Module& m = *impl_->mod;
    if (!configureModuleForHost(m, err)) return false;
    if (file.fractals.empty()) { err = "nothing to compile"; return false; }

    const bool is3d = file.dimensions() == 3;
    bool stochastic = false;
    for (const auto& d : file.fractals) stochastic = stochastic || isStochastic(d);

    Emitter e(m, is3d);
    e.declareRuntime(stochastic && opts.recursive);

    std::map<std::string, RecursiveFractal*>       rec;
    std::vector<std::unique_ptr<RecursiveFractal>> recOwner;
    if (opts.recursive)
        for (const auto& d : file.fractals) {
            recOwner.push_back(std::make_unique<RecursiveFractal>(e, d));
            rec[d.name] = recOwner.back().get();
        }

    Function* fractlMain = Function::Create(FunctionType::get(e.voidTy, false),
                                            Function::ExternalLinkage, "fractl_main", m);
    IRBuilder<> b(BasicBlock::Create(e.ctx, "entry", fractlMain));
    Value* path = b.CreateGlobalString(opts.outPath, is3d ? "out_path" : "svg_path");

    if (!is3d) {
        const FractalDecl& d = file.fractals.front();
        b.CreateCall(e.fn("turtle_init", e.voidTy, {e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.ptrTy}),
                     {e.num(d.step), e.num(d.angle), e.num(d.startX), e.num(d.startY),
                      e.num(d.heading), path});
        if (opts.recursive) {
            if (isStochastic(d))
                b.CreateCall(e.fn("fractl_seed", e.voidTy, {e.i64Ty}), {ConstantInt::get(e.i64Ty, d.seed)});
            b.CreateCall(rec[d.name]->axiom());
        } else {
            for (const TurtleCmd& c : lower(d, expand(d))) e.op(b, c);
        }
        b.CreateCall(e.v0("turtle_finish"));
    } else if (opts.recursive) {
        b.CreateCall(e.fn("turtle3_init", e.voidTy, {e.ptrTy}), {path});
        StructType* instTy = StructType::create(e.ctx, {e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy,
                                                        e.i64Ty, e.i32Ty}, "instance");
        std::vector<Constant*> rows;
        for (const Instance& in : instances)
            rows.push_back(ConstantStruct::get(instTy, {e.num(in.x), e.num(in.y), e.num(in.z), e.num(in.yaw),
                                                        e.num(in.scale), ConstantInt::get(e.i64Ty, in.seed),
                                                        ConstantInt::get(e.i32Ty, static_cast<int>(in.fractal))}));
        ArrayType* tableTy = ArrayType::get(instTy, rows.size());
        auto* table = new GlobalVariable(m, tableTy, true, GlobalValue::PrivateLinkage,
                                         ConstantArray::get(tableTy, rows), "instances");
        table->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

        BasicBlock* entry = b.GetInsertBlock();
        BasicBlock* loop  = BasicBlock::Create(e.ctx, "instance", fractlMain);
        BasicBlock* latch = BasicBlock::Create(e.ctx, "next", fractlMain);
        BasicBlock* done  = BasicBlock::Create(e.ctx, "done", fractlMain);
        b.CreateBr(rows.empty() ? done : loop);

        b.SetInsertPoint(loop);
        PHINode* i = b.CreatePHI(e.i64Ty, 2, "i");
        i->addIncoming(ConstantInt::get(e.i64Ty, 0), entry);
        Value* row = b.CreateInBoundsGEP(tableTy, table, {ConstantInt::get(e.i64Ty, 0), i}, "row");
        auto field = [&](unsigned k, Type* ty, const char* n) {
            return b.CreateLoad(ty, b.CreateStructGEP(instTy, row, k), n);
        };
        Value* x = field(0, e.dblTy, "x"),   *y = field(1, e.dblTy, "y"), *z = field(2, e.dblTy, "z");
        Value* yaw = field(3, e.dblTy, "yaw"), *scale = field(4, e.dblTy, "scale");
        Value* seed = field(5, e.i64Ty, "seed"), *which = field(6, e.i32Ty, "fractal");
        SwitchInst* sw = b.CreateSwitch(which, latch, static_cast<unsigned>(file.fractals.size()));

        for (size_t k = 0; k < file.fractals.size(); ++k) {
            const FractalDecl& d = file.fractals[k];
            BasicBlock* bb = BasicBlock::Create(e.ctx, "grow." + d.name, fractlMain, latch);
            sw->addCase(b.getInt32(static_cast<std::uint32_t>(k)), bb);
            IRBuilder<> fb(bb);
            Value* name = fb.CreateGlobalString(d.name, "name." + d.name);
            Value* cols = fb.CreateGlobalString(joinColors(d.colors), "colors." + d.name);
            fb.CreateCall(e.fn("turtle3_begin", e.voidTy,
                               {e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.ptrTy, e.ptrTy}),
                          {x, y, z, yaw, scale, e.num(d.width), e.num(d.taper), name, cols});
            if (isStochastic(d)) fb.CreateCall(e.fn("fractl_seed", e.voidTy, {e.i64Ty}), {seed});
            fb.CreateCall(rec[d.name]->axiom());
            fb.CreateCall(e.v0("turtle3_end"));
            fb.CreateBr(latch);
        }

        b.SetInsertPoint(latch);
        Value* next = b.CreateAdd(i, ConstantInt::get(e.i64Ty, 1), "i.next");
        i->addIncoming(next, latch);
        b.CreateCondBr(b.CreateICmpULT(next, ConstantInt::get(e.i64Ty, rows.size())), loop, done);

        b.SetInsertPoint(done);
        b.CreateCall(e.v0("turtle3_finish"));
    } else {
        b.CreateCall(e.fn("turtle3_init", e.voidTy, {e.ptrTy}), {path});
        std::map<std::string, Value*> names, colors;
        std::map<std::pair<size_t, std::uint64_t>, Function*> unrolled;
        for (const Instance& in : instances) {
            const FractalDecl& d = file.fractals[in.fractal];
            if (!names.count(d.name)) {
                names[d.name]  = b.CreateGlobalString(d.name, "name." + d.name);
                colors[d.name] = b.CreateGlobalString(joinColors(d.colors), "colors." + d.name);
            }
            b.CreateCall(e.fn("turtle3_begin", e.voidTy,
                              {e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.dblTy, e.ptrTy, e.ptrTy}),
                         {e.num(in.x), e.num(in.y), e.num(in.z), e.num(in.yaw), e.num(in.scale),
                          e.num(d.width), e.num(d.taper), names[d.name], colors[d.name]});
            auto key = std::make_pair(in.fractal, isStochastic(d) ? in.seed : std::uint64_t(0));
            Function*& fnRef = unrolled[key];
            if (!fnRef) {
                fnRef = Function::Create(FunctionType::get(e.voidTy, false), Function::InternalLinkage,
                                         d.name + ".unrolled." + std::to_string(unrolled.size()), m);
                IRBuilder<> ub(BasicBlock::Create(e.ctx, "entry", fnRef));
                for (const TurtleCmd& c : lower(d, expand(d, in.seed))) e.op(ub, c);
                ub.CreateRetVoid();
            }
            b.CreateCall(fnRef);
            b.CreateCall(e.v0("turtle3_end"));
        }
        b.CreateCall(e.v0("turtle3_finish"));
    }
    b.CreateRetVoid();

    Function* cMain = Function::Create(FunctionType::get(e.i32Ty, false),
                                       Function::ExternalLinkage, "main", m);
    IRBuilder<> mb(BasicBlock::Create(e.ctx, "entry", cMain));
    mb.CreateCall(fractlMain);
    mb.CreateRet(ConstantInt::get(e.i32Ty, 0));

    std::string msg;
    raw_string_ostream os(msg);
    if (verifyModule(m, &os)) {
        err = "IR verification failed: " + os.str();
        return false;
    }
    return true;
}

unsigned Backend::foldTurns() {
    using namespace llvm;
    auto rotation = [](Instruction& I) -> CallInst* {
        auto* call = dyn_cast<CallInst>(&I);
        if (!call || !call->getCalledFunction()) return nullptr;
        StringRef n = call->getCalledFunction()->getName();
        if (n != "turtle_turn" && n != "turtle3_yaw" && n != "turtle3_pitch" && n != "turtle3_roll")
            return nullptr;
        return isa<ConstantFP>(call->getArgOperand(0)) ? call : nullptr;
    };
    unsigned folds = 0;
    for (Function& F : *impl_->mod) {
        for (BasicBlock& BB : F) {
            for (auto it = BB.begin(); it != BB.end();) {
                CallInst* a      = rotation(*it);
                auto      nextIt = std::next(it);
                if (!a || nextIt == BB.end()) { ++it; continue; }
                CallInst* bcall = rotation(*nextIt);
                if (!bcall || bcall->getCalledFunction() != a->getCalledFunction()) { ++it; continue; }
                double sum = cast<ConstantFP>(a->getArgOperand(0))->getValueAPF().convertToDouble() +
                             cast<ConstantFP>(bcall->getArgOperand(0))->getValueAPF().convertToDouble();
                bcall->eraseFromParent();
                ++folds;
                if (std::fmod(sum, 360.0) == 0.0) {
                    it = a->eraseFromParent();
                    if (it != BB.begin()) --it;
                } else {
                    a->setArgOperand(0, ConstantFP::get(a->getArgOperand(0)->getType(), sum));
                }
            }
        }
    }
    return folds;
}

IRStats Backend::stats() const {
    IRStats s;
    if (!impl_->mod) return s;
    for (const llvm::Function& F : *impl_->mod) {
        if (F.isDeclaration()) continue;
        ++s.functions;
        for (const llvm::BasicBlock& BB : F)
            for (const llvm::Instruction& I : BB) {
                ++s.instructions;
                if (llvm::isa<llvm::CallInst>(I)) ++s.calls;
            }
    }
    return s;
}

bool Backend::emitIR(const std::string& path, std::string& err) const {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_Text);
    if (ec) { err = "cannot write '" + path + "': " + ec.message(); return false; }
    impl_->mod->print(out, nullptr);
    return true;
}

bool Backend::emitObject(const std::string& path, std::string& err) {
    std::string msg;
    llvm::raw_string_ostream os(msg);
    if (llvm::verifyModule(*impl_->mod, &os)) {
        err = "module verification failed: " + os.str();
        return false;
    }
    return emitObjectFile(*impl_->mod, path, err);
}

bool Backend::runJIT(std::string& err, double& compileMs, double& runMs) {
    if (!impl_->mod) { err = "module already consumed"; return false; }
    return runModuleJIT(std::move(impl_->ctx), std::move(impl_->mod), err, compileMs, runMs);
}

}
