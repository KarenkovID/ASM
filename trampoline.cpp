#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

template<typename... Args>
struct args_count;

template<>
struct args_count<> {
    const static int INTEGER = 0;
    const static int SSE = 0;
};

template<typename First, typename... Other>
struct args_count<First, Other...> {
    const static int INTEGER = args_count<Other...>::INTEGER + 1;
    const static int SSE = args_count<Other...>::SSE;
};

template<typename... Other>
struct args_count<float, Other...> {
    const static int INTEGER = args_count<Other...>::INTEGER;
    const static int SSE = args_count<Other...>::SSE + 1;
};

template<typename... Other>
struct args_count<double, Other...> {
    const static int INTEGER = args_count<Other...>::INTEGER;
    const static int SSE = args_count<Other...>::SSE + 1;
};

class trampoline_allocator {

    static const size_t PAGE_SIZE = 4096;
    static const size_t TRAMPOLINE_SIZE = 128;
    static void **memory_ptr;

public:
    static void *malloc() {
        if (memory_ptr == nullptr) {
            char *page_ptr = (char *) mmap(
                    nullptr,
                    PAGE_SIZE,
                    PROT_EXEC | PROT_READ | PROT_writeCode,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1,
                    0);
            memory_ptr = (void **) page_ptr;
            char *cur = nullptr;
            for (size_t i = TRAMPOLINE_SIZE; i < PAGE_SIZE; i += TRAMPOLINE_SIZE) {
                cur = page_ptr + i;
                *(void **) (cur - TRAMPOLINE_SIZE) = cur;
            }
            *(void **) cur = nullptr;
        }
        void *ptr = memory_ptr;
        memory_ptr = (void **) *memory_ptr;
        return ptr;
    }

    static void free(void *ptr) {
        *(void **) ptr = memory_ptr;
        memory_ptr = (void **) ptr;
    }
};

void **trampoline_allocator::memory_ptr = nullptr;

template<typename T>
struct trampoline;

template<typename R, typename... Args>
struct trampoline<R(Args...)> {

    template<typename F>
    trampoline(F const &func)
            : functor(new F(func)),
              caller(&do_call < F > ),
              deleter(do_delete < F > ) {
        code = trampoline_allocator::malloc();
        dst_code = (char *) code;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
        if (args_count<Args ...>::INTEGER < 6) {
            for (int i = args_count<Args ...>::INTEGER - 1; i >= 0; i--) {
                writeCode(shifting[i]);
            }
            writeCode("\x48\xBF");//mov rdi, imm
            *(void **) dst_code = functor;
            dst_code += 8;
            writeCode("\x48\xB8");//mov rax, imm
            *(void **) dst_code = (void *) &do_call<F>;
            dst_code += 8;
            writeCode("\xFF\xE0");//jmp rax
        } else {

            ///shifting registers
            int size = args_count<Args ...>::INTEGER - 5 + std::max(args_count<Args ...>::SSE - 8, 0);
            size *= 8;

            ///mov r11, [rsp]
            writeCode("\x4C\x8B\x1C\x24");

            for (int i = 5; i >= 0; i--) {
                writeCode(shifting[i]);
            }
            ///end shifting

            ///saving stack pos and calculating end of "our" stack
            ///mov rax, rsp
            ///add rax, size
            writeCode("\x48\x89\xE0\x48\x05");
            *(int32_t *) dst_code = size;
            dst_code += 4;


            writeCode("\x48\x81\xC4");//add rsp, 0x8
            *(int32_t *) dst_code = 8;
            dst_code += 4;
            char *begin_loop_label = dst_code;

            ///cmp rax, 0xrsp
            ///je end_loop_label
            writeCode("\x48\x39\xE0\x74");

            char *end_loop_label = dst_code;
            ++dst_code;

            writeCode("\x48\x81\xC4");//add rsp, 0x8
            *(int32_t *) dst_code = 8;
            dst_code += 4;

            ///mov rdi, [rsp]
            ///mov [rsp - 8], rdi
            ///jmp begin_loop_label
            writeCode("\x48\x8B\x3C\x24\x48\x89\x7C\x24\xF8\xEB");

            *dst_code = static_cast<char>(begin_loop_label - dst_code - 1);
            ++dst_code;
            *end_loop_label = static_cast<char>(dst_code - end_loop_label - 1);

            ///mov [rsp], r11
            ///sub rsp, size
            writeCode("\x4C\x89\x1C\x24\x48\x81\xEC");
            *(int32_t *) dst_code = size;
            dst_code += 4;

            writeCode("\x48\xBF");//mov rdi, imm
            *(void **) dst_code = functor;
            dst_code += 8;

            writeCode("\x48\xB8");//mov rax, imm
            *(void **) dst_code = (void *) &do_call<F>;
            dst_code += 8;

            ///call rax
            ///pop r9
            ///mov r11, [rsp + size - 8]
            writeCode("\xFF\xD0\x41\x59\x4C\x8B\x9C\x24");
            *(int32_t *) dst_code = size - 8;
            dst_code += 4;

            ///mov [rsp], r11
            ///ret
            writeCode("\x4C\x89\x1C\x24\xC3");
        }
#pragma clang diagnostic pop
        dst_code = nullptr;
    }

