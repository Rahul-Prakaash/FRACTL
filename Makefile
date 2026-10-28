
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CFLAGS   ?= -std=c11 -Wall -Wextra -O2

LLVM_CONFIG ?= $(shell command -v llvm-config-20 2>/dev/null || command -v llvm-config 2>/dev/null)
LLVM_CXXFLAGS = $(patsubst -I%,-isystem %,$(shell $(LLVM_CONFIG) --cxxflags)) -Wall -Wextra -O2
LLVM_LDFLAGS  = $(shell $(LLVM_CONFIG) --link-static --ldflags --libs core support target native orcjit) \
                -Wl,--as-needed $(subst -lzstd,-l:libzstd.so.1,$(shell $(LLVM_CONFIG) --link-static --system-libs))

HEADERS := ast.hpp ir.hpp lexer.hpp parser.hpp rewriter.hpp lower.hpp frontend.hpp rng.hpp scene.hpp
DEMOS   := $(basename $(wildcard *.fr))
DEMOS3D := $(basename $(shell grep -l 'dimensions: *3' *.fr 2>/dev/null))
DEMOS2D := $(filter-out $(DEMOS3D),$(DEMOS))
BUILD   := build

ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

DRIVER := fr_frontend$(EXE)
TEST   := tests/test_frontend$(EXE)

all: fractlc runtime.o $(DRIVER)

frontend: $(DRIVER)

$(BUILD):
	mkdir -p $(BUILD)


$(DRIVER): fr_frontend.cpp $(HEADERS) json.hpp
	$(CXX) $(CXXFLAGS) -o $@ fr_frontend.cpp

$(TEST): tests/test_frontend.cpp $(HEADERS) json.hpp
	$(CXX) $(CXXFLAGS) -I. -o $@ tests/test_frontend.cpp

demo-frontend: $(DRIVER)
	@for f in $(DEMOS); do echo "=== $$f.fr ==="; ./$(DRIVER) $$f.fr --ast --stats; done


$(BUILD)/codegen.o: codegen.cpp codegen.hpp $(HEADERS) | $(BUILD)
	$(CXX) $(LLVM_CXXFLAGS) -fexceptions -c codegen.cpp -o $@

$(BUILD)/emit_object.o: emit_object.cpp | $(BUILD)
	$(CXX) $(LLVM_CXXFLAGS) -c emit_object.cpp -o $@

$(BUILD)/jit.o: jit.cpp runtime.h | $(BUILD)
	$(CXX) $(LLVM_CXXFLAGS) -c jit.cpp -o $@

$(BUILD)/fractlc.o: fractlc.cpp codegen.hpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c fractlc.cpp -o $@

fractlc: $(BUILD)/fractlc.o $(BUILD)/codegen.o $(BUILD)/emit_object.o $(BUILD)/jit.o runtime.o
	$(CXX) $^ $(LLVM_LDFLAGS) -o $@

runtime.o: runtime.c runtime.h
	$(CC) $(CFLAGS) -c runtime.c -o $@

$(BUILD)/%.svg: %.fr fractlc runtime.o | $(BUILD)
	./fractlc $< -o $(BUILD)/$*.o --svg $@ --emit-ir $(BUILD)/$*.ll
	$(CC) $(BUILD)/$*.o runtime.o -lm -o $(BUILD)/$*
	./$(BUILD)/$*

$(BUILD)/%.f3d: %.fr fractlc runtime.o | $(BUILD)
	./fractlc $< -o $(BUILD)/$*.o --out $@ --emit-ir $(BUILD)/$*.ll
	$(CC) $(BUILD)/$*.o runtime.o -lm -o $(BUILD)/$*
	./$(BUILD)/$*

$(DEMOS2D): %: $(BUILD)/%.svg
$(DEMOS3D): %: $(BUILD)/%.f3d

demo: $(addprefix $(BUILD)/,$(addsuffix .svg,$(DEMOS2D)))
	mkdir -p viewer/data
	@for n in $(DEMOS2D); do cp $(BUILD)/$$n.svg $(BUILD)/$$n.ll viewer/data/; done
	@echo 'copied $(DEMOS2D) into viewer/data - now run .\build.ps1 -Viewer on Windows'


$(BUILD)/test_runtime: tests/test_runtime.c runtime.c runtime.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_runtime.c runtime.c -lm -o $@

test-frontend: $(TEST)
	./$(TEST) .

test-backend: fractlc runtime.o $(BUILD)/test_runtime
	./$(BUILD)/test_runtime
	sh tests/test_pipeline.sh

test-golden: fractlc runtime.o
	sh tests/run_golden.sh

test-modes: fractlc runtime.o
	sh tests/test_modes.sh

test: test-frontend test-backend test-golden test-modes

golden: fractlc runtime.o
	sh tests/run_golden.sh --update

bundle: fractlc runtime.o
	sh tools/make_bundle.sh

clean:
	rm -rf $(BUILD) fractlc runtime.o $(DRIVER) $(TEST)

.PHONY: all frontend demo-frontend $(DEMOS) demo test test-frontend test-backend \
        test-golden test-modes golden bundle clean
.SECONDARY:
