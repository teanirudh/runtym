APP := app
DIR := $(APP)/build
YCSB_LOG := $(APP)/ycsb/gen-log
TPCC_LOG := $(APP)/tpcc/gen-log

.PHONY: gen-ycsb build-ycsb run-ycsb gen-tpcc build-tpcc run-tpcc clean

gen-ycsb:
	@test -n "$(RD)" -a -n "$(WR)" || \
	  { echo "usage: make gen RD=<reads> WR=<writes>"; exit 1; }
	@cd $(YCSB_LOG) && \
	  g++ -O3 -DRD=$(RD) -DWR=$(WR) -DHR=$(or $(HR),0) -DHW=$(or $(HW),0) \
	  -o generator generate_ycsb_zipf.cc && ./generator

build-ycsb:
	@test -n "$(RD)" -a -n "$(WR)" || \
	  { echo "usage: make build RD=<reads> WR=<writes>"; exit 1; }
	@cmake -S $(APP) -B $(DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_CXX_FLAGS="-DRPC_LATENCY -DLOG_LATENCY -DRD=$(RD) -DWR=$(WR) -DSL=$(or $(SL),0)" && \
	cmake --build $(DIR) --target ycsb && mkdir -p $(DIR)/results

run-ycsb:
	@test -n "$(NC)" -a -n "$(IN)" -a -n "$(LG)" || \
	  { echo "usage: make run NC=<num_cores> IN=<interval> LG=<log_file>"; exit 1; }
	@test -f $(DIR)/ycsb || \
	  { echo "error: missing build $(DIR)/ycsb"; exit 1; }
	@src="$(CURDIR)/$(YCSB_LOG)/$(LG)"; \
	test -f "$$src" || { echo "missing input: $$src"; exit 1; }; \
	cd $(DIR) && we=$$(($(shell nproc) - 1)); ws=$$((we - $(NC) + 1)); \
	start=$$(date +%s); \
	sudo taskset -c "$$ws-$$we" ./ycsb -n $(NC) "$$src" -i $(IN); \
	end=$$(date +%s); echo "time taken: $$((end - start)) seconds"

gen-tpcc:
	@test -n "$(WH)" || \
	  { echo "usage: make gen-tpcc WH=<warehouses>"; exit 1; }
	@cd $(TPCC_LOG) && python3 generate_tpcc.py --warehouses $(WH)

build-tpcc:
	@test -n "$(WH)" || \
	  { echo "usage: make build-tpcc WH=<warehouses>"; exit 1; }
	@cmake -S $(APP) -B $(DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_CXX_FLAGS="-DRPC_LATENCY -DLOG_LATENCY -DNUM_WAREHOUSES=$(WH)" && \
	cmake --build $(DIR) --target tpcc && mkdir -p $(DIR)/results

run-tpcc:
	@test -n "$(NC)" -a -n "$(IN)" || \
	  { echo "usage: make run-tpcc NC=<num_cores> IN=<interval>"; exit 1; }
	@test -f $(DIR)/tpcc || \
	  { echo "error: missing build $(DIR)/tpcc"; exit 1; }
	@src="$(CURDIR)/$(TPCC_LOG)/tpcc.txt"; \
	test -f "$$src" || { echo "missing input: $$src (run make gen-tpcc WH=...)"; exit 1; }; \
	cd $(DIR) && we=$$(($(shell nproc) - 1)); ws=$$((we - $(NC) + 1)); \
	start=$$(date +%s); \
	sudo taskset -c "$$ws-$$we" ./tpcc -n $(NC) "$$src" -i $(IN); \
	end=$$(date +%s); echo "time taken: $$((end - start)) seconds"

clean:
	rm -rf $(APP)/build $(YCSB_LOG)/*.txt $(TPCC_LOG)/tpcc.txt
