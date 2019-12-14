package com.example.android_mfcc;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.Arrays;

public class TopLevelMFCCProcessor implements AudioReceiverListener {

    private static final String TAG = TopLevelMFCCProcessor.class.getSimpleName();

    TopLevelMFCCProcessor ( ScrollingHeatMapView mfccView, ScrollingHeatMapView fftView ) {
        mMfccView        = mfccView;
        mFftView         = fftView;
        mAudioAggregator = new AudioChunkAggregator();
        mMFCC            = new MFCCJava();
        mUiHandler       = new Handler(Looper.getMainLooper());
    }

    public void onAudioArrivalMonauralPCM( short[] chunk ) {

        mAudioAggregator.putChunk(chunk);
        Handler uiHandler = new Handler(Looper.getMainLooper());
        while ( mAudioAggregator.totalNumSamples() >= 400 ) {

            double[]       samples400 = mAudioAggregator.getConsecutive400SamplesInDouble();
            final double[] fft_mfcc   = mMFCC.generateMFCCAndPowerSpectrum( samples400 );
            final double[] mfccArray  = Arrays.copyOfRange(fft_mfcc, 0, 27);
            final double[] fftArray   = Arrays.copyOfRange(fft_mfcc, 27, 27 + 256);

            uiHandler.post( new Runnable() {
                @Override
                public void run() {
                    try {
                        mMfccView.setNewColumn(mfccArray);
                        mFftView.setNewColumn(fftArray);
                    } catch (Exception e) {
                        Log.e(TAG, "Exception: " + e);
                    }
                }
            });
        }
    }

    AudioChunkAggregator mAudioAggregator;
    MFCCJava             mMFCC;
    ScrollingHeatMapView mMfccView;
    ScrollingHeatMapView mFftView;
    Handler              mUiHandler;

}
