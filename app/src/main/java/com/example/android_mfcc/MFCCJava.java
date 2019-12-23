package com.example.android_mfcc;

import android.util.Log;

import static java.lang.Math.exp;
import static java.lang.Math.log;
import static java.lang.Math.cos;
import static java.lang.Math.PI;
import static java.lang.Math.log10;
import static java.lang.Math.sin;
import static java.lang.Math.sqrt;

public class MFCCJava implements MFCCInterface {

    private static final String TAG = MFCCJava.class.getSimpleName();

    final float  cSampleRate        = 16000.0f;
    final int    cFrameSizeSamples  = 400;     // 25[ms] @ 16KHz
    final int    cFrameShiftSamples = 160;     // 10[ms] @ 16KHz
    final int    cNumPointsFFT      = 512;
    final float  cPreemphTap0       = 0.96f;
    final float  cFilterBankMaxFreq = 8000.0f;
    final float  cFilterBankMinFreq = 300.0f;
    final int    cNumFilterBanks    = 26;

    static int log_counter = -1;

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
     *  @param samples_real400 : time domain 400 real samples.
     *  @return energy density at 256 points after FFT.
     */
    public float[] spectralDensity( float[] samples_real400 ) {

        // 2. Pre-Emphasis & Hamming window oer 400 samples.
        float[] windowed_complex512 = mHammingWindow.preEmphasisHammingAndMakeComplexForFFT( samples_real400, cPreemphTap0 );

        // 3. 512 point FFT.
        float[] fft_complex512 = mFFT512.transform(windowed_complex512);
        float[] power_real256    = new float[256];
        for (int i = 0; i < 256 ; i++) {

            float re = fft_complex512[ 2*i     ];
            float im = fft_complex512[ 2*i + 1 ];
            power_real256[i] = (float)Math.max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;

        }
        return power_real256;
    }

    /** @brief
     *
     * @param samples_real400 : time domain 400 real samples
     * @return real 27 MFCCs
     */
    public float[] generateMFCC( int exec_type, float[] samples_real400 ) {

        // 2. Pre-Emphasis & Hamming window oer 400 samples.
        float[] windowed_complex512 = mHammingWindow.preEmphasisHammingAndMakeComplexForFFT( samples_real400, cPreemphTap0 );

        // 3. 512 point FFT.
        float[] fft_complex512 = mFFT512.transform(windowed_complex512);

        // 4. Log Mel coefficients
        float[] mel_coeffs_real26 = mMelFilterBanks.findLogMelCoeffs( fft_complex512 );

        // 5. DCT
        float[] mfcc_real27 = mDCT.transform(mel_coeffs_real26);

        return mfcc_real27;
    }


    /** @brief
     * @param exec_type       : not used.
     * @param samples_real400 : time domain 400 real samples
     * @return real 27 MFCCs and 256-point real power spectrum.
     */
    public float[] generateMFCCAndPowerSpectrum( int exec_type, float[] samples_real400 ) {

        log_counter++;

        // 2. Pre-Emphasis & Hamming window oer 400 samples.
        float[] windowed_complex512 = mHammingWindow.preEmphasisHammingAndMakeComplexForFFT( samples_real400, cPreemphTap0 );

        // 3. 512 point FFT.
        float[] fft_complex512 = mFFT512.transform(windowed_complex512);

        // 4. Log Mel coefficients
        float[] mel_coeffs_real26 = mMelFilterBanks.findLogMelCoeffs( fft_complex512 );

        // 5. DCT
        float[] mfcc_real27 = mDCT.transform(mel_coeffs_real26);

        float[] mfcc_27_power_real256    = new float[ 27 + 256 ];

        for (int i = 0; i < 256 ; i++) {

            float re = fft_complex512[ 2*i     ];
            float im = fft_complex512[ 2*i + 1 ];

            mfcc_27_power_real256[ i+27 ] = (float)Math.max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;
        }

        return mfcc_27_power_real256;

    }

    HammingWindowJava    mHammingWindow;
    FFT512Java           mFFT512;
    MelFilterBanksJava   mMelFilterBanks;
    DCTJava              mDCT;



}
