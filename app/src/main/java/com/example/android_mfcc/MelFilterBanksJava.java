package com.example.android_mfcc;

import android.util.Log;

import static java.lang.Math.exp;
import static java.lang.Math.log;

public class MelFilterBanksJava {


    private static final String TAG = MelFilterBanksJava.class.getSimpleName();

    static int log_counter = 0;

    /** @brief constructor.
     *
     */
    public MelFilterBanksJava () {

        constructSampleToBin();

    }

    /** @brief find log Mel filter bank coefficients
     *
     *  @param fftPoints : complex 256 points in (re,im) pairs from 512 point FFT.
     *  @return Mel filter bank coefficients in real numbers.
     */
    public float[] findLogMelCoeffs(float[] fftPoints) {

        float[] array_out = new float[cNumFilterBanks];

        for ( int i = 0; i < cNumFilterBanks; i++) {
            array_out[i] = 0.0f;
        }

        for (int i = 0; i < cNumSamples; i++) {

            sampleToBin stb = mSampleToBin[i];

            final float re  = fftPoints[ 2*i   ];
            final float im  = fftPoints[ 2*i+1 ];
            final float pwr = re*re + im*im;

            if ( stb.bin1() != -1)  {
                array_out[ stb.bin1() ] += ( pwr * stb.coeff1() ) ;
            }
            if ( stb.bin2() != -1)  {
                array_out[ stb.bin2() ] += ( pwr * stb.coeff2() );
            }
        }

        for ( int i = 0; i < cNumFilterBanks; i++ ) {
            if ( array_out[i] < cMelFloor ) {
                array_out[i] = cMelFloor;
            }
            array_out[i] = (float)log( (double)array_out[i] ) ;
        }

        return array_out;
    }

    class sampleToBin {

        public sampleToBin( int bin1, int bin2, float coeff1 ) {
            mBin1   = bin1;
            mBin2   = bin2;
            mCoeff1 = coeff1;
            mCoeff2 = 1.0f - coeff1;
        }
        int    bin1()   { return mBin1;   }
        int    bin2()   { return mBin2;   }
        float  coeff1() { return mCoeff1; }
        float  coeff2() { return mCoeff2; }

        private int   mBin1   = -1;
        private int   mBin2   = -1;  // -1 if blank
        private float mCoeff1 = 0.0f;
        private float mCoeff2 = 0.0f;

    };


    private void constructSampleToBin () {

        float filterBankMaxMel = freqToMel( cFilterBankMaxFreq );
        float filterBankMinMel = freqToMel( cFilterBankMinFreq );
        float intervalMel      =   (  filterBankMaxMel -  filterBankMinMel )
                / ( (float)cNumFilterBanks + 1.0f );

        int[]    melBoundariesSamplePoint = new int   [ cNumFilterBanks + 2 ];
        float[] melBoundariesMelFreq      = new float [ cNumFilterBanks + 2 ];

        for ( int i = 0; i <= cNumFilterBanks + 1 ; i++ ) {

            final float m = filterBankMinMel + (float)i * intervalMel;
            final float f = melToFreq( m );
            final int    s = freqToSampleNum( f ) ;

            melBoundariesSamplePoint[i] = s;
            melBoundariesMelFreq    [i] = m;
        }

        for ( int i = 0; i < cNumSamples ; i++ ) {

            final float f = sampleNumToFreq( i );
            final float m = freqToMel( f );

            if (   ( m < melBoundariesMelFreq[ 0 ]                )
                    || ( melBoundariesMelFreq[ cNumFilterBanks ]  < m ) ) {
                mSampleToBin[i] = new sampleToBin( -1, -1, 0.0f );
            }

            else if (    ( melBoundariesMelFreq[ 0 ] <= m  )
                    && ( m < melBoundariesMelFreq[ 1 ]   )  ) {
                mSampleToBin[i] = new sampleToBin( 0, -1, ( m - melBoundariesMelFreq[0] ) / intervalMel );
            }

            else if (    ( melBoundariesMelFreq[ cNumFilterBanks  - 1 ] <= m  )
                    && ( m < melBoundariesMelFreq[ cNumFilterBanks  ]       )  ) {
                mSampleToBin[i] = new sampleToBin( cNumFilterBanks  - 1 , -1, ( melBoundariesMelFreq[cNumFilterBanks ] - m ) / intervalMel );
            }

            else {
                for ( int j = 1; j < cNumFilterBanks ; j++ ) {
                    if (    ( melBoundariesMelFreq[ j ] <= m    )
                            && ( m < melBoundariesMelFreq[ j + 1 ] ) ) {
                        mSampleToBin[i] = new sampleToBin( j , j + 1, ( melBoundariesMelFreq[j+1] - m ) / intervalMel );
                        break;
                    }
                }
            }
        }
    }

    private float sampleNumToFreq( final int i ) {
        return ( (cHalfSampleRate / cNumSamplesD)  * (float) i );
    }

    private int freqToSampleNum( final float f ) {
        return (int)( ( cNumSamplesD / cHalfSampleRate ) * f ) ;
    }

    private float freqToMel ( final float f ) {
        return 1125.0f * (float)log( 1.0 + (double)f / 700.0);
    }

    private float melToFreq ( final float m ) {
        return  700.0f * (float)( exp( m / 1125.0) - 1.0 );
    }


    private final int    cNumFilterBanks    =    26;
    private final int    cNumSamples        =   256;
    private final float cNumSamplesD       =   256.0f;
    private final float cSampleRate        = 16000.0f;
    private final float cHalfSampleRate    =  8000.0f;
    private final float cFilterBankMaxFreq =  8000.0f;
    private final float cFilterBankMinFreq =   300.0f;
    private final float cMelFloor          =     1.0f;

    private sampleToBin[] mSampleToBin = new sampleToBin[ cNumSamples ];

}
