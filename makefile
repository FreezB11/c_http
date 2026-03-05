CC      = gcc
CXX     = g++
CFLAGS  = -O2 -Wall -Wextra -std=c11   -Iinclude
CXXFLAGS= -O2 -Wall -Wextra -std=c++17 -Iinclude
LDFLAGS = -lpthread -lm

SRC_C   = src/conn.c   \
          src/http.c   \
          src/parse.c  \
          src/response.c \
          src/route.c  \
          src/worker.c \
          src/json.c

SRC_CPP = src/app.cpp

OBJ_C   = $(SRC_C:.c=.o)
OBJ_CPP = $(SRC_CPP:.cpp=.o)
ALL_OBJ = $(OBJ_C) $(OBJ_CPP)

LIB     = libhttp.a
EXAMPLE = example/server

.PHONY: all clean lib example

all: lib example

lib: $(LIB)

$(LIB): $(ALL_OBJ)
	ar rcs $@ $^
	@echo "Built $(LIB)"

example: $(EXAMPLE)

$(EXAMPLE): example/main.cc $(LIB)
	$(CXX) $(CXXFLAGS) $< -L. -lhttp $(LDFLAGS) -o $@
	@echo "Built $(EXAMPLE)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(ALL_OBJ) $(LIB) $(EXAMPLE)

# ─── install (optional) ────────────────────────────────────────────────
PREFIX ?= /usr/local

install: $(LIB)
	install -d $(PREFIX)/lib $(PREFIX)/include/http
	install -m 644 $(LIB) $(PREFIX)/lib/
	install -m 644 include/http/*.h   $(PREFIX)/include/http/
	install -m 644 include/http/*.hpp $(PREFIX)/include/http/
	@echo "Installed to $(PREFIX)"