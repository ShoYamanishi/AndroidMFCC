#include <jni.h>
#include <string>
#include <atomic>
#include <istream>
#include <ostream>
#include <math.h>
#include <sys/time.h>

#include <cpu-features.h>
#include <complex>

#include "logging_macros.h"

#if defined(HAVE_NEON) && defined(HAVE_NEON_X86)
/*
 * The latest version and instruction for NEON_2_SSE.h is at:
 *    https://github.com/intel/ARM_NEON_2_x86_SSE
 */
#include "NEON_2_SSE.h"
#elif defined(HAVE_NEON)
#include <arm_neon.h>
#endif

//#define EXPERIMENT_NEON

#define EXPERIMENT_NEON

///////////////////////////////////////
/////// DEBUGGING TOOLS BEGIN /////////
///////////////////////////////////////

static int log_counter = -1;

static char log_samples[ 4096 ];

static void reset_log_samples()
{
    log_samples[ 0 ] = '\0';
}

static void add_log_sample( const float& v )
{
    char cur_str[ 256 ];

    sprintf( cur_str, " %.2lf", v );
    strcat( log_samples, cur_str );
}

static void print_part(
    const int          part_num,
    const float* const arr,
    const int          len
) {

    reset_log_samples();

    for ( int i = 0; i < len / 4; i++ ) {
        add_log_sample( arr[ i ] );
    }

    LOGI( "Part %d: [%s]", part_num, log_samples );
}


static void print_points( const char* message, const float* arr, int len )
{
    if ( len >  64 ) {
        const int part_len = len / 4;
        LOGI( "%s", message );
        print_part( 1,   arr,              part_len );
        print_part( 2, &(arr[part_len  ]), part_len );
        print_part( 3, &(arr[part_len*2]), part_len );
        print_part( 4, &(arr[part_len*3]), part_len );
    }
    else {

        reset_log_samples();
        for (int i = 0; i < len; i++) {
            add_log_sample(arr[i]);
        }
        LOGI("%s [%s]", message, log_samples);
    }
}

static double getTimeStampInSeconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double) tv.tv_sec + ((double) tv.tv_usec / 1000000);
}

///////////////////////////////////////
/////// DEBUGGING TOOLS END   /////////
///////////////////////////////////////


static void check_cpu_feature() {

    AndroidCpuFamily family;
    uint64_t features;
    char buffer[512];
    strlcpy(buffer, "", sizeof buffer);
    family = android_getCpuFamily();
    if ((family != ANDROID_CPU_FAMILY_ARM) &&
        (family != ANDROID_CPU_FAMILY_X86)) {
        strlcat(buffer, "Not an ARM and not an X86 CPU !\n", sizeof buffer);
    }

    features = android_getCpuFeatures();
    if (((features & ANDROID_CPU_ARM_FEATURE_ARMv7) == 0) &&
        ((features & ANDROID_CPU_X86_FEATURE_SSSE3) == 0)) {
        strlcat(buffer, "Not an ARMv7 and not an X86 SSSE3 CPU !\n", sizeof buffer);
    }

    if (((features & ANDROID_CPU_ARM_FEATURE_NEON) == 0) &&
        ((features & ANDROID_CPU_X86_FEATURE_SSSE3) == 0))
    {
        strlcat(buffer, "CPU doesn't support NEON !\n", sizeof buffer);
    }
    LOGI("CPUINFO:[%s]", buffer);
}


class HammingWindow {

public:
    /** @brief constructor
     *
     *  @param windowSizeSamples : number of samples in one input frame  (usually 400)
     *  @param preEmphTap0       : pre-emphasis coefficient              (usually around 0.95)
     */
    HammingWindow( const int windowSizeSamples, const float preEmphTap0 )
            :mWindowSizeSamples( windowSizeSamples )
            ,mPreEmphTap0      ( preEmphTap0       )
    {
        makeHammingWindow();
    }

    ~HammingWindow() {
        delete[] mHammingWindow;
    }


    /** @brief performs conversion on one real-valued frame, and generates windowed frame
     *
     *  @param array_in     : input  samples (frame) whose length is windowSizeSamples
     *  @param array_out    : output samples (frame) whose length is windowSizeSamples
     */
    void preEmphasisHammingAndMakeComplexForFFT_cpp( float* array_in, float* array_out ) {

        array_out[0] = 0.0;

        for ( int i = 1; i < mWindowSizeSamples; i++ ) {
            array_out[ i ] = mHammingWindow[ i ] * ( array_in[ i ]  - mPreEmphTap0 * array_in[ i - 1 ] );
        }
    }

#ifdef HAVE_NEON
    void preEmphasisHammingAndMakeComplexForFFT_neon( float* array_in, float* array_out ) {

        array_out[0] = 0.0;

        const float coeff1 = -1.0*mPreEmphTap0;

        for ( int i = 1;  i < mWindowSizeSamples; i += 4 ) {

            const float32x4_t tap0 = vld1q_f32( &array_in[i  ] );
            float32x4_t       tap1 = vld1q_f32( &array_in[i-1] );
            tap1 = vmulq_n_f32( tap1, coeff1 ); // tap1 = tap1 * coeff1
            const float32x4_t outvec = vaddq_f32(tap0, tap1); // outvec = tap0 + tap1

            vst1q_f32( &(array_out[ i ]), outvec );
        }
    }
#endif

private:
    void makeHammingWindow() {

        mHammingWindow = new float[mWindowSizeSamples];

        for ( int i = 0; i < mWindowSizeSamples; i++ ) {
            mHammingWindow[i] = 0.54 - 0.46 * cos( 2.0 * M_PI * (float)i / (float)(mWindowSizeSamples - 1) );
        }

    }

    const int   mWindowSizeSamples;
    const float mPreEmphTap0;
    float*      mHammingWindow;
};


class FFT512 {

public:

    FFT512 () {
        makeTwiddles();
    }

