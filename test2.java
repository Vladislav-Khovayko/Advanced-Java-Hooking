/*
 * Copyright (C) 2025 Vladislav Khovayko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// javac test2.java
// convert to C byte array, and keep a copy of it in get_offsets.h
//
// currently has 10 wrappers for each return type, 5 are static, 5 arent, this should mean that up to 5 threads may enter any return type at once

class myDefinedClass{
    public static void dummy_func(){
        return;
    }

    public static native void nativeBridgeVoid(); //tested
    public static native boolean nativeBridgeBool(); //tested
    public static native byte nativeBridgeByte();
    public static native char nativeBridgeChar();
    public static native short nativeBridgeShort();
    public static native int nativeBridgeInt();
    public static native long nativeBridgeLong();
    public static native float nativeBridgeFloat();
    public static native double nativeBridgeDouble();
    public static native Object nativeBridgeObject();

//void:
    //not static void
    public void nativeWrapperVoid1(int a, int b, float c, char d){ //IIFC
        nativeBridgeVoid();
    }
    public void nativeWrapperVoid2(int a, int b, char c, float d){//IICF
        nativeBridgeVoid();
    }
    public void nativeWrapperVoid3(int a, float b, int c, char d){//IFIC
        nativeBridgeVoid();
    }
    public void nativeWrapperVoid4(int a, float b, char c, int d){//IFCI
        nativeBridgeVoid();
    }
    public void nativeWrapperVoid5(int a, char b, int c, float d){//ICIF
        nativeBridgeVoid();
    }
    //static void
    public static void nativeWrapperVoid1S(int a, char b, float c, int d){//ICFI
        nativeBridgeVoid();
    }
    public static void nativeWrapperVoid2S(float a, int b, int c, char d){//FIIC
        nativeBridgeVoid();
    }
    public static void nativeWrapperVoid3S(float a, int b, char c, int d){//FICI
        nativeBridgeVoid();
    }
    public static void nativeWrapperVoid4S(float a, char b, int c, int d){//FCII
        nativeBridgeVoid();
    }
    public static void nativeWrapperVoid5S(char a, int b, int c, float d){//CIIF
        nativeBridgeVoid();
    }


//boolean
    //not static boolean
    public boolean nativeWrapperBool1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeBool();
    }
    public boolean nativeWrapperBool2(int a, int b, char c, float d){//IICF
        return nativeBridgeBool();
    }
    public boolean nativeWrapperBool3(int a, float b, int c, char d){//IFIC
        return nativeBridgeBool();
    }
    public boolean nativeWrapperBool4(int a, float b, char c, int d){//IFCI
        return nativeBridgeBool();
    }
    public boolean nativeWrapperBool5(int a, char b, int c, float d){//ICIF
        return nativeBridgeBool();
    }
    //static boolean
    public static boolean nativeWrapperBool1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeBool();
    }
    public static boolean nativeWrapperBool2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeBool();
    }
    public static boolean nativeWrapperBool3S(float a, int b, char c, int d){//FICI
        return nativeBridgeBool();
    }
    public static boolean nativeWrapperBool4S(float a, char b, int c, int d){//FCII
        return nativeBridgeBool();
    }
    public static boolean nativeWrapperBool5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeBool();
    }

//byte
    //not static byte
    public byte nativeWrapperByte1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeByte();
    }
    public byte nativeWrapperByte2(int a, int b, char c, float d){//IICF
        return nativeBridgeByte();
    }
    public byte nativeWrapperByte3(int a, float b, int c, char d){//IFIC
        return nativeBridgeByte();
    }
    public byte nativeWrapperByte4(int a, float b, char c, int d){//IFCI
        return nativeBridgeByte();
    }
    public byte nativeWrapperByte5(int a, char b, int c, float d){//ICIF
        return nativeBridgeByte();
    }
    //static byte
    public static byte nativeWrapperByte1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeByte();
    }
    public static byte nativeWrapperByte2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeByte();
    }
    public static byte nativeWrapperByte3S(float a, int b, char c, int d){//FICI
        return nativeBridgeByte();
    }
    public static byte nativeWrapperByte4S(float a, char b, int c, int d){//FCII
        return nativeBridgeByte();
    }
    public static byte nativeWrapperByte5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeByte();
    }

//char
    //not static char
    public char nativeWrapperChar1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeChar();
    }
    public char nativeWrapperChar2(int a, int b, char c, float d){//IICF
        return nativeBridgeChar();
    }
    public char nativeWrapperChar3(int a, float b, int c, char d){//IFIC
        return nativeBridgeChar();
    }
    public char nativeWrapperChar4(int a, float b, char c, int d){//IFCI
        return nativeBridgeChar();
    }
    public char nativeWrapperChar5(int a, char b, int c, float d){//ICIF
        return nativeBridgeChar();
    }
    //static char
    public static char nativeWrapperChar1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeChar();
    }
    public static char nativeWrapperChar2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeChar();
    }
    public static char nativeWrapperChar3S(float a, int b, char c, int d){//FICI
        return nativeBridgeChar();
    }
    public static char nativeWrapperChar4S(float a, char b, int c, int d){//FCII
        return nativeBridgeChar();
    }
    public static char nativeWrapperChar5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeChar();
    }

//short
    //not static short
    public short nativeWrapperShort1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeShort();
    }
    public short nativeWrapperShort2(int a, int b, char c, float d){//IICF
        return nativeBridgeShort();
    }
    public short nativeWrapperShort3(int a, float b, int c, char d){//IFIC
        return nativeBridgeShort();
    }
    public short nativeWrapperShort4(int a, float b, char c, int d){//IFCI
        return nativeBridgeShort();
    }
    public short nativeWrapperShort5(int a, char b, int c, float d){//ICIF
        return nativeBridgeShort();
    }
    //static short
    public static short nativeWrapperShort1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeShort();
    }
    public static short nativeWrapperShort2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeShort();
    }
    public static short nativeWrapperShort3S(float a, int b, char c, int d){//FICI
        return nativeBridgeShort();
    }
    public static short nativeWrapperShort4S(float a, char b, int c, int d){//FCII
        return nativeBridgeShort();
    }
    public static short nativeWrapperShort5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeShort();
    }

//int
    //not static int
    public int nativeWrapperInt1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeInt();
    }
    public int nativeWrapperInt2(int a, int b, char c, float d){//IICF
        return nativeBridgeInt();
    }
    public int nativeWrapperInt3(int a, float b, int c, char d){//IFIC
        return nativeBridgeInt();
    }
    public int nativeWrapperInt4(int a, float b, char c, int d){//IFCI
        return nativeBridgeInt();
    }
    public int nativeWrapperInt5(int a, char b, int c, float d){//ICIF
        return nativeBridgeInt();
    }
    //static int
    public static int nativeWrapperInt1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeInt();
    }
    public static int nativeWrapperInt2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeInt();
    }
    public static int nativeWrapperInt3S(float a, int b, char c, int d){//FICI
        return nativeBridgeInt();
    }
    public static int nativeWrapperInt4S(float a, char b, int c, int d){//FCII
        return nativeBridgeInt();
    }
    public static int nativeWrapperInt5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeInt();
    }

//long
    //not static long
    public long nativeWrapperLong1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeLong();
    }
    public long nativeWrapperLong2(int a, int b, char c, float d){//IICF
        return nativeBridgeLong();
    }
    public long nativeWrapperLong3(int a, float b, int c, char d){//IFIC
        return nativeBridgeLong();
    }
    public long nativeWrapperLong4(int a, float b, char c, int d){//IFCI
        return nativeBridgeLong();
    }
    public long nativeWrapperLong5(int a, char b, int c, float d){//ICIF
        return nativeBridgeLong();
    }
    //static long
    public static long nativeWrapperLong1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeLong();
    }
    public static long nativeWrapperLong2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeLong();
    }
    public static long nativeWrapperLong3S(float a, int b, char c, int d){//FICI
        return nativeBridgeLong();
    }
    public static long nativeWrapperLong4S(float a, char b, int c, int d){//FCII
        return nativeBridgeLong();
    }
    public static long nativeWrapperLong5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeLong();
    }

//float
    //not static float
    public float nativeWrapperFloat1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeFloat();
    }
    public float nativeWrapperFloat2(int a, int b, char c, float d){//IICF
        return nativeBridgeFloat();
    }
    public float nativeWrapperFloat3(int a, float b, int c, char d){//IFIC
        return nativeBridgeFloat();
    }
    public float nativeWrapperFloat4(int a, float b, char c, int d){//IFCI
        return nativeBridgeFloat();
    }
    public float nativeWrapperFloat5(int a, char b, int c, float d){//ICIF
        return nativeBridgeFloat();
    }
    //static float
    public static float nativeWrapperFloat1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeFloat();
    }
    public static float nativeWrapperFloat2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeFloat();
    }
    public static float nativeWrapperFloat3S(float a, int b, char c, int d){//FICI
        return nativeBridgeFloat();
    }
    public static float nativeWrapperFloat4S(float a, char b, int c, int d){//FCII
        return nativeBridgeFloat();
    }
    public static float nativeWrapperFloat5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeFloat();
    }

//double
    //not static double
    public double nativeWrapperDouble1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeDouble();
    }
    public double nativeWrapperDouble2(int a, int b, char c, float d){//IICF
        return nativeBridgeDouble();
    }
    public double nativeWrapperDouble3(int a, float b, int c, char d){//IFIC
        return nativeBridgeDouble();
    }
    public double nativeWrapperDouble4(int a, float b, char c, int d){//IFCI
        return nativeBridgeDouble();
    }
    public double nativeWrapperDouble5(int a, char b, int c, float d){//ICIF
        return nativeBridgeDouble();
    }
    //static double
    public static double nativeWrapperDouble1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeDouble();
    }
    public static double nativeWrapperDouble2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeDouble();
    }
    public static double nativeWrapperDouble3S(float a, int b, char c, int d){//FICI
        return nativeBridgeDouble();
    }
    public static double nativeWrapperDouble4S(float a, char b, int c, int d){//FCII
        return nativeBridgeDouble();
    }
    public static double nativeWrapperDouble5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeDouble();
    }

//Object
    //not static Object
    public Object nativeWrapperObject1(int a, int b, float c, char d){ //IIFC
        return nativeBridgeObject();
    }
    public Object nativeWrapperObject2(int a, int b, char c, float d){//IICF
        return nativeBridgeObject();
    }
    public Object nativeWrapperObject3(int a, float b, int c, char d){//IFIC
        return nativeBridgeObject();
    }
    public Object nativeWrapperObject4(int a, float b, char c, int d){//IFCI
        return nativeBridgeObject();
    }
    public Object nativeWrapperObject5(int a, char b, int c, float d){//ICIF
        return nativeBridgeObject();
    }
    //static Object
    public static Object nativeWrapperObject1S(int a, char b, float c, int d){//ICFI
        return nativeBridgeObject();
    }
    public static Object nativeWrapperObject2S(float a, int b, int c, char d){//FIIC
        return nativeBridgeObject();
    }
    public static Object nativeWrapperObject3S(float a, int b, char c, int d){//FICI
        return nativeBridgeObject();
    }
    public static Object nativeWrapperObject4S(float a, char b, int c, int d){//FCII
        return nativeBridgeObject();
    }
    public static Object nativeWrapperObject5S(char a, int b, int c, float d){//CIIF
        return nativeBridgeObject();
    }



} //end of class