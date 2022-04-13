int isalpha(int);
int isdigit(int);

namespace std {
using ::isalpha;
inline int isdigit(int i) { return ::isdigit(i); }
} // namespace std

int main()
{
    int c1 = '1';
    char c2 = '2';
    signed char c3 = '3';
    unsigned char c4 = '4';

    (void) std::isalpha('x'); // expected-warning {{'isalpha' called with 'char' argument which may be UB. Use static_cast to unsigned char}}
    (void) ::isalpha('y');    // expected-warning {{'isalpha' called with 'char' argument which may be UB. Use static_cast to unsigned char}}
    (void) isalpha('z');      // expected-warning {{'isalpha' called with 'char' argument which may be UB. Use static_cast to unsigned char}}

    (void) std::isdigit('a'); // expected-warning {{'std::isdigit' called with 'char' argument which may be UB. Use static_cast to unsigned char}}
    (void) ::isdigit('b');    // expected-warning {{'isdigit' called with 'char' argument which may be UB. Use static_cast to unsigned char}}
    (void) isdigit('c');      // expected-warning {{'isdigit' called with 'char' argument which may be UB. Use static_cast to unsigned char}}


    (void) std::isalpha(c1);
    (void) std::isalpha(c2); // expected-warning {{'isalpha' called with 'char' argument which may be UB. Use static_cast to unsigned char}}
    (void) std::isalpha(c3); // expected-warning {{'isalpha' called with 'signed char' argument which may be UB. Use static_cast to unsigned char}}
    (void) std::isalpha(c4);

    return 0;
}
