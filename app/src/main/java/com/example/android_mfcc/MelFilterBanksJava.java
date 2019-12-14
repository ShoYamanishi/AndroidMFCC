package com.example.android_mfcc;

import android.util.Log;

import static java.lang.Math.exp;
import static java.lang.Math.log;

public class MelFilterBanksJava {


    private static final String TAG = MelFilterBanksJava.class.getSimpleName();

    /** @brief constructor.
     *
     */
    public MelFilterBanksJava () {

        constructSampleToBin();

        //for ( int i = 0 ; i < mSampleToBin.length ; i++) {
        //    sampleToBin bin = mSampleToBin[i];
        //    Log.i(TAG, "Bin[" + String.valueOf(i) + "]:" +
        //    String.valueOf(bin.bin1()) + " " +
        //    String.valueOf(bin.bin2()) + " " +
        //    String.valueOf(bin.coeff1()) + " " +
        //    String.valueOf(bin.coeff2()) );
        //}
    }

    /** @brief find log Mel filter bank coefficients
     *
     * @param fftPoints : complex 256 points in (re,im) pairs from 512 point FFT.
     * @return Mel filter bank coefficients in real numbers.
     */
    public double[] findLogMelCoeffs(double[] fftPoints) {

        double[] array_out = new double[cNumFilterBanks];

        for ( int i = 0; i < cNumFilterBanks; i++) {
            array_out[i] = 0.0;
        }

        for (int i = 0; i < cNumSamples; i++) {

            sampleToBin stb = mSampleToBin[i];

            final double re  = fftPoints[ 2*i   ];
            final double im  = fftPoints[ 2*i+1 ];
            final double pwr = re*re + im*im;

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
            array_out[i] = log( array_out[i] ) ;
        }
        return array_out;
    }

    class sampleToBin {

        public sampleToBin( int bin1, int bin2, double coeff1 ) {
            mBin1   = bin1;
            mBin2   = bin2;
            mCoeff1 = coeff1;
            mCoeff2 = 1.0 - coeff1;
        }
        int    bin1()   { return mBin1;   }
        int    bin2()   { return mBin2;   }
        double coeff1() { return mCoeff1; }
        double coeff2() { return mCoeff2; }

        private int    mBin1   = -1;
        private int    mBin2   = -1;  // -1 if blank
        private double mCoeff1 = 0.0;
        private double mCoeff2 = 0.0;

    };


    private void constructSampleToBin () {

        double filterBankMaxMel = freqToMel( cFilterBankMaxFreq );
        double filterBankMinMel = freqToMel( cFilterBankMinFreq );
        double intervalMel      =   (  filterBankMaxMel -  filterBankMinMel )
                / ( (float)cNumFilterBanks + 1.0 );

        int[]    melBoundariesSamplePoint = new int    [ cNumFilterBanks + 2 ];
        double[] melBoundariesMelFreq     = new double [ cNumFilterBanks + 2 ];

        for ( int i = 0; i <= cNumFilterBanks + 1 ; i++ ) {

            final double m = filterBankMinMel + (double)i * intervalMel;
            final double f = melToFreq( m );
            final int    s = freqToSampleNum( f ) ;

            melBoundariesSamplePoint[i] = s;
            melBoundariesMelFreq    [i] = m;
            Log.i(TAG, "Sample point: " + String.valueOf(s) + " Freq: " + String.valueOf(f) + " Mel: " + String.valueOf(m) );

        }

        for ( int i = 0; i < cNumSamples ; i++ ) {

            final double f = sampleNumToFreq( i );
            final double m = freqToMel( f );

            if (   ( m < melBoundariesMelFreq[ 0 ]                )
                    || ( melBoundariesMelFreq[ cNumFilterBanks ]  < m ) ) {
                Log.i(TAG, "ckp1");
                mSampleToBin[i] = new sampleToBin( -1, -1, 0.0 );
            }

            else if (    ( melBoundariesMelFreq[ 0 ] <= m  )
                    && ( m < melBoundariesMelFreq[ 1 ]   )  ) {
                Log.i(TAG, "ckp2");
                mSampleToBin[i] = new sampleToBin( 0, -1, ( m - melBoundariesMelFreq[0] ) / intervalMel );
            }

            else if (    ( melBoundariesMelFreq[ cNumFilterBanks  - 1 ] <= m  )
                    && ( m < melBoundariesMelFreq[ cNumFilterBanks  ]       )  ) {
                Log.i(TAG, "ckp3");
                mSampleToBin[i] = new sampleToBin( cNumFilterBanks  - 1 , -1, ( melBoundariesMelFreq[cNumFilterBanks ] - m ) / intervalMel );
            }

            else {
                for ( int j = 1; j < cNumFilterBanks ; j++ ) {
                    if (    ( melBoundariesMelFreq[ j ] <= m    )
                            && ( m < melBoundariesMelFreq[ j + 1 ] ) ) {
                        Log.i(TAG, "ckp4");
                        mSampleToBin[i] = new sampleToBin( j , j + 1, ( melBoundariesMelFreq[j+1] - m ) / intervalMel );
                        break;
                    }
                }
            }
        }
    }

    private double sampleNumToFreq( final int i ) {
        return ( (cHalfSampleRate / cNumSamplesD)  * (double) i );
    }

    private int freqToSampleNum( final double f ) {
        return (int)( ( cNumSamplesD / cHalfSampleRate ) * f ) ;
    }

    private double freqToMel ( final double f ) {
        return 1125.0 * log( 1.0 + f / 700.0);
    }

    private double melToFreq ( final double m ) {
        return  700.0 * ( exp( m / 1125.0) - 1.0 );
    }


    private final int    cNumFilterBanks    =    26;
    private final int    cNumSamples        =   256;
    private final double cNumSamplesD       =   256.0;
    private final double cSampleRate        = 16000.0;
    private final double cHalfSampleRate    =  8000.0;
    private final double cFilterBankMaxFreq =  8000.0;
    private final double cFilterBankMinFreq =   300.0;
    private final double cMelFloor          =     1.0;

    private sampleToBin[] mSampleToBin = new sampleToBin[ cNumSamples ];

}
