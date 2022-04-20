cc = gcc
c_flags = -g -Wall -Wextra -pedantic
san_addr = #-fsanitize=address
target = play

c_files = $(wildcard *.c)
o_files = $(patsubst %.c, build/%.o, $(c_files))

build/%.o: %.c
	$(cc) $(c_flags) -c -o $@ $^ $(san_addr)

$(target): $(o_files)
	$(cc) -o $@ $^ $(san_addr)

clean:
	rm $(o_files) $(target)
