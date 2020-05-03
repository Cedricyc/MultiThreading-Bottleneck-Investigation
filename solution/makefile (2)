cc = g++
flags = -std=c++11 -lpthread -g
NORMAL:all.cpp
	$(cc) $(flags) $< -o $@ 
BRUTE_FORCE:all.cpp
	$(cc) $(flags) $< -o $@ -D BRUTE_FORCE
NO_CALC:all.cpp
	$(cc) $(flags) $< -o $@ -D NO_CALC
LOCK:all.cpp
	$(cc) $(flags) $< -o $@ -D USE_LOCK
clean:
	rm -rf NORMAL BRUTE_FORCE NO_CALC LOCK
	