    ~trampoline() {
        if (functor != nullptr) {
            deleter(functor);
        }
        trampoline_allocator::free(code);
    }

    trampoline(trampoline &&other) : code(other.code), deleter(other.deleter), functor(other.functor) {
        other.functor = nullptr;
    }

    trampoline(const trampoline &) = delete;

    trampoline &operator=(trampoline &&other) {
        functor = other.functor;
        code = other.code;
        deleter = other.deleter;
        other.functor = nullptr;
        return *this;
    }

    trampoline &operator=(const trampoline &other) = delete;

    R (*get() const )(Args ...args) {
        return (R (*)(Args ...args)) code;
    }

private:
    const char *shifting[6] = {
            "\x48\x89\xFE", //mov rsi, rdi 1 -> 2
            "\x48\x89\xF2", //mov rdx, rsi 2 -> 3
            "\x48\x89\xD1", //mov rcx, rdx 3 -> 4
            "\x49\x89\xC8", //mov r8,  rcx 4 -> 5
            "\x4D\x89\xC1", //mov r9,  r8  5 -> 6
            "\x41\x51"      //push r9
    };

    void *functor;
    void *code;
    char *dst_code;

    R (*caller)(void *obj, Args ...args);

    void (*deleter)(void *);

    template<typename F>
    static void do_delete(void *obj) {
        delete ((F *) obj);
    }

    template<typename F>
    static R do_call(void *functor, Args ...args) {
        return (*(F *) functor)(args...);
    }

    void writeCode(const char *op_code) {
        while (*op_code) {
            *(dst_code++) = *(op_code++);
        }
    }
};

//--------------------------- TESTS ------------------------------------------

using namespace std;
const double EPS = 0.001;

void tests_0() {

    trampoline<int(int, int, int, int, int)> t0(
            [&](int x0, int x1, int x2, int x3, int x4) { return x0 + x1 + x2 + x3 + x4; });
    assert(1 + 2 + 3 + 4 + 5 == t0.get()(1, 2, 3, 4, 5));

    trampoline<int(int *, int *, int *, int *, int *)> t1(
            [&](int *x0, int *x1, int *x2, int *x3, int *x4) { return *x0 + *x1 + *x2 + *x3 + *x4; });
    unique_ptr<int> m[5];
    for (size_t i = 0; i < 5; ++i) {
        m[i] = unique_ptr<int>(new int(i + 1));
    }
    assert(1 + 2 + 3 + 4 + 5 == t1.get()(m[0].get(), m[1].get(), m[2].get(), m[3].get(), m[4].get()));

    trampoline<double(int, int, int, int, int, double, double, double, double, float, float, float, float)> t2(
            [&](int x0, int x1, int x2, int x3, int x4,
                double y0, double y1, double y2, double y3,
                float z0, float z1, float z2, float z3) {
                return y0 * z0 + y1 * z1 + y2 * z2 + y3 * z3 + x0 + x1 + x2 + x3 + x4;
            });
    double ans = t2.get()(1, 1, 1, 1, 1, 3.5, 3.5, 3.5, 3.5, 2.5, 2.5, 2.5, 2.5);
    double real_ans = 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 1 + 1 + 1 + 1 + 1;
    assert(ans - real_ans > -EPS && ans - real_ans < EPS);

    trampoline<double(int, int, int, int, int, double, double, double, double, double, float, float, float, float,
                      float)> t3(
            [&](int x0, int x1, int x2, int x3, int x4,
                double y0, double y1, double y2, double y3, double y4,
                float z0, float z1, float z2, float z3, float z4) {
                return y0 * z0 + y1 * z1 + y2 * z2 + y3 * z3 + y4 * z4 + x0 + x1 + x2 + x3 + x4;
            });
    ans = t3.get()(1, 1, 1, 1, 1, 3.5, 3.5, 3.5, 3.5, 3.5, 2.5, 2.5, 2.5, 2.5, 2.5);
    real_ans = 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 1 + 1 + 1 + 1 + 1;
    assert(ans - real_ans > -EPS && ans - real_ans < EPS);

    cout << "tests_0 (only shift reg): OK\n";
};

