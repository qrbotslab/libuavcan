/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#pragma once

#include <stdint.h>
#include <limits>
#include <uavcan/data_type.hpp>
#include <uavcan/util/compile_time.hpp>
#include <uavcan/impl_constants.hpp>
#include <uavcan/marshal/type_util.hpp>
#include <uavcan/marshal/integer_spec.hpp>
#include <cmath>

#ifndef UAVCAN_CPP_VERSION
# error UAVCAN_CPP_VERSION
#endif
#if UAVCAN_CPP_VERSION < UAVCAN_CPP11
# include <math.h>     // Needed for isfinite()
#endif

namespace uavcan
{

template <unsigned int BitLen>
struct NativeFloatSelector
{
    struct ErrorNoSuchFloat;
    typedef typename Select<(sizeof(float) * 8 >= BitLen), float,
            typename Select<(sizeof(double) * 8 >= BitLen), double,
            typename Select<(sizeof(long double) * 8 >= BitLen), long double,
                              ErrorNoSuchFloat>::Result>::Result>::Result Type;
};


class IEEE754Converter
{
    // TODO: Non-IEEE float support for float32 and float64
    static uint16_t nativeNonIeeeToHalf(float value);
    static float halfToNativeNonIeee(uint16_t value);

    IEEE754Converter();

public:
    /// UAVCAN requires rounding to nearest for all float conversions
    static std::float_round_style roundstyle() { return std::round_to_nearest; }

    template <unsigned int BitLen>
    static typename IntegerSpec<BitLen, SignednessUnsigned, CastModeTruncate>::StorageType
    toIeee(typename NativeFloatSelector<BitLen>::Type value)
    {
        typedef typename IntegerSpec<BitLen, SignednessUnsigned, CastModeTruncate>::StorageType IntType;
        typedef typename NativeFloatSelector<BitLen>::Type FloatType;
        StaticAssert<sizeof(FloatType) * 8 == BitLen && std::numeric_limits<FloatType>::is_iec559>::check();
        union { IntType i; FloatType f; } u;
        u.f = value;
        return u.i;
    }

    template <unsigned int BitLen>
    static typename NativeFloatSelector<BitLen>::Type
    toNative(typename IntegerSpec<BitLen, SignednessUnsigned, CastModeTruncate>::StorageType value)
    {
        typedef typename IntegerSpec<BitLen, SignednessUnsigned, CastModeTruncate>::StorageType IntType;
        typedef typename NativeFloatSelector<BitLen>::Type FloatType;
        StaticAssert<sizeof(FloatType) * 8 == BitLen && std::numeric_limits<FloatType>::is_iec559>::check();
        union { IntType i; FloatType f; } u;
        u.i = value;
        return u.f;
    }
};
template <>
inline typename IntegerSpec<16, SignednessUnsigned, CastModeTruncate>::StorageType
IEEE754Converter::toIeee<16>(typename NativeFloatSelector<16>::Type value)
{
    return nativeNonIeeeToHalf(value);
}
template <>
inline typename NativeFloatSelector<16>::Type
IEEE754Converter::toNative<16>(typename IntegerSpec<16, SignednessUnsigned, CastModeTruncate>::StorageType value)
{
    return halfToNativeNonIeee(value);
}

template <unsigned int BitLen> struct IEEE754Limits;
template <> struct IEEE754Limits<16>
{
    static typename NativeFloatSelector<16>::Type max() { return 65504.0; }
    static typename NativeFloatSelector<16>::Type epsilon() { return 9.77e-04; }
};
template <> struct IEEE754Limits<32>
{
    static typename NativeFloatSelector<32>::Type max() { return 3.40282346638528859812e+38; }
    static typename NativeFloatSelector<32>::Type epsilon() { return 1.19209289550781250000e-7; }
};
template <> struct IEEE754Limits<64>
{
    static typename NativeFloatSelector<64>::Type max() { return 1.79769313486231570815e+308L; }
    static typename NativeFloatSelector<64>::Type epsilon() { return 2.22044604925031308085e-16L; }
};


template <unsigned int BitLen_, CastMode CastMode>
class FloatSpec : public IEEE754Limits<BitLen_>
{
    FloatSpec();

public:
    enum { BitLen = BitLen_ };
    enum { MinBitLen = BitLen };
    enum { MaxBitLen = BitLen };
    enum { IsPrimitive = 1 };

    typedef typename NativeFloatSelector<BitLen>::Type StorageType;

    enum { IsExactRepresentation = (sizeof(StorageType) * 8 == BitLen) && std::numeric_limits<StorageType>::is_iec559 };

    using IEEE754Limits<BitLen>::max;
    using IEEE754Limits<BitLen>::epsilon;
    static std::float_round_style roundstyle() { return IEEE754Converter::roundstyle(); }

    static int encode(StorageType value, ScalarCodec& codec, TailArrayOptimizationMode)
    {
        // cppcheck-suppress duplicateExpression
        if (CastMode == CastModeSaturate)
            saturate(value);
        else
            truncate(value);
        return codec.encode<BitLen>(IEEE754Converter::toIeee<BitLen>(value));
    }

    static int decode(StorageType& out_value, ScalarCodec& codec, TailArrayOptimizationMode)
    {
        typename IntegerSpec<BitLen, SignednessUnsigned, CastModeTruncate>::StorageType ieee = 0;
        const int res = codec.decode<BitLen>(ieee);
        if (res <= 0)
            return res;
        out_value = IEEE754Converter::toNative<BitLen>(ieee);
        return res;
    }

    static void extendDataTypeSignature(DataTypeSignature&) { }

private:
    static inline void saturate(StorageType& value)
    {
        using namespace std;
        if (!IsExactRepresentation && isfinite(value))
        {
            if (value > max())
                value = max();
            else if (value < -max())
                value = -max();
        }
    }

    static inline void truncate(StorageType& value)
    {
        using namespace std;
        if (!IsExactRepresentation && isfinite(value))
        {
            if (value > max())
                value = std::numeric_limits<StorageType>::infinity();
            else if (value < -max())
                value = -std::numeric_limits<StorageType>::infinity();
        }
    }
};


template <unsigned int BitLen, CastMode CastMode>
struct YamlStreamer<FloatSpec<BitLen, CastMode> >
{
    typedef typename FloatSpec<BitLen, CastMode>::StorageType StorageType;

    template <typename Stream>
    static void stream(Stream& s, const StorageType value, int)
    {
        s << value;
    }
};

}
