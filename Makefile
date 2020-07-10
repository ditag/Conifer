conifer : src/main.c src/kraken_stats.c src/kraken_taxo.c
	gcc -std=c99 -Wall -O3 -I third_party/uthash/src -I . src/kraken_stats.c src/kraken_taxo.c src/main.c -o conifer -lm

tests : test/tests.c src/kraken_stats.c src/kraken_taxo.c
	cmake third_party/Unity/CMakeLists.txt
	make -C third_party/Unity/
	clang -Wall -I third_party/uthash/src -I . -I third_party/Unity/src src/kraken_stats.c src/kraken_taxo.c $< -L third_party/Unity/src -o tests -lunity -lm



