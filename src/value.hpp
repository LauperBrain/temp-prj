#ifndef _VALUE_HPP_
#define _VALUE_HPP_

#include <iostream>

enum class value_t : std::uint8_t
{
    null,
    string,
    boolean,
    number_integer,
    number_unsigned,
    number_float,
    binary,
    discarded
};

using StringType = char *;
using BoolenType = bool;
using NumberIntegerType = int64_t;
using NumberUnsignedType = uint64_t;
using NumberFloatType = double;
using BinaryType = uint8_t;
using Null

using string_t = StringType;

union value
{
    /// string (stored with pointer to save storage)
    

};









#endif // _VALUE_HPP_
