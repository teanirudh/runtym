APP := app
NPROC := $(shell nproc)

DIR := $(APP)/$(if $(filter release,$(MODE)),build,build-dbg)
TYPE := $(if $(filter release,$(MODE)),Release,Debug)
SUDO := $(if $(filter release,$(MODE)),sudo,)

.PHONY: generate build run clean

generate:
	@case "$(BENCH)" in \
	  ycsb) test -f $(APP)/ycsb/gen-log/ycsb_zipfian_no_cont.txt || { \
	          cd $(APP)/ycsb/gen-log && g++ -o generator -O3 generate_ycsb_zipf.cc && \
	          ./generator -d zipfian -c no_cont; } ;; \
	  tpcc) test -f $(APP)/tpcc/gen-log/tpcc.txt || { \
	          cd $(APP)/tpcc/gen-log && python3 generate_tpcc.py; } ;; \
	  *) echo "usage: make generate BENCH=ycsb|tpcc"; exit 1 ;; \
	esac

build:
	@test "$(MODE)" = debug -o "$(MODE)" = release || \
	  { echo "usage: make build MODE=debug|release BENCH=ycsb|tpcc"; exit 1; }
	@test "$(BENCH)" = ycsb -o "$(BENCH)" = tpcc || \
	  { echo "usage: make build MODE=debug|release BENCH=ycsb|tpcc"; exit 1; }
	@cmake -S $(APP) -B $(DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(TYPE) \
	  -DCMAKE_CXX_FLAGS="-DRPC_LATENCY -DLOG_LATENCY" && \
	cmake --build $(DIR) --target $(BENCH) && mkdir -p $(DIR)/results

run:
	@test -n "$(CORES)$(INTERVAL)$(SOURCE)" && \
	  { test "$(MODE)" = debug -o "$(MODE)" = release; } && \
	  { test "$(BENCH)" = ycsb -o "$(BENCH)" = tpcc; } || \
	  { echo "usage: make run MODE=debug|release BENCH=ycsb|tpcc CORES=<n> INTERVAL=<spec> SOURCE=<file>"; exit 1; }
	@test -f $(DIR)/$(BENCH) || \
	  { echo "missing $(DIR)/$(BENCH) — run make build MODE=$(MODE) BENCH=$(BENCH)"; exit 1; }
	@cd $(DIR) && test -f $(SOURCE) || { echo "missing input: $(DIR)/$(SOURCE)"; exit 1; }
	@cd $(DIR) && worker_end=$$(($(NPROC) - 1)); worker_start=$$((worker_end - $(CORES) + 1)); \
	echo "$(MODE) $(BENCH) cores=$(CORES) interval=$(INTERVAL) source=$(SOURCE)"; \
	echo "dispatcher cores: 0-3"; echo "worker cores: $$worker_start-$$worker_end"; \
	start=$$(date +%s); \
	$(SUDO) taskset -c "$$worker_start-$$worker_end" \
	  ./$(BENCH) -n $(CORES) $(SOURCE) -i $(INTERVAL); \
	end=$$(date +%s); echo "Time taken: $$((end - start)) seconds"
