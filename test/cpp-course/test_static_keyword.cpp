#include "test_static_keyword.h"
#include "test_static_keyword.h"

static int e = 42;

int boo()
{
    int f, g;
    return f - g;
}

static int local_foo() // expected-warning {{it's recommended to use anonymous namespace instead of 'static'}}
{
    return 123;
}

namespace  {
static int h = 4;

static int local_boo() // expected-warning {{it's recommended to use anonymous namespace instead of 'static'}}
{
    return 321;
}

int non_static()
{
    return 132;
}
}