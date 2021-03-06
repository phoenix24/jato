/*
 * Copyright (C) 2009 Pekka Enberg
 *
 * This file is released under the 2-clause BSD license. Please refer to the
 * file LICENSE for details.
 */
package jvm;

/**
 * @author Pekka Enberg
 */
public class FloatConversionTest extends TestCase {
    public static float i2f(int val) {
        return (float) val;
    }

    public static void testIntToFloatConversion() {
        assertEquals(2.0f, i2f(2));
        assertEquals(-3000f, i2f(-3000));
        assertEquals(-2147483648.0f, i2f(-2147483648));
        assertEquals(2147483647.0f, i2f(2147483647));
    }

    public static int f2i(float val) {
        return (int) val;
    }

    public static void testFloatToIntConversion() {
        assertEquals(0, f2i(Float.NaN));
        assertEquals(Integer.MIN_VALUE, f2i(Float.NEGATIVE_INFINITY));
        assertEquals(Integer.MAX_VALUE, f2i(Float.POSITIVE_INFINITY));
        assertEquals(2, f2i(2.5f));
        assertEquals(-1000, f2i(-1000.0101f));
        assertEquals(-2147483648, f2i(-2147483648.0f));
        assertEquals(2147483647, f2i(2147483647.0f));
    }

    public static float l2f(long val) {
        return (float) val;
    }

    public static void testLongToFloatConversion() {
        assertEquals(2.0f, l2f(2L));
        assertEquals(-3000f, l2f(-3000L));
        assertEquals(-2147483648.0f, l2f(-2147483648L));
        assertEquals(2147483647.0f, l2f(2147483647L));
        assertEquals(-9223372036854775808.0f, l2f(-9223372036854775808L));
        assertEquals(9223372036854775807.0f, l2f(9223372036854775807L));
    }

    public static long f2l(float val) {
        return (long) val;
    }

    public static void testFloatToLongConversion() {
        assertEquals(0, f2l(Float.NaN));
        assertEquals(Long.MIN_VALUE, f2l(Float.NEGATIVE_INFINITY));
        assertEquals(Long.MAX_VALUE, f2l(Float.POSITIVE_INFINITY));
        assertEquals(2L, f2l(2.5f));
        assertEquals(-1000L, f2l(-1000.0101f));
        assertEquals(-2147483648L, f2l(-2147483648.0f));
        assertEquals(2147483648L, f2l(2147483647.0f));
        assertEquals(-9223372036854775808L, f2l(-9223372036854775808.0f));
        assertEquals(9223372036854775807L, f2l(9223372036854775807.0f));
    }

    public static void main(String[] args) {
        testFloatToIntConversion();
        testIntToFloatConversion();
        testFloatToLongConversion();
        testLongToFloatConversion();
    }
}
