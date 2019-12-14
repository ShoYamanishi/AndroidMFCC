package com.example.android_mfcc;

import android.util.Log;

import static java.lang.Math.exp;
import static java.lang.Math.log;
import static java.lang.Math.cos;
import static java.lang.Math.PI;
import static java.lang.Math.log10;
import static java.lang.Math.sin;
import static java.lang.Math.sqrt;

public class MFCCJava {

    private static final String TAG = MFCCJava.class.getSimpleName();

    final double cSampleRate        = 16000.0;
    final int    cFrameSizeSamples  = 400;     // 25[ms] @ 16KHz
    final int    cFrameShiftSamples = 160;     // 10[ms] @ 16KHz
    final int    cNumPointsFFT      = 512;
    final double cPreemphTap0       = 0.96;
    final double cFilterBankMaxFreq = 8000.0;
    final double cFilterBankMinFreq = 300.0;
    final int    cNumFilterBanks    = 26;

    /** @brief constructor
     */
    public MFCCJava() {

        mHammingWindow   = new HammingWindowJava( cFrameSizeSamples, cNumPointsFFT );
        mFFT512          = new FFT512Java();
        mMelFilterBanks  = new MelFilterBanksJava();
        mDCT             = new DCTJava( cNumFilterBanks );

    }

    /** @brief generates spectral density in 256 points.
     *
     * @param samples_real400 : time domain 400 real samples.
     * @return energy density at 256 points after FFT.
     */
    public double[] spectralDensity( double[] samples_real400 ) {

        // 2. Pre-Emphasis & Hamming window oer 400 samples.
        double[] windowed_complex512 = mHammingWindow.preEmphasisHammingAndMakeComplexForFFT( samples_real400, cPreemphTap0 );

        // 3. 512 point FFT.
        double[] fft_complex512 = mFFT512.transform(windowed_complex512);
        double[] power_real256    = new double[256];
        for (int i = 0; i < 256 ; i++) {

            double re = fft_complex512[ 2*i     ];
            double im = fft_complex512[ 2*i + 1 ];

            //float micDB = (float)(20.0 * Math.log10( ( (float)mRMS ) / AmplitudeRef ));

            //power_real256[i] = Math.max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;
            //Log.i(TAG, "power: " + String.valueOf(power_real256[i]));
        }
        return power_real256;
    }

    /** @brief
     *
     * @param samples_real400 : time domain 400 real samples
     * @return real 27 MFCCs
     */
    public double[] generateMFCC( double[] samples_real400 ) {

        // 2. Pre-Emphasis & Hamming window oer 400 samples.
        double[] windowed_complex512 = mHammingWindow.preEmphasisHammingAndMakeComplexForFFT( samples_real400, cPreemphTap0 );

        // 3. 512 point FFT.
        double[] fft_complex512 = mFFT512.transform(windowed_complex512);

        // 4. Log Mel coefficients
        double[] mel_coeffs_real26 = mMelFilterBanks.findLogMelCoeffs( fft_complex512 );

        // 5. DCT
        double[] mfcc_real27 = mDCT.transform(mel_coeffs_real26);


        String output = "MFCC: ";
        for ( int i = 0; i < mfcc_real27.length; i++ ) {
            mfcc_real27[i] =  mfcc_real27[i] / 10.0 + 0.5 ;
            output += String.valueOf(mfcc_real27[i]);
            output += " ";
        }
        Log.i(TAG, output);
        return mfcc_real27;
    }


    /** @brief
     *
     * @param samples_real400 : time domain 400 real samples
     * @return real 27 MFCCs and 256-point real power spectrum.
     */
    public double[] generateMFCCAndPowerSpectrum( double[] samples_real400 ) {

        // 2. Pre-Emphasis & Hamming window oer 400 samples.
        double[] windowed_complex512 = mHammingWindow.preEmphasisHammingAndMakeComplexForFFT( samples_real400, cPreemphTap0 );

        // 3. 512 point FFT.
        double[] fft_complex512 = mFFT512.transform(windowed_complex512);

        // 4. Log Mel coefficients
        double[] mel_coeffs_real26 = mMelFilterBanks.findLogMelCoeffs( fft_complex512 );

        // 5. DCT
        double[] mfcc_real27 = mDCT.transform(mel_coeffs_real26);

        double[] mfcc_27_power_real256    = new double[ 27 + 256 ];

        for ( int i = 0; i < mfcc_real27.length; i++ ) {
            mfcc_27_power_real256[i] = mfcc_real27[i] / 5.0 + 0.5;

        }

        for (int i = 0; i < 256 ; i++) {

            double re = fft_complex512[ 2*i     ];
            double im = fft_complex512[ 2*i + 1 ];

            mfcc_27_power_real256[ i+27 ] = Math.max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;
        }

        return mfcc_27_power_real256;
    }


    HammingWindowJava    mHammingWindow;
    FFT512Java           mFFT512;
    MelFilterBanksJava   mMelFilterBanks;
    DCTJava              mDCT;

}
