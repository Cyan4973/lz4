CC ?= cc
CXX ?= c++
LZ4_DIR ?= ../..
SILESIA_DIR ?= silesia
BENCH_PREFIX ?=
CPPFLAGS += -I$(LZ4_DIR)/lib
CFLAGS ?= -O3 -DNDEBUG
C90FLAGS := -std=c90 -Wall -Wextra -Werror -Wdeclaration-after-statement -Wstrict-prototypes -Wc++-compat -Wno-long-long
LZ4_SRC := $(LZ4_DIR)/lib/lz4.c $(LZ4_DIR)/lib/lz4hc.c
POINT_SRC := lz4point.c
MONO_SRC := lz4point.c lz4point_mono.c
HEADERS := lz4point.h lz4point_internal.h lz4point_read.h lz4point_mono.h
SANFLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all check check-large check-cxx check-sanitize bench bench-ranges clean
all: bench-point bench-mono bench-range

bench-point: bench_point.c $(POINT_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) bench_point.c $(POINT_SRC) $(LZ4_SRC) -o $@
bench-mono: bench_mono.c bench_point.c $(MONO_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) bench_mono.c $(MONO_SRC) $(LZ4_SRC) -o $@
test-point: test_point.c $(POINT_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) test_point.c $(POINT_SRC) $(LZ4_SRC) -o $@
test-mono: test_mono.c $(MONO_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) test_mono.c $(MONO_SRC) $(LZ4_SRC) -o $@
test-large: test_large.c $(POINT_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) test_large.c $(POINT_SRC) $(LZ4_SRC) -o $@
test-mono-large: test_mono_large.c $(MONO_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) test_mono_large.c $(MONO_SRC) $(LZ4_SRC) -o $@
test-point-sanitized: test_point.c $(POINT_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(C90FLAGS) $(SANFLAGS) test_point.c $(POINT_SRC) $(LZ4_SRC) -o $@
test-mono-sanitized: test_mono.c $(MONO_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(C90FLAGS) $(SANFLAGS) test_mono.c $(MONO_SRC) $(LZ4_SRC) -o $@

check: test-point test-mono test-range check-cxx
	./test-point
	./test-mono
	./test-range
check-large: test-large test-mono-large
	./test-large
	./test-mono-large
check-cxx:
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -x c++ -fsyntax-only $(MONO_SRC)
check-sanitize: test-point-sanitized test-mono-sanitized test-range-sanitized
	ASAN_OPTIONS=detect_leaks=1 ./test-point-sanitized
	ASAN_OPTIONS=detect_leaks=1 ./test-mono-sanitized
	ASAN_OPTIONS=detect_leaks=1 ./test-range-sanitized

bench: all
	mkdir -p measurements
	$(BENCH_PREFIX) ./bench-point $(SILESIA_DIR)/* > measurements/flat-fast.csv
	$(BENCH_PREFIX) ./bench-point --dict $(SILESIA_DIR)/* > measurements/flat-fast-dict.csv
	$(BENCH_PREFIX) ./bench-point --hc9 $(SILESIA_DIR)/* > measurements/flat-hc9.csv
	$(BENCH_PREFIX) ./bench-point --hc9 --dict $(SILESIA_DIR)/* > measurements/flat-hc9-dict.csv
	$(BENCH_PREFIX) ./bench-mono $(SILESIA_DIR)/* > measurements/mono-fast.csv
	$(BENCH_PREFIX) ./bench-mono --dict $(SILESIA_DIR)/* > measurements/mono-fast-dict.csv
	$(BENCH_PREFIX) ./bench-mono --hc9 $(SILESIA_DIR)/* > measurements/mono-hc9.csv
	$(BENCH_PREFIX) ./bench-mono --hc9 --dict $(SILESIA_DIR)/* > measurements/mono-hc9-dict.csv

clean:
	rm -f bench-point bench-mono test-point test-mono test-large test-mono-large test-point-sanitized test-mono-sanitized test-range test-range-sanitized bench-range

test-range: test_range.c $(MONO_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) test_range.c $(MONO_SRC) $(LZ4_SRC) -o $@
test-range-sanitized: test_range.c $(MONO_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(C90FLAGS) $(SANFLAGS) test_range.c $(MONO_SRC) $(LZ4_SRC) -o $@
bench-range: bench_range.c bench_point.c $(MONO_SRC) $(HEADERS) $(LZ4_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C90FLAGS) bench_range.c $(MONO_SRC) $(LZ4_SRC) -o $@

bench-ranges: bench-range
	mkdir -p measurements
	$(BENCH_PREFIX) ./bench-range $(SILESIA_DIR)/* > measurements/range-fast.csv
	$(BENCH_PREFIX) ./bench-range --dict $(SILESIA_DIR)/* > measurements/range-fast-dict.csv
	$(BENCH_PREFIX) ./bench-range --hc9 $(SILESIA_DIR)/* > measurements/range-hc9.csv
	$(BENCH_PREFIX) ./bench-range --hc9 --dict $(SILESIA_DIR)/* > measurements/range-hc9-dict.csv
