#include <mutex>
#include <shared_mutex>

void a()
{
    std::mutex m;
    std::unique_lock<std::mutex> g(m);
    g.release(); // expected-warning {{std::unique_lock 'release()' method result unused, mutex is locked}}
}

void b()
{
    std::mutex m;
    std::unique_lock g(m);
    g.release(); // expected-warning {{std::unique_lock 'release()' method result unused, mutex is locked}}
}

void c()
{
    std::mutex m;
    using alias = std::unique_lock<std::mutex>;

    alias g(m);
    g.release(); // expected-warning {{std::unique_lock 'release()' method result unused, mutex is locked}}
}

void d()
{
    std::mutex m;
    typedef std::unique_lock<std::mutex> alias;

    alias g(m);
    g.release(); // expected-warning {{std::unique_lock 'release()' method result unused, mutex is locked}}
}

void e()
{
    std::shared_mutex m;
    std::shared_lock<std::shared_mutex> g(m);
    g.release(); // expected-warning {{std::shared_lock 'release()' method result unused, mutex is locked}}
}

void f()
{
    std::shared_mutex m;
    std::shared_lock g(m);
    g.release(); // expected-warning {{std::shared_lock 'release()' method result unused, mutex is locked}}
}

void g()
{
    std::shared_mutex m;
    using alias = std::shared_lock<std::shared_mutex>;

    alias g(m);
    g.release(); // expected-warning {{std::shared_lock 'release()' method result unused, mutex is locked}}
}

void h()
{
    std::shared_mutex m;
    typedef std::shared_lock<std::shared_mutex> alias;

    alias g(m);
    g.release(); // expected-warning {{std::shared_lock 'release()' method result unused, mutex is locked}}
}

template<class Lock, class Mutex>
void tst_template_unique(Mutex & mutex)
{
    Lock guard(mutex);
    guard.release(); // expected-warning {{std::unique_lock 'release()' method result unused, mutex is locked}}
}

template<class Lock, class Mutex>
void tst_template_shared(Mutex & mutex)
{
    Lock guard(mutex);
    guard.release(); // expected-warning {{std::shared_lock 'release()' method result unused, mutex is locked}}
}

void i()
{
    std::shared_mutex m;
    tst_template_unique<std::unique_lock<std::shared_mutex>>(m);
    tst_template_shared<std::shared_lock<std::shared_mutex>>(m);
}

void j()
{
    std::mutex m;
    std::unique_lock<std::mutex> g(m);
    (void) g.release(); // expected-warning {{std::unique_lock 'release()' method result unused, mutex is locked}}
}

void no_warning()
{
    std::mutex m;
    std::unique_lock<std::mutex> g(m);
    auto asdf = g.release();
    asdf->unlock();
}

void warn_ugly_code()
{
    std::mutex m;
    std::unique_lock<std::mutex> g(m);
    g.release()->unlock(); // expected-warning {{use g.unlock() instead}}
}

void no_warning3()
{
    std::mutex m;
    std::unique_lock<std::mutex> g(m);
    std::mutex * mm = nullptr;
    mm = g.release();
    mm->unlock();
}

void warn_no_unlock()
{
    std::mutex m;
    std::unique_lock<std::mutex> g(m);
    auto asdf = g.release(); // expected-warning {{released mutex is not unlocked}}
}
