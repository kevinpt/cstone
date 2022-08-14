#ifndef MIN_UINT_HPP
#define MIN_UINT_HPP

/*
Create an integer type that is the minimum size needed to represent a value.
This is useful for templatized code to tailor integer types to optimal sizes
for various platforms.

https://stackoverflow.com/questions/31334291/select-an-integer-type-based-on-template-integer-parameter
*/


template<uintmax_t K> struct MinUInt {
  typedef typename MinUInt<(K & (K - 1)) == 0 ? K / 2 : (K & (K - 1))>::type type;
};

template<> struct MinUInt<0> {
  typedef uint8_t type;
};

template<> struct MinUInt<256> {
  typedef uint16_t type;
};

template<> struct MinUInt<65536> {
  typedef uint32_t type;
};

// Extend specializations as needed


#endif // MIN_UINT_HPP
