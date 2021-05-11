#include <algorithm>
#include <random>
#include <type_traits>

void bad_funcs()
{
    int n = std::rand(); // expected-warning {{'std::rand' is not thread-safe function. Consider using random generators (e.g., std::mt19937)}}

    std::vector<int> perm(n);
    std::iota(perm.begin(), perm.end(), 0);
    std::random_shuffle(perm.begin(), perm.end()); // expected-warning {{std::random_shuffle was deprecated since C++14 and removed since C++17}}
}

void bad_construction()
{
    std::mt19937 gen1(0);           // expected-warning {{same seed for a deterministic random engine produces same output}}
                                    // expected-warning@-1 {{constructing random engine for only a few numbers is not the best practice}}
    std::mt19937_64 gen2;           // expected-warning {{same seed for a deterministic random engine produces same output}}
                                    // expected-warning@-1 {{constructing random engine for only a few numbers is not the best practice}}
    std::minstd_rand gen3(23423);   // expected-warning {{same seed for a deterministic random engine produces same output}}
                                    // expected-warning@-1 {{constructing random engine for only a few numbers is not the best practice}}
    std::minstd_rand0 gen4;         // expected-warning {{same seed for a deterministic random engine produces same output}}
                                    // expected-warning@-1 {{constructing random engine for only a few numbers is not the best practice}}
    std::knuth_b gen5(232);         // expected-warning {{same seed for a deterministic random engine produces same output}}
                                    // expected-warning@-1 {{constructing random engine for only a few numbers is not the best practice}}
}

void rand_in_cycle()
{
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<long> dist;

    while (gen()); // expected-warning {{you probably want to use 'uniform_int_distribution' here}}

    for (;dist(gen););
}

void rand_once()
{
    std::mt19937 gen(std::random_device{}()); // expected-warning {{constructing random engine for only a few numbers is not the best practice}}

    int a = gen(); // expected-warning {{you probably want to use 'uniform_int_distribution' here}}
    std::knuth_b gen1(std::random_device{}()); // expected-warning {{constructing random engine for only a few numbers is not the best practice}}

    static std::mt19937 gen3(std::random_device{}()); // OK

    std::uniform_int_distribution<unsigned long> dist(0, 10);
    if (dist(gen)) {
        int b = dist(gen1);
    } else {
        a = std::mt19937{0}();  // expected-warning {{constructing random engine for only a few numbers is not the best practice}}
                                // expected-warning@-1 {{same seed for a deterministic random engine produces same output}}
        a = std::mt19937{}();   // expected-warning {{constructing random engine for only a few numbers is not the best practice}}
                                // expected-warning@-1 {{same seed for a deterministic random engine produces same output}}
        a = std::mt19937(0)();  // expected-warning {{constructing random engine for only a few numbers is not the best practice}}
                                // expected-warning@-1 {{same seed for a deterministic random engine produces same output}}
        a = std::mt19937()();   // expected-warning {{constructing random engine for only a few numbers is not the best practice}}
                                // expected-warning@-1 {{same seed for a deterministic random engine produces same output}}
    }
}

void construct_engine()
{
    std::mt19937 gen1{std::random_device{}()};

    std::random_device rdev;
    std::mt19937 gen2{rdev()};
    std::mt19937 gen3(rdev());

    std::uniform_int_distribution dist(0, 1);

    for (dist(gen1); dist(gen2); dist(gen3)); // suppressing warning of single usage

    gen1.seed(rdev());
}

struct some_engine
{
    some_engine(unsigned long) {}

    void method(unsigned long);
};

void use_random_device(int seed)
{
    std::random_device rdev;

    some_engine engine(rdev()); // expected-warning {{'std::random_device' may work too slow, please, only use it to init seed of some random engine}}
    engine.method(rdev()); // expected-warning {{'std::random_device' may work too slow, please, only use it to init seed of some random engine}}
}

struct random_user
{
    mutable std::random_device m_rdev;
    int field;

    random_user()
    {
        std::mt19937 rd(std::random_device{}());
        field = std::uniform_int_distribution(0, 10)(rd); // no warning for random device in ctor
    }

    int one_shot_rand()
    {
        std::mt19937 rd(std::random_device{}()); // expected-warning {{constructing random engine for only a few numbers is not the best practice}}
        return field + std::uniform_int_distribution(0, 10)(rd);
    }

    unsigned long get_random() const
    {
        return std::uniform_int_distribution(0, 10)(m_rdev); // expected-warning {{'std::random_device' may work too slow, please, only use it to init seed of some random engine}}
    }
};