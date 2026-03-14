chaosc:
	clang++ -o chaosc chaosc.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -fexceptions
