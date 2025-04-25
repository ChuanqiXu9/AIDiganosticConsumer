// clang++ test/unique.cpp -fplugin=build/libAIDiagnosticConsumer.so -std=c++17
// CLANG_AI_REPLY_LANG=English clang++ test/unique.cpp -fplugin=build/libAIDiagnosticConsumer.so -std=c++17
// CLANG_AI_REPLY_LANG=Japenese clang++ test/unique.cpp -fplugin=build/libAIDiagnosticConsumer.so -std=c++17

#include <memory>
class Impl;
class A {
public:
	A();
	
	void whatever();
private:
	std::unique_ptr<Impl> impl;
};

void func() {
	A a;
}
