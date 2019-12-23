package com.example.android_mfcc;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.time.Instant;
import java.util.Arrays;

public class TopLevelMFCCProcessor implements AudioReceiverListener {

    private static final String TAG = TopLevelMFCCProcessor.class.getSimpleName();

    TopLevelMFCCProcessor ( ScrollingHeatMapView mfccView, ScrollingHeatMapView fftView ) {
        mMfccView        = mfccView;
        mFftView         = fftView;
        mAudioAggregator = new AudioChunkAggregator();
        mMFCCJava        = new MFCCJava();
        mMFCCCPP         = new MFCCCPP();
        mUiHandler       = new Handler(Looper.getMainLooper());

    }

    public void onAudioArrivalMonauralPCM( short[] chunk ) {

        mAudioAggregator.putChunk( chunk );
        Handler uiHandler = new Handler( Looper.getMainLooper() );

        while ( mAudioAggregator.totalNumSamples() >= 400 ) {

            float[] samples400 = mAudioAggregator.getConsecutive400SamplesInfloat();

            double time1 = getTimeStampInSeconds();

            final float[] mfcc_fft_cpp  = mMFCCCPP. generateMFCCAndPowerSpectrum( 1, samples400 );

            double time2 = getTimeStampInSeconds();

            final float[] mfcc_fft_neon = mMFCCCPP. generateMFCCAndPowerSpectrum( 0, samples400 );

            double time3 = getTimeStampInSeconds();

            final float[] mfcc_fft_java = mMFCCJava.generateMFCCAndPowerSpectrum( 0, samples400 );

            double time4 = getTimeStampInSeconds();

            mAccumCount++;

            if ( mAccumCount >= 1000 ) {

                // Wait until the system stabilizes.
                mAccumTimeCPP  = mDecay * mAccumTimeCPP  + ( 1.0 - mDecay )*( time2 - time1 );
                mAccumTimeNEON = mDecay * mAccumTimeNEON + ( 1.0 - mDecay )*( time3 - time2 );
                mAccumTimeJava = mDecay * mAccumTimeJava + ( 1.0 - mDecay )*( time4 - time3 );
            }

            if ( mAccumCount >= 1000 && mAccumCount % 100 == 0 ) {
                Log.i( TAG, String.format(
                    "Iteration: %d  Time i NEON: %.6f Time in CPP: %.6f, Time in Java: %.6f",
                    mAccumCount, mAccumTimeNEON, mAccumTimeCPP, mAccumTimeJava
                ) );
            }

            for ( int i = 0; i < 27 ; i++ ) {
                mfcc_fft_neon[i] =mfcc_fft_neon[i] / 5.0f + 0.5f;
            }

            final float[] mfccArray = Arrays.copyOfRange( mfcc_fft_neon,  0, 27       );
            final float[] fftArray  = Arrays.copyOfRange( mfcc_fft_neon, 27, 27 + 256 );

            uiHandler.post( new Runnable() {
                @Override
                public void run() {
                try {
                    mMfccView.setNewColumn( mfccArray );
                    mFftView. setNewColumn( fftArray  );
                    } catch ( Exception e ) {
                        Log.e(TAG, "Exception: " + e);
                    }
                }
            } );
        }
    }

    private double getTimeStampInSeconds() {

        Instant timeStamp = Instant.now();
        double currentTime = (double) timeStamp.getEpochSecond();
        currentTime += ((double) timeStamp.getNano()) / 1000000000;
        return currentTime;
    }

    private final double
                   mDecay         = 0.999;
    private double mAccumTimeCPP  = 0.0;
    private double mAccumTimeNEON = 0.0;
    private double mAccumTimeJava = 0.0;
    private int    mAccumCount    = 0;


    AudioChunkAggregator mAudioAggregator;
    MFCCInterface        mMFCCJava;
    MFCCInterface        mMFCCCPP;
    ScrollingHeatMapView mMfccView;
    ScrollingHeatMapView mFftView;
    Handler              mUiHandler;
}
