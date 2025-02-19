/*
    enoki/map.h -- Preprocessor meta-macro to apply a macro to list entries

    The contents of this file are rather ugly, but they provide two operations
    that are extremely useful for code generation, and which work reliably on a
    variety of different compilers (in particular, I've tested GCC with both
    -std=c++11/14/17 and -std=gnu++11/14/17, which have some macro-related
    differences, Clang, and MSVC.).

    The first meta-macro

        ENOKI_MAP(MY_MACRO, a, b, c)

    expands to

        MY_MACRO(a) MY_MACRO(b) MY_MACRO(c)

    The second

        ENOKI_MAPC(MY_MACRO, a, b, c)

    expands to

        MY_MACRO(a), MY_MACRO(b), MY_MACRO(c)

    (note the extra commans between arguments). The implementation supports a
    maximum of 32 arguments, which ought to be enough for everyone.

    The implementation is based on tricks proposed by Laurent Deniau and
    Joshua Ryan (https://stackoverflow.com/questions/6707148).

    Enoki is a C++ template library for efficient vectorization and
    differentiation of numerical kernels on modern processor architectures.

    Copyright (c) 2020 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

// Macro to compute the size of __VA_ARGS__ (including 0, works on Clang/GCC/MSVC)
#define ENOKI_EVAL(x) x
#define ENOKI_VA_SIZE_3(_, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, \
        _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, \
        _27, _28, _29, _30, _31, N, ...) N
#define ENOKI_VA_SIZE_2(...) ENOKI_EVAL(ENOKI_VA_SIZE_3(__VA_ARGS__))
#if defined(__GNUC__) && !defined(__clang__)
#  define ENOKI_VA_SIZE_1(...) _ __VA_OPT__(,) __VA_ARGS__ , \
    31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
    16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#else
#  define ENOKI_VA_SIZE_1(...) _, ##__VA_ARGS__ , \
    31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
    16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#endif
#define ENOKI_VA_SIZE(...) ENOKI_VA_SIZE_2(ENOKI_VA_SIZE_1(__VA_ARGS__))

#define ENOKI_MAPN_0(...)
#define ENOKI_MAPC_0(...)
#define ENOKI_MAPN_1(Z,a) Z(a)
#define ENOKI_MAPC_1(Z,a) Z(a)
#define ENOKI_MAPN_2(Z,a,b) Z(a)Z(b)
#define ENOKI_MAPC_2(Z,a,b) Z(a),Z(b)
#define ENOKI_MAPN_3(Z,a,b,c) Z(a)Z(b)Z(c)
#define ENOKI_MAPC_3(Z,a,b,c) Z(a),Z(b),Z(c)
#define ENOKI_MAPN_4(Z,a,b,c,d) Z(a)Z(b)Z(c)Z(d)
#define ENOKI_MAPC_4(Z,a,b,c,d) Z(a),Z(b),Z(c),Z(d)
#define ENOKI_MAPN_5(Z,a,b,c,d,e) Z(a)Z(b)Z(c)Z(d)Z(e)
#define ENOKI_MAPC_5(Z,a,b,c,d,e) Z(a),Z(b),Z(c),Z(d),Z(e)
#define ENOKI_MAPN_6(Z,a,b,c,d,e,f) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)
#define ENOKI_MAPC_6(Z,a,b,c,d,e,f) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f)
#define ENOKI_MAPN_7(Z,a,b,c,d,e,f,g) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)
#define ENOKI_MAPC_7(Z,a,b,c,d,e,f,g) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g)
#define ENOKI_MAPN_8(Z,a,b,c,d,e,f,g,h) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)
#define ENOKI_MAPC_8(Z,a,b,c,d,e,f,g,h) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h)
#define ENOKI_MAPN_9(Z,a,b,c,d,e,f,g,h,i) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)
#define ENOKI_MAPC_9(Z,a,b,c,d,e,f,g,h,i) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i)
#define ENOKI_MAPN_10(Z,a,b,c,d,e,f,g,h,i,j) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)
#define ENOKI_MAPC_10(Z,a,b,c,d,e,f,g,h,i,j) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j)
#define ENOKI_MAPN_11(Z,a,b,c,d,e,f,g,h,i,j,k) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)
#define ENOKI_MAPC_11(Z,a,b,c,d,e,f,g,h,i,j,k) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k)
#define ENOKI_MAPN_12(Z,a,b,c,d,e,f,g,h,i,j,k,l) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)
#define ENOKI_MAPC_12(Z,a,b,c,d,e,f,g,h,i,j,k,l) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l)
#define ENOKI_MAPN_13(Z,a,b,c,d,e,f,g,h,i,j,k,l,m) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)
#define ENOKI_MAPC_13(Z,a,b,c,d,e,f,g,h,i,j,k,l,m) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m)
#define ENOKI_MAPN_14(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)
#define ENOKI_MAPC_14(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n)
#define ENOKI_MAPN_15(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)
#define ENOKI_MAPC_15(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o)
#define ENOKI_MAPN_16(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)
#define ENOKI_MAPC_16(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p)
#define ENOKI_MAPN_17(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)
#define ENOKI_MAPC_17(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q)
#define ENOKI_MAPN_18(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)
#define ENOKI_MAPC_18(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r)
#define ENOKI_MAPN_19(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)
#define ENOKI_MAPC_19(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s)
#define ENOKI_MAPN_20(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)
#define ENOKI_MAPC_20(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t)
#define ENOKI_MAPN_21(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)
#define ENOKI_MAPC_21(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u)
#define ENOKI_MAPN_22(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)
#define ENOKI_MAPC_22(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v)
#define ENOKI_MAPN_23(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)
#define ENOKI_MAPC_23(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w)
#define ENOKI_MAPN_24(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)
#define ENOKI_MAPC_24(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x)
#define ENOKI_MAPN_25(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)Z(y)
#define ENOKI_MAPC_25(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x),Z(y)
#define ENOKI_MAPN_26(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)Z(y)Z(z)
#define ENOKI_MAPC_26(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x),Z(y),Z(z)
#define ENOKI_MAPN_27(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)Z(y)Z(z)Z(A)
#define ENOKI_MAPC_27(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x),Z(y),Z(z),Z(A)
#define ENOKI_MAPN_28(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)Z(y)Z(z)Z(A)Z(B)
#define ENOKI_MAPC_28(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x),Z(y),Z(z),Z(A),Z(B)
#define ENOKI_MAPN_29(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)Z(y)Z(z)Z(A)Z(B)Z(C)
#define ENOKI_MAPC_29(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x),Z(y),Z(z),Z(A),Z(B),Z(C)
#define ENOKI_MAPN_30(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C,D) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)Z(y)Z(z)Z(A)Z(B)Z(C)Z(D)
#define ENOKI_MAPC_30(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C,D) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x),Z(y),Z(z),Z(A),Z(B),Z(C),Z(D)
#define ENOKI_MAPN_31(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C,D,E) Z(a)Z(b)Z(c)Z(d)Z(e)Z(f)Z(g)Z(h)Z(i)Z(j)Z(k)Z(l)Z(m)Z(n)Z(o)Z(p)Z(q)Z(r)Z(s)Z(t)Z(u)Z(v)Z(w)Z(x)Z(y)Z(z)Z(A)Z(B)Z(C)Z(D)Z(E)
#define ENOKI_MAPC_31(Z,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C,D,E) Z(a),Z(b),Z(c),Z(d),Z(e),Z(f),Z(g),Z(h),Z(i),Z(j),Z(k),Z(l),Z(m),Z(n),Z(o),Z(p),Z(q),Z(r),Z(s),Z(t),Z(u),Z(v),Z(w),Z(x),Z(y),Z(z),Z(A),Z(B),Z(C),Z(D),Z(E)

#define ENOKI_CONCAT_(a,b) a ## b
#define ENOKI_CONCAT(a,b) ENOKI_CONCAT_(a,b)
#define ENOKI_MAP_(M, Z, ...) ENOKI_EVAL(M(Z, __VA_ARGS__))
#define ENOKI_MAP(Z, ...)  ENOKI_MAP_(ENOKI_CONCAT(ENOKI_MAPN_, ENOKI_VA_SIZE(__VA_ARGS__)), Z, __VA_ARGS__)
#define ENOKI_MAPC(Z, ...) ENOKI_MAP_(ENOKI_CONCAT(ENOKI_MAPC_, ENOKI_VA_SIZE(__VA_ARGS__)), Z, __VA_ARGS__)