    /** @brief main function
     *
     *  @param samples_re : (in)  samples real
     *  @param samples_im : (in)  samples imaginary
     *  @param points_re  : (out) points real
     *  @param points_im  : (out) points imaginary
     *
     *  @return : frequency domain points in (re,im) pairs
     */
    void transform_cpp( float* samples_re, float* samples_im, float* points_re, float* points_im ) {

        memcpy( mArrayIn512re, samples_re, sizeof(float)* 512 );
        memcpy( mArrayIn512im, samples_im, sizeof(float)* 512 );

        cooley_tukey_fft_512_cpp( 0 );

        memcpy( points_re, mArrayOut512re, sizeof(float)* 512 );
        memcpy( points_im, mArrayOut512im, sizeof(float)* 512 );

    }

#ifdef HAVE_NEON
    void transform_neon( float* samples_re, float* samples_im, float* points_re, float* points_im ) {

        memcpy( mArrayIn512re, samples_re, sizeof(float)* 512 );
        memcpy( mArrayIn512im, samples_im, sizeof(float)* 512 );

        cooley_tukey_fft_512_neon( 0 );

        memcpy( points_re, mArrayOut512re, sizeof(float)* 512 );
        memcpy( points_im, mArrayOut512im, sizeof(float)* 512 );

    }
#endif

private:

    void makeTwiddles() {

        makeTwiddle( mTwiddle512re, mTwiddle512im, 512 );
        makeTwiddle( mTwiddle256re, mTwiddle256im, 256 );
        makeTwiddle( mTwiddle128re, mTwiddle128im, 128 );
        makeTwiddle( mTwiddle64re,  mTwiddle64im,   64 );
        makeTwiddle( mTwiddle32re,  mTwiddle32im,   32 );
        makeTwiddle( mTwiddle16re,  mTwiddle16im,   16 );
        makeTwiddle( mTwiddle8re,   mTwiddle8im,     8 );
        makeTwiddle( mTwiddle4re,   mTwiddle4im,     4 );
        makeTwiddle( mTwiddle2re,   mTwiddle2im,     2 );

    }


    void makeTwiddle( float re[], float im[], const int N ) {

        const double dN  = (double)N;

        for ( int k = 0; k < N / 2 ; k++ ) {

            const double theta = -2.0 * M_PI * (double)k / dN;

            re[ k ] = (float) cos( theta ); // re
            im[ k ] = (float) sin( theta ); // im
        }
    }


    float mTwiddle512re[256];
    float mTwiddle512im[256];
    float mTwiddle256re[128];
    float mTwiddle256im[128];
    float mTwiddle128re[ 64];
    float mTwiddle128im[ 64];
    float mTwiddle64re [ 32];
    float mTwiddle64im [ 32];
    float mTwiddle32re [ 16];
    float mTwiddle32im [ 16];
    float mTwiddle16re [  8];
    float mTwiddle16im [  8];
    float mTwiddle8re  [  4];
    float mTwiddle8im  [  4];
    float mTwiddle4re  [  2];
    float mTwiddle4im  [  2];
    float mTwiddle2re  [  1];
    float mTwiddle2im  [  1];

    float mArrayIn512re [512];// Input to 512 FFT
    float mArrayIn512im [512];
    float mArrayIn256re [512];
    float mArrayIn256im [512];
    float mArrayIn128re [512];
    float mArrayIn128im [512];
    float mArrayIn64re  [512];
    float mArrayIn64im  [512];
    float mArrayIn32re  [512];
    float mArrayIn32im  [512];
    float mArrayIn16re  [512];
    float mArrayIn16im  [512];
    float mArrayIn8re   [512];
    float mArrayIn8im   [512];
    float mArrayIn4re   [512];
    float mArrayIn4im   [512];
    float mArrayIn2re   [512];
    float mArrayIn2im   [512];

    float mArrayOut512re [512];// Output to 512 FFT
    float mArrayOut512im [512];
    float mArrayOut256re [512];
    float mArrayOut256im [512];
    float mArrayOut128re [512];
    float mArrayOut128im [512];
    float mArrayOut64re  [512];
    float mArrayOut64im  [512];
    float mArrayOut32re  [512];
    float mArrayOut32im  [512];
    float mArrayOut16re  [512];
    float mArrayOut16im  [512];
    float mArrayOut8re   [512];
    float mArrayOut8im   [512];
    float mArrayOut4re   [512];
    float mArrayOut4im   [512];
    float mArrayOut2re   [512];
    float mArrayOut2im   [512];

    inline void cooley_tukey_fft_2( const size_t pos_base ) {

        float *const array_in_re       = mArrayIn2re;
        float *const array_in_im       = mArrayIn2im;
        float *const array_out_re      = mArrayOut2re;
        float *const array_out_im      = mArrayOut2im;

        const float * const twiddle_re = mTwiddle2re;
        const float * const twiddle_im = mTwiddle2im;

        // Butterfly
        const float tw_re = twiddle_re[ 0 ];
        const float tw_im = twiddle_im[ 0 ];

        const float v1_re = array_in_re [ pos_base     ];
        const float v1_im = array_in_im [ pos_base     ];
        const float v2_re = array_in_re [ pos_base + 1 ];
        const float v2_im = array_in_im [ pos_base + 1 ];

        array_out_re[ pos_base     ] = v1_re + v2_re;
        array_out_im[ pos_base     ] = v1_im + v2_im;
        array_out_re[ pos_base + 1 ] = v1_re - v2_re;
        array_out_im[ pos_base + 1 ] = v1_im - v2_im;
    }


    inline void cooley_tukey_fft_4( const size_t pos_base ) {

        const size_t half_width = 2;

        float *const array_in_re        = mArrayIn4re;
        float *const array_in_im        = mArrayIn4im;
        float *const array_in_child_re  = mArrayIn2re;
        float *const array_in_child_im  = mArrayIn2im;

        float *const array_out_re       = mArrayOut4re;
        float *const array_out_im       = mArrayOut4im;
        float *const array_out_child_re = mArrayOut2re;
        float *const array_out_child_im = mArrayOut2im;

        const float * const twiddle_re  = mTwiddle4re;
        const float * const twiddle_im  = mTwiddle4im;

        // Splitting into even and odd.

        array_in_child_re[ pos_base              ] = array_in_re[ pos_base     ]; // even re
        array_in_child_im[ pos_base              ] = array_in_im[ pos_base     ]; // even re
        array_in_child_re[ pos_base + half_width ] = array_in_re[ pos_base + 1 ]; // odd re
        array_in_child_im[ pos_base + half_width ] = array_in_im[ pos_base + 1 ]; // odd re

        array_in_child_re[ pos_base + 1              ] = array_in_re[ pos_base + 2 ]; // even re
        array_in_child_im[ pos_base + 1              ] = array_in_im[ pos_base + 2 ]; // even re
        array_in_child_re[ pos_base + half_width + 1 ] = array_in_re[ pos_base + 3 ]; // odd re
        array_in_child_im[ pos_base + half_width + 1 ] = array_in_im[ pos_base + 3 ]; // odd re

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_2( pos_base              );
        cooley_tukey_fft_2( pos_base + half_width );

        // Butterfly
        {
            const float v1_re = array_out_child_re[ pos_base              ];
            const float v1_im = array_out_child_im[ pos_base              ];
            const float v2_re = array_out_child_re[ pos_base + half_width ];
            const float v2_im = array_out_child_im[ pos_base + half_width ];

            array_out_re[ pos_base              ] = v1_re + v2_re;
            array_out_im[ pos_base              ] = v1_im + v2_im;
            array_out_re[ pos_base + half_width ] = v1_re - v2_re;
            array_out_im[ pos_base + half_width ] = v1_im - v2_im;
        }
        {
            const float v1_re = array_out_child_re[ pos_base + 1              ];
            const float v1_im = array_out_child_im[ pos_base + 1              ];
            const float v2_re = array_out_child_re[ pos_base + half_width + 1 ];
            const float v2_im = array_out_child_im[ pos_base + half_width + 1 ];

            array_out_re[ pos_base + 1              ] = v1_re + v2_im;
            array_out_im[ pos_base + 1              ] = v1_im - v2_re;
            array_out_re[ pos_base + half_width + 1 ] = v1_re - v2_im;
            array_out_im[ pos_base + half_width + 1 ] = v1_im + v2_re;

        }

    }


