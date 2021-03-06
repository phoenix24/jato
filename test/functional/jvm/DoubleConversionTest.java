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
public class DoubleConversionTest extends TestCase {
    public static double i2d(int val) {
        return (double) val;
    }

    public static void testIntToDoubleConversion() {
        assertEquals(2.0, i2d(2));
        assertEquals(-3000, i2d(-3000));
        assertEquals(-2147483648.0, i2d(-2147483648));
        assertEquals(2147483647.0, i2d(2147483647));
    }

    public static int d2i(double val) {
        return (int) val;
    }

    public static void testDoubleToIntConversion() {
        assertEquals(0, d2i(Double.NaN));
        assertEquals(Integer.MIN_VALUE, d2i(Double.NEGATIVE_INFINITY));
        assertEquals(Integer.MAX_VALUE, d2i(Double.POSITIVE_INFINITY));
        assertEquals(2, d2i(2.5f));
        assertEquals(-1000, d2i(-1000.0101));
        assertEquals(-2147483648, d2i(-2147483648.0));
        assertEquals(2147483647, d2i(2147483647.0));
    }

    public static double l2d(long val) {
        return (double) val;
    }

    public static void testLongToDoubleConversion() {
        assertEquals(2.0, l2d(2L));
        assertEquals(-3000, l2d(-3000L));
        assertEquals(-2147483648.0, l2d(-2147483648L));
        assertEquals(2147483647.0, l2d(2147483647L));
        assertEquals(-9223372036854775808.0, l2d(-9223372036854775808L));
        assertEquals(9223372036854775807.0, l2d(9223372036854775807L));
    }

    public static long d2l(double val) {
        return (long) val;
    }

    public static void testDoubleToLongConversion() {
        assertEquals(0, d2l(Double.NaN));
        assertEquals(Long.MIN_VALUE, d2l(Double.NEGATIVE_INFINITY));
        assertEquals(Long.MAX_VALUE, d2l(Double.POSITIVE_INFINITY));
        assertEquals(2L, d2l(2.5f));
        assertEquals(-1000L, d2l(-1000.0101));
        assertEquals(-2147483648L, d2l(-2147483648.0));
        assertEquals(2147483647L, d2l(2147483647.0));
        assertEquals(-9223372036854775808L, d2l(-9223372036854775808.0));
        assertEquals(9223372036854775807L, d2l(9223372036854775807.0));
    }

    public static void main(String[] args) {
        testDoubleToIntConversion();
        testIntToDoubleConversion();
        testDoubleToLongConversion();
        testLongToDoubleConversion();
    }
}
