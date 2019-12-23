package com.example.android_mfcc;

public interface MFCCInterface {

    /** @brief generates 27 MFCC
     *
     * @param exec_type        : 0-Neon 1-Cpp, Any-Java
     * @param samples_real400
     * @return 27 MFCC
     */
    public float[] generateMFCC                ( int exec_type, float[] samples_real400 );

    /** @brief generates 27 MFCC and then 256 Spectrum
     *
     * @param exec_type        : 0-Neon 1-Cpp, Any-Java
     * @param samples_real400
     * @return 27 MFCC + 256 Power spectrum
     */
    public float[] generateMFCCAndPowerSpectrum( int exec_type, float[] samples_real400 );


}
