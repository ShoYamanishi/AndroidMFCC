package com.example.android_mfcc;

import android.media.AudioRecord;
import android.media.AudioFormat;
import android.media.MediaRecorder;


import android.util.Log;
import android.os.Handler;
import android.os.Looper;

public class AudioReceiver {

    private static final String TAG = AudioReceiver.class.getSimpleName();

    private static final int RECORDING_RATE =16000;
    private static final int CHANNEL = AudioFormat.CHANNEL_IN_MONO;
    private static final int FORMAT = AudioFormat.ENCODING_PCM_16BIT;


    private AudioRecord recorder;

    private Thread mReceivingThread;

    private static int BUFFER_SIZE;
    short[] buffer;

    int mRMS;
    int mPeak;

    AudioReceiverListener mListener;


    AudioReceiver(AudioReceiverListener listener) {
        mListener = listener;
        receiveAndForward();
    }

    void receiveAndForward() {
        BUFFER_SIZE = AudioRecord.getMinBufferSize( RECORDING_RATE, CHANNEL, FORMAT );
        recorder = new AudioRecord(MediaRecorder.AudioSource.MIC,
                RECORDING_RATE, CHANNEL, FORMAT, BUFFER_SIZE * 10);
        buffer = new short[BUFFER_SIZE];
        recorder.startRecording();

        mReceivingThread = new Thread(new Runnable() {

            @Override
            public void run() {
                try {
                    Handler uiHandler = new Handler(Looper.getMainLooper());
                    while (true) {

                        int lengthRead = recorder.read(buffer, 0, buffer.length);
                        //Log.i(TAG, "length read: " + String.valueOf(lengthRead) );
                        short[] arrayToForward = new short[lengthRead];
                        System.arraycopy( buffer, 0, arrayToForward, 0,  lengthRead );
                        mListener.onAudioArrivalMonauralPCM(arrayToForward);

                    }
                } catch (Exception e) {
                    Log.e(TAG, "Exception: " + e);
                }
            }
        });

        mReceivingThread.start();

    }

}