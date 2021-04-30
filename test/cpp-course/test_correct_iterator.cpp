#include <cstddef>
#include <iterator>
#include <type_traits>
#include <vector>

template <bool is_const>
struct Iterator // expected-warning {{'Iterator<true>' declared as iterator in 'IntRng', but it doesn't complete iterator contract:}}
{
    using value_type = std::conditional_t<is_const, const int, int>;
    typedef value_type & reference;
    using pointer = value_type *;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator & operator = (const Iterator &) = default;

    bool operator == (const Iterator & other) const;
    friend bool operator != (const Iterator & lhs, const Iterator & rhs);

    reference operator * ();
    pointer operator -> ();

    Iterator operator ++ (); // should return reference
    Iterator operator ++(int);

    Iterator operator -- (); // should return reference
    Iterator operator -- (int);

    int * data;
};

struct IntRng
{
    using iterator = Iterator<true>; // expected-note {{declared as 'iterator' alias here}}
    using const_iterator = Iterator<false>;

    iterator begin();
};

struct DoubleRng
{
    struct iterator // expected-warning {{'iterator' declared as iterator in 'DoubleRng', but it doesn't complete iterator contract:}}
                    // expected-note@-1 {{declared as 'iterator' alias here}}
    {
        using value_type = double;
    };
};

struct LongRng
{
    struct iterator // good iterator
    {
        using value_type = long;
        using reference = value_type &;
        using pointer = value_type *;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;

        iterator(const iterator &) = default;
        iterator & operator = (const iterator &) = default;

        reference operator * ();
        pointer operator -> ();

        iterator & operator ++ ();
        iterator operator ++ (int);

        iterator & operator -- ();
        iterator operator -- (int);

        bool operator == (const iterator &) const;
        bool operator != (const iterator &) const;
    };
};

template <class T>
struct TemplateRng
{
    template <bool is_const>
    struct TempIter // expected-warning {{'TempIter' declared as iterator in 'TemplateRng', but it doesn't complete iterator contract:}}
    {
        using value_type = std::conditional_t<is_const, const T, T>;
        using reference = value_type &;
        using pointer = value_type *;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        value_type operator * (); // should return reference
        pointer operator -> ();

        TempIter & operator ++ ();
        TempIter operator ++ (int);

        friend bool operator == (const TempIter &, const TempIter &);
        friend bool operator != (const TempIter &, const TempIter &);
    };

    using iterator = TempIter<false>; // expected-note {{declared as 'iterator' alias here}}

    iterator begin(); // instantiate 'iterator'
    iterator end();
};


void instantiate()
{
    TemplateRng<int> rng;
    for (int el : rng) { ++el; }
}

struct NonCopyable
{
    NonCopyable(const NonCopyable &) = delete; // expected-note-re + {{{{.*}}}}
    NonCopyable & operator = (const NonCopyable &) = delete; // expected-note-re + {{{{.*}}}}
};

struct NonCopyableRng
{
    struct iterator // expected-warning {{'iterator' declared as iterator in 'NonCopyableRng', but it doesn't complete iterator contract:}}
                    // expected-note@-1 {{declared as 'iterator' alias here}}
    {
        using value_type = NonCopyable;
        using reference = value_type &;
        using pointer = value_type *;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        NonCopyable val; // expected-note-re + {{{{.*}}}}

        iterator(const iterator &) = default; // expected-warning {{explicitly defaulted copy constructor is implicitly deleted}}
        iterator & operator = (const iterator &) = default; // expected-warning {{explicitly defaulted copy assignment operator is implicitly deleted}}

        reference operator * ();
        pointer operator -> ();

        iterator & operator ++ ();
        iterator operator ++ (int);

        bool operator == (const iterator &) const;
        bool operator != (const iterator &) const;
    };
};

struct ConstIntRng
{
    struct iterator // expected-warning {{'iterator' declared as iterator in 'ConstIntRng', but it doesn't complete iterator contract:}}
                    // expected-note@-1 {{declared as 'iterator' alias here}}
    {
        using value_type = const int;
        using reference = value_type &;
        using pointer = value_type *;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        reference operator * ();
        pointer operator -> ();

        iterator & operator ++ ();
        iterator operator ++ (int);

        bool operator == (const iterator &) const;
        // no '!=' comparison
    };
};