package com.example.android_mfcc;

public class MFCCCPP implements MFCCInterface {

    private static final String TAG = MFCCCPP.class.getSimpleName();

    static {
        System.loadLibrary( "mfcc_impl01" );
    }


    /** @brief generates 27 MFCC
     *
     * @param exec_type       : 0         - Use NEON/SSE intrinsics.
     *                          Otherwise - NOEN/SSE not used
     * @param samples_real400 : time domain 400 real samples
     * @return 27 MFCC
     */
    public native float[] generateMFCC( int exec_type, float[] samples_real400 );

    /** @brief
     *
     * @param exec_type       : 0         - Use NEON/SSE intrinsics.
     *                          Otherwise - NOEN/SSE not used
     * @param samples_real400 : time domain 400 real samples
     * @return real 27 MFCCs and 256-point real power spectrum.
     */
    public native float[] generateMFCCAndPowerSpectrum( int exec_type, float[] samples_real400 );

};