    inline void cooley_tukey_fft_8_cpp( const size_t pos_base ) {

        const size_t half_width = 4;

        float *const array_in_re = mArrayIn8re;
        float *const array_in_im = mArrayIn8im;
        float *const array_in_child_re = mArrayIn4re;
        float *const array_in_child_im = mArrayIn4im;

        float *const array_out_re = mArrayOut8re;
        float *const array_out_im = mArrayOut8im;
        float *const array_out_child_re = mArrayOut4re;
        float *const array_out_child_im = mArrayOut4im;

        const float *const twiddle_re = mTwiddle8re;
        const float *const twiddle_im = mTwiddle8im;

        // Splitting into even and odd.

        for (size_t i = 0; i < half_width; i++) {

            array_in_child_re[ pos_base + i              ] = array_in_re[ pos_base + 2 * i     ]; // even re
            array_in_child_im[ pos_base + i              ] = array_in_im[ pos_base + 2 * i     ]; // even re
            array_in_child_re[ pos_base + half_width + i ] = array_in_re[ pos_base + 2 * i + 1 ]; // odd re
            array_in_child_im[ pos_base + half_width + i ] = array_in_im[ pos_base + 2 * i + 1 ]; // odd re
        }

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_4( pos_base );
        cooley_tukey_fft_4( pos_base + half_width );

        // Butterfly
        butterfly_cpp( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }

#ifdef HAVE_NEON
    inline void cooley_tukey_fft_8_neon( const size_t pos_base ) {

        const size_t half_width = 4;

        float *const array_in_re = mArrayIn8re;
        float *const array_in_im = mArrayIn8im;
        float *const array_in_child_re = mArrayIn4re;
        float *const array_in_child_im = mArrayIn4im;

        float *const array_out_re = mArrayOut8re;
        float *const array_out_im = mArrayOut8im;
        float *const array_out_child_re = mArrayOut4re;
        float *const array_out_child_im = mArrayOut4im;

        const float *const twiddle_re = mTwiddle8re;
        const float *const twiddle_im = mTwiddle8im;

        // Splitting into even and odd.
        float32x4x2_t interleaved_chunk = vld2q_f32( &(array_in_re[ pos_base ]) );
        vst1q_f32( (float32_t *)( &(array_in_child_re[pos_base    ]) ), interleaved_chunk.val[0] );
        vst1q_f32( (float32_t *)( &(array_in_child_re[pos_base + 4]) ), interleaved_chunk.val[1] );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_4( pos_base );
        cooley_tukey_fft_4( pos_base + half_width );

        // Butterfly
        butterfly_neon( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }
#endif


    inline void cooley_tukey_fft_16_cpp( const size_t pos_base ) {

        const size_t half_width = 8;

        float *const array_in_re        = mArrayIn16re;
        float *const array_in_im        = mArrayIn16im;
        float *const array_in_child_re  = mArrayIn8re;
        float *const array_in_child_im  = mArrayIn8im;

        float *const array_out_re       = mArrayOut16re;
        float *const array_out_im       = mArrayOut16im;
        float *const array_out_child_re = mArrayOut8re;
        float *const array_out_child_im = mArrayOut8im;

        const float * const twiddle_re  = mTwiddle16re;
        const float * const twiddle_im  = mTwiddle16im;

        // Splitting into even and odd.

        deinterleave_cpp(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_cpp(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_8_cpp( pos_base              );
        cooley_tukey_fft_8_cpp( pos_base + half_width );

        // Butterfly
        butterfly_cpp( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }

#ifdef HAVE_NEON
    inline void cooley_tukey_fft_16_neon( const size_t pos_base ) {

        const size_t half_width = 8;

        float *const array_in_re        = mArrayIn16re;
        float *const array_in_im        = mArrayIn16im;
        float *const array_in_child_re  = mArrayIn8re;
        float *const array_in_child_im  = mArrayIn8im;

        float *const array_out_re       = mArrayOut16re;
        float *const array_out_im       = mArrayOut16im;
        float *const array_out_child_re = mArrayOut8re;
        float *const array_out_child_im = mArrayOut8im;

        const float * const twiddle_re  = mTwiddle16re;
        const float * const twiddle_im  = mTwiddle16im;

        // Splitting into even and odd.
        deinterleave_neon(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_neon(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_8_neon( pos_base              );
        cooley_tukey_fft_8_neon( pos_base + half_width );

        // Butterfly
        butterfly_neon( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }
#endif


    inline void cooley_tukey_fft_32_cpp( const size_t pos_base ) {

        const size_t half_width = 16;

        float *const array_in_re        = mArrayIn32re;
        float *const array_in_im        = mArrayIn32im;
        float *const array_in_child_re  = mArrayIn16re;
        float *const array_in_child_im  = mArrayIn16im;

        float *const array_out_re       = mArrayOut32re;
        float *const array_out_im       = mArrayOut32im;
        float *const array_out_child_re = mArrayOut16re;
        float *const array_out_child_im = mArrayOut16im;

        const float * const twiddle_re  = mTwiddle32re;
        const float * const twiddle_im  = mTwiddle32im;

        // Splitting into even and odd.

        deinterleave_cpp(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_cpp(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_16_cpp( pos_base              );
        cooley_tukey_fft_16_cpp( pos_base + half_width );

        // Butterfly
        butterfly_cpp( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }

#ifdef HAVE_NEON
    inline void cooley_tukey_fft_32_neon( const size_t pos_base ) {

        const size_t half_width = 16;

        float *const array_in_re        = mArrayIn32re;
        float *const array_in_im        = mArrayIn32im;
        float *const array_in_child_re  = mArrayIn16re;
        float *const array_in_child_im  = mArrayIn16im;

        float *const array_out_re       = mArrayOut32re;
        float *const array_out_im       = mArrayOut32im;
        float *const array_out_child_re = mArrayOut16re;
        float *const array_out_child_im = mArrayOut16im;

        const float * const twiddle_re  = mTwiddle32re;
        const float * const twiddle_im  = mTwiddle32im;

        // Splitting into even and odd.
        deinterleave_neon(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_neon(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_16_neon( pos_base              );
        cooley_tukey_fft_16_neon( pos_base + half_width );

        // Butterfly
        butterfly_neon( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }
#endif


    inline void cooley_tukey_fft_64_cpp( const size_t pos_base ) {

        const size_t half_width = 32;

        float *const array_in_re        = mArrayIn64re;
        float *const array_in_im        = mArrayIn64im;
        float *const array_in_child_re  = mArrayIn32re;
        float *const array_in_child_im  = mArrayIn32im;

        float *const array_out_re       = mArrayOut64re;
        float *const array_out_im       = mArrayOut64im;
        float *const array_out_child_re = mArrayOut32re;
        float *const array_out_child_im = mArrayOut32im;

        const float * const twiddle_re  = mTwiddle64re;
        const float * const twiddle_im  = mTwiddle64im;

        // Splitting into even and odd.
        deinterleave_cpp(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_cpp(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_32_cpp( pos_base              );
        cooley_tukey_fft_32_cpp( pos_base + half_width );

        // Butterfly
        butterfly_cpp( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }

#ifdef HAVE_NEON
    inline void cooley_tukey_fft_64_neon( const size_t pos_base ) {

        const size_t half_width = 32;

        float *const array_in_re        = mArrayIn64re;
        float *const array_in_im        = mArrayIn64im;
        float *const array_in_child_re  = mArrayIn32re;
        float *const array_in_child_im  = mArrayIn32im;

        float *const array_out_re       = mArrayOut64re;
        float *const array_out_im       = mArrayOut64im;
        float *const array_out_child_re = mArrayOut32re;
        float *const array_out_child_im = mArrayOut32im;

        const float * const twiddle_re  = mTwiddle64re;
        const float * const twiddle_im  = mTwiddle64im;

        // Splitting into even and odd.
        deinterleave_neon(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_neon(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_32_neon( pos_base              );
        cooley_tukey_fft_32_neon( pos_base + half_width );

        // Butterfly
        butterfly_neon( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }
#endif

    inline void cooley_tukey_fft_128_cpp( const size_t pos_base ) {

        const size_t half_width = 64;

        float *const array_in_re        = mArrayIn128re;
        float *const array_in_im        = mArrayIn128im;
        float *const array_in_child_re  = mArrayIn64re;
        float *const array_in_child_im  = mArrayIn64im;

        float *const array_out_re       = mArrayOut128re;
        float *const array_out_im       = mArrayOut128im;
        float *const array_out_child_re = mArrayOut64re;
        float *const array_out_child_im = mArrayOut64im;

        const float * const twiddle_re  = mTwiddle128re;
        const float * const twiddle_im  = mTwiddle128im;

        // Splitting into even and odd.
        deinterleave_cpp(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_cpp(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_64_cpp( pos_base );
        cooley_tukey_fft_64_cpp( pos_base + half_width );

        // Butterfly
        butterfly_cpp( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }

#ifdef HAVE_NEON
    inline void cooley_tukey_fft_128_neon( const size_t pos_base ) {

        const size_t half_width = 64;

        float *const array_in_re        = mArrayIn128re;
        float *const array_in_im        = mArrayIn128im;
        float *const array_in_child_re  = mArrayIn64re;
        float *const array_in_child_im  = mArrayIn64im;

        float *const array_out_re       = mArrayOut128re;
        float *const array_out_im       = mArrayOut128im;
        float *const array_out_child_re = mArrayOut64re;
        float *const array_out_child_im = mArrayOut64im;

        const float * const twiddle_re  = mTwiddle128re;
        const float * const twiddle_im  = mTwiddle128im;

        // Splitting into even and odd.
        deinterleave_neon(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_neon(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_64_neon(pos_base);
        cooley_tukey_fft_64_neon(pos_base + half_width);

        // Butterfly
        butterfly_neon( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }
#endif

    inline void cooley_tukey_fft_256_cpp( const size_t pos_base ) {

        const size_t half_width = 128;

        float *const array_in_re        = mArrayIn256re;
        float *const array_in_im        = mArrayIn256im;
        float *const array_in_child_re  = mArrayIn128re;
        float *const array_in_child_im  = mArrayIn128im;

        float *const array_out_re       = mArrayOut256re;
        float *const array_out_im       = mArrayOut256im;
        float *const array_out_child_re = mArrayOut128re;
        float *const array_out_child_im = mArrayOut128im;

        const float * const twiddle_re  = mTwiddle256re;
        const float * const twiddle_im  = mTwiddle256im;

        // Splitting into even and odd.
        deinterleave_cpp(
            &( array_in_re      [ pos_base ] ),
            half_width * 2,
            &( array_in_child_re[ pos_base ] ),
            &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_cpp(
            &( array_in_im      [ pos_base ] ),
            half_width * 2,
            &( array_in_child_im[ pos_base ] ),
            &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_128_cpp( pos_base              );
        cooley_tukey_fft_128_cpp( pos_base + half_width );

        // Butterfly
        butterfly_cpp( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }

#ifdef HAVE_NEON
    inline void cooley_tukey_fft_256_neon( const size_t pos_base ) {

        const size_t half_width = 128;

        float *const array_in_re        = mArrayIn256re;
        float *const array_in_im        = mArrayIn256im;
        float *const array_in_child_re  = mArrayIn128re;
        float *const array_in_child_im  = mArrayIn128im;

        float *const array_out_re       = mArrayOut256re;
        float *const array_out_im       = mArrayOut256im;
        float *const array_out_child_re = mArrayOut128re;
        float *const array_out_child_im = mArrayOut128im;

        const float * const twiddle_re  = mTwiddle256re;
        const float * const twiddle_im  = mTwiddle256im;

        // Splitting into even and odd.
        deinterleave_neon(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_neon(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_128_neon( pos_base              );
        cooley_tukey_fft_128_neon( pos_base + half_width );

        // Butterfly
        butterfly_neon( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }
#endif

    inline void cooley_tukey_fft_512_cpp( const size_t pos_base ) {

        const size_t half_width = 256;

        float * const array_in_re        = mArrayIn512re;
        float * const array_in_im        = mArrayIn512im;
        float * const array_in_child_re  = mArrayIn256re;
        float * const array_in_child_im  = mArrayIn256im;

        float * const array_out_re       = mArrayOut512re;
        float * const array_out_im       = mArrayOut512im;
        float * const array_out_child_re = mArrayOut256re;
        float * const array_out_child_im = mArrayOut256im;

        const float * const twiddle_re   = mTwiddle512re;
        const float * const twiddle_im   = mTwiddle512im;

        // Splitting into even and odd.
        deinterleave_cpp(
            &( array_in_re      [ pos_base ] ),
            half_width * 2,
            &( array_in_child_re[ pos_base ] ),
            &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_cpp(
            &( array_in_im      [ pos_base ] ),
            half_width * 2,
            &( array_in_child_im[ pos_base ] ),
            &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_256_cpp( pos_base              );
        cooley_tukey_fft_256_cpp( pos_base + half_width );

        // Butterfly
        butterfly_cpp( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }

#ifdef HAVE_NEON
    inline void cooley_tukey_fft_512_neon( const size_t pos_base ) {

        const size_t half_width = 256;

        float * const array_in_re        = mArrayIn512re;
        float * const array_in_im        = mArrayIn512im;
        float * const array_in_child_re  = mArrayIn256re;
        float * const array_in_child_im  = mArrayIn256im;

        float * const array_out_re       = mArrayOut512re;
        float * const array_out_im       = mArrayOut512im;
        float * const array_out_child_re = mArrayOut256re;
        float * const array_out_child_im = mArrayOut256im;

        const float * const twiddle_re   = mTwiddle512re;
        const float * const twiddle_im   = mTwiddle512im;

        // Splitting into even and odd.
        deinterleave_neon(
                &( array_in_re      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_re[ pos_base ] ),
                &( array_in_child_re[ pos_base + half_width ] ) );

        deinterleave_neon(
                &( array_in_im      [ pos_base ] ),
                half_width * 2,
                &( array_in_child_im[ pos_base ] ),
                &( array_in_child_im[ pos_base + half_width ] ) );

        // Recursive FFT expected to be expanded here.
        cooley_tukey_fft_256_neon( pos_base              );
        cooley_tukey_fft_256_neon( pos_base + half_width );

        // Butterfly
        butterfly_neon( twiddle_re, twiddle_im, array_out_child_re, array_out_child_im, array_out_re, array_out_im, pos_base, half_width );
    }
#endif

    inline void butterfly_cpp(
        const float * const twiddle_re,
        const float * const twiddle_im,
        float * const       array_in_re,
        float * const       array_in_im,
        float * const       array_out_re,
        float * const       array_out_im,
        const int           pos_base,
        const int           half_width
    ) {

        for ( size_t i = 0; i < half_width; i++ ) {

            const float tw_re = twiddle_re[ i ];
            const float tw_im = twiddle_im[ i ];

            const float v1_re = array_in_re[ pos_base + i              ];
            const float v1_im = array_in_im[ pos_base + i              ];
            const float v2_re = array_in_re[ pos_base + half_width + i ];
            const float v2_im = array_in_im[ pos_base + half_width + i ];

            const float offset_re = tw_re * v2_re - tw_im * v2_im;
            const float offset_im = tw_re * v2_im + tw_im * v2_re;

            array_out_re[ pos_base + i              ] = v1_re + offset_re;
            array_out_im[ pos_base + i              ] = v1_im + offset_im;
            array_out_re[ pos_base + half_width + i ] = v1_re - offset_re;
            array_out_im[ pos_base + half_width + i ] = v1_im - offset_im;
        }
    }

#ifdef HAVE_NEON
    inline void butterfly_neon(
            const float * const twiddle_re,
            const float * const twiddle_im,
            float * const       array_in_re,
            float * const       array_in_im,
            float * const       array_out_re,
            float * const       array_out_im,
            const int           pos_base,
            const int           half_width
    ) {
        for ( size_t i = 0; i < half_width; i+=4 ) {

            const float32x4_t tw_re     = vld1q_f32( &( twiddle_re[i] )                          );
            const float32x4_t tw_im     = vld1q_f32( &( twiddle_im[i] )                          );
            const float32x4_t v1_re_pre = vld1q_f32( &( array_in_re[pos_base + i] )              );
            const float32x4_t v1_im_pre = vld1q_f32( &( array_in_im[pos_base + i] )              );
            const float32x4_t v2_re_pre = vld1q_f32( &( array_in_re[pos_base + half_width + i] ) );
            const float32x4_t v2_im_pre = vld1q_f32( &( array_in_im[pos_base + half_width + i] ) );

            // const float offset_re = tw_re * v2_re - tw_im * v2_im;
            const float32x4_t offset_re_part1 = vmulq_f32( tw_re, v2_re_pre );
            const float32x4_t offset_re       = vmlsq_f32( offset_re_part1, tw_im, v2_im_pre );

            // const float offset_im = tw_re * v2_im + tw_im * v2_re;
            const float32x4_t offset_im_part1 = vmulq_f32( tw_re, v2_im_pre );
            const float32x4_t offset_im       = vmlaq_f32( offset_im_part1, tw_im, v2_re_pre );

            const float32x4_t v1_re = vaddq_f32( v1_re_pre, offset_re );
            const float32x4_t v1_im = vaddq_f32( v1_im_pre, offset_im );
            const float32x4_t v2_re = vsubq_f32( v1_re_pre, offset_re );
            const float32x4_t v2_im = vsubq_f32( v1_im_pre, offset_im );

            vst1q_f32( &( array_out_re[ pos_base + i              ] ), v1_re );
            vst1q_f32( &( array_out_re[ pos_base + half_width + i ] ), v2_re );
            vst1q_f32( &( array_out_im[ pos_base + i              ] ), v1_im );
            vst1q_f32( &( array_out_im[ pos_base + half_width + i ] ), v2_im );
        }
    }
#endif

    inline void deinterleave_cpp( const float * const src, const int src_len, float* const even, float* const odd ) {

        for ( size_t dst_i = 0; dst_i < src_len / 2; dst_i++ ) {
            even[dst_i] = src[ dst_i * 2     ];
            odd [dst_i] = src[ dst_i * 2 + 1 ];
        }
    }


#ifdef HAVE_NEON
    inline void deinterleave_neon( const float * const src, const int src_len, float* const even, float* const odd ) {

        for ( size_t dst_i = 0; dst_i < src_len/2; dst_i += 4 ) {

            float32x4x2_t interleaved_chunk = vld2q_f32( &(src[dst_i*2]) );

            vst1q_f32( (float32_t *)( &( even[dst_i] ) ), interleaved_chunk.val[0] );
            vst1q_f32( (float32_t *)( &(  odd[dst_i] ) ), interleaved_chunk.val[1] );

        }
    }
#endif

};


class sampleToBin {

public:
    sampleToBin()
            :mBin1   ( -1)
            ,mBin2   ( -1)
            ,mCoeff1 (0.0)
            ,mCoeff2 (0.0) {;}

    void setValues( const int bin1, const int bin2, const double coeff1 ) {

        mBin1   = bin1;
        mBin2   = bin2;
        mCoeff1 = coeff1;
        mCoeff2 = (1.0 - coeff1);
    }


    int    bin1()   const { return mBin1;   }
    int    bin2()   const { return mBin2;   }
    double coeff1() const { return mCoeff1; }
    double coeff2() const { return mCoeff2; }

private:
    int    mBin1;
    int    mBin2;
    double mCoeff1;
    double mCoeff2;

};


class MelFilterBanks {

public:
    static const int   cNumFilterBanks ;
    static const int   cNumSamples;
    static const float cNumSamplesD;
    static const float cSampleRate;
    static const float cHalfSampleRate;
    static const float cFilterBankMaxFreq;
    static const float cFilterBankMinFreq;
    static const float cMelFloor;


    /** @brief constructor.
     */
    MelFilterBanks () {

        mSampleToBin = new sampleToBin[ cNumSamples ];
        constructSampleToBin( cNumSamples );

    }

    ~MelFilterBanks () {
        delete[] mSampleToBin;
    }

    /** @brief find log Mel filter bank coefficients
     *
     *  @param points_re : (in)  first 256 points from complex 512-point FFT, real parts
     *  @param points_im : (in)  first 256 points from complex 512-point FFT, imaginary parts
     *  @param mel_bins  : (out) Log Mel filter bank energy coefficients in real values
     */
    void findLogMelCoeffs( float* points_re, float* points_im, float* mel_bins ) {

        for ( int i = 0; i < cNumFilterBanks; i++ ) {
            mel_bins[ i ] = 0.0;
        }

        for ( int i = 0; i < cNumSamples; i++ ) {

            sampleToBin& stb = mSampleToBin[ i ];

            const float& re  = points_re[ i ];
            const float& im  = points_im[ i ];
            const float& pwr = re*re + im*im;

            if ( stb.bin1() != -1 )  {
                mel_bins[ stb.bin1() ] += ( pwr * stb.coeff1() ) ;
            }
            if ( stb.bin2() != -1)  {
                mel_bins[ stb.bin2() ] += ( pwr * stb.coeff2() );
            }
        }

        for ( int i = 0; i < cNumFilterBanks; i++ ) {
            if ( mel_bins[ i ] < cMelFloor ) {
                mel_bins[ i ] = cMelFloor;
            }
            mel_bins[ i ] = log( mel_bins[ i ] ) ;
        }
    }


    void constructSampleToBin (const int numSamples ) {

        mSampleToBin = new sampleToBin[ cNumSamples ];

        double filterBankMaxMel = freqToMel( cFilterBankMaxFreq );
        double filterBankMinMel = freqToMel( cFilterBankMinFreq );

        double intervalMel      =     (  filterBankMaxMel -  filterBankMinMel )
                                    / ( (float)cNumFilterBanks + 1.0 );

        int   melBoundariesSamplePoint[ cNumFilterBanks + 2 ];
        float melBoundariesMelFreq    [ cNumFilterBanks + 2 ];

        for ( int i = 0; i <= cNumFilterBanks + 1 ; i++ ) {

            const float& m = filterBankMinMel + (float)i * intervalMel;
            const float& f = melToFreq( m );
            const int    s = freqToSampleNum( f ) ;

            melBoundariesSamplePoint[ i ] = s;
            melBoundariesMelFreq    [ i ] = m;
        }

        for ( int i = 0; i < cNumSamples ; i++ ) {

            const float f = sampleNumToFreq( i );
            const float m = freqToMel( f );

            if (   ( m < melBoundariesMelFreq[ 0 ]                )
                || ( melBoundariesMelFreq[ cNumFilterBanks ]  < m ) ) {

                mSampleToBin[i].setValues( -1, -1, 0.0 );
            }

            else if (    ( melBoundariesMelFreq[ 0 ] <= m  )
                      && ( m < melBoundariesMelFreq[ 1 ]   )  ) {

                mSampleToBin[i].setValues( 0, -1, ( m - melBoundariesMelFreq[0] ) / intervalMel );
            }

            else if (    ( melBoundariesMelFreq[ cNumFilterBanks  - 1 ] <= m  )
                      && ( m < melBoundariesMelFreq[ cNumFilterBanks  ]       )  ) {

                mSampleToBin[i].setValues( cNumFilterBanks  - 1 , -1, ( melBoundariesMelFreq[cNumFilterBanks ] - m ) / intervalMel );
            }

            else {
                for ( int j = 1; j < cNumFilterBanks ; j++ ) {
                    if (    ( melBoundariesMelFreq[ j ] <= m    )
                         && ( m < melBoundariesMelFreq[ j + 1 ] ) ) {

                        mSampleToBin[i].setValues( j , j + 1, ( melBoundariesMelFreq[j+1] - m ) / intervalMel );
                        break;
                    }
                }
            }
        }
    }

    float sampleNumToFreq( const int& i ) {

        return ( (cHalfSampleRate / cNumSamplesD)  * (float) i );
    }

    int freqToSampleNum( const float& f ) {

        return (int)( ( cNumSamplesD / cHalfSampleRate ) * f ) ;
    }

    float freqToMel ( const float& f ) {

        return 1125.0 * log( 1.0 + f / 700.0);
    }

    float melToFreq ( const float& m ) {

        return  700.0 * ( exp( m / 1125.0) - 1.0 );
    }

    sampleToBin* mSampleToBin;

};

const int   MelFilterBanks::cNumFilterBanks    =    26;
const int   MelFilterBanks::cNumSamples        =   256;
const float MelFilterBanks::cNumSamplesD       =   256.0;
const float MelFilterBanks::cSampleRate        = 16000.0;
const float MelFilterBanks::cHalfSampleRate    =  8000.0;
const float MelFilterBanks::cFilterBankMaxFreq =  8000.0;
const float MelFilterBanks::cFilterBankMinFreq =   300.0;
const float MelFilterBanks::cMelFloor          =     1.0;


class DCT {

public:

    /** @brief constructor. it pre-calculates a table.
     *
     *  @param numPoints : number of points in the input.
     */
    DCT ( const int numPoints ) {

        mNumPoints = numPoints;
        mNumPointsRoundUp4 = ((mNumPoints + 3) / 4) * 4;
        makeDCTTable();

    }

    ~DCT() { delete[] mDCTTable; }


    /** @brief main function for DCT
     *  @param samples_in  : (in)  time domain samples
     *  @param samples_out : (out) freq-domain samples
     */
    void transform_cpp( const float* const samples_in, float* const samples_out ) const {

        for ( int i = 0; i < mNumPoints + 1 ; i++ ) {

            float val = 0.0;

            for ( int j = 0; j < mNumPoints; j++ ) {

                val += mDCTTable[ ( mNumPoints + 1 ) * i + j ] * samples_in[ j ];
            }

            samples_out[i] = val;
        }
    }

#ifdef HAVE_NEON
    void transform_neon( const float* const samples_in, float* const samples_out ) const {

        for ( int i = 0; i < mNumPoints + 1 ; i++ ) {

            float val = 0.0;

            float32x4_t sumQuadF = vdupq_n_f32(0.0);

            for ( int j = 0; j < mNumPointsRoundUp4; j+=4 ) {

                float32x4_t cur_sample = vld1q_f32( &( samples_in[j] ) );
                float32x4_t cur_dct    = vld1q_f32( &( mDCTTable[( mNumPoints + 1 ) * i + j ] ) );
                sumQuadF = vmlaq_f32( sumQuadF, cur_dct, cur_sample );

            }

            val += vgetq_lane_f32( sumQuadF, 0 );
            val += vgetq_lane_f32( sumQuadF, 1 );
            val += vgetq_lane_f32( sumQuadF, 2 );
            val += vgetq_lane_f32( sumQuadF, 3 );

            samples_out[i] = val;

        }
    }
#endif

private:

    void makeDCTTable() {

        // Allocate redundant memory and padd with zero for 4-lane SIMD operations.
        mDCTTable = new float[ (mNumPoints + 1) * mNumPointsRoundUp4 ];
        memset ( mDCTTable, 0, sizeof(float) * ( (mNumPoints + 1) * mNumPointsRoundUp4 ) );

        const float C = sqrt( 2.0 / mNumPoints );

        for ( int i = 0; i <= mNumPoints; i++ ) {

            for (int j = 0; j < mNumPoints; j++ ) {

                const float di = (float)i ;
                const float dj = (float)j + 0.5 ;
                mDCTTable[ ( mNumPoints + 1 )* i + j ] = C * cos( M_PI * di * dj / (float)mNumPoints );
            }
        }
    }

    int    mNumPoints;
    int    mNumPointsRoundUp4;
    float* mDCTTable;
};


class MFCC {

public:

    static constexpr float cSampleRate              = 16000.0;
    static constexpr int   cFrameSizeSamples        = 400;     // 25[ms] @ 16KHz
    static constexpr int   cFrameShiftSamples       = 160;     // 10[ms] @ 16KHz
    static constexpr int   cNumPointsFFT            = 512;
    static constexpr float cPreemphTap0             = 0.96;
    static constexpr float cFilterBankMaxFreq       = 8000.0;
    static constexpr float cFilterBankMinFreq       = 300.0;
    static constexpr int   cNumFilterBanks          = 26;
    static constexpr int   cNumFilterBankssRoundUp4 = 28;

    /** @brief constructor
     */
    MFCC()
        :mHammingWindow( cFrameSizeSamples, cPreemphTap0 )
        ,mFFT512()
        ,mMelFilterBanks()
        ,mDCT( cNumFilterBanks )
    {
        memset( mWindowedSamples_re, 0, sizeof(float) * cNumPointsFFT            );
        memset( mWindowedSamples_im, 0, sizeof(float) * cNumPointsFFT            );
        memset( mMelFilterBankBins,  0, sizeof(float) * cNumFilterBankssRoundUp4 );
    }

    ~MFCC() {
        ;
    }

    /** @brief generates spectral density in 256 points.
     *
     *  @param samples_real400 : time domain 400 real samples.
     *  @return energy density at 256 points after FFT.
     */
    void spectralDensity_cpp( float* samples_real400, float* power_real_256 ) {

        // 1. Pre-Emphasis & Hamming window oer 400 samples.
        mHammingWindow.preEmphasisHammingAndMakeComplexForFFT_cpp( samples_real400, mWindowedSamples_re );

        // 2. 512 point FFT.
        mFFT512.transform_cpp( mWindowedSamples_re, mWindowedSamples_im,  mFFT512_re,  mFFT512_im );

        for (int i = 0; i < 256 ; i++) {

            const float re = mFFT512_re[ i ];
            const float im = mFFT512_im[ i ];

            power_real_256[i] = std::max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;
        }
    }

#ifdef HAVE_NEON
    void spectralDensity_neon( float* samples_real400, float* power_real_256 ) {

        // 1. Pre-Emphasis & Hamming window oer 400 samples.
        mHammingWindow.preEmphasisHammingAndMakeComplexForFFT_neon( samples_real400, mWindowedSamples_re );

        // 2. 512 point FFT.
        mFFT512.transform_neon( mWindowedSamples_re, mWindowedSamples_im,  mFFT512_re,  mFFT512_im );

        for (int i = 0; i < 256 ; i++) {

            const float re = mFFT512_re[ i ];
            const float im = mFFT512_im[ i ];

            power_real_256[i] = std::max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;
        }
    }
#endif

    /** @brief
     *
     *  @param samples_real400 : time domain 400 real samples
     *  @return real 27 MFCCs
     */
    void generateMFCC_cpp( float* samples_real400, float* mfcc ) {

        // 1. Pre-Emphasis & Hamming window oer 400 samples.
        mHammingWindow.preEmphasisHammingAndMakeComplexForFFT_cpp( samples_real400, mWindowedSamples_re );

        // 2. 512 point FFT.
        mFFT512.transform_cpp( mWindowedSamples_re, mWindowedSamples_im,  mFFT512_re,  mFFT512_im );

        // 3. Log Mel coefficients
        mMelFilterBanks.findLogMelCoeffs(  mFFT512_re,  mFFT512_im, mMelFilterBankBins );

        // 4. DCT
        mDCT.transform_cpp( mMelFilterBankBins, mfcc );

    }

#ifdef HAVE_NEON
    void generateMFCC_neon( float* samples_real400, float* mfcc ) {

        // 1. Pre-Emphasis & Hamming window oer 400 samples.
        mHammingWindow.preEmphasisHammingAndMakeComplexForFFT_neon( samples_real400, mWindowedSamples_re );

        // 2. 512 point FFT.
        mFFT512.transform_neon( mWindowedSamples_re, mWindowedSamples_im,  mFFT512_re,  mFFT512_im );

        // 3. Log Mel coefficients
        mMelFilterBanks.findLogMelCoeffs(  mFFT512_re,  mFFT512_im, mMelFilterBankBins );

        // 4. DCT
        mDCT.transform_cpp( mMelFilterBankBins, mfcc );

    }
#endif

    /** @brief
      *
      * @param samples_real400 : time domain 400 real samples
      * @return real 27 MFCCs and real 256-point power spectrum.
      */
    void generateMFCCAndPowerSpectrum_cpp( float* samples_real400, float* mfcc_fft ) {
        log_counter++;
        // 1. Pre-Emphasis & Hamming window oer 400 samples.
        mHammingWindow.preEmphasisHammingAndMakeComplexForFFT_cpp( samples_real400, mWindowedSamples_re );

        // 2. 512 point FFT.
        mFFT512.transform_cpp( mWindowedSamples_re, mWindowedSamples_im,  mFFT512_re,  mFFT512_im );

        for (int i = 0; i < 256 ; i++) {

            const float re = mFFT512_re[ i ];
            const float im = mFFT512_im[ i ];

            mfcc_fft[i+27] = std::max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;
        }

        // 3. Log Mel coefficients
        mMelFilterBanks.findLogMelCoeffs(  mFFT512_re,  mFFT512_im, mMelFilterBankBins );

        // 4. DCT
        mDCT.transform_cpp( mMelFilterBankBins, mfcc_fft );
    }

#ifdef HAVE_NEON
    void generateMFCCAndPowerSpectrum_neon( float* samples_real400, float* mfcc_fft ) {
        log_counter++;
        // 1. Pre-Emphasis & Hamming window oer 400 samples.
        mHammingWindow.preEmphasisHammingAndMakeComplexForFFT_neon( samples_real400, mWindowedSamples_re );

        // 2. 512 point FFT.
        mFFT512.transform_neon( mWindowedSamples_re, mWindowedSamples_im,  mFFT512_re,  mFFT512_im );

        for (int i = 0; i < 256 ; i++) {

            const float re = mFFT512_re[ i ];
            const float im = mFFT512_im[ i ];

            mfcc_fft[i+27] = std::max( 0.0, log10( (re * re + im * im) ) / 10.0 ) ;
        }

        // 3. Log Mel coefficients
        mMelFilterBanks.findLogMelCoeffs(  mFFT512_re,  mFFT512_im, mMelFilterBankBins );

        // 4. DCT
        mDCT.transform_neon( mMelFilterBankBins, mfcc_fft );
    }
#endif

    HammingWindow  mHammingWindow;
    FFT512         mFFT512;
    MelFilterBanks mMelFilterBanks;
    DCT            mDCT;

    float mWindowedSamples_re [ cNumPointsFFT   ];
    float mWindowedSamples_im [ cNumPointsFFT   ];
    float mFFT512_re          [ cNumPointsFFT   ];
    float mFFT512_im          [ cNumPointsFFT   ];
    float mMelFilterBankBins  [ cNumFilterBankssRoundUp4 ];

};

static MFCC mfccInst;

extern "C" JNIEXPORT jfloatArray
JNICALL Java_com_example_android_1mfcc_MFCCCPP_generateMFCC(
        JNIEnv*     env,
        jobject     jthis,
        jint        execution_type,
        jfloatArray samples_real400
) {
    jboolean isCopy;
    jfloat*  samples_real400_jfloat = env->GetFloatArrayElements( samples_real400, &isCopy );
    jsize    samples_real400_size   = env->GetArrayLength       ( samples_real400 );

    jfloatArray mfcc_27;
    mfcc_27 = env->NewFloatArray( 27 );
    if ( mfcc_27 == nullptr ) {
        return nullptr;
    }

    jfloat mfcc_27_jfloat[ 27 ];
    if ( execution_type == 0 ) {
        mfccInst.generateMFCC_neon( samples_real400_jfloat, mfcc_27_jfloat );
    }
    else {
        mfccInst.generateMFCC_cpp( samples_real400_jfloat, mfcc_27_jfloat );
    }

    env->SetFloatArrayRegion      ( mfcc_27, 0, 27, mfcc_27_jfloat );
    env->ReleaseFloatArrayElements( samples_real400, samples_real400_jfloat, 0 );

    return mfcc_27;

}

extern "C" JNIEXPORT jfloatArray
JNICALL Java_com_example_android_1mfcc_MFCCCPP_generateMFCCAndPowerSpectrum(
        JNIEnv*     env,
        jobject     jthis,
        jint        execution_type,
        jfloatArray samples_real400
) {
    jboolean isCopy;
    jfloat*  samples_real400_jfloat = env->GetFloatArrayElements( samples_real400, &isCopy );
    jsize    samples_real400_size   = env->GetArrayLength       ( samples_real400 );

    jfloatArray mfcc_27_fft_256;
    mfcc_27_fft_256 = env->NewFloatArray( 27 + 256 );
    if ( mfcc_27_fft_256 == nullptr ) {
        return nullptr;
    }

    jfloat mfcc_27_fft_256_jfloat[ 27 + 256 ];
    if ( execution_type == 0 ) {
        mfccInst.generateMFCCAndPowerSpectrum_neon( samples_real400_jfloat, mfcc_27_fft_256_jfloat );
    }
    else {
        mfccInst.generateMFCCAndPowerSpectrum_cpp( samples_real400_jfloat, mfcc_27_fft_256_jfloat );
    }

    env->SetFloatArrayRegion      ( mfcc_27_fft_256, 0, 27 + 256, mfcc_27_fft_256_jfloat );
    env->ReleaseFloatArrayElements( samples_real400, samples_real400_jfloat, 0 );

    return mfcc_27_fft_256;

}

