.PHONY: all clean package docker docker_push docker_alpine builddocs localdocs deploydocs test benchmark test_valgrind

all:
	@$(MAKE) -C ./src all

clean:
	@$(MAKE) -C ./src $@

clean-parser:
	$(MAKE) -C ./deps/libcypher-parser distclean

clean-graphblas:
	$(MAKE) -C ./deps/GraphBLAS clean

package: all
	@$(MAKE) -C ./src package

docker:
	@$(MAKE) -C ./build/docker
	@docker build . -t redislabs/redisgraph

builddocs:
	@mkdocs build

localdocs: builddocs
	@mkdocs serve

deploydocs: builddocs
	@mkdocs gh-deploy

test:
	@$(MAKE) -C ./src test

benchmark:
	@$(MAKE) -C ./src benchmark

memcheck:
	@$(MAKE) -C ./src memcheck

format:
	astyle -Q --options=.astylerc -R --ignore-exclude-errors "./*.c,*.h,*.cpp"