void tests_1() {

    trampoline<int(int, int, int, int, int, int, int, int)> t1(
            [&](int x0, int x1, int x2, int x3, int x4, int x5, int x6, int x7) {
                return x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7;
            });
    assert(1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 == t1.get()(1, 2, 3, 4, 5, 6, 7, 8));

    trampoline<double(int, int, int, int, int, int, int, int, double, float)> t2(
            [&](int x0, int x1, int x2, int x3, int x4, int x5, int x6, int x7, double y0, float z0) {
                return x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 + y0 * z0;
            });
    double ans = t2.get()(1, 2, 3, 4, 5, 6, 7, 8, 3.5, 2.5);
    double real_ans = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 3.5 * 2.5;
    assert(ans - real_ans > -EPS && ans - real_ans < EPS);

    trampoline<double(int, int, int, int, int, int, int, int, double, double, double, double, double, float, float,
                      float, float, float)> t3(
            [&](int x0, int x1, int x2, int x3, int x4, int x5, int x6, int x7,
                double y0, double y1, double y2, double y3, double y4,
                float z0, float z1, float z2, float z3, float z4) {
                return y0 * z0 + y1 * z1 + y2 * z2 + y3 * z3 + y4 * z4 + x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7;
            });
    ans = t3.get()(1, 1, 1, 1, 1, 1, 1, 1, 3.5, 3.5, 3.5, 3.5, 3.5, 2.5, 2.5, 2.5, 2.5, 2.5);
    real_ans = 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 3.5 * 2.5 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1;
    assert(ans - real_ans > -EPS && ans - real_ans < EPS);

    vector<char> v;
    for (int i = 0; i < 9; ++i) {
        v.push_back(char(65 + i));
    }
    trampoline<int(int, int, int, int, int, int, int, int)> t4(
            [&](int x0, int x1, int x2, int x3, int x4, int x5, int x6, int x7) {
                return x0 * x1 * x2 * x3 * x4 * x5 * x6 * x7;
            });
    assert(1 * 2 * 3 * 4 * 5 * 6 * 7 * 8 == t4.get()(1, 2, 3, 4, 5, 6, 7, 8));
    bool ok = v.size() == 9;
    for (int i = 0; i < 9; ++i) {
        ok &= v[i] == char(65 + i);
    }
    assert(ok);

    cout << "tests_1 (with stack): OK\n";
}


void tests_2() {

    trampoline<int(int, int, int)> t0 = [&](int x0, int x1, int x2) { return x0 + x1 + x2; };
    assert(6 == t0.get()(1, 2, 3));

    trampoline<int(int, int, int)> t1(move(t0));
    assert(6 == t1.get()(1, 2, 3));

    trampoline<int(int, int, int)> t2 = [&](int x0, int x1, int x2) {
        return -x0 - x1 - x2;
    };
    assert(-6 == t2.get()(1, 2, 3));

    t2 = move(t1);
    assert(6 == t2.get()(1, 2, 3));

    cout << "tests_2 (copy and assign): OK\n";
}

int main() {
    cout << "default test:\n";
    int b = 123;
    trampoline<int(int)> tr([&](int a) { return printf("%d %d\n", a, b); });
    auto p = tr.get();
    p(5);
    b = 124;
    p(6);

    tests_0();
    tests_1();
    tests_2();

    return 0;
}