package com.example.android_mfcc;

import android.util.Log;

import static java.lang.Math.PI;
import static java.lang.Math.cos;

/** @brief performs pre-emphasis (2-taps) and Hamming windowing on real-valued samples.
 *
 */
public class HammingWindowJava {

    private static final String TAG = HammingWindowJava.class.getSimpleName();

    static int log_counter = 0;

    /** @brief constructor
     *
     * @param windowSizeSamples : number of samples in one input frame  (usually 400)
     * @param outputSizeSamples : number of samples in the output frame (usually 512)
     */
    HammingWindowJava(final int windowSizeSamples, final int outputSizeSamples ) {

        mWindowSizeSamples = windowSizeSamples;
        mOutputSizeSamples = outputSizeSamples;
        makeHammingWindow();

    }

    /** @brief performs conversion on one real-valued frame, and generates windowed
     *         complex-valued frame (imaginary part is zero).
     *
     * @param array_in     : input samples (frame) whose length is windowSizeSamples
     * @param preEmphTap0  : pre-emphasis coefficient usually around 0.95
     * @return             : output samples allocated whose length is outputSizeSamples.
     *                       the length in float is twice as it is an complex-valued array.
     *                       e.g. mOutputSizeSamples = 512 => float[1024]
     */
    float[] preEmphasisHammingAndMakeComplexForFFT(float[] array_in, final float preEmphTap0 ) {

        float[] array_out = new float[ mOutputSizeSamples * 2];

        array_out[0] = 0.0f;
        array_out[1] = 0.0f;

        for ( int i = 1; i < mWindowSizeSamples; i++ ) {
            array_out[2 * i    ] = mHammingWindow[i] * ( array_in[i]  - preEmphTap0 * array_in[i-1] );
            array_out[2 * i + 1] = 0.0f;
        }

        // Pad the rest with zeros.
        for ( int i = mWindowSizeSamples; i < mOutputSizeSamples; i++ ) {
            array_out[ 2* i     ] = 0.0f;
            array_out[ 2* i + 1 ] = 0.0f;
        }

        return array_out;
    }

    private void makeHammingWindow() {

        mHammingWindow = new float[mWindowSizeSamples];

        for ( int i = 0; i < mWindowSizeSamples; i++ ) {
            mHammingWindow[i] = 0.54f - 0.46f * (float)cos(2.0 * PI * (double)i / (double)(mWindowSizeSamples - 1));
        }
    }

    private int       mWindowSizeSamples;
    private int       mOutputSizeSamples;
    private float[]   mHammingWindow;
}
