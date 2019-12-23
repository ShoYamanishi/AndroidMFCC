package com.example.android_mfcc;

import static java.lang.Math.PI;
import static java.lang.Math.cos;
import static java.lang.Math.sin;


/** @brief performs 512-point radix-2 complex FFT.
 *
 *  x(n) : input frame 0 <= n < N = 512
 *  X(k) : FFT points  0 <= k < N = 512
 *
 *  let
 *     f_1(n) = x(2n)
 *     f_2(n) = x(2n+1)   n = 0,1, ... N/2 - 1
 *
 *  then
 *      X(k)       = F_1(k) + W^k_N * F_2(k)  : k = 0, 1, ... N/2 - 1
 *      X(k + N/2) = F_1(k) - W^k_N * F_2(k)  : k = 0, 1, ... N/2 - 1
 *
 * where
 *      F_1(k) is N/2-point FFT of f_1(n)
 *      F_2(k) is N/2-point FFT of f_2(n)
 *
 *      W^k_N = exp ( -2πi k / N )
 *
 * @reference : "Digital signal processing" by Proakis, Manolakis 4th edition
 *               Chap 8: Efficient Computation of the DFT: Fast Fourier Transform Algorithms
 *
 * @remark : Note on complex value implementation
 *         a complex value is represented by a consecutive pair of floats.
 *         i.e. an array of complex values will look like this:
 *         float C[10];  // Five complex values.
 *         C[0] = Re0, C[1] = Im0,
 *         C[2] = Re1, C[3] = Im1,
 *         ...
 *         C[8] = Re4, C[9] = Im4
 *
 */
public class FFT512Java {

    /** @brief constructor. Creates twiddle arrays.
     */
    public FFT512Java () {
        makeTwiddles();
    }


    /** @brief main function
     *
     * @param samples : time domain complex input sample points in (re,im) pairs
     * @return : frequency domain points in (re,im) pairs
     */
    public float[] transform( float[] samples ) {

        return cooley_tukey_recursive( samples );

    }


    private void deinterleave( float[] even, float[] odd, float[] src ) {

        for (int i = 0; i < ( src.length / 4 ) ; i += 1 ) {

            even[ i * 2     ] = src[ i * 4     ]; // even re
            even[ i * 2 + 1 ] = src[ i * 4 + 1 ]; // even im

            odd [ i * 2     ] = src[ i * 4 + 2 ]; // odd re
            odd [ i * 2 + 1 ] = src[ i * 4 + 3 ]; // odd im
        }
    }


    private float[] cooley_tukey_recursive( float[] array_in ) {
        final int numPoints = array_in.length / 2;

        if ( numPoints == 1 ) {
            float[] array_out = new float[2];
            array_out[0] = array_in[0];
            array_out[1] = array_in[1];
            return array_out; // Single point
        }

        float[] array_even_in = new float[ numPoints ];
        float[] array_odd_in  = new float[ numPoints ];

        deinterleave( array_even_in, array_odd_in, array_in );

        float [] array_even_out = cooley_tukey_recursive( array_even_in );
        float [] array_odd_out  = cooley_tukey_recursive( array_odd_in  );

        float [] array_out = new float[numPoints * 2];

        System.arraycopy(array_even_out, 0, array_out, 0,         numPoints     );
        System.arraycopy(array_odd_out,  0, array_out, numPoints, numPoints     );

        // Butterfly

        float[] twiddle = getTwiddleArray( numPoints );

        final int halfNumPoints = numPoints / 2;

        for ( int i = 0; i < halfNumPoints; i++) {

            final float tw_re = twiddle[ 2 * i     ];
            final float tw_im = twiddle[ 2 * i + 1 ];

            final float v1_re = array_out[ 2 * i ];
            final float v1_im = array_out[ 2 * i + 1 ];
            final float v2_re = array_out[ 2 * (halfNumPoints + i) ];
            final float v2_im = array_out[ 2 * (halfNumPoints + i) + 1 ];

            // tw * v2 = (tw_re, tw_im i) * (v2_re, v2_im i)
            //         = (tw_re * v2_re - tw_im * v2_im), (tw_re * v2_im + tw_im * v2_re ) i

            final float offset_re = tw_re * v2_re - tw_im * v2_im;
            final float offset_im = tw_re * v2_im + tw_im * v2_re;

            // X(k)       = F_1(k) + W^k_N * F_2(k)  : k = 0, 1, ... N/2 - 1
            // X(k + N/2) = F_1(k) - W^k_N * F_2(k)  : k = 0, 1, ... N/2 - 1

            array_out[ 2 * i                         ] = v1_re + offset_re;
            array_out[ 2 * i + 1                     ] = v1_im + offset_im;
            array_out[ 2 * ( i + halfNumPoints )     ] = v1_re - offset_re;
            array_out[ 2 * ( i + halfNumPoints ) + 1 ] = v1_im - offset_im;
        }

        return array_out;
    }


    private float[] getTwiddleArray( final int numPoints ) {

        if      ( numPoints == 512 ) {
            return mTwiddle512;
        }
        else if ( numPoints == 256 ) {
            return mTwiddle256;
        }
        else if ( numPoints == 128 ) {
            return mTwiddle128;
        }
        else if ( numPoints == 64 ) {
            return mTwiddle64;
        }
        else if ( numPoints == 32 ) {
            return mTwiddle32;
        }
        else if ( numPoints == 16 ) {
            return mTwiddle16;
        }
        else if ( numPoints == 8 ) {
            return mTwiddle8;
        }
        else if ( numPoints == 4 ) {
            return mTwiddle4;
        }
        else if ( numPoints == 2 ) {
            return mTwiddle2;
        }
        else if ( numPoints == 1 ) {
            return mTwiddle1;
        }

        return null;
    }


    private void makeTwiddles() {

        mTwiddle512 = makeTwiddle( 512 );
        mTwiddle256 = makeTwiddle( 256 );
        mTwiddle128 = makeTwiddle( 128 );
        mTwiddle64  = makeTwiddle(  64 );
        mTwiddle32  = makeTwiddle(  32 );
        mTwiddle16  = makeTwiddle(  16 );
        mTwiddle8   = makeTwiddle(   8 );
        mTwiddle4   = makeTwiddle(   4 );
        mTwiddle2   = makeTwiddle(   2 );
        mTwiddle1   = makeTwiddle(   1 );

    }


    private float[] makeTwiddle( final int N ) {

        float[] tw = new float[ N ];

        final float dN  = (float)N;

        for ( int k = 0; k < N / 2 ; k++ ) {

            float theta = -2.0f * (float)PI * (float)k / dN;
            tw[2*k]   = (float)cos(theta); // re
            tw[2*k+1] = (float)sin(theta); // im

        }
        return tw;
    }


    // complex values in consecutive pairs for better locality
    // The suffix number indicates n (sample points) but the allocation
    // of complext points for it is n/2, i.e. float[n]
    private float[]   mTwiddle512;
    private float[]   mTwiddle256;
    private float[]   mTwiddle128;
    private float[]   mTwiddle64;
    private float[]   mTwiddle32;
    private float[]   mTwiddle16;
    private float[]   mTwiddle8;
    private float[]   mTwiddle4;
    private float[]   mTwiddle2;
    private float[]   mTwiddle1;

}
